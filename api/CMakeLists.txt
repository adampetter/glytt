file(GLOB_RECURSE SOURCES
    "source/*.c"
    "source/*.cpp")

list(APPEND SOURCES
    source/io/buzzer.cpp
    source/io/led.cpp
    source/system/cli.cpp
    source/transmission/bluetooth.cpp
    source/transmission/mock_transceiver.cpp
    source/transmission/wifi_frame.cpp
    source/transmission/wifi_ignore.cpp
    source/transmission/wifi_tracking.cpp)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS include
                       REQUIRES esp_timer esp_driver_gpio esp_driver_uart esp_driver_spi esp_driver_i2c esp_wifi esp_event esp_netif nvs_flash bt)