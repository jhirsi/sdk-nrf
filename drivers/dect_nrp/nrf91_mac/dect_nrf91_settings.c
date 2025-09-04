/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>
#include <zephyr/settings/settings.h>
#include <nrf_modem_dect_mac.h>

#include "dect_nrf91_settings.h"

K_SEM_DEFINE(dect_settings_init_sema, 0, 1);

/* TODO? generate random long rd id as a default? */

/* Default nrf91 settings */
static const struct dect_settings_association association_data = {
	.max_beacon_rx_failures = 10,
	.min_sensitivity_dbm = -95, /* MIN_SENSITIVITY_LEVEL per MAC spec */
};

static const struct dect_settings_network_beacon nw_beacon_data = {
	.beacon_period = DECT_MAC_NW_BEACON_PERIOD_2000MS,
	.channel = DECT_MAC_NW_BEACON_CHANNEL_NOT_USED,
};

static const struct dect_settings_cluster cluster_beacon_data = {
	.beacon_period = DECT_MAC_CLUSTER_BEACON_PERIOD_2000MS,
	.max_beacon_tx_power_dbm = 4,
	.max_cluster_power_dbm = 0,
	.max_num_neighbors = CONFIG_DECT_NRP_MAC_CLUSTER_MAX_CHILD_ASSOCIATION_COUNT,
	.channel_loaded_percent = 80,
	.neighbor_inactivity_disconnect_timer_ms = 1800000, /* 30 min */
};

static const struct dect_settings_auto_start auto_start_data = {
	.activate = true, /* Activate DECT NR+ stack on bootup */
};

static const struct dect_settings_rssi_scan rssi_scan_data = {
	.time_per_channel_ms = 200,
	.scan_suitable_percent = DECT_NRF91_SETT_DEFAULT_RSSI_SCAN_SUITABLE_PERCENT,
	.busy_threshold_dbm = DECT_NRF91_SETT_DEFAULT_RSSI_SCAN_THRESHOLD_MAX,
	.free_threshold_dbm = DECT_NRF91_SETT_DEFAULT_RSSI_SCAN_THRESHOLD_MIN,
};

static const struct dect_settings_security_conf security_configuration_data = {
	.mode = NRF_MODEM_DECT_MAC_SECURITY_MODE_1, /* Enabled */
	.integrity_key = {0x54, 0x41, 0x50, 0x50, 0x41, 0x52, 0x41, 0x21, 0x54, 0x41, 0x50, 0x50,
			  0x41, 0x52, 0x41, 0x21},
	.cipher_key = {0x54, 0x41, 0x50, 0x50, 0x41, 0x52, 0x41, 0x21, 0x54, 0x41, 0x50, 0x50, 0x41,
		       0x52, 0x41, 0x21},
};

static const struct dect_settings common_settings_data = {
	.region = DECT_SETTINGS_REGION_EU,
	.identities.network_id = DECT_NRF91_DEFAULT_NW_ID,
	.identities.transmitter_long_rd_id = DECT_NRF91_DEFAULT_LONG_RD_ID_ID,
	.device_type = DECT_NRF91_DEFAULT_DEVICE_TYPE,
	.auto_start = auto_start_data,
	.band_nbr = 1,
	.power_save = false,
	.tx.max_power_dbm = 0,
	.tx.max_mcs = 4,
	.rssi_scan = rssi_scan_data,
	.cluster = cluster_beacon_data,
	.network_join = {
		.target_ft_long_rd_id = DECT_SETT_NETWORK_JOIN_TARGET_FT_ANY,
	},
	.nw_beacon = nw_beacon_data,
	.association = association_data,
	.sec_conf = security_configuration_data,
};

static const struct dect_nrf91_settings settings_data_defaults = {
	.net_mgmt_common = common_settings_data,
};
static struct dect_nrf91_settings settings_data = settings_data_defaults;

LOG_MODULE_DECLARE(DECT_NRP_MAC, CONFIG_DECT_NRP_MAC_LOG_LEVEL);

/**************************************************************************************************/

