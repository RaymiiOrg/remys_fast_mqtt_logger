cmake_minimum_required(VERSION 3.11)

project(remys_fast_mqtt_logger)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

find_package(Threads REQUIRED)

add_executable(remys_fast_mqtt_logger main.cpp)

target_link_libraries(remys_fast_mqtt_logger
        PRIVATE
        ${PAHO_MQTT_C_LIBRARIES}
        ${PAHO_MQTT_CPP_LIBRARIES}
        Threads::Threads
        paho-mqttpp3
        paho-mqtt3as
)

install(TARGETS remys_fast_mqtt_logger
        RUNTIME DESTINATION /usr/bin
)