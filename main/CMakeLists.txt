# main/CMakeLists.txt
idf_component_register(
    SRCS 
        "smart_plug.c"
    INCLUDE_DIRS "."
    REQUIRES
        ade9153a
        wifi_manager
        mqtt_manager
        hardware
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        esp_timer
        mqtt
        json           
        driver
        freertos
        lwip
        esp-tls
        mbedtls
)