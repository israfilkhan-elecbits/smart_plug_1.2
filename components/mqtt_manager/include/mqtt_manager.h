// smart_plug/components/mqtt_manager/include/mqtt_manager.h
#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Shadow state structure (matches original)
 */
typedef struct {
    bool power;                         // Relay state
    bool overload_protection;            // Enable overload protection
    bool energy_monitoring;               // Enable energy monitoring
    float voltage_reading;                // Voltage reading in volts
    float current_reading;                 // Current reading in amps
    float power_reading;                   // Power reading in watts
    float energy_total;                    // Total energy in Wh
    float temperature;                     // Temperature in °C
    time_t last_wake_up_time;               // Last system wake-up time
    time_t last_reset_timestamp;            // Actual reset timestamp
    uint32_t connection_attempts;            // Connection attempts
} shadow_state_t;

/**
 * @brief MQTT connection status
 */
typedef enum {
    MQTT_DISCONNECTED,
    MQTT_CONNECTING,
    MQTT_CONNECTED,
    MQTT_ERROR
} mqtt_status_t;

/**
 * @brief Initialize MQTT manager
 * 
 * @return true if successful
 */
bool mqtt_manager_init(void);

/**
 * @brief Start MQTT manager
 * 
 * @return true if started
 */
bool mqtt_manager_start(void);

/**
 * @brief Stop MQTT manager
 */
void mqtt_manager_stop(void);

/**
 * @brief MQTT manager main handler (call in main loop)
 */
void mqtt_manager_handle(void);

/**
 * @brief Connect to AWS IoT MQTT broker
 * 
 * @return true if connected
 */
bool mqtt_manager_connect(void);

/**
 * @brief Disconnect from MQTT broker
 */
void mqtt_manager_disconnect(void);

/**
 * @brief Check if MQTT is connected
 * 
 * @return true if connected
 */
bool mqtt_manager_is_connected(void);

/**
 * @brief Get current MQTT status
 * 
 * @return mqtt_status_t Current status
 */
mqtt_status_t mqtt_manager_get_status(void);

/**
 * @brief Publish telemetry data
 * 
 * @param json_payload JSON string to publish
 * @return true if published
 */
bool mqtt_manager_publish_telemetry(const char *json_payload);

/**
 * @brief Update device shadow
 * 
 * @param voltage Voltage reading
 * @param current Current reading
 * @param power Power reading
 * @param energy Energy total
 * @param temp Temperature
 * @param relay_state Relay state
 * @return true if updated
 */
bool mqtt_manager_update_shadow(float voltage, float current, float power,
                                float energy, float temp, bool relay_state);

/**
 * @brief Get current shadow state
 * 
 * @return const shadow_state_t* Pointer to shadow state
 */
const shadow_state_t* mqtt_manager_get_shadow_state(void);

/**
 * @brief Synchronize system time via NTP
 * 
 * @return true if time synchronized
 */
bool mqtt_manager_sync_time(void);

/**
 * @brief Get current epoch time
 * 
 * @return time_t Current time (0 if not synced)
 */
time_t mqtt_manager_get_current_time(void);

/**
 * @brief Set relay control callback
 * 
 * @param callback Function to call when relay command received
 */
void mqtt_manager_set_relay_callback(void (*callback)(bool state));

/**
 * @brief Set energy reset callback
 * 
 * @param callback Function to call when energy reset command received
 */
void mqtt_manager_set_energy_reset_callback(void (*callback)(void));

/**
 * @brief Set shadow update callback
 * 
 * @param callback Function to call when shadow updated
 */
void mqtt_manager_set_shadow_update_callback(void (*callback)(const shadow_state_t *state));

#ifdef __cplusplus
}
#endif

#endif /* MQTT_MANAGER_H */