# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sensor_adv_adv)


target_sources(app PRIVATE src/main.c)
#Módulo Comum Obrigatório
add_subdirectory(../common/src/sensor_common ${CMAKE_CURRENT_BINARY_DIR}/sensor_common)
add_subdirectory(src/modules/sensor_scanner)
add_subdirectory(src/modules/concentrator_periph)
add_subdirectory(src/modules/worker_shadow_service)
add_subdirectory_ifdef(CONFIG_ACCEPT_LIST src/modules/accept_list_service)


