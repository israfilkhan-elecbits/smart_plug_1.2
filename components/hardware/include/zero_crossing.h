// smart_plug/components/hardware/include/zero_crossing.h
#ifndef ZERO_CROSSING_H
#define ZERO_CROSSING_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize zero-crossing detection
 * 
 * @param gpio_pin GPIO pin for zero-crossing input
 */
void zero_crossing_init(int gpio_pin);

/**
 * @brief Start zero-crossing detection (enable interrupt)
 */
void zero_crossing_start(void);

/**
 * @brief Stop zero-crossing detection (disable interrupt)
 */
void zero_crossing_stop(void);

/**
 * @brief Check if zero-crossing detected since last check
 * 
 * @return true if detected
 */
bool zero_crossing_detected(void);

/**
 * @brief Get last zero-crossing timestamp (microseconds)
 * 
 * @return uint32_t Timestamp in microseconds
 */
uint32_t zero_crossing_get_last_time(void);

/**
 * @brief Get last zero-crossing period (microseconds)
 * 
 * @return uint32_t Period in microseconds
 */
uint32_t zero_crossing_get_last_period(void);

/**
 * @brief Get zero-crossing counter
 * 
 * @return uint32_t Number of zero-crossings since start
 */
uint32_t zero_crossing_get_counter(void);

/**
 * @brief Calculate frequency from zero-crossing periods
 * 
 * @return float Frequency in Hz (0 if no valid period)
 */
float zero_crossing_calculate_frequency(void);

/**
 * @brief Wait for next zero-crossing with timeout
 * 
 * @param timeout_ms Timeout in milliseconds
 * @return true if zero-crossing detected within timeout
 */
bool zero_crossing_wait(uint32_t timeout_ms);

/**
 * @brief Reset zero-crossing detected flag
 */
void zero_crossing_reset_flag(void);

#ifdef __cplusplus
}
#endif

#endif /* ZERO_CROSSING_H */