static int dect_nrf91_settings_write_all(struct dect_nrf91_settings *dect_sett)
{
	int ret = settings_save_one(DECT_NRF91_SETT_TREE_KEY "/" DECT_NRF91_SETT_COMMON_CONFIG_KEY,
				    dect_sett, sizeof(struct dect_nrf91_settings));

	if (ret) {
		LOG_ERR("Cannot save dect common settings, err: %d", ret);
		return ret;
	}

	settings_data = *dect_sett;

	LOG_INF("dect common settings saved");

	return ret;
}

struct dect_nrf91_settings_write_status
dect_nrf91_settings_write(struct dect_nrf91_settings *dect_sett_in)
{
	struct dect_nrf91_settings_write_status return_status = {
		.status = 0,
		.reactivate = false,
	};
	struct dect_nrf91_settings *current_sett_ptr = dect_nrf91_settings_ref_get();
	uint16_t write_scope_bitmap_in =
		dect_sett_in->net_mgmt_common.cmd_params.write_scope_bitmap;
	struct dect_settings *current_sett = &current_sett_ptr->net_mgmt_common;
	struct dect_settings *new_sett = &dect_sett_in->net_mgmt_common;

	if (write_scope_bitmap_in == DECT_SETTINGS_WRITE_SCOPE_ALL) {
		return_status.status = dect_nrf91_settings_write_all(dect_sett_in);
		if (return_status.status) {
			return return_status;
		}
		return_status.reactivate = true;
		return return_status;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_AUTO_START) {
		current_sett->auto_start.activate = new_sett->auto_start.activate;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_REGION) {
		current_sett->region = new_sett->region;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_DEVICE_TYPE) {
		current_sett->device_type = new_sett->device_type;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_IDENTITIES) {
		current_sett->identities.network_id = new_sett->identities.network_id;
		current_sett->identities.transmitter_long_rd_id =
			new_sett->identities.transmitter_long_rd_id;
		return_status.reactivate = true;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_TX) {
		current_sett->tx.max_power_dbm = new_sett->tx.max_power_dbm;
		current_sett->tx.max_mcs = new_sett->tx.max_mcs;
		return_status.reactivate = true;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_POWER_SAVE) {
		current_sett->power_save = new_sett->power_save;
		return_status.reactivate = true;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_BAND_NBR) {
		current_sett->band_nbr = new_sett->band_nbr;
		return_status.reactivate = true;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_RSSI_SCAN) {
		current_sett->rssi_scan.scan_suitable_percent =
			new_sett->rssi_scan.scan_suitable_percent;
		current_sett->rssi_scan.time_per_channel_ms =
			new_sett->rssi_scan.time_per_channel_ms;
		current_sett->rssi_scan.busy_threshold_dbm = new_sett->rssi_scan.busy_threshold_dbm;
		current_sett->rssi_scan.free_threshold_dbm = new_sett->rssi_scan.free_threshold_dbm;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_CLUSTER) {
		current_sett->cluster.beacon_period = new_sett->cluster.beacon_period;
		current_sett->cluster.max_cluster_power_dbm =
			new_sett->cluster.max_cluster_power_dbm;
		current_sett->cluster.max_beacon_tx_power_dbm =
			new_sett->cluster.max_beacon_tx_power_dbm;
		current_sett->cluster.max_num_neighbors =
			new_sett->cluster.max_num_neighbors;
		current_sett->cluster.channel_loaded_percent =
			new_sett->cluster.channel_loaded_percent;
		current_sett->cluster.neighbor_inactivity_disconnect_timer_ms =
			new_sett->cluster.neighbor_inactivity_disconnect_timer_ms;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_NW_BEACON) {
		current_sett->nw_beacon.beacon_period = new_sett->nw_beacon.beacon_period;
		current_sett->nw_beacon.channel = new_sett->nw_beacon.channel;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_ASSOCIATION) {
		current_sett->association.max_beacon_rx_failures =
			new_sett->association.max_beacon_rx_failures;
		current_sett->association.min_sensitivity_dbm =
			new_sett->association.min_sensitivity_dbm;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_NETWORK_JOIN) {
		current_sett->network_join.target_ft_long_rd_id =
			new_sett->network_join.target_ft_long_rd_id;
	}
	if (write_scope_bitmap_in & DECT_SETTINGS_WRITE_SCOPE_SECURITY_CONFIGURATION) {
		current_sett->sec_conf.mode = new_sett->sec_conf.mode;
		memcpy(current_sett->sec_conf.integrity_key, new_sett->sec_conf.integrity_key,
		       sizeof(current_sett->sec_conf.integrity_key));
		memcpy(current_sett->sec_conf.cipher_key, new_sett->sec_conf.cipher_key,
		       sizeof(current_sett->sec_conf.cipher_key));
		return_status.reactivate = true;
	}

	return_status.status =
		settings_save_one(DECT_NRF91_SETT_TREE_KEY "/" DECT_NRF91_SETT_COMMON_CONFIG_KEY,
				  current_sett_ptr, sizeof(struct dect_nrf91_settings));
	if (return_status.status) {
		LOG_ERR("Cannot save dect settings, err: %d", return_status.status);
	}

	return return_status;
}

