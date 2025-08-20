#
# Copyright (c) 2025 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
zephyr_library()
zephyr_library_sources(
	dect_nrf91.c
	dect_nrf91_rx.c
	dect_nrf91_ctrl.c
	dect_nrf91_utils.c
	dect_nrf91_settings.c
)
zephyr_library_sources_ifdef(CONFIG_DECT_NRP_MAC_BORDER_ROUTER dect_nrf91_sink.c)

# For ipv6.h
zephyr_library_include_directories(
	${ZEPHYR_BASE}/subsys/net/ip
)
