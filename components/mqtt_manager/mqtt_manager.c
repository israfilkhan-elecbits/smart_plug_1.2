// smart_plug/components/mqtt_manager/mqtt_manager.c
#include "mqtt_manager.h"
#include "aws_certs.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_event.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_sntp.h"  // Only this SNTP header - NOT lwip/apps/sntp.h
#include <string.h>
#include <time.h>
#include <inttypes.h>

static const char *TAG = "MQTT_MGR";

/*===============================================================================
  Configuration (from Kconfig)
  ===============================================================================*/

#ifndef CONFIG_THING_NAME
#define CONFIG_THING_NAME "Smart_Plug_1"
#endif

#ifndef CONFIG_AWS_IOT_ENDPOINT
#define CONFIG_AWS_IOT_ENDPOINT "a1lgz1948lk3nw-ats.iot.ap-south-1.amazonaws.com"
#endif

#ifndef CONFIG_FIRMWARE_VERSION
#define CONFIG_FIRMWARE_VERSION "1.2.0"
#endif

/*===============================================================================
  Topics - EXACT match with original
  ===============================================================================*/

#define TOPIC_SHADOW_UPDATE     "$aws/things/" CONFIG_THING_NAME "/shadow/update"
#define TOPIC_SHADOW_DELTA      "$aws/things/" CONFIG_THING_NAME "/shadow/update/delta"
#define TOPIC_TELEMETRY         "smartplug/telemetry"
#define TOPIC_CONTROL           "smartplug/control"
#define TOPIC_LWT               "device/" CONFIG_THING_NAME "/state"

/*===============================================================================
  Static Variables
  ===============================================================================*/

static esp_mqtt_client_handle_t mqtt_client = NULL;
static mqtt_status_t current_status = MQTT_DISCONNECTED;
static shadow_state_t shadow_state = {0};
static bool shadow_initialized = false;

// Callbacks
static void (*relay_callback)(bool state) = NULL;
static void (*energy_reset_callback)(void) = NULL;
static void (*shadow_update_callback)(const shadow_state_t *state) = NULL;

// Time sync
static time_t last_time_sync = 0;
static SemaphoreHandle_t time_sync_semaphore = NULL;

// Reconnect
static uint32_t reconnect_attempts = 0;
static uint32_t last_reconnect_attempt = 0;
#define RECONNECT_INTERVAL_MS 5000

// LWT message (exact match with original)
#define LWT_MESSAGE_DISCONNECTED "{\"state\":{\"reported\":{\"device_status\":{\"connected\":\"false\"}}}}"
#define LWT_MESSAGE_CONNECTED    "{\"state\":{\"reported\":{\"device_status\":{\"connected\":\"true\"}}}}"

/*===============================================================================
  Time Synchronization
  ===============================================================================*/

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized");
    if (time_sync_semaphore) {
        xSemaphoreGive(time_sync_semaphore);
    }
}

bool mqtt_manager_sync_time(void)
{
    if (!time_sync_semaphore) {
        time_sync_semaphore = xSemaphoreCreateBinary();
    }
    
    ESP_LOGI(TAG, "Synchronizing time via NTP...");
    
    // Initialize SNTP - using ESP-IDF v5.5.2 API
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.nist.gov");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    
    // Wait for time to be set
    time_t now = 0;
    int retry = 0;
    const int max_retry = 20;
    
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < max_retry) {
        ESP_LOGD(TAG, "Waiting for time... (%d/%d)", retry, max_retry);
        vTaskDelay(pdMS_TO_TICKS(500));
        retry++;
    }
    
    time(&now);
    
    if (now < 1000000000) {
        ESP_LOGE(TAG, "Failed to synchronize time");
        return false;
    }
    
    // Wait for semaphore or timeout
    if (xSemaphoreTake(time_sync_semaphore, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGW(TAG, "Time sync notification timeout");
    }
    
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "Time synchronized: %s", asctime(&timeinfo));
    
    last_time_sync = now;
    return true;
}

