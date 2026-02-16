// smart_plug/components/wifi_manager/include/wifi_manager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi connection status
 */
typedef enum {
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_CONNECTION_FAILED,
    WIFI_STATUS_SETUP_MODE,
} wifi_status_t;

// REMOVED: wifi_config_t structure - this conflicts with ESP-IDF

/**
 * @brief Initialize WiFi manager
 * 
 * @return true if successful
 */
bool wifi_manager_init(void);

/**
 * @brief Start WiFi manager
 * 
 * @return true if connected or in setup mode
 */
bool wifi_manager_start(void);

/**
 * @brief Stop WiFi manager
 */
void wifi_manager_stop(void);

/**
 * @brief WiFi manager main handler (call in main loop)
 */
void wifi_manager_handle(void);

/**
 * @brief Get current WiFi status
 * 
 * @return wifi_status_t Current status
 */
wifi_status_t wifi_manager_get_status(void);

/**
 * @brief Check if connected to WiFi
 * 
 * @return true if connected
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Check if in setup mode (captive portal)
 * 
 * @return true if in setup mode
 */
bool wifi_manager_is_setup_mode(void);

/**
 * @brief Get current IP address
 * 
 * @return const char* IP address string
 */
const char* wifi_manager_get_ip(void);

/**
 * @brief Get current RSSI
 * 
 * @return int RSSI in dBm
 */
int wifi_manager_get_rssi(void);

/**
 * @brief Get current SSID
 * 
 * @return const char* SSID string
 */
const char* wifi_manager_get_ssid(void);

/**
 * @brief Save WiFi credentials to NVS
 * 
 * @param ssid SSID to save
 * @param password Password to save
 * @return true if successful
 */
bool wifi_manager_save_credentials(const char *ssid, const char *password);

/**
 * @brief Reset WiFi credentials (erase from NVS)
 * 
 * @return true if successful
 */
bool wifi_manager_reset_credentials(void);

/**
 * @brief Connect with saved credentials
 * 
 * @return true if connection initiated
 */
bool wifi_manager_connect_saved(void);

/**
 * @brief Disconnect WiFi
 */
void wifi_manager_disconnect(void);

/**
 * @brief Start captive portal for setup
 */
void wifi_manager_start_captive_portal(void);

/**
 * @brief Stop captive portal
 */
void wifi_manager_stop_captive_portal(void);

/**
 * @brief Set LED callback for status indication
 * 
 * @param callback Function to call for LED control
 */
void wifi_manager_set_led_callback(void (*callback)(bool state));

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */