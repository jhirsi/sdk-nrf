#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_include_directories(app PRIVATE .)

target_sources(app PRIVATE auto_connect.c)
target_sources(app PRIVATE auto_connect_shell.c)
