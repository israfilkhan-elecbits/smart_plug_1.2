// smart_plug/main/smart_plug.c
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "cJSON.h"

// Define HIGH/LOW for Arduino compatibility
#ifndef LOW
#define LOW 0
#endif
#ifndef HIGH
#define HIGH 1
#endif

#include "ade9153a_api.h"
#include "relay.h"
#include "led.h"
#include "button.h"
#include "zero_crossing.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"

static const char *TAG = "SMART_PLUG";

/*===============================================================================
  Configuration from Kconfig
  ===============================================================================*/

// Timing
#define MEASUREMENT_INTERVAL_MS     CONFIG_MEASUREMENT_INTERVAL_MS
#define PUBLISH_INTERVAL_MS         CONFIG_PUBLISH_INTERVAL_MS
#define STORAGE_SAVE_INTERVAL_MS    CONFIG_STORAGE_SAVE_INTERVAL_MS
#define OFFLINE_SAVE_INTERVAL_MS    CONFIG_OFFLINE_SAVE_INTERVAL_MS
#define DEBUG_INTERVAL_MS           CONFIG_DEBUG_INTERVAL_MS
#define LED_BLINK_INTERVAL_MS       CONFIG_LED_BLINK_INTERVAL_MS

// Pins
#define PIN_CS          CONFIG_CS_PIN
#define PIN_RESET       CONFIG_RESET_PIN
#define PIN_RELAY       CONFIG_RELAY_PIN
#define PIN_LED         CONFIG_STATUS_LED_PIN
#define PIN_BUTTON      CONFIG_BUTTON_PIN
#define PIN_ZC          CONFIG_ZC_PIN
#define PIN_SPI_MOSI    CONFIG_SPI_MOSI_PIN
#define PIN_SPI_MISO    CONFIG_SPI_MISO_PIN
#define PIN_SPI_SCK     CONFIG_SPI_SCK_PIN
#define SPI_SPEED_HZ    CONFIG_SPI_SPEED_HZ

// NVS namespaces
#define NVS_NS_SYSTEM   CONFIG_NVS_NS_SYSTEM
#define NVS_NS_WIFI     CONFIG_NVS_NS_WIFI
#define NVS_NS_METER    CONFIG_NVS_NS_METER_DATA

/*===============================================================================
  Calibration Data - From Arduino main.cpp
  ===============================================================================*/

typedef struct {
    float voltage_coefficient;     // For REG_AVRMS_2
    float current_coefficient;     // For REG_AIRMS_2
    float power_coefficient;        // For REG_AWATT
    float energy_coefficient;       // For REG_AWATTHR_HI
    float current_offset;           // Current offset for low values
} calibration_t;

// Default calibration values 
static const calibration_t DEFAULT_CALIBRATION = {
    .voltage_coefficient = 13.1488f,      // For REG_AVRMS_2
    .current_coefficient = 0.3711543f,   // For REG_AIRMS_2
    .power_coefficient = 0.66498695f,     // For REG_AWATT
    .energy_coefficient = 0.858307f,      // For REG_AWATTHR_HI
    .current_offset = 0.019f              // Current offset for low values
};

/*===============================================================================
  Data Structures
  ===============================================================================*/

typedef struct {
    int32_t raw_voltage_rms;
    uint32_t raw_current_rms;
    int32_t raw_active_power;
    int32_t raw_energy;
} raw_measurements_t;

typedef struct {
    float voltage_rms;
    float current_rms;
    float active_power;
    float apparent_power;
    float reactive_power;
    float power_factor;
    float frequency;
    float temperature;
    float energy_wh;
    bool waveform_clipped;
    
    int32_t avg_raw_voltage_rms;
    uint32_t avg_raw_current_rms;
    int32_t avg_raw_active_power;
    int32_t avg_raw_energy;
    
    bool synchronized;
    uint32_t zc_timestamp;
    float voltage_at_zc;
    float current_at_zc;
} measurements_t;

/*===============================================================================
  Static Variables
  ===============================================================================*/

static ade9153a_t ade_dev;
static measurements_t meas;
static calibration_t cal = DEFAULT_CALIBRATION;  // Always use defaults, never saved to NVS
static raw_measurements_t *raw_buffer = NULL;
static uint8_t buffer_index = 0;
static bool buffer_ready = false;

static bool ade_initialized = false;
static bool measurement_valid = false;
static bool zc_sync_enabled = true;

static float cumulative_energy = 0;
static uint32_t last_energy_calc_time = 0;
static uint32_t last_publish_time = 0;
static uint32_t last_storage_save = 0;
static uint32_t last_debug_print = 0;
static uint32_t system_start_time = 0;

static TaskHandle_t measurement_task_handle = NULL;
static TaskHandle_t mqtt_task_handle = NULL;

/*===============================================================================
  Helper function to clean up old calibration data from NVS 
  ===============================================================================*/

static void cleanup_old_calibration_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_METER, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        // Try to erase calibration key if it exists (from older firmware versions)
        err = nvs_erase_key(nvs, "calibration");
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Removed old calibration data from NVS");
            nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
}

/*===============================================================================
  NVS Storage Functions 
  ===============================================================================*/

static void save_energy_to_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_METER, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }
    
    err = nvs_set_blob(nvs, "energy_total", &cumulative_energy, sizeof(float));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save energy: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return;
    }
    
    uint8_t relay_state = relay_get_state() ? 1 : 0;
    err = nvs_set_u8(nvs, "relay_state", relay_state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save relay state: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return;
    }
    
    err = nvs_commit(nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "SAVED to NVS: energy=%.3f Wh, relay=%s (value=%d)",
                 cumulative_energy, relay_state ? "ON" : "OFF", relay_state);
    }
    
    nvs_close(nvs);
}

static void load_energy_from_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_METER, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved energy found, using defaults");
        return;
    }
    
    size_t len = sizeof(float);
    err = nvs_get_blob(nvs, "energy_total", &cumulative_energy, &len);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "No energy_total found");
        cumulative_energy = 0;
    }
    
    uint8_t relay_state = 0;
    err = nvs_get_u8(nvs, "relay_state", &relay_state);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS read: relay_state=%d (saved value)", relay_state);
        relay_set(relay_state != 0);
        ESP_LOGI(TAG, "Loaded relay state: %s", relay_state ? "ON" : "OFF");
    } else {
        ESP_LOGI(TAG, "No relay_state found in NVS, keeping default");
    }
    
    nvs_close(nvs);
    
    ESP_LOGI(TAG, "Loaded from NVS: energy=%.3f Wh", cumulative_energy);
}

static void save_offline_data(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NS_METER, NVS_READWRITE, &nvs);
    if (err != ESP_OK) return;
    
    nvs_set_blob(nvs, "last_voltage", &meas.voltage_rms, sizeof(float));
    nvs_set_blob(nvs, "last_current", &meas.current_rms, sizeof(float));
    nvs_set_blob(nvs, "last_power", &meas.active_power, sizeof(float));
    nvs_set_blob(nvs, "last_temp", &meas.temperature, sizeof(float));
    nvs_set_blob(nvs, "energy_total", &cumulative_energy, sizeof(float));
    
    uint8_t relay_state = relay_get_state() ? 1 : 0;
    nvs_set_u8(nvs, "relay_state", relay_state);
    
    uint32_t now = esp_timer_get_time() / 1000;
    nvs_set_u32(nvs, "last_save", now);
    
    nvs_commit(nvs);
    nvs_close(nvs);
    
    ESP_LOGD(TAG, "Offline data saved");
}

/*===============================================================================
  ADE9153A Functions 
  ===============================================================================*/

static void print_spi_debug(uint8_t *tx_data, int tx_len, uint8_t *rx_data, int rx_len)
{
    ESP_LOGI(TAG, "SPI Transaction:");
    printf("  TX: ");
    for (int i = 0; i < tx_len; i++) {
        printf("0x%02X ", tx_data[i]);
    }
    printf("\n");
    if (rx_data && rx_len > 0) {
        printf("  RX: ");
        for (int i = 0; i < rx_len; i++) {
            printf("0x%02X ", rx_data[i]);
        }
        printf("\n");
    }
}

