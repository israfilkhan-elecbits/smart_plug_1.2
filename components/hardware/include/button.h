// smart_plug/components/hardware/include/button.h
#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Button event types
 */
typedef enum {
    BUTTON_EVENT_NONE,
    BUTTON_EVENT_SHORT_PRESS,    // < 1 second
    BUTTON_EVENT_LONG_PRESS,      // 4 seconds
    BUTTON_EVENT_VERY_LONG_PRESS, // 7 seconds
    BUTTON_EVENT_HOLD_10S,        // 10 seconds (reset)
    BUTTON_EVENT_RELEASED,
} button_event_t;

/**
 * @brief Button callback function type
 */
typedef void (*button_callback_t)(button_event_t event, uint32_t param);

/**
 * @brief Initialize button
 * 
 * @param gpio_pin GPIO pin for button (with pull-up)
 * @param callback Callback function for button events
 */
void button_init(int gpio_pin, button_callback_t callback);

/**
 * @brief Button task handler - must be called periodically
 */
void button_task_handler(void);

/**
 * @brief Start button task (creates FreeRTOS task)
 */
void button_task_start(void);

/**
 * @brief Get current button state
 * 
 * @return true = pressed (LOW), false = released (HIGH)
 */
bool button_is_pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */