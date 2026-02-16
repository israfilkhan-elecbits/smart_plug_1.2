// smart_plug/components/hardware/led.c
#include "led.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "LED";
static int led_gpio = -1;
static led_mode_t current_mode = LED_MODE_OFF;
static bool led_state = false;
static uint32_t last_blink_time = 0;
static uint32_t pattern_counter = 0;
static TaskHandle_t led_task_handle = NULL;

// Blink intervals in milliseconds
#define BLINK_SLOW_INTERVAL  500
#define BLINK_FAST_INTERVAL  200
#define BLINK_RAPID_INTERVAL 100

void led_init(int gpio_pin)
{
    led_gpio = gpio_pin;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << led_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    gpio_set_level(led_gpio, 0);
    ESP_LOGI(TAG, "LED initialized on GPIO %d", led_gpio);
}

void led_set_mode(led_mode_t mode)
{
    if (current_mode != mode) {
        current_mode = mode;
        pattern_counter = 0;
        ESP_LOGD(TAG, "LED mode set to %d", mode);
    }
}

led_mode_t led_get_mode(void)
{
    return current_mode;
}

void led_set_state(bool state)
{
    if (led_gpio < 0) return;
    led_state = state;
    gpio_set_level(led_gpio, state ? 1 : 0);
}

static void led_update(void)
{
    if (led_gpio < 0) return;
    
    uint32_t now = esp_timer_get_time() / 1000; // Convert to milliseconds
    uint32_t interval = 0;
    bool new_state = false;
    
    switch (current_mode) {
        case LED_MODE_OFF:
            new_state = false;
            break;
            
        case LED_MODE_ON:
            new_state = true;
            break;
            
        case LED_MODE_BLINK_SLOW:
            interval = BLINK_SLOW_INTERVAL;
            if (now - last_blink_time >= interval) {
                last_blink_time = now;
                led_state = !led_state;
            }
            new_state = led_state;
            break;
            
        case LED_MODE_BLINK_FAST:
            interval = BLINK_FAST_INTERVAL;
            if (now - last_blink_time >= interval) {
                last_blink_time = now;
                led_state = !led_state;
            }
            new_state = led_state;
            break;
            
        case LED_MODE_BLINK_RAPID:
            interval = BLINK_RAPID_INTERVAL;
            if (now - last_blink_time >= interval) {
                last_blink_time = now;
                led_state = !led_state;
            }
            new_state = led_state;
            break;
            
        case LED_MODE_BLINK_PATTERN:
            // Pattern: 2 fast blinks, pause - for reset indication
            interval = 100;
            if (now - last_blink_time >= interval) {
                last_blink_time = now;
                pattern_counter++;
                
                // Pattern: ON, ON, OFF, OFF, OFF, OFF, repeat
                if (pattern_counter % 8 == 0 || pattern_counter % 8 == 1) {
                    new_state = true;
                } else {
                    new_state = false;
                }
                led_state = new_state;
            }
            new_state = led_state;
            break;
    }
    
    gpio_set_level(led_gpio, new_state ? 1 : 0);
}

void led_task_handler(void)
{
    led_update();
}

static void led_task(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(50); // Update every 50ms
    
    while (1) {
        vTaskDelayUntil(&last_wake, interval);
        led_update();
    }
}

void led_task_start(void)
{
    if (led_task_handle == NULL) {
        xTaskCreate(led_task, "led_task", 2048, NULL, 5, &led_task_handle);
        ESP_LOGI(TAG, "LED task started");
    }
}