static bool initialize_ade9153a(void)
{
    ESP_LOGI(TAG, "╔═══════════════════════════════════════════");
    ESP_LOGI(TAG, "║ Initializing ADE9153A");
    ESP_LOGI(TAG, "╚═══════════════════════════════════════════");
    
    uint8_t init_step = 0;
    
    //===========================================================
    // Step 1: Hardware Reset
    //===========================================================
    init_step = 1;
    ESP_LOGI(TAG, "[Step %d] Hardware reset", init_step);
    
    gpio_set_direction(PIN_RESET, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RESET, LOW);      // Pull reset low
    vTaskDelay(pdMS_TO_TICKS(10));        // 10ms reset pulse
    
    gpio_set_level(PIN_RESET, HIGH);     // Release reset
    vTaskDelay(pdMS_TO_TICKS(100));       // 100ms power-up time
    
    //===========================================================
    // Step 2: SPI Initialization
    //===========================================================
    init_step = 2;
    ESP_LOGI(TAG, "[Step %d] SPI initialization", init_step);
    
    if (!ade9153a_init(&ade_dev, SPI_SPEED_HZ, PIN_CS,
                       PIN_SPI_SCK, PIN_SPI_MOSI, PIN_SPI_MISO)) {
        ESP_LOGE(TAG, "Step %d failed: SPI init", init_step);
        return false;
    }
    
    //===========================================================
    // Step 3: Start DSP
    //===========================================================
    init_step = 3;
    ESP_LOGI(TAG, "[Step %d] Starting DSP", init_step);
    
    ade9153a_write_16(&ade_dev, REG_RUN, ADE9153A_RUN_ON);
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait for DSP to start
    
    //===========================================================
    // Step 4: Verify Chip ID
    //===========================================================
    init_step = 4;
    ESP_LOGI(TAG, "[Step %d] Verifying chip ID", init_step);
    
    uint32_t version = ade9153a_read_32(&ade_dev, REG_VERSION_PRODUCT);
    ESP_LOGI(TAG, "   Version Register: 0x%08lX", version);
    
    if (version != 0x0009153A) {
        ESP_LOGE(TAG, "Step %d failed: Expected 0x0009153A, got 0x%08lX", 
                 init_step, version);
        
        // Try one more time after longer delay
        ESP_LOGW(TAG, "Retrying communication...");
        vTaskDelay(pdMS_TO_TICKS(200));
        version = ade9153a_read_32(&ade_dev, REG_VERSION_PRODUCT);
        
        if (version != 0x0009153A) {
            ESP_LOGE(TAG, "ADE9153A still not detected after retry");
            return false;
        }
        ESP_LOGI(TAG, "ADE9153A detected on retry! ID=0x%08lX", version);
    } else {
        ESP_LOGI(TAG, "ADE9153A detected successfully!");
    }
    
    //===========================================================
    // Step 5: Configure Zero-Crossing Detection
    //===========================================================
    init_step = 5;
    ESP_LOGI(TAG, "[Step %d] Zero-crossing configuration", init_step);
    
    ade9153a_write_16(&ade_dev, REG_CFMODE, 0x0001);  // CF2 = zero-crossing output
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ade9153a_write_16(&ade_dev, REG_ZX_CFG, 0x0001);  // Enable ZX detection
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ade9153a_write_16(&ade_dev, REG_ZXTHRSH, 0x000A); // ZX threshold
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ade9153a_write_16(&ade_dev, REG_ZXTOUT, 0x03E8);  // Timeout value
    vTaskDelay(pdMS_TO_TICKS(1));
    
    //===========================================================
    // Step 6: Apply Standard Configuration
    //===========================================================
    init_step = 6;
    ESP_LOGI(TAG, "[Step %d] Standard configuration", init_step);
    
    ade9153a_setup(&ade_dev);
    
    //===========================================================
    // Step 7: Additional Configuration
    //===========================================================
    init_step = 7;
    ESP_LOGI(TAG, "[Step %d] Additional configuration", init_step);
    
    ade9153a_write_16(&ade_dev, REG_AI_PGAGAIN, 0x000A);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ade9153a_write_32(&ade_dev, REG_CONFIG0, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ade9153a_write_16(&ade_dev, REG_EP_CFG, ADE9153A_EP_CFG);  // Note: 0x0009 from their code
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ade9153a_write_16(&ade_dev, REG_EGY_TIME, ADE9153A_EGY_TIME);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    // Pre-calculated gain values from Arduino
    ade9153a_write_32(&ade_dev, REG_AVGAIN, 0xFFF36B16);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ade9153a_write_32(&ade_dev, REG_AIGAIN, 7316126);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ade9153a_write_16(&ade_dev, REG_PWR_TIME, 3906);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ade9153a_write_16(&ade_dev, REG_TEMP_CFG, 0x000C);
    vTaskDelay(pdMS_TO_TICKS(1));
    
    ade9153a_write_16(&ade_dev, REG_COMPMODE, 0x0005);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    //===========================================================
    // Step 8: Ensure RUN is still set
    //===========================================================
    init_step = 8;
    ESP_LOGI(TAG, "[Step %d] Verifying RUN state", init_step);
    
    ade9153a_write_16(&ade_dev, REG_RUN, 0x0001);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    //===========================================================
    // Step 9: Final Verification
    //===========================================================
    init_step = 9;
    ESP_LOGI(TAG, "[Step %d] Final verification", init_step);
    
    version = ade9153a_read_32(&ade_dev, REG_VERSION_PRODUCT);
    ESP_LOGI(TAG, "   Final verification: 0x%08lX", version);
    
    if (version != 0x0009153A) {
        ESP_LOGE(TAG, "Step %d failed: Communication lost", init_step);
        return false;
    }
    
    //===========================================================
    // Step 10: Initialize Zero-Crossing Hardware
    //===========================================================
    init_step = 10;
    ESP_LOGI(TAG, "[Step %d] Zero-crossing hardware", init_step);
    
    zero_crossing_init(PIN_ZC);
    zero_crossing_start();
    
    //===========================================================
    // Step 11: Clear measurement structure
    //===========================================================
    init_step = 11;
    memset(&meas, 0, sizeof(meas));
    
    ESP_LOGI(TAG, "\n ADE9153A initialization successful!");
    ESP_LOGI(TAG, "   All %d steps completed", init_step);
    
    return true;
}

static bool read_raw_measurement(raw_measurements_t *raw)
{
    if (!ade_initialized) return false;
    
    // Read registers - using synchronized registers
    raw->raw_voltage_rms = (int32_t)ade9153a_read_32(&ade_dev, REG_AVRMS_2);
    raw->raw_current_rms = ade9153a_read_32(&ade_dev, REG_AIRMS_2);
    raw->raw_active_power = (int32_t)ade9153a_read_32(&ade_dev, REG_AWATT);
    raw->raw_energy = (int32_t)ade9153a_read_32(&ade_dev, REG_AWATTHR_HI);
    
    // Quick verification
    uint32_t version_check = ade9153a_read_32(&ade_dev, REG_VERSION_PRODUCT);
    if (version_check != 0x0009153A) {
        ESP_LOGW(TAG, "Chip verification failed: 0x%08lX", version_check);
        return false;
    }
    
    return true;
}

static void apply_averaging(void)
{
    uint8_t avg_samples = CONFIG_DEFAULT_AVERAGE_SAMPLES;
    
    if (avg_samples < 1 || !raw_buffer) {
        if (buffer_index > 0) {
            meas.avg_raw_voltage_rms = raw_buffer[0].raw_voltage_rms;
            meas.avg_raw_current_rms = raw_buffer[0].raw_current_rms;
            meas.avg_raw_active_power = raw_buffer[0].raw_active_power;
            meas.avg_raw_energy = raw_buffer[0].raw_energy;
        }
        return;
    }
    
    int64_t sum_voltage = 0;
    uint64_t sum_current = 0;
    int64_t sum_power = 0;
    int64_t sum_energy = 0;
    
    uint8_t samples = buffer_ready ? avg_samples : buffer_index;
    if (samples == 0) return;
    
    for (uint8_t i = 0; i < samples; i++) {
        sum_voltage += raw_buffer[i].raw_voltage_rms;
        sum_current += raw_buffer[i].raw_current_rms;
        sum_power += raw_buffer[i].raw_active_power;
        sum_energy += raw_buffer[i].raw_energy;
    }
    
    meas.avg_raw_voltage_rms = sum_voltage / samples;
    meas.avg_raw_current_rms = sum_current / samples;
    meas.avg_raw_active_power = sum_power / samples;
    meas.avg_raw_energy = sum_energy / samples;
}

static bool read_measurements(void)
{
    if (!ade_initialized || !raw_buffer) return false;
    
    if (!read_raw_measurement(&raw_buffer[buffer_index])) {
        measurement_valid = false;
        return false;
    }
    
    buffer_index++;
    if (buffer_index >= CONFIG_DEFAULT_AVERAGE_SAMPLES) {
        buffer_index = 0;
        buffer_ready = true;
    }
    
    apply_averaging();
    measurement_valid = true;
    return true;
}

static void calculate_measurements(void)
{
    if (!measurement_valid) return;
    
    // Apply calibration to averaged values
    meas.voltage_rms = (float)meas.avg_raw_voltage_rms * 
                       cal.voltage_coefficient / 1000000.0f;
    
    meas.current_rms = (float)meas.avg_raw_current_rms * 
                       cal.current_coefficient / 1000000.0f;
    
    // Low current offset
    if (meas.current_rms < 0.5f) {
        meas.current_rms += cal.current_offset;
    }
    
    // Power calculation using averaged value
    float raw_power = fabsf((float)meas.avg_raw_active_power);
    meas.active_power = raw_power * cal.power_coefficient / 1000.0f;
    
    // Apparent power
    meas.apparent_power = meas.voltage_rms * meas.current_rms;
    
    // Reactive power
    if (meas.apparent_power > 0.1f) {
        float app_sq = meas.apparent_power * meas.apparent_power;
        float act_sq = meas.active_power * meas.active_power;
        meas.reactive_power = sqrtf(app_sq - act_sq);
    } else {
        meas.reactive_power = 0.0f;
    }
    
    // Calculate frequency from zero-crossing
    float zc_frequency = zero_crossing_calculate_frequency();
    if (zc_frequency > 0.0f) {
        meas.frequency = zc_frequency;
    } else {
        // Fallback to register-based frequency calculation
        uint32_t period_raw = ade9153a_read_32(&ade_dev, REG_APERIOD);
        if (period_raw > 0) {
            meas.frequency = (4000.0f * 65536.0f) / (float)(period_raw + 1);
        } else {
            meas.frequency = 0.0f;
        }
    }
    
    // Read power factor
    int32_t powerFactor_raw = (int32_t)ade9153a_read_32(&ade_dev, REG_APF);
    meas.power_factor = fabsf((float)powerFactor_raw / 134217728.0f);
    
    // Read temperature
    temperature_t temp;
    ade9153a_read_temperature(&ade_dev, &temp);
    meas.temperature = temp.TemperatureVal;
    
    // Check for clipping
    meas.waveform_clipped = (abs(meas.avg_raw_voltage_rms) > 8000000) || 
                           (meas.avg_raw_current_rms > 8000000);
    
    // Zero-crossing sync info
    if (zc_sync_enabled && zero_crossing_detected()) {
        meas.synchronized = true;
        meas.zc_timestamp = zero_crossing_get_last_time();
        zero_crossing_reset_flag();
    } else {
        meas.synchronized = false;
        meas.zc_timestamp = 0;
    }
}

static void update_energy_accumulation(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (last_energy_calc_time == 0) {
        last_energy_calc_time = now;
        return;
    }
    
    float delta_hours = (float)(now - last_energy_calc_time) / 3600000.0f;
    float energy_increment = meas.active_power * delta_hours;
    
    if (energy_increment > 0 && relay_get_state()) {
        cumulative_energy += energy_increment;
        meas.energy_wh = cumulative_energy;
        
        // Save energy periodically
        static float last_saved_energy = 0;
        if (fabsf(cumulative_energy - last_saved_energy) > 0.1f || 
            now - last_storage_save > STORAGE_SAVE_INTERVAL_MS) {
            save_energy_to_nvs();
            last_saved_energy = cumulative_energy;
            last_storage_save = now;
        }
    }
    
    last_energy_calc_time = now;
}

static void validate_measurements(void)
{
    // Basic sanity checks
    if (meas.voltage_rms > 300.0f) {
        measurement_valid = false;
        return;
    }
    
    if (meas.current_rms < 0.0f || meas.current_rms > 100.0f) {
        measurement_valid = false;
        return;
    }
    
    // Check frequency validity
    if (meas.frequency > 0 && (meas.frequency < 45.0f || meas.frequency > 65.0f)) {
        measurement_valid = false;
        return;
    }
    
    measurement_valid = true;
}

static void check_zc_synchronization(void)
{
    static uint32_t last_zc_check = 0;
    static uint32_t last_zc_count = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (now - last_zc_check > 10000) {  // Check every 10 seconds
        last_zc_check = now;
        
        uint32_t current_count = zero_crossing_get_counter();
        uint32_t zc_events = current_count - last_zc_count;
        last_zc_count = current_count;
        
        // Calculate expected zero-crossings (2 per cycle) at 50Hz
        uint32_t expected_zc_events = 10 * 2 * 50;
        
        if (zc_events < expected_zc_events * 0.5) {
            ESP_LOGW(TAG, "Low zero-crossing count: %lu (expected ~%lu)", 
                     zc_events, expected_zc_events);
        }
    }
}

static void print_measurements(void)
{
    static uint32_t measurement_count = 0;
    
    ESP_LOGI(TAG, "\n═══════════════════════════════════════════");
    ESP_LOGI(TAG, "         MEASUREMENT #%lu", ++measurement_count);
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    
    // System Status
    ESP_LOGI(TAG, "SYSTEM STATUS");
    ESP_LOGI(TAG, "   Relay:        %s", relay_get_state() ? "ON" : "OFF");
    ESP_LOGI(TAG, "   Temperature:  %.1f°C", meas.temperature);
    ESP_LOGI(TAG, "   Frequency:    %.2f Hz", meas.frequency);
    
    // Power Measurements
    ESP_LOGI(TAG, "\nPOWER MEASUREMENTS");
    ESP_LOGI(TAG, "   Voltage:      %7.3f V", meas.voltage_rms);
    ESP_LOGI(TAG, "   Current:      %7.3f A", meas.current_rms);
    ESP_LOGI(TAG, "   Power (Active): %6.3f W", meas.active_power);
    
    if (meas.reactive_power > 0.1f) {
        ESP_LOGI(TAG, "   Power (Reactive): %5.3f VAR", meas.reactive_power);
    }
    
    if (meas.apparent_power > 0.1f) {
        ESP_LOGI(TAG, "   Power (Apparent): %5.3f VA", meas.apparent_power);
    }
    
    // Energy & Power Quality
    ESP_LOGI(TAG, "\nENERGY & QUALITY");
    ESP_LOGI(TAG, "   Energy Total:  %.3f Wh", cumulative_energy);
    ESP_LOGI(TAG, "   Power Factor:  %.3f", meas.power_factor);
    
    // Status Indicators
    ESP_LOGI(TAG, "\nSTATUS INDICATORS");
    ESP_LOGI(TAG, "   Waveform:     %s", meas.waveform_clipped ? "CLIPPED" : "Clean");
    ESP_LOGI(TAG, "   ZC Sync:      %s", meas.synchronized ? "Synced" : "Pending");
    ESP_LOGI(TAG, "   Valid Data:   %s", measurement_valid ? "Valid" : "Invalid");
    
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
}

/*===============================================================================
  Reset Task - Separate task for handling device reset
  ===============================================================================*/

static void reset_device_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Reset task started - removing WiFi credentials and restarting");
    
    // Small delay to let button task complete
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Remove credentials from NVS
    ESP_LOGI(TAG, "Removing WiFi credentials now (10s hold reached)");
    wifi_manager_reset_credentials();
    
    // Wait for NVS write to complete
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "Restarting now...");
    esp_restart();
    
    // Should never reach here
    vTaskDelete(NULL);
}