time_t mqtt_manager_get_current_time(void)
{
    time_t now;
    time(&now);
    if (now < 1000000000) {
        return 0;
    }
    return now;
}

/*===============================================================================
  MQTT Event Handler
  ===============================================================================*/

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            current_status = MQTT_CONNECTED;
            reconnect_attempts = 0;
            
            // Subscribe to shadow delta
            esp_mqtt_client_subscribe(mqtt_client, TOPIC_SHADOW_DELTA, 1);
            ESP_LOGI(TAG, "Subscribed to: %s", TOPIC_SHADOW_DELTA);
            
            // Subscribe to control topic
            esp_mqtt_client_subscribe(mqtt_client, TOPIC_CONTROL, 1);
            ESP_LOGI(TAG, "Subscribed to: %s", TOPIC_CONTROL);
            
            // Publish connected LWT
            esp_mqtt_client_publish(mqtt_client, TOPIC_LWT,
                                    LWT_MESSAGE_CONNECTED, 0, 1, 1);
            
            // Request shadow
            esp_mqtt_client_publish(mqtt_client, "$aws/things/" CONFIG_THING_NAME "/shadow/get",
                                    "", 0, 0, 0);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            current_status = MQTT_DISCONNECTED;
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT unsubscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT published, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT data received, topic: %.*s", event->topic_len, event->topic);
            
            // Handle shadow delta
            if (strncmp(event->topic, TOPIC_SHADOW_DELTA, event->topic_len) == 0) {
                cJSON *root = cJSON_ParseWithLength(event->data, event->data_len);
                if (root) {
                    cJSON *state = cJSON_GetObjectItem(root, "state");
                    if (state) {
                        cJSON *relay = cJSON_GetObjectItem(state, "relay_status");
                        if (relay && cJSON_IsString(relay)) {
                            bool new_state = (strcmp(relay->valuestring, "true") == 0);
                            if (new_state != shadow_state.power) {
                                shadow_state.power = new_state;
                                if (relay_callback) {
                                    relay_callback(new_state);
                                }
                            }
                        }
                        
                        cJSON *reset = cJSON_GetObjectItem(state, "reset_energy");
                        if (reset && cJSON_IsString(reset) && 
                            strcmp(reset->valuestring, "true") == 0) {
                            if (energy_reset_callback) {
                                energy_reset_callback();
                            }
                            shadow_state.energy_total = 0;
                            shadow_state.last_reset_timestamp = time(NULL);
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            // Handle control topic
            else if (strncmp(event->topic, TOPIC_CONTROL, event->topic_len) == 0) {
                cJSON *root = cJSON_ParseWithLength(event->data, event->data_len);
                if (root) {
                    cJSON *relay = cJSON_GetObjectItem(root, "relay_state");
                    if (relay && cJSON_IsBool(relay)) {
                        bool new_state = cJSON_IsTrue(relay);
                        if (relay_callback) {
                            relay_callback(new_state);
                        }
                    }
                    
                    cJSON *reset = cJSON_GetObjectItem(root, "reset_energy");
                    if (reset && cJSON_IsBool(reset) && cJSON_IsTrue(reset)) {
                        if (energy_reset_callback) {
                            energy_reset_callback();
                        }
                    }
                }
                cJSON_Delete(root);
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            current_status = MQTT_ERROR;
            break;
            
        default:
            ESP_LOGD(TAG, "MQTT event: %d", event->event_id);
            break;
    }
}

/*===============================================================================
  Public API
  ===============================================================================*/

bool mqtt_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing MQTT manager");
    
    // Initialize shadow state
    memset(&shadow_state, 0, sizeof(shadow_state));
    shadow_state.power = false;
    shadow_state.overload_protection = true;
    shadow_state.energy_monitoring = true;
    
    return true;
}

bool mqtt_manager_start(void)
{
    if (mqtt_client) {
        ESP_LOGW(TAG, "MQTT already started");
        return true;
    }
    
    ESP_LOGI(TAG, "Starting MQTT manager");
    
    // Sync time first
    if (!mqtt_manager_sync_time()) {
        ESP_LOGW(TAG, "Time sync failed, continuing anyway");
    }
    
    // Generate client ID with random suffix (same as original)
    char client_id[64];
    uint32_t random_suffix = esp_random() & 0xFFFF;
    snprintf(client_id, sizeof(client_id), "%s_%04" PRIX32, CONFIG_THING_NAME, random_suffix);
    
    // Configure MQTT with TLS - CORRECT FOR ESP-IDF v5.5.2
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = "mqtts://" CONFIG_AWS_IOT_ENDPOINT ":8883",
            },
            .verification = {
                .use_global_ca_store = true,
                .skip_cert_common_name_check = false,
            },
        },
        .credentials = {
            .authentication = {
                .certificate = aws_cert_crt,
                .key = aws_cert_private,
            },
            .client_id = client_id,
            .set_null_client_id = false,
        },
        .session = {
            .keepalive = 15,
            .disable_clean_session = false,
            .last_will = {
                .topic = TOPIC_LWT,
                .msg = LWT_MESSAGE_DISCONNECTED,
                .msg_len = 0,
                .qos = 1,
                .retain = true,
            },
        },
        .buffer = {
            .size = 2048,
            .out_size = 2048,
        },
    };
    
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return false;
    }
    
    // Register event handler
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, 
                                   mqtt_event_handler, NULL);
    
    return true;
}

