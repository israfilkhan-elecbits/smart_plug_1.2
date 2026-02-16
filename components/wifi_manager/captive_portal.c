// smart_plug/components/wifi_manager/captive_portal.c
#include "captive_portal.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static const char *TAG = "CAPTIVE_PORTAL";

// HTTP server handle
static httpd_handle_t server = NULL;

// DNS server socket
static int dns_socket = -1;
static TaskHandle_t dns_task_handle = NULL;
static bool dns_running = false;

// SoftAP configuration
#define AP_SSID_PREFIX      "SmartPlug_Setup_"
#define AP_MAX_CONNECTIONS   4
#define AP_CHANNEL           6

// HTML content (embedded)
extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[] asm("_binary_index_html_end");

// Forward declarations
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t scan_get_handler(httpd_req_t *req);
static esp_err_t connect_post_handler(httpd_req_t *req);
static esp_err_t reset_get_handler(httpd_req_t *req);
static esp_err_t status_get_handler(httpd_req_t *req);
static esp_err_t captive_portal_detection_handler(httpd_req_t *req);
static esp_err_t redirect_to_root_handler(httpd_req_t *req);
static void dns_server_task(void *pvParameters);
static void restart_task_3s(void *arg);
static void restart_task_5s(void *arg);

// URI handlers
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t scan = {
    .uri       = "/scan",
    .method    = HTTP_GET,
    .handler   = scan_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t connect_uri = {
    .uri       = "/connect",
    .method    = HTTP_POST,
    .handler   = connect_post_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t reset = {
    .uri       = "/reset",
    .method    = HTTP_GET,
    .handler   = reset_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t status = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = status_get_handler,
    .user_ctx  = NULL
};

// Comprehensive list of captive portal detection URLs
static const char* captive_portal_paths[] = {
    "/generate_204",
    "/generate204",
    "/hotspot-detect.html",
    "/library/test/success.html",
    "/kindle-wifi/wifistub.html",
    "/success.txt",
    "/connecttest.txt",
    "/fwlink/",
    "/redirect",
    "/canonical.html",
    "/ncsi.txt",
    "/check_network_status",
    "/status.php",
    "/status.html",
    "/success.html",
    "/captive-portal",
    "/portal",
    "/hotspot",
    "/login",
    "/connect",
    "/check",
    "/nmctx",
    "/nocable",
    "/auth",
    "/authenticate",
    "/start",
    "/init",
    "/setup",
    "/config",
    "/configure",
    "/wifi",
    "/network",
    "/connection",
    "/detect",
    "/detect.html",
    "/detect.php",
    "/detect.jsp",
    "/detect.do",
    "/ncsi",
    "/ncsi.html",
    "/ncsi.php",
    "/ncsi.jsp",
    "/ncsi.do",
    "/hotspotui",
    "/hotspotui.html",
    "/hotspotui.php",
    "/hotspotui.jsp",
    "/hotspotui.do",
    NULL
};

// Common user agents that expect specific responses
static bool is_apple_device(const char* user_agent)
{
    if (!user_agent) return false;
    return (strstr(user_agent, "iPhone") != NULL ||
            strstr(user_agent, "iPad") != NULL ||
            strstr(user_agent, "iPod") != NULL ||
            strstr(user_agent, "Mac") != NULL ||
            strstr(user_agent, "Apple") != NULL);
}

static bool is_android_device(const char* user_agent)
{
    if (!user_agent) return false;
    return (strstr(user_agent, "Android") != NULL ||
            strstr(user_agent, "android") != NULL);
}

static bool is_windows_device(const char* user_agent)
{
    if (!user_agent) return false;
    return (strstr(user_agent, "Windows") != NULL ||
            strstr(user_agent, "MSIE") != NULL ||
            strstr(user_agent, "Trident") != NULL);
}

/* Restart helper functions */
static void restart_task_3s(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}

static void restart_task_5s(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}

/* Generate AP SSID from MAC address */
static void generate_ap_ssid(char *ssid, size_t len)
{
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_AP, mac);
    snprintf(ssid, len, "%s%02X%02X", AP_SSID_PREFIX, mac[4], mac[5]);
}

/* Start SoftAP */
static void start_softap(void)
{
    char ap_ssid[32];
    generate_ap_ssid(ap_ssid, sizeof(ap_ssid));
    
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .password = "",
            .max_connection = AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_OPEN,
            .channel = AP_CHANNEL,
        },
    };
    strlcpy((char *)wifi_config.ap.ssid, ap_ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen(ap_ssid);
    
    ESP_LOGI(TAG, "Starting SoftAP with SSID: %s", ap_ssid);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* URL decoding helper */
static void url_decode(const char *src, char *dst, size_t dst_len)
{
    size_t i = 0, j = 0;
    char code[3] = {0};
    
    while (src[i] && j < dst_len - 1) {
        if (src[i] == '%' && isxdigit((unsigned char)src[i+1]) && isxdigit((unsigned char)src[i+2])) {
            code[0] = src[i+1];
            code[1] = src[i+2];
            dst[j++] = (char)strtol(code, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

/* HTTP Handlers */

static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving root page");
    
    size_t html_len = index_html_end - index_html_start;
    
    // Add headers to prevent caching
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, html_len);
    
    return ESP_OK;
}

static esp_err_t scan_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Scanning networks");
    
    // Scan WiFi networks
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_records = NULL;
    
    esp_wifi_scan_start(NULL, true);
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count > 0) {
        ap_records = malloc(ap_count * sizeof(wifi_ap_record_t));
        if (ap_records) {
            esp_wifi_scan_get_ap_records(&ap_count, ap_records);
        }
    }
    
    // Build JSON response
    char *response = NULL;
    size_t response_len = 0;
    FILE *stream = open_memstream(&response, &response_len);
    
    fprintf(stream, "[");
    for (int i = 0; i < ap_count; i++) {
        if (i > 0) fprintf(stream, ",");
        
        // Simple JSON escaping for SSID
        fprintf(stream, "{\"ssid\":\"");
        for (int j = 0; ap_records[i].ssid[j]; j++) {
            if (ap_records[i].ssid[j] == '"' || ap_records[i].ssid[j] == '\\') {
                fputc('\\', stream);
            }
            fputc(ap_records[i].ssid[j], stream);
        }
        fprintf(stream, "\",\"rssi\":%d}", ap_records[i].rssi);
    }
    fprintf(stream, "]");
    fclose(stream);
    
    if (ap_records) {
        free(ap_records);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, response_len);
    
    free(response);
    
    return ESP_OK;
}

static esp_err_t connect_post_handler(httpd_req_t *req)
{
    char content[256];
    char ssid[64] = {0};
    char password[64] = {0};
    
    // Read POST data
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid POST data");
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    ESP_LOGI(TAG, "Received connection request");
    
    // Parse form data
    char *token = strtok(content, "&");
    while (token != NULL) {
        if (strncmp(token, "ssid=", 5) == 0) {
            url_decode(token + 5, ssid, sizeof(ssid));
        } else if (strncmp(token, "password=", 9) == 0) {
            url_decode(token + 9, password, sizeof(password));
        }
        token = strtok(NULL, "&");
    }
    
    if (strlen(ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "SSID: %s", ssid);
    
    // Save credentials to NVS
    extern bool wifi_manager_save_credentials(const char *ssid, const char *password);
    if (!wifi_manager_save_credentials(ssid, password)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save credentials");
        return ESP_FAIL;
    }
    
    // Set flag in system NVS
    nvs_handle_t nvs;
    if (nvs_open("system", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, "justSetup", 1);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
    
    // Send connecting page
    const char *response = 
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta http-equiv='refresh' content='3;url=/' />"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "</head>"
        "<body style='font-family: Arial; text-align: center; padding: 50px;'>"
        "<h2>Connecting...</h2>"
        "<p>Device will restart and connect to network.</p>"
        "<p>This page will refresh in 3 seconds.</p>"
        "</body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, strlen(response));
    
    // Schedule restart
    xTaskCreate(restart_task_3s, "restart_task", 2048, NULL, 5, NULL);
    
    return ESP_OK;
}

static esp_err_t reset_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Resetting credentials");
    
    extern bool wifi_manager_reset_credentials(void);
    wifi_manager_reset_credentials();
    
    const char *response = 
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta http-equiv='refresh' content='5;url=/' />"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "</head>"
        "<body style='font-family: Arial; text-align: center; padding: 50px;'>"
        "<h2>Reset Complete</h2>"
        "<p>WiFi credentials have been cleared.</p>"
        "<p>Restarting in 5 seconds...</p>"
        "</body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, strlen(response));
    
    // Schedule restart
    xTaskCreate(restart_task_5s, "restart_task", 2048, NULL, 5, NULL);
    
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char mac[18] = {0};
    uint8_t mac_bytes[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac_bytes);
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_bytes[0], mac_bytes[1], mac_bytes[2],
             mac_bytes[3], mac_bytes[4], mac_bytes[5]);
    
    char response[128];
    snprintf(response, sizeof(response), "{\"mac\":\"%s\"}", mac);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, strlen(response));
    
    return ESP_OK;
}