/*===============================================================================
  Button Callback
  ===============================================================================*/

static void button_event_handler(button_event_t event, uint32_t param)
{
    static uint32_t last_valid_press = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    
    switch (event) {
        case BUTTON_EVENT_SHORT_PRESS:
            if (now - last_valid_press > CONFIG_PRESS_COOLDOWN_MS) {
                last_valid_press = now;
                ESP_LOGI(TAG, "Button short press - toggling relay");
                relay_toggle();
                
                // FORCE SAVE IMMEDIATELY
                ESP_LOGI(TAG, "Forcing immediate NVS save after button press");
                save_energy_to_nvs();
                
                // Update shadow if connected
                if (wifi_manager_is_connected() && mqtt_manager_is_connected()) {
                    mqtt_manager_update_shadow(meas.voltage_rms, meas.current_rms,
                                               meas.active_power, cumulative_energy,
                                               meas.temperature, relay_get_state());
                }
            }
            break;
            
        case BUTTON_EVENT_LONG_PRESS:
            ESP_LOGI(TAG, "Button long press (4s)");
            led_set_mode(LED_MODE_BLINK_FAST);
            break;
            
        case BUTTON_EVENT_VERY_LONG_PRESS:
            ESP_LOGI(TAG, "Button very long press (7s)");
            led_set_mode(LED_MODE_BLINK_RAPID);
            break;
            
        case BUTTON_EVENT_HOLD_10S:
            ESP_LOGW(TAG, "Button hold 10s - initiating reset sequence");
            led_set_mode(LED_MODE_BLINK_PATTERN);
            
            // Create a separate task for reset to avoid stack overflow
            xTaskCreate(reset_device_task, "reset_task", 4096, NULL, 10, NULL);
            break;
            
        case BUTTON_EVENT_RELEASED:
            // Only handle normal releases (less than 10s)
            if (param < 10000) {
                // Reset LED based on mode
                if (wifi_manager_is_setup_mode()) {
                    led_set_mode(LED_MODE_BLINK_SLOW);
                } else if (wifi_manager_is_connected()) {
                    led_set_mode(LED_MODE_ON);
                } else {
                    led_set_mode(LED_MODE_OFF);
                }
            }
            // If param >= 10000, we already handled it in HOLD_10S, so do nothing here
            break;
            
        default:
            break;
    }
}

