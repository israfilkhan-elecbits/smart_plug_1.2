# smart_plug/components/hardware/CMakeLists.txt
idf_component_register(SRCS "relay.c" "led.c" "button.c" "zero_crossing.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver freertos esp_timer)  