struct dect_nrf91_settings_write_status dect_nrf91_settings_defaults_set(void)
{
	struct dect_nrf91_settings_write_status return_status = {
		.status = 0,
		.reactivate = true,
	};

	return_status.status =
		settings_save_one(DECT_NRF91_SETT_TREE_KEY "/" DECT_NRF91_SETT_COMMON_CONFIG_KEY,
				  &settings_data_defaults, sizeof(struct dect_nrf91_settings));
	if (return_status.status) {
		LOG_ERR("Cannot set dect default settings, err: %d", return_status.status);
		return return_status;
	}
	memcpy(&settings_data, &settings_data_defaults, sizeof(struct dect_nrf91_settings));
	LOG_INF("Default settings set.");

	return return_status;
}

int dect_nrf91_settings_read(struct dect_nrf91_settings *dect_sett)
{
	memcpy(dect_sett, &settings_data, sizeof(struct dect_nrf91_settings));
	return 0;
}

struct dect_nrf91_settings *dect_nrf91_settings_ref_get(void)
{
	return &settings_data;
}

static int dect_nrf91_settings_handler(const char *key, size_t len, settings_read_cb read_cb,
				       void *cb_arg)
{
	int ret = 0;

	if (strcmp(key, DECT_NRF91_SETT_COMMON_CONFIG_KEY) == 0) {
		ret = read_cb(cb_arg, &settings_data, sizeof(settings_data));
		if (ret < 0) {
			printk("Failed to read dect phy_settings_data, error: %d", ret);
			return ret;
		}
		return 0;
	}
	return -ENOENT;
}

static int dect_nrf91_settings_loaded(void)
{
	k_sem_give(&dect_settings_init_sema);
	return 0;
}

static struct settings_handler cfg = {.name = DECT_NRF91_SETT_TREE_KEY,
				      .h_set = dect_nrf91_settings_handler,
				      .h_commit = dect_nrf91_settings_loaded};

int dect_nrf91_settings_init(void)
{
	int ret = 0;

	/* Set the initial defaults from the code: */
	settings_data.net_mgmt_common = common_settings_data;

	ret = settings_subsys_init();
	if (ret) {
		printk("(%s): failed to initialize settings subsystem, error: %d", (__func__), ret);
		goto exit;
	}
	ret = settings_register(&cfg);
	if (ret) {
		printk("Cannot register settings handler %d", ret);
		goto exit;
	}
	ret = settings_load_subtree(DECT_NRF91_SETT_TREE_KEY);
	if (ret) {
		printk("(%s): cannot load settings %d", (__func__), ret);
		goto exit;
	}

exit:
	ret = k_sem_take(&dect_settings_init_sema, K_SECONDS(2));
	if (ret) {
		printk("(%s): dect_nrf91_settings_init timeout.\n", (__func__));
	}

	return 0;
}
