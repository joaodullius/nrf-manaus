# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gatway_ble)

target_sources(app PRIVATE src/main.c)

#add_subdirectory_ifdef(CONFIG_GATEWAY_BLE src/modules/gateway_ble)
add_subdirectory(../common/src/sensor_common ${CMAKE_CURRENT_BINARY_DIR}/sensor_common)
add_subdirectory(../concentrator/src/modules/worker_shadow_service ${CMAKE_CURRENT_BINARY_DIR}/worker_shadow_service)
add_subdirectory(src/modules/gateway_ble)
add_subdirectory(src/modules/gateway_lte)
add_subdirectory(src/modules/bt_simple_service_client)