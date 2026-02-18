// smart_plug/components/hardware/include/led.h
#ifndef LED_H
#define LED_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED operation modes
 */
typedef enum {
    LED_MODE_OFF,           // LED always off
    LED_MODE_ON,            // LED always on
    LED_MODE_BLINK_SLOW,    // Slow blinking (500ms) - Setup mode
    LED_MODE_BLINK_FAST,    // Fast blinking (200ms) - 4s hold
    LED_MODE_BLINK_RAPID,   // Rapid blinking (100ms) - 7s hold
    LED_MODE_BLINK_PATTERN, // Custom pattern for reset indication
} led_mode_t;

/**
 * @brief Initialize LED
 * 
 * @param gpio_pin GPIO pin for LED
 */
void led_init(int gpio_pin);

/**
 * @brief Set LED mode
 * 
 * @param mode LED operation mode
 */
void led_set_mode(led_mode_t mode);

/**
 * @brief Get current LED mode
 * 
 * @return led_mode_t Current mode
 */
led_mode_t led_get_mode(void);

/**
 * @brief Set LED state directly (for manual control)
 * 
 * @param state true = ON, false = OFF
 */
void led_set_state(bool state);

/**
 * @brief LED task handler - must be called periodically
 */
void led_task_handler(void);

/**
 * @brief Start LED task (creates FreeRTOS task)
 */
void led_task_start(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_H */