/*===============================================================================
  MQTT Callbacks
  ===============================================================================*/

static void mqtt_relay_callback(bool state)
{
    static uint32_t last_relay_callback = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (now - last_relay_callback < 500) {
        return;
    }
    
    last_relay_callback = now;
    
    ESP_LOGI(TAG, "MQTT relay command: %s", state ? "ON" : "OFF");
    relay_set(state);
    
    // FORCE SAVE IMMEDIATELY
    ESP_LOGI(TAG, "Forcing immediate NVS save after MQTT command");
    save_energy_to_nvs();
    
    // Update shadow immediately
    if (mqtt_manager_is_connected()) {
        mqtt_manager_update_shadow(meas.voltage_rms, meas.current_rms,
                                   meas.active_power, cumulative_energy,
                                   meas.temperature, state);
    }
}

static void mqtt_energy_reset_callback(void)
{
    ESP_LOGI(TAG, "MQTT energy reset command");
    cumulative_energy = 0.0f;
    meas.energy_wh = 0.0f;
    save_energy_to_nvs();
    
    // Update shadow
    if (mqtt_manager_is_connected()) {
        mqtt_manager_update_shadow(meas.voltage_rms, meas.current_rms,
                                   meas.active_power, cumulative_energy,
                                   meas.temperature, relay_get_state());
    }
}

static void mqtt_shadow_callback(const shadow_state_t *state)
{
    static uint32_t last_shadow_update = 0;
    
    if (esp_timer_get_time()/1000 - last_shadow_update < 1000) {
        ESP_LOGD(TAG, "Shadow update too frequent");
        return;
    }
    
    last_shadow_update = esp_timer_get_time()/1000;
    ESP_LOGD(TAG, "Shadow updated");
}

/*===============================================================================
  Telemetry Publishing
  ===============================================================================*/

static void publish_telemetry(void)
{
    if (!wifi_manager_is_connected() || !mqtt_manager_is_connected()) {
        return;
    }
    
    time_t now = mqtt_manager_get_current_time();
    if (now == 0) {
        now = esp_timer_get_time() / 1000000;
    }
    
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddStringToObject(root, "device_id", CONFIG_THING_NAME);
    cJSON_AddNumberToObject(root, "timestamp", now);
    cJSON_AddNumberToObject(root, "Temperature", meas.temperature);
    cJSON_AddBoolToObject(root, "relay_state", relay_get_state());
    cJSON_AddStringToObject(root, "firmware_version", CONFIG_FIRMWARE_VERSION);
    
    cJSON *voltage = cJSON_AddObjectToObject(root, "voltage");
    cJSON_AddNumberToObject(voltage, "rms_v", meas.voltage_rms);
    
    cJSON *current = cJSON_AddObjectToObject(root, "current");
    cJSON_AddNumberToObject(current, "rms_a", meas.current_rms);
    
    cJSON *power = cJSON_AddObjectToObject(root, "power");
    cJSON_AddNumberToObject(power, "active_w", meas.active_power);
    cJSON_AddNumberToObject(power, "reactive_var", meas.reactive_power);
    cJSON_AddNumberToObject(power, "apparent_va", meas.apparent_power);
    
    cJSON *energy = cJSON_AddObjectToObject(root, "energy");
    cJSON_AddNumberToObject(energy, "cumulative_wh", cumulative_energy);
    
    cJSON *quality = cJSON_AddObjectToObject(root, "power_quality");
    cJSON_AddNumberToObject(quality, "power_factor", meas.power_factor);
    cJSON_AddNumberToObject(quality, "frequency_hz", meas.frequency);
    cJSON_AddStringToObject(quality, "phase_angle_deg", "0.000");
    
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddNumberToObject(wifi, "rssi_dbm", wifi_manager_get_rssi());
    cJSON_AddStringToObject(wifi, "ip_address", wifi_manager_get_ip());
    cJSON_AddStringToObject(wifi, "ssid", wifi_manager_get_ssid());
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (json_str) {
        mqtt_manager_publish_telemetry(json_str);
        free(json_str);
    }
    
    // Update shadow
    mqtt_manager_update_shadow(meas.voltage_rms, meas.current_rms,
                               meas.active_power, cumulative_energy,
                               meas.temperature, relay_get_state());
}

