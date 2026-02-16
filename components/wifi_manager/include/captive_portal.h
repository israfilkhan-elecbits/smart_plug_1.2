// smart_plug/components/wifi_manager/include/captive_portal.h
#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start captive portal (SoftAP + DNS + HTTP)
 */
void captive_portal_start(void);

/**
 * @brief Stop captive portal
 */
void captive_portal_stop(void);

/**
 * @brief Handle captive portal clients (call periodically)
 */
void captive_portal_handle(void);

/**
 * @brief Check if captive portal is running
 * 
 * @return true if running
 */
bool captive_portal_is_running(void);

/**
 * @brief Get the number of connected stations
 * 
 * @return uint8_t Number of connected clients
 */
uint8_t captive_portal_get_station_count(void);

#ifdef __cplusplus
}
#endif

#endif /* CAPTIVE_PORTAL_H */