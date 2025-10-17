/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_NRF91_SETTINGS_H
#define DECT_NRF91_SETTINGS_H

#include <zephyr/kernel.h>
#include "dect_net_l2_mgmt.h"

#define DECT_NRF91_SETT_TREE_KEY	  "dect_nrf91_settings"
#define DECT_NRF91_SETT_COMMON_CONFIG_KEY "common_settings"

/************************************************************************************************/

#define DECT_NRF91_DEFAULT_NW_ID	 (CONFIG_DECT_NRP_MAC_DEFAULT_NW_ID)
#define DECT_NRF91_LONG_RD_ID_ID_NOT_SET 0

#if defined(CONFIG_DECT_NRP_MAC_DEFAULT_DEV_TYPE_PT)
#define DECT_NRF91_DEFAULT_DEVICE_TYPE DECT_DEVICE_TYPE_PT
#elif defined(CONFIG_DECT_NRP_MAC_DEFAULT_DEV_TYPE_FT)
#define DECT_NRF91_DEFAULT_DEVICE_TYPE DECT_DEVICE_TYPE_FT
#else
#error "No default DECT device type defined"
#endif

/* "busy" if max(RSSI-1) > DECT_PHY_SETT_DEFAULT_RSSI_SCAN_THRESHOLD_MAX */
#define DECT_NRF91_SETT_DEFAULT_RSSI_SCAN_THRESHOLD_MAX -71

/* "possible" if DECT_PHY_SETT_DEFAULT_RSSI_SCAN_THRESHOLD_MIN < max(RSSI-1) ≤
 * DECT_PHY_SETT_DEFAULT_RSSI_SCAN_THRESHOLD_MAX
 */

/* free" if max(RSSI-1) ≤ DECT_PHY_SETT_DEFAULT_RSSI_SCAN_THRESHOLD_MIN*/
#define DECT_NRF91_SETT_DEFAULT_RSSI_SCAN_THRESHOLD_MIN -85

/* SCAN_SUITABLE as per MAC spec */
#define DECT_NRF91_SETT_DEFAULT_RSSI_SCAN_SUITABLE_PERCENT 75

/************************************************************************************************/

struct dect_nrf91_settings {
	struct dect_settings net_mgmt_common;
};

int dect_nrf91_settings_read(struct dect_nrf91_settings *dect_sett);
struct dect_nrf91_settings *dect_nrf91_settings_ref_get(void);

struct dect_nrf91_settings_write_status {
	int status;	 /* 0 if success, negative on error */
	bool reactivate; /* if returned true,
			  * then re-configure/activate DECT NR+ stack after settings change to make
			  * sure that the new settings are applied into modem.
			  */
	uint16_t failure_scope_bitmap; /* bitmask of dect_settings_cmd_params_write_scope
					* indicating which scopes failed to be written,
					* in success zero
					*/
};

struct dect_nrf91_settings_write_status dect_nrf91_settings_defaults_set(void);
struct dect_nrf91_settings_write_status
dect_nrf91_settings_write(struct dect_nrf91_settings *dect_sett);

int dect_nrf91_settings_init(void);

#endif /* DECT_NRF91_SETTINGS_H */