void mqtt_manager_stop(void)
{
    if (mqtt_client) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }
    current_status = MQTT_DISCONNECTED;
}

bool mqtt_manager_connect(void)
{
    if (!mqtt_client) {
        ESP_LOGE(TAG, "MQTT not initialized");
        return false;
    }
    
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected");
        return false;
    }
    
    if (current_status == MQTT_CONNECTED) {
        return true;
    }
    
    ESP_LOGI(TAG, "Connecting to AWS IoT...");
    current_status = MQTT_CONNECTING;
    reconnect_attempts++;
    
    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        current_status = MQTT_ERROR;
        return false;
    }
    
    return true;
}

void mqtt_manager_disconnect(void)
{
    if (mqtt_client && current_status == MQTT_CONNECTED) {
        // Publish disconnected LWT
        esp_mqtt_client_publish(mqtt_client, TOPIC_LWT,
                                LWT_MESSAGE_DISCONNECTED, 0, 1, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        esp_mqtt_client_stop(mqtt_client);
    }
    current_status = MQTT_DISCONNECTED;
}

void mqtt_manager_handle(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Auto-reconnect logic
    if (current_status == MQTT_DISCONNECTED || current_status == MQTT_ERROR) {
        if (now - last_reconnect_attempt > RECONNECT_INTERVAL_MS) {
            last_reconnect_attempt = now;
            if (wifi_manager_is_connected()) {
                mqtt_manager_connect();
            }
        }
    }
}

bool mqtt_manager_is_connected(void)
{
    return (current_status == MQTT_CONNECTED);
}

mqtt_status_t mqtt_manager_get_status(void)
{
    return current_status;
}

bool mqtt_manager_publish_telemetry(const char *json_payload)
{
    if (!mqtt_client || current_status != MQTT_CONNECTED) {
        ESP_LOGW(TAG, "Cannot publish: not connected");
        return false;
    }
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, TOPIC_TELEMETRY,
                                         json_payload, 0, 0, 0);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish telemetry");
        return false;
    }
    
    ESP_LOGD(TAG, "Telemetry published, msg_id=%d", msg_id);
    return true;
}

bool mqtt_manager_update_shadow(float voltage, float current, float power,
                                float energy, float temp, bool relay_state)
{
    if (!mqtt_client || current_status != MQTT_CONNECTED) {
        return false;
    }
    
    // Update shadow state
    shadow_state.voltage_reading = voltage;
    shadow_state.current_reading = current;
    shadow_state.power_reading = power;
    shadow_state.energy_total = energy;
    shadow_state.temperature = temp;
    shadow_state.power = relay_state;
    
    if (shadow_state.last_wake_up_time == 0) {
        shadow_state.last_wake_up_time = time(NULL);
    }
    
    // Build shadow update JSON (exact match with original)
    cJSON *root = cJSON_CreateObject();
    cJSON *state = cJSON_AddObjectToObject(root, "state");
    cJSON *reported = cJSON_AddObjectToObject(state, "reported");
    
    // Device details
    cJSON_AddStringToObject(reported, "welcome", "aws-iot");
    
    cJSON *device_details = cJSON_AddObjectToObject(reported, "device_details");
    cJSON_AddStringToObject(device_details, "device_id", CONFIG_THING_NAME);
    cJSON_AddStringToObject(device_details, "local_ip", wifi_manager_get_ip());
    cJSON_AddStringToObject(device_details, "wifi_ssid", wifi_manager_get_ssid());
    
    // OTA
    cJSON *ota = cJSON_AddObjectToObject(reported, "ota");
    cJSON_AddStringToObject(ota, "fw_version", CONFIG_FIRMWARE_VERSION);
    
    // Device diagnosis
    cJSON *diagnosis = cJSON_AddObjectToObject(reported, "device_diagnosis");
    cJSON_AddStringToObject(diagnosis, "network", "WiFi");
    cJSON_AddNumberToObject(diagnosis, "connection_attempt", reconnect_attempts);
    
    time_t now = time(NULL);
    if (now > 0) {
        cJSON_AddNumberToObject(diagnosis, "timestamp", now);
        
        if (shadow_state.last_reset_timestamp > 0) {
            int last_reset = now - shadow_state.last_reset_timestamp;
            if (last_reset < 0) last_reset = 0;
            cJSON_AddNumberToObject(diagnosis, "last_reset", last_reset);
        } else {
            cJSON_AddNumberToObject(diagnosis, "last_reset", 0);
        }
    }
    
    // Device status
    cJSON *status = cJSON_AddObjectToObject(reported, "device_status");
    cJSON_AddStringToObject(status, "connected", 
                           wifi_manager_is_connected() ? "true" : "false");
    cJSON_AddNumberToObject(status, "rssi", wifi_manager_get_rssi());
    
    // Meter details
    cJSON *meter = cJSON_AddObjectToObject(reported, "meter_details");
    cJSON_AddNumberToObject(meter, "current_reading", current);
    cJSON_AddNumberToObject(meter, "power_reading", power);
    cJSON_AddNumberToObject(meter, "energy_total", energy);
    cJSON_AddNumberToObject(meter, "voltage_reading", voltage);
    cJSON_AddNumberToObject(meter, "temperature", temp);
    
    // Relay status
    cJSON_AddStringToObject(reported, "relay_status", relay_state ? "true" : "false");
    
    // Desired section
    cJSON *desired = cJSON_AddObjectToObject(state, "desired");
    cJSON_AddStringToObject(desired, "welcome", "aws-iot");
    cJSON_AddStringToObject(desired, "relay_status", relay_state ? "true" : "false");
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to create shadow JSON");
        return false;
    }
    
    int msg_id = esp_mqtt_client_publish(mqtt_client, TOPIC_SHADOW_UPDATE,
                                         json_str, 0, 0, 0);
    
    free(json_str);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish shadow update");
        return false;
    }
    
    ESP_LOGD(TAG, "Shadow updated, msg_id=%d", msg_id);
    shadow_initialized = true;
    
    if (shadow_update_callback) {
        shadow_update_callback(&shadow_state);
    }
    
    return true;
}

const shadow_state_t* mqtt_manager_get_shadow_state(void)
{
    return &shadow_state;
}

void mqtt_manager_set_relay_callback(void (*callback)(bool state))
{
    relay_callback = callback;
}

void mqtt_manager_set_energy_reset_callback(void (*callback)(void))
{
    energy_reset_callback = callback;
}

void mqtt_manager_set_shadow_update_callback(void (*callback)(const shadow_state_t *state))
{
    shadow_update_callback = callback;
}