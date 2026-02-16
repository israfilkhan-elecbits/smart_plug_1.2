// smart_plug/components/hardware/button.c
#include "button.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h" 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "BUTTON";
static int button_gpio = -1;
static button_callback_t user_callback = NULL;
static TaskHandle_t button_task_handle = NULL;

// Debounce and timing constants 
#define DEBOUNCE_DELAY_MS    50
#define SHORT_PRESS_MAX_MS   1000
#define LONG_PRESS_MS        4000
#define VERY_LONG_PRESS_MS   7000
#define RESET_HOLD_MS        10000
#define PRESS_COOLDOWN_MS    500

// Button state machine
static struct {
    bool current_state;
    bool last_state;
    bool stable_state;
    uint32_t last_debounce_time;
    uint32_t press_start_time;
    uint32_t last_valid_press_time;
    bool press_active;
    button_event_t pending_event;
    uint32_t pending_param;
} btn = {0};

void button_init(int gpio_pin, button_callback_t callback)
{
    button_gpio = gpio_pin;
    user_callback = callback;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << button_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // External pull-up or internal
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Initialize state
    btn.current_state = gpio_get_level(button_gpio);
    btn.last_state = btn.current_state;
    btn.stable_state = btn.current_state;
    btn.last_debounce_time = esp_timer_get_time() / 1000;
    btn.press_active = false;
    btn.pending_event = BUTTON_EVENT_NONE;
    
    ESP_LOGI(TAG, "Button initialized on GPIO %d", button_gpio);
}

bool button_is_pressed(void)
{
    if (button_gpio < 0) return false;
    return (gpio_get_level(button_gpio) == 0); // Active low
}

static void process_button_event(button_event_t event, uint32_t param)
{
    if (user_callback) {
        user_callback(event, param);
    }
}

void button_task_handler(void)
{
    if (button_gpio < 0) return;
    
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Read current state
    btn.current_state = gpio_get_level(button_gpio);
    
    // Debounce logic
    if (btn.current_state != btn.last_state) {
        btn.last_debounce_time = now;
    }
    
    if ((now - btn.last_debounce_time) > DEBOUNCE_DELAY_MS) {
        if (btn.current_state != btn.stable_state) {
            btn.stable_state = btn.current_state;
            
            // State changed
            if (btn.stable_state == 0) { // Pressed (LOW)
                btn.press_start_time = now;
                btn.press_active = true;
                ESP_LOGD(TAG, "Button pressed");
            } else { // Released (HIGH)
                if (btn.press_active) {
                    uint32_t press_duration = now - btn.press_start_time;
                    btn.press_active = false;
                    
                    // Check if this is a valid press (not too short, not within cooldown)
                    if (press_duration > 50 && 
                        (now - btn.last_valid_press_time) > PRESS_COOLDOWN_MS) {
                        
                        btn.last_valid_press_time = now;
                        
                        if (press_duration < SHORT_PRESS_MAX_MS) {
                            ESP_LOGI(TAG, "Short press detected (%lu ms)", press_duration);
                            process_button_event(BUTTON_EVENT_SHORT_PRESS, press_duration);
                        }
                    }
                    
                    process_button_event(BUTTON_EVENT_RELEASED, press_duration);
                }
            }
        }
    }
    
    // Check for long press durations while button is pressed
    if (btn.press_active) {
        uint32_t hold_duration = now - btn.press_start_time;
        
        // Check thresholds in order
        if (hold_duration >= RESET_HOLD_MS) {
            static bool reset_reported = false;
            if (!reset_reported) {
                ESP_LOGW(TAG, "10 second hold detected - RESET");
                process_button_event(BUTTON_EVENT_HOLD_10S, hold_duration);
                reset_reported = true;
            }
        } else if (hold_duration >= VERY_LONG_PRESS_MS) {
            static bool very_long_reported = false;
            if (!very_long_reported) {
                ESP_LOGI(TAG, "7 second hold detected");
                process_button_event(BUTTON_EVENT_VERY_LONG_PRESS, hold_duration);
                very_long_reported = true;
            }
        } else if (hold_duration >= LONG_PRESS_MS) {
            static bool long_reported = false;
            if (!long_reported) {
                ESP_LOGI(TAG, "4 second hold detected");
                process_button_event(BUTTON_EVENT_LONG_PRESS, hold_duration);
                long_reported = true;
            }
        }
    } else {
        // Reset reporting flags when button released
        // These are static so they persist across function calls
        static bool reset_reported = false;
        static bool very_long_reported = false;
        static bool long_reported = false;
        
        // Reset all flags
        reset_reported = false;
        very_long_reported = false;
        long_reported = false;
    }
    
    btn.last_state = btn.current_state;
}

static void button_task(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(10); // Check every 10ms
    
    while (1) {
        vTaskDelayUntil(&last_wake, interval);
        button_task_handler();
    }
}

void button_task_start(void)
{
    if (button_task_handle == NULL) {
        xTaskCreate(button_task, "button_task", 2048, NULL, 6, &button_task_handle);
        ESP_LOGI(TAG, "Button task started");
    }
}