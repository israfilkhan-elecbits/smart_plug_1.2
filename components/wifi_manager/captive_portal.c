// smart_plug/components/wifi_manager/captive_portal.c
#include "captive_portal.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
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
#define AP_SSID_PREFIX      "SmartPlug_"
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
static void dns_server_task(void *pvParameters);
static void restart_task_3s(void *arg);
static void restart_task_5s(void *arg);

// URI handlers
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
};

static const httpd_uri_t scan = {
    .uri       = "/scan",
    .method    = HTTP_GET,
    .handler   = scan_get_handler,
};

static const httpd_uri_t connect_uri = {
    .uri       = "/connect",
    .method    = HTTP_POST,
    .handler   = connect_post_handler,
};

static const httpd_uri_t reset = {
    .uri       = "/reset",
    .method    = HTTP_GET,
    .handler   = reset_get_handler,
};

static const httpd_uri_t status = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = status_get_handler,
};

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

/* Configure SoftAP (don't start, just configure) */
static void configure_softap(void)
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
    
    ESP_LOGI(TAG, "Configuring SoftAP with SSID: %s", ap_ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
}

/* URL decoding helper */
static void url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src) {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst++ = '\0';
}

/* HTTP Handlers */
static esp_err_t root_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serving root page");
    
    size_t html_len = index_html_end - index_html_start;
    
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start, html_len);
    
    return ESP_OK;
}

static esp_err_t scan_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Scanning networks");
    
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    
    // Store original mode to restore if needed
    wifi_mode_t original_mode = mode;
    
    // If in AP-only mode, we can still scan in APSTA mode
    // The WiFi hardware supports scanning while in AP mode on ESP32
    
    // Configure scan for better results
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300
    };
    
    esp_err_t scan_err = esp_wifi_scan_start(&scan_config, true);
    if (scan_err != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(scan_err));
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_records = NULL;
    
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "Found %d networks", ap_count);
    
    if (ap_count > 0) {
        ap_records = malloc(ap_count * sizeof(wifi_ap_record_t));
        if (ap_records) {
            esp_wifi_scan_get_ap_records(&ap_count, ap_records);
        }
    }
    
    char *response = NULL;
    size_t response_len = 0;
    FILE *stream = open_memstream(&response, &response_len);
    
    fprintf(stream, "[");
    for (int i = 0; i < ap_count; i++) {
        if (i > 0) fprintf(stream, ",");
        
        // JSON escape SSID
        fprintf(stream, "{\"ssid\":\"");
        for (int j = 0; ap_records[i].ssid[j]; j++) {
            if (ap_records[i].ssid[j] == '"' || ap_records[i].ssid[j] == '\\') {
                fputc('\\', stream);
            }
            fputc(ap_records[i].ssid[j], stream);
        }
        fprintf(stream, "\",\"rssi\":%d,\"authmode\":%d}", 
                ap_records[i].rssi, ap_records[i].authmode);
    }
    fprintf(stream, "]");
    fclose(stream);
    
    if (ap_records) free(ap_records);
    
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
    
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid POST data");
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    ESP_LOGI(TAG, "Received connection request");
    
    char *token = strtok(content, "&");
    while (token) {
        if (strncmp(token, "ssid=", 5) == 0) {
            url_decode(ssid, token + 5);
        } else if (strncmp(token, "password=", 9) == 0) {
            url_decode(password, token + 9);
        }
        token = strtok(NULL, "&");
    }
    
    if (strlen(ssid) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "SSID: %s", ssid);
    
    extern bool wifi_manager_save_credentials(const char *ssid, const char *password);
    wifi_manager_save_credentials(ssid, password);
    
    nvs_handle_t nvs;
    if (nvs_open("system", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, "justSetup", 1);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
    
    const char *response = 
        "<!DOCTYPE html>"
        "<html><head>"
        "<meta http-equiv='refresh' content='3;url=/'>"
        "</head><body style='text-align:center;padding:50px;'>"
        "<h2>Connecting...</h2>"
        "<p>Device will restart and connect.</p>"
        "</body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, strlen(response));
    
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
        "<html><head>"
        "<meta http-equiv='refresh' content='5;url=/'>"
        "</head><body style='text-align:center;padding:50px;'>"
        "<h2>Reset Complete</h2>"
        "<p>Restarting in 5 seconds...</p>"
        "</body></html>";
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, response, strlen(response));
    
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

/* DNS Server Task */
static void dns_server_task(void *pvParameters)
{
    char rx_buffer[512];
    struct sockaddr_in dest_addr;
    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);
    
    dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (dns_socket < 0) {
        ESP_LOGE(TAG, "Failed to create DNS socket");
        dns_running = false;
        vTaskDelete(NULL);
        return;
    }
    
    int opt = 1;
    setsockopt(dns_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(53);
    
    if (bind(dns_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind DNS socket");
        close(dns_socket);
        dns_socket = -1;
        dns_running = false;
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "DNS server started on port 53");
    dns_running = true;
    
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("AP");
    uint32_t ap_ip = (192 << 24) | (168 << 16) | (4 << 8) | 1;
    
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ap_ip = ip_info.ip.addr;
    }
    
    while (dns_running) {
        int len = recvfrom(dns_socket, rx_buffer, sizeof(rx_buffer) - 1, 0,
                          (struct sockaddr *)&source_addr, &socklen);
        
        if (len < 0) continue;
        
        uint8_t *response = (uint8_t *)rx_buffer;
        
        if ((response[2] & 0x80) == 0) {
            response[2] |= 0x80;
            response[3] |= 0x00;
            response[7] = 1;
            
            int offset = 12;
            while (offset < len && response[offset] != 0) {
                if ((response[offset] & 0xC0) == 0xC0) {
                    offset += 2;
                    break;
                }
                offset += response[offset] + 1;
            }
            if (offset < len && response[offset] == 0) offset++;
            offset += 4;
            
            int answer_offset = offset;
            
            response[answer_offset++] = 0xC0;
            response[answer_offset++] = 0x0C;
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x01;
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x01;
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x3C;
            response[answer_offset++] = 0x00;
            response[answer_offset++] = 0x04;
            response[answer_offset++] = (ap_ip >> 0) & 0xFF;
            response[answer_offset++] = (ap_ip >> 8) & 0xFF;
            response[answer_offset++] = (ap_ip >> 16) & 0xFF;
            response[answer_offset++] = (ap_ip >> 24) & 0xFF;
            
            sendto(dns_socket, response, answer_offset, 0,
                   (struct sockaddr *)&source_addr, sizeof(source_addr));
        }
    }
    
    close(dns_socket);
    dns_socket = -1;
    vTaskDelete(NULL);
}

/* Public API */
void captive_portal_start(void)
{
    // Configure SoftAP (don't change mode, just configure)
    configure_softap();
    
    // Start DNS server
    if (!dns_running) {
        xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, &dns_task_handle);
    }
    
    // Start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &scan);
        httpd_register_uri_handler(server, &connect_uri);
        httpd_register_uri_handler(server, &reset);
        httpd_register_uri_handler(server, &status);
        
        ESP_LOGI(TAG, "HTTP server started on port 80");
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