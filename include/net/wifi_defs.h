/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef WIFI_DEFS_H_
#define WIFI_DEFS_H_

/** @file wifi_defs.h
 * @brief Module to ....
 */

#include <zephyr/kernel.h>
#include <zephyr/net/wifi.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup wifi_defs common Wi-Fi stuff
 * @{
 */

/* 2 chars per byte, colon separated */
#define WIFI_MAC_ADDR_STR_LEN ((WIFI_MAC_ADDR_LEN * 2) + 5)

struct wifi_ap_info {
	char mac_addr_str[WIFI_MAC_ADDR_STR_LEN + 1];
	char ssid_str[WIFI_SSID_MAX_LEN + 1];
	uint8_t channel;
	int8_t rssi;
};

struct wifi_scan_info {
	struct wifi_ap_info *ap_info;
	uint16_t cnt;
};

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* WIFI_DEFS_H_ */
