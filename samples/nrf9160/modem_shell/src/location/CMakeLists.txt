#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_include_directories(app PRIVATE .)

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/location_shell.c)
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/location_cmd_utils.c)
target_sources_ifdef(CONFIG_MOSH_METRICS app PRIVATE location_metrics.c)
