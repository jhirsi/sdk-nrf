#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library()

zephyr_library_sources(
  dect_rpc_common.c
  dect_rpc_group.c
)

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
)