/* Captive portal detection handler */
static esp_err_t captive_portal_detection_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Captive portal detection: %s", req->uri);
    
    // Get user agent for device-specific responses
    char user_agent[128];
    esp_err_t ret = httpd_req_get_hdr_value_str(req, "User-Agent", user_agent, sizeof(user_agent));
    const char* user_agent_ptr = (ret == ESP_OK) ? user_agent : NULL;
    
    // Add headers to prevent caching
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    
    // Device-specific responses
    if (is_apple_device(user_agent_ptr)) {
        ESP_LOGD(TAG, "Apple device detected, sending success page");
        const char *response = "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, response, strlen(response));
        return ESP_OK;
    }
    else if (is_android_device(user_agent_ptr)) {
        ESP_LOGD(TAG, "Android device detected, sending 204");
        httpd_resp_set_status(req, "204 No Content");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    else if (is_windows_device(user_agent_ptr)) {
        ESP_LOGD(TAG, "Windows device detected, sending 204");
        httpd_resp_set_status(req, "204 No Content");
        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }
    
    // Default response - 204 No Content (works for most devices)
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* Redirect all other requests to root */
static esp_err_t redirect_to_root_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Redirecting to captive portal: %s", req->uri);
    
    // Add headers to prevent caching
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    
    // Redirect to the main setup page
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* DNS Server Task */
static void dns_server_task(void *pvParameters)
{
    char rx_buffer[512];
    struct sockaddr_in dest_addr;
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    
    // Create UDP socket
    dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (dns_socket < 0) {
        ESP_LOGE(TAG, "Failed to create DNS socket");
        dns_running = false;
        vTaskDelete(NULL);
        return;
    }
    
    // Allow reusing address
    int opt = 1;
    setsockopt(dns_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to port 53
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(53);
    
    int err = bind(dns_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to bind DNS socket (errno %d)", errno);
        close(dns_socket);
        dns_socket = -1;
        dns_running = false;
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "DNS server started on port 53");
    dns_running = true;
    
    // Get AP IP address
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("AP");
    uint32_t ap_ip = 0;
    
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ap_ip = ip_info.ip.addr;
        ESP_LOGI(TAG, "AP IP address: " IPSTR, IP2STR(&ip_info.ip));
    } else {
        // Default fallback
        ap_ip = (192 << 24) | (168 << 16) | (4 << 8) | 1;
        ESP_LOGW(TAG, "Could not get AP IP, using default: 192.168.4.1");
    }
    
    while (dns_running) {
        int len = recvfrom(dns_socket, rx_buffer, sizeof(rx_buffer) - 1, 0,
                          (struct sockaddr *)&source_addr, &socklen);
        
        if (len < 0) {
            if (dns_running) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            }
            continue;
        }
        
        // Log DNS query (first few bytes only to avoid flooding)
        ESP_LOGD(TAG, "DNS query from %s", inet_ntoa(source_addr.sin_addr));
        
        // Simple DNS response - always return our IP for any query
        uint8_t *response = (uint8_t *)rx_buffer;
        
        // Check if it's a query (QR bit not set)
        if ((response[2] & 0x80) == 0) {
            // Build response (copy query and set QR bit)
            response[2] |= 0x80;  // Set QR bit (response)
            response[3] |= 0x00;  // No error
            response[7] = 1;       // Set answer count to 1
            
            // Find the end of the query name
            int offset = 12; // Start after header
            while (offset < len && response[offset] != 0) {
                if ((response[offset] & 0xC0) == 0xC0) {
                    // Compression pointer - skip 2 bytes
                    offset += 2;
                    break;
                }
                offset += response[offset] + 1;
            }
            if (offset < len && response[offset] == 0) {
                offset++; // Skip the null terminator
            }
            
            // Skip QTYPE and QCLASS
            offset += 4;
            
            // Add answer section
            int answer_offset = offset;
            
            // Pointer to query name (compression)
            response[answer_offset++] = 0xC0;
            response[answer_offset++] = 0x0C;
            
            // Type A (host address)
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x01;
            
            // Class IN
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x01;
            
            // TTL (60 seconds)
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x3C;
            
            // Data length (4 bytes)
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x04;
            
            // Our IP address
            response[answer_offset++] = (ap_ip >> 0) & 0xFF;
            response[answer_offset++] = (ap_ip >> 8) & 0xFF;
            response[answer_offset++] = (ap_ip >> 16) & 0xFF;
            response[answer_offset++] = (ap_ip >> 24) & 0xFF;
            
            // Update total length
            int total_len = answer_offset;
            
            // Send response
            int sent = sendto(dns_socket, response, total_len, 0,
                             (struct sockaddr *)&source_addr, sizeof(source_addr));
            
            if (sent > 0) {
                ESP_LOGD(TAG, "DNS response sent to %s", inet_ntoa(source_addr.sin_addr));
            } else {
                ESP_LOGW(TAG, "Failed to send DNS response");
            }
        }
    }
    
    close(dns_socket);
    dns_socket = -1;
    vTaskDelete(NULL);
}

