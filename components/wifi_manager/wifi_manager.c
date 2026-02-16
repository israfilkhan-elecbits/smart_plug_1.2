// smart_plug/components/wifi_manager/wifi_manager.c
#include "wifi_manager.h"
#include "captive_portal.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <string.h>

// Define MACSTR as fallback if not defined
#ifndef MACSTR
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#endif

static const char *TAG = "WIFI_MGR";

/* FreeRTOS event group for WiFi events */
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_FAIL_BIT         BIT1
#define WIFI_SETUP_BIT        BIT2

static EventGroupHandle_t wifi_event_group = NULL;
static wifi_status_t current_status = WIFI_STATUS_DISCONNECTED;
static char current_ip[16] = "0.0.0.0";
static char current_ssid[32] = "";
static int current_rssi = 0;
static bool setup_mode = false;
static bool auto_reconnect = true;
static uint32_t connection_timeout = 30000; // 30 seconds
static void (*led_callback)(bool state) = NULL;

/* NVS namespace for WiFi credentials */
#define NVS_NS_WIFI "wifi"

/* Saved credentials */
static char saved_ssid[32] = "";
static char saved_password[64] = "";

/* Forward declarations */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);

bool wifi_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing WiFi manager");
    
    // Create event group if not already created
    if (wifi_event_group == NULL) {
        wifi_event_group = xEventGroupCreate();
    }
    
    // Initialize TCP/IP network interface
    esp_netif_init();
    esp_event_loop_create_default();
    
    // Create default WiFi station and AP
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    // Set WiFi to station mode by default
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi manager initialized");
    return true;
}

static void load_credentials_from_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err;
    
    // Clear existing
    saved_ssid[0] = '\0';
    saved_password[0] = '\0';
    
    err = nvs_open(NVS_NS_WIFI, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No WiFi credentials namespace found");
        return;
    }
    
    size_t len = sizeof(saved_ssid);
    err = nvs_get_str(nvs, "ssid", saved_ssid, &len);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No SSID found in NVS");
    }
    
    len = sizeof(saved_password);
    err = nvs_get_str(nvs, "password", saved_password, &len);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No password found in NVS");
    }
    
    nvs_close(nvs);
    
    if (strlen(saved_ssid) > 0) {
        ESP_LOGI(TAG, "Loaded credentials for SSID: %s", saved_ssid);
    }
}

bool wifi_manager_save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    esp_err_t err;
    
    err = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_str(nvs, "ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save SSID: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return false;
    }
    
    err = nvs_set_str(nvs, "password", password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save password: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return false;
    }
    
    err = nvs_commit(nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return false;
    }
    
    nvs_close(nvs);
    
    // Update local copy
    strlcpy(saved_ssid, ssid, sizeof(saved_ssid));
    strlcpy(saved_password, password, sizeof(saved_password));
    
    ESP_LOGI(TAG, "WiFi credentials saved for SSID: %s", ssid);
    return true;
}

bool wifi_manager_reset_credentials(void)
{
    nvs_handle_t nvs;
    esp_err_t err;
    
    err = nvs_open(NVS_NS_WIFI, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }
    
    nvs_erase_key(nvs, "ssid");
    nvs_erase_key(nvs, "password");
    nvs_commit(nvs);
    nvs_close(nvs);
    
    // Clear local copy
    saved_ssid[0] = '\0';
    saved_password[0] = '\0';
    
    ESP_LOGI(TAG, "WiFi credentials reset");
    return true;
}

bool wifi_manager_connect_saved(void)
{
    // Load credentials from NVS
    load_credentials_from_nvs();
    
    if (strlen(saved_ssid) == 0) {
        ESP_LOGI(TAG, "No saved credentials found");
        return false;
    }
    
    ESP_LOGI(TAG, "Connecting to %s...", saved_ssid);
    
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strlcpy((char *)wifi_config.sta.ssid, saved_ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, saved_password, sizeof(wifi_config.sta.password));
    
    // Clear event group bits
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    current_status = WIFI_STATUS_CONNECTING;
    
    // Wait for connection or timeout
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(connection_timeout));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to %s", saved_ssid);
        current_status = WIFI_STATUS_CONNECTED;
        return true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to %s", saved_ssid);
        current_status = WIFI_STATUS_CONNECTION_FAILED;
        return false;
    } else {
        ESP_LOGE(TAG, "Connection timeout");
        current_status = WIFI_STATUS_CONNECTION_FAILED;
        return false;
    }
}

