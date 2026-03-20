#
# Copyright (c) 2026 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

add_subdirectory(common)
# Only add DECT NR+ RPC client when explicitly enabled.
if(CONFIG_DECT_NR_RPC_CLIENT STREQUAL "y" AND NOT CONFIG_DECT_NR_RPC_SERVER STREQUAL "y")
  add_subdirectory(client)
endif()
if(CONFIG_DECT_NR_RPC_SERVER STREQUAL "y")
  add_subdirectory(server)
endif()
