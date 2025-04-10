#
# Copyright (c) 2025 Joao Dullius
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bt_simple_service_client.c)
target_include_directories(app PRIVATE .)
