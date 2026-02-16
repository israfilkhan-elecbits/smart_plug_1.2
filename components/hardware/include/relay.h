// smart_plug/components/hardware/include/relay.h
#ifndef RELAY_H
#define RELAY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize relay control
 * 
 * @param gpio_pin GPIO pin for relay
 * @param initial_state Initial state (true = ON, false = OFF)
 */
void relay_init(int gpio_pin, bool initial_state);

/**
 * @brief Set relay state
 * 
 * @param state true = ON, false = OFF
 */
void relay_set(bool state);

/**
 * @brief Get current relay state
 * 
 * @return true = ON, false = OFF
 */
bool relay_get_state(void);

/**
 * @brief Toggle relay state
 */
void relay_toggle(void);

#ifdef __cplusplus
}
#endif

#endif /* RELAY_H */