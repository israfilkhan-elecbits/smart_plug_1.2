// smart_plug/components/hardware/zero_crossing.c
#include "zero_crossing.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "ZC";

// Static variables for ISR and task communication
static volatile bool zc_detected = false;
static volatile uint32_t last_zc_time = 0;
static volatile uint32_t last_zc_period = 0;
static volatile uint32_t zc_counter = 0;
static int zc_gpio = -1;
static SemaphoreHandle_t zc_semaphore = NULL;

// ISR - must be in IRAM 
static void IRAM_ATTR zero_crossing_isr(void *arg)
{
    uint32_t now = (uint32_t)esp_timer_get_time();
    BaseType_t wake = pdFALSE;
    
    // Calculate period between zero crossings 
    if (last_zc_time > 0) {
        last_zc_period = now - last_zc_time;
    }
    
    last_zc_time = now;
    zc_detected = true;
    zc_counter++;
    
    // Give semaphore to task if waiting
    if (zc_semaphore != NULL) {
        xSemaphoreGiveFromISR(zc_semaphore, &wake);
    }
    
    if (wake) {
        portYIELD_FROM_ISR();
    }
}

void zero_crossing_init(int gpio_pin)
{
    zc_gpio = gpio_pin;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << zc_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE  // Rising edge interrupt 
    };
    gpio_config(&io_conf);
    
    // Create semaphore for task synchronization
    zc_semaphore = xSemaphoreCreateBinary();
    
    ESP_LOGI(TAG, "Zero-crossing initialized on GPIO %d", zc_gpio);
}

void zero_crossing_start(void)
{
    if (zc_gpio < 0) {
        ESP_LOGE(TAG, "Zero-crossing not initialized");
        return;
    }
    
    // Install ISR service if not already done
    static bool isr_service_installed = false;
    if (!isr_service_installed) {
        gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
        isr_service_installed = true;
    }
    
    // Add ISR handler 
    gpio_isr_handler_add(zc_gpio, zero_crossing_isr, NULL);
    
    // Reset state
    zc_detected = false;
    last_zc_time = 0;
    last_zc_period = 0;
    zc_counter = 0;
    
    ESP_LOGI(TAG, "Zero-crossing detection started");
}

void zero_crossing_stop(void)
{
    if (zc_gpio < 0) return;
    
    gpio_isr_handler_remove(zc_gpio);  // matches detachInterrupt()
    ESP_LOGI(TAG, "Zero-crossing detection stopped");
}

bool zero_crossing_detected(void)
{
    return zc_detected;
}

uint32_t zero_crossing_get_last_time(void)
{
    return last_zc_time;
}

uint32_t zero_crossing_get_last_period(void)
{
    return last_zc_period;
}

uint32_t zero_crossing_get_counter(void)
{
    return zc_counter;
}

float zero_crossing_calculate_frequency(void)
{
    if (last_zc_period == 0) {
        return 0.0f;
    }
    
    // Calculate frequency from period (period is in microseconds)
    // Formula: f = 1,000,000 / period (microseconds)
    float frequency = 1000000.0f / (float)last_zc_period;
    
    // Validate frequency (should be around 50Hz or 60Hz)
    if (frequency < 45.0f || frequency > 65.0f) {
        return 0.0f;
    }
    
    return frequency;
}

bool zero_crossing_wait(uint32_t timeout_ms)
{
    if (zc_semaphore == NULL) {
        return false;
    }
    
    // Wait for semaphore from ISR 
    if (xSemaphoreTake(zc_semaphore, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        return true;
    }
    
    return false;
}

void zero_crossing_reset_flag(void)
{
    zc_detected = false;
}
