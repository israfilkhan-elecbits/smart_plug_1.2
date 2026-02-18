// smart_plug/components/hardware/relay.c

#include "relay.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "RELAY";
static int relay_gpio = -1;
static bool current_state = false;

void relay_init(int gpio_pin, bool initial_state)
{
    relay_gpio = gpio_pin;
    current_state = initial_state;
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << relay_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    gpio_set_level(relay_gpio, current_state ? 1 : 0);
    ESP_LOGI(TAG, "Relay initialized on GPIO %d, initial state: %s", 
             relay_gpio, current_state ? "ON" : "OFF");
}

void relay_set(bool state)
{
    if (relay_gpio < 0) {
        ESP_LOGE(TAG, "Relay not initialized");
        return;
    }
    
    if (state != current_state) {
        current_state = state;
        gpio_set_level(relay_gpio, current_state ? 1 : 0);
        ESP_LOGI(TAG, "Relay turned %s (current_state=%d)", 
                 current_state ? "ON" : "OFF", current_state);
    } else {
        ESP_LOGD(TAG, "Relay already %s, no change", state ? "ON" : "OFF");
    }
}

bool relay_get_state(void)
{
    ESP_LOGD(TAG, "relay_get_state() returning: %d", current_state);
    return current_state;
}

void relay_toggle(void)
{
    ESP_LOGI(TAG, "Toggling relay from %s", current_state ? "ON" : "OFF");
    relay_set(!current_state);
}