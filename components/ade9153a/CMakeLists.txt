# smart_plug/components/ade9153a/CMakeLists.txt
idf_component_register(SRCS "ade9153a_driver.c" "ade9153a_api.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES driver spi_flash nvs_flash esp_timer)  