bool wifi_manager_start(void)
{
    // Try to connect with saved credentials
    if (wifi_manager_connect_saved()) {
        return true;
    }
    
    // If no saved credentials or connection failed, start captive portal
    ESP_LOGI(TAG, "Starting captive portal for setup");
    wifi_manager_start_captive_portal();
    return true;
}

void wifi_manager_stop(void)
{
    if (setup_mode) {
        wifi_manager_stop_captive_portal();
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
}

void wifi_manager_handle(void)
{
    static uint32_t last_reconnect = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (setup_mode) {
        captive_portal_handle();
        return;
    }
    
    // Auto-reconnect logic
    if (auto_reconnect && current_status == WIFI_STATUS_DISCONNECTED) {
        if (now - last_reconnect > 30000) { // Try every 30 seconds
            last_reconnect = now;
            if (strlen(saved_ssid) > 0) {
                ESP_LOGI(TAG, "Auto-reconnecting...");
                esp_wifi_connect();
            }
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    static int retry_count = 0;
    
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (current_status == WIFI_STATUS_CONNECTED) {
            ESP_LOGI(TAG, "WiFi disconnected");
            current_status = WIFI_STATUS_DISCONNECTED;
            if (led_callback) led_callback(false);
        }
        
        if (!setup_mode) {
            if (retry_count < 5) {
                esp_wifi_connect();
                retry_count++;
                ESP_LOGI(TAG, "Retry connection (%d/5)", retry_count);
            } else {
                xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
                retry_count = 0;
            }
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(current_ip, sizeof(current_ip),
                 IPSTR, IP2STR(&event->ip_info.ip));
        
        ESP_LOGI(TAG, "Got IP: %s", current_ip);
        current_status = WIFI_STATUS_CONNECTED;
        retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        
        if (led_callback) led_callback(true);
        
        // Get RSSI
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            current_rssi = ap_info.rssi;
            strlcpy(current_ssid, (char *)ap_info.ssid, sizeof(current_ssid));
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "AP started");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Station connected to AP: MAC: " MACSTR, MAC2STR(event->mac));
    }
}

bool wifi_manager_is_connected(void)
{
    return (current_status == WIFI_STATUS_CONNECTED);
}

bool wifi_manager_is_setup_mode(void)
{
    return setup_mode;
}

const char* wifi_manager_get_ip(void)
{
    return current_ip;
}

int wifi_manager_get_rssi(void)
{
    return current_rssi;
}

const char* wifi_manager_get_ssid(void)
{
    return current_ssid;
}

wifi_status_t wifi_manager_get_status(void)
{
    return current_status;
}

void wifi_manager_set_led_callback(void (*callback)(bool state))
{
    led_callback = callback;
}

void wifi_manager_disconnect(void)
{
    esp_wifi_disconnect();
    current_status = WIFI_STATUS_DISCONNECTED;
}

void wifi_manager_start_captive_portal(void)
{
    if (setup_mode) return;
    
    ESP_LOGI(TAG, "Starting captive portal");
    setup_mode = true;
    current_status = WIFI_STATUS_SETUP_MODE;
    
    // Stop station mode
    esp_wifi_stop();
    
    // Start captive portal
    captive_portal_start();
}

void wifi_manager_stop_captive_portal(void)
{
    if (!setup_mode) return;
    
    ESP_LOGI(TAG, "Stopping captive portal");
    setup_mode = false;
    
    // Stop captive portal
    captive_portal_stop();
    
    // Restart station mode
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    
    // Try to connect
    if (strlen(saved_ssid) > 0) {
        wifi_manager_connect_saved();
    }
}