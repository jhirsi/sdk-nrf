#
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
zephyr_library_named(dect_nrp_lib)

zephyr_library()
zephyr_library_include_directories(.)
zephyr_library_sources(src/utils/dect_nrp_utils.c)