/*===============================================================================
  Measurement Task
  ===============================================================================*/

static void measurement_task(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(MEASUREMENT_INTERVAL_MS);
    
    ESP_LOGI(TAG, "Measurement task started, interval=%d ms", MEASUREMENT_INTERVAL_MS);
    
    while (1) {
        vTaskDelayUntil(&last_wake, interval);
        
        if (ade_initialized) {
            // Synchronize with zero-crossing before reading
            if (zc_sync_enabled) {
                if (zero_crossing_wait(50)) {  // Wait up to 50ms
                    meas.synchronized = true;
                    esp_rom_delay_us(100);
                } else {
                    meas.synchronized = false;
                }
            }
            
            if (read_measurements()) {
                calculate_measurements();
                update_energy_accumulation();
                validate_measurements();
            }
        }
        
        check_zc_synchronization();
        
        uint32_t now = esp_timer_get_time() / 1000;
        if (now - last_debug_print > DEBUG_INTERVAL_MS) {
            last_debug_print = now;
            print_measurements();
        }
    }
}

/*===============================================================================
  MQTT Task
  ===============================================================================*/

static void mqtt_task(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(100);
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    while (1) {
        vTaskDelayUntil(&last_wake, interval);
        
        wifi_manager_handle();
        mqtt_manager_handle();
        
        uint32_t now = esp_timer_get_time() / 1000;
        
        if (wifi_manager_is_connected() && mqtt_manager_is_connected()) {
            if (now - last_publish_time > PUBLISH_INTERVAL_MS) {
                last_publish_time = now;
                publish_telemetry();
            }
        } else if (!wifi_manager_is_connected()) {
            if (now - last_storage_save > OFFLINE_SAVE_INTERVAL_MS) {
                last_storage_save = now;
                save_offline_data();
            }
        }
        
        led_task_handler();
        button_task_handler();
    }
}

/*===============================================================================
  Main Application
  ===============================================================================*/

