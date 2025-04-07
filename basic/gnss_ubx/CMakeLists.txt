cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_temp_ble)

# Ensure sensor_common is registered correctly
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../sensor_common)

# Add main.c for this sensor module
target_sources(app PRIVATE src/main.c)