/* Public API */

void captive_portal_start(void)
{
    // Start SoftAP
    start_softap();
    
    // Small delay for AP to be ready
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Start DNS server task
    if (!dns_running) {
        xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
    }
    
    // Start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_uri_handlers = 80;  // Increased from 30 to 80 to accommodate all handlers
    config.stack_size = 8192;
    config.server_port = 80;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Register main handlers
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &scan);
        httpd_register_uri_handler(server, &connect_uri);
        httpd_register_uri_handler(server, &reset);
        httpd_register_uri_handler(server, &status);
        
        // Register all captive portal detection paths
        for (int i = 0; captive_portal_paths[i] != NULL; i++) {
            httpd_uri_t detection_uri = {
                .uri       = captive_portal_paths[i],
                .method    = HTTP_GET,
                .handler   = captive_portal_detection_handler,
                .user_ctx  = NULL
            };
            httpd_register_uri_handler(server, &detection_uri);
            
            // Also register POST method for same paths
            detection_uri.method = HTTP_POST;
            httpd_register_uri_handler(server, &detection_uri);
        }
        
        // Register catch-all handler (this should be last)
        httpd_uri_t catch_all = {
            .uri       = "/*",
            .method    = HTTP_GET,
            .handler   = redirect_to_root_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &catch_all);
        
        // Also catch POST requests
        catch_all.method = HTTP_POST;
        httpd_register_uri_handler(server, &catch_all);
        
        ESP_LOGI(TAG, "HTTP server started on port 80 with captive portal support");
        ESP_LOGI(TAG, "Registered main handlers + %d detection paths + catch-all", 
                 sizeof(captive_portal_paths)/sizeof(captive_portal_paths[0]) - 1);
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}

void captive_portal_stop(void)
{
    if (server) {
        httpd_stop(server);
        server = NULL;
    }
    
    dns_running = false;
    if (dns_socket >= 0) {
        close(dns_socket);
        dns_socket = -1;
    }
    if (dns_task_handle) {
        vTaskDelete(dns_task_handle);
        dns_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "Captive portal stopped");
}

void captive_portal_handle(void)
{
    // Nothing to do here
}

bool captive_portal_is_running(void)
{
    return (server != NULL);
}

uint8_t captive_portal_get_station_count(void)
{
    wifi_sta_list_t stations;
    esp_wifi_ap_get_sta_list(&stations);
    return stations.num;
}