void app_main(void)
{
    ESP_LOGI(TAG, "\n═══════════════════════════════════════════");
    ESP_LOGI(TAG, "        SMART PLUG v%s", CONFIG_FIRMWARE_VERSION);
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "Device ID: %s", CONFIG_THING_NAME);
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupted, erasing...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
    
    // Clean up any old calibration data from previous firmware versions
    cleanup_old_calibration_nvs();
    
    system_start_time = esp_timer_get_time() / 1000;
    
    // Initialize LED and button first
    led_init(PIN_LED);
    button_init(PIN_BUTTON, button_event_handler);
    
    // Load saved energy and relay state FIRST
    ESP_LOGI(TAG, "Loading saved state from NVS...");
    load_energy_from_nvs();
    
    // THEN initialize relay (without overwriting the loaded state)
    // relay_init will use the state we just loaded via relay_set in load_energy_from_nvs
    relay_init(PIN_RELAY, relay_get_state());
    
    // Display calibration values
    ESP_LOGI(TAG, "Calibration: V=%.6f I=%.6f P=%.6f E=%.6f Offset=%.3f",
             cal.voltage_coefficient, cal.current_coefficient,
             cal.power_coefficient, cal.energy_coefficient,
             cal.current_offset);
    
    // Check if just completed setup
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS_SYSTEM, NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t just_setup = 0;
        if (nvs_get_u8(nvs, "justSetup", &just_setup) == ESP_OK && just_setup) {
            ESP_LOGI(TAG, "Just completed setup mode");
            nvs_close(nvs);
            
            nvs_open(NVS_NS_SYSTEM, NVS_READWRITE, &nvs);
            nvs_set_u8(nvs, "justSetup", 0);
            nvs_commit(nvs);
        }
        nvs_close(nvs);
    }
    
    // Initialize WiFi manager
    wifi_manager_init();
    wifi_manager_set_led_callback(led_set_state);
    
    // Start WiFi
    if (wifi_manager_start()) {
        ESP_LOGI(TAG, "WiFi manager started");
    }
    
    // Initialize ADE9153A
    ESP_LOGI(TAG, "Initializing ADE9153A...");
    ade_initialized = initialize_ade9153a();
    
    if (ade_initialized) {
        // Allocate averaging buffer
        if (CONFIG_DEFAULT_AVERAGE_SAMPLES > 0) {
            raw_buffer = malloc(CONFIG_DEFAULT_AVERAGE_SAMPLES * sizeof(raw_measurements_t));
            if (raw_buffer) {
                memset(raw_buffer, 0, CONFIG_DEFAULT_AVERAGE_SAMPLES * sizeof(raw_measurements_t));
                ESP_LOGI(TAG, "Averaging buffer allocated (%d samples)", CONFIG_DEFAULT_AVERAGE_SAMPLES);
            } else {
                ESP_LOGE(TAG, "Failed to allocate averaging buffer");
            }
        }
    } else {
        ESP_LOGE(TAG, "ADE9153A initialization failed");
        led_set_mode(LED_MODE_BLINK_SLOW);
    }
    
    // Initialize MQTT manager
    mqtt_manager_init();
    mqtt_manager_set_relay_callback(mqtt_relay_callback);
    mqtt_manager_set_energy_reset_callback(mqtt_energy_reset_callback);
    mqtt_manager_set_shadow_update_callback(mqtt_shadow_callback);
    
    // Start MQTT if WiFi is connected and not in setup mode
    if (wifi_manager_is_connected() && !wifi_manager_is_setup_mode()) {
        ESP_LOGI(TAG, "WiFi connected, waiting for network stability...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        if (mqtt_manager_start()) {
            ESP_LOGI(TAG, "MQTT manager started");
            vTaskDelay(pdMS_TO_TICKS(500));
            
            if (!mqtt_manager_connect()) {
                ESP_LOGW(TAG, "MQTT connection attempt failed, will retry in background");
            }
        } else {
            ESP_LOGE(TAG, "Failed to start MQTT manager");
        }
    } else {
        if (wifi_manager_is_setup_mode()) {
            ESP_LOGI(TAG, "In setup mode, MQTT not started");
        } else {
            ESP_LOGW(TAG, "WiFi not connected, MQTT will start when WiFi connects");
        }
    }
    
    // Start LED task
    led_task_start();
    
    // Start button task
    button_task_start();
    
    // Set initial LED mode
    if (wifi_manager_is_setup_mode()) {
        led_set_mode(LED_MODE_BLINK_SLOW);
    } else if (wifi_manager_is_connected()) {
        led_set_mode(LED_MODE_ON);
    } else {
        led_set_mode(LED_MODE_OFF);
    }
    
    // Create tasks
    xTaskCreate(measurement_task, "measure", 4096, NULL, 5, &measurement_task_handle);
    xTaskCreate(mqtt_task, "mqtt", 8192, NULL, 4, &mqtt_task_handle);
    
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, "System ready - Uptime: %lu ms", system_start_time);
    ESP_LOGI(TAG, "Relay final state after boot: %s", relay_get_state() ? "ON" : "OFF");
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
}