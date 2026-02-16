# smart_plug/components/wifi_manager/CMakeLists.txt
idf_component_register(SRCS "wifi_manager.c" "captive_portal.c"
                    EMBED_FILES "index.html" 
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvs_flash esp_wifi esp_netif esp_event
                             esp_http_server esp_timer)