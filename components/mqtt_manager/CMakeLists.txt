# components/mqtt_manager/CMakeLists.txt
idf_component_register(
    SRCS 
        "mqtt_manager.c"
        "aws_certs.c"          
    INCLUDE_DIRS "include"
    REQUIRES 
        mqtt
        json
        lwip
        nvs_flash 
        esp-tls
        esp_timer
        freertos
        wifi_manager    
)