#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

add_subdirectory_ifdef(CONFIG_DECT_NRP_RPC common)
add_subdirectory_ifdef(CONFIG_DECT_NRP_RPC_CLIENT client)
add_subdirectory_ifdef(CONFIG_DECT_NRP_RPC_SERVER server)

