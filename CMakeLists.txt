# smart_plug/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Set project 
set(EXTRA_COMPONENT_DIRS 
    components/ade9153a
    components/hardware
    components/wifi_manager
    components/mqtt_manager
)

# Include ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(smart_plug_1.2)