# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_adv_adv)


target_sources(app PRIVATE src/main.c)

#Módulo Local
add_subdirectory(src/modules/parser_gnss)

#Módulo Comum Obrigatório
add_subdirectory(../common/src/sensor_common ${CMAKE_CURRENT_BINARY_DIR}/sensor_common)
add_subdirectory(../common/src/sensor_ble ${CMAKE_CURRENT_BINARY_DIR}/sensor_ble)

#Módulo Comum Opcional
add_subdirectory_ifdef(CONFIG_BATTERY_LEVEL ../common/src/battery_level ${CMAKE_CURRENT_BINARY_DIR}/battery_level)
add_subdirectory_ifdef(CONFIG_SENSOR_BLE_SERVICE ../common/src/sensor_ble_service ${CMAKE_CURRENT_BINARY_DIR}/sensor_ble_service)

