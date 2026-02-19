file(GLOB_RECURSE SOURCES
    "source/*.c"
    "source/*.cpp")

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS include
                       REQUIRES esp_timer esp_driver_gpio esp_driver_uart esp_driver_spi esp_driver_i2c)