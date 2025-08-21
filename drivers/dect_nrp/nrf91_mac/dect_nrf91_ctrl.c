/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/byteorder.h>

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/shell/shell.h>

#include <modem/nrf_modem_lib.h>
#include <nrf_modem_at.h>

#include <nrf_errno.h>
#include <nrf_modem_dect_mac.h>

#include <net/dect_nrp_utils.h>

#include "dect_net_l2_mgmt.h"

#include "dect_nrf91_common.h"
#include "dect_nrf91_utils.h"
#include "dect_nrf91_settings.h"
#include "dect_nrf91.h"
#include "dect_nrf91_rx.h"
#include "dect_nrf91_sink.h"
#include "dect_nrf91_ctrl.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(DECT_NRP_MAC, CONFIG_DECT_NRP_MAC_LOG_LEVEL);

#include "net_private.h" /* For net_sprint_ipv6_addr */

K_SEM_DEFINE(dect_mac_libmodem_api_sema, 0, 1);
K_SEM_DEFINE(dect_mac_ctrl_reactivate_sema, 0, 1);

K_MUTEX_DEFINE(dect_mac_ctrl_data_mtx);
typedef enum {
	CTRL_PT_ASSOCIATION_STATE_NONE,
	CTRL_PT_ASSOCIATION_STATE_CLUSTER_FOUND,
	CTRL_PT_ASSOCIATION_STATE_CLUSTER_BEACON_RECEIVED,
	CTRL_PT_ASSOCIATION_STATE_ASSOCIATED,
} ctrl_pt_association_state_t;

typedef enum {
	CTRL_FT_CLUSTER_STATE_NONE = 0,
	CTRL_FT_CLUSTER_STATE_STARTING,
	CTRL_FT_CLUSTER_STATE_STARTED,
} ctrl_ft_cluster_state_t;

typedef enum {
	CTRL_FT_NW_BEACON_STATE_NONE = 0,
	CTRL_FT_NW_BEACON_STATE_STARTING,
	CTRL_FT_NW_BEACON_STATE_STARTED,
	CTRL_FT_NW_BEACON_STATE_STOPPING,
} ctrl_ft_nw_beacon_state_t;

typedef enum {
	CTRL_FT_NETWORK_STATE_NONE = 0,
	CTRL_FT_NETWORK_STATE_STARTING,
	CTRL_FT_NETWORK_STATE_CREATED,
} ctrl_ft_network_state_t;

typedef struct {
	ctrl_pt_association_state_t pt_association_state;

	uint32_t parent_long_rd_id;
	uint32_t network_id;
	uint16_t channel;
} ctrl_pt_association_config_t;

struct dect_nrf91_ctrl_rssi_scan_data {
	bool cmd_on_going; /* actual RSSI command */

	bool rssi_scan_result_last_best_stored;
	struct dect_rssi_scan_result_data rssi_scan_result_last_best;
};

#define DECT_NRF91_CTRL_CLUSTER_SCAN_DATA_MAX_CHANNELS 30
struct dect_nrf91_ctrl_scan_data {
	bool on_going;

	/* Storage for channels where we have seen cluster */
	struct {
		uint16_t channel;
		int8_t rssi_2;
	} cluster_channels[DECT_NRF91_CTRL_CLUSTER_SCAN_DATA_MAX_CHANNELS];
	uint8_t current_cluster_channel_index;

	dect_scan_result_cb_t scan_result_cb;
	struct nrf_modem_dect_mac_network_scan_params scan_params;
};

typedef enum {
	CTRL_MDM_DEACTIVATED = 0,
	CTRL_MDM_ACTIVATE_REQ,
	CTRL_MDM_ACTIVATED,
	CTRL_MDM_DEACTIVATE_REQ,
	CTRL_MDM_REACTIVATING_DEACTIVATE,
	CTRL_MDM_REACTIVATING_CONFIGURE,
} ctrl_mdm_activation_state_t;

struct dect_nrf91_ctrl_dlc_data_tx_info {
	uint32_t transaction_id;
	uint32_t data_len;
	bool req_on_going;
};

/* States of all commands */
/* TODO: better hierarchy for state data? */
static struct dect_nrf91_ctrl_data {
	bool debug;

	struct nrf_modem_dect_mac_capability_ntf_cb_params mdm_capas;

	ctrl_ft_cluster_state_t ft_cluster_state;
	ctrl_ft_network_state_t ft_network_state;
	ctrl_ft_nw_beacon_state_t ft_nw_beacon_state;
	ctrl_mdm_activation_state_t mdm_activation_state;
	ctrl_pt_association_config_t ass_config;

	struct dect_nrf91_ctrl_rssi_scan_data rssi_scan_data;
	struct dect_nrf91_ctrl_scan_data scan_data;
	dect_nrf91_ctrl_configure_params_t configure_params;

	bool tx_mdm_flow_ctrl_on;
#if defined(CONFIG_DECT_NRP_MAC_NRF_TX_FLOW_CTRL_BASED_ON_MDM_TX_DLC_REQS)
	uint32_t total_unacked_tx_data_amount;
	uint16_t total_unacked_req_amount;
	struct dect_nrf91_ctrl_dlc_data_tx_info
		dlc_data_tx_infos[DECT_NRF91_DLC_DATA_INFO_MAX_COUNT];
#endif
	struct net_if *iface;
} ctrl_data;

/**************************************************************************************************/

static void dect_nrf91_ctrl_rssi_scan_data_init(bool actual_command)
{
	ctrl_data.rssi_scan_data.cmd_on_going = actual_command;
	ctrl_data.rssi_scan_data.rssi_scan_result_last_best_stored = false;
	memset(&ctrl_data.rssi_scan_data.rssi_scan_result_last_best, 0,
	       sizeof(ctrl_data.rssi_scan_data.rssi_scan_result_last_best));
	ctrl_data.rssi_scan_data.rssi_scan_result_last_best.busy_percentage = 100;
	ctrl_data.rssi_scan_data.rssi_scan_result_last_best.scan_suitable_percent = 0;
}

static bool dect_nrf91_ctrl_rssi_scan_data_result_data_last_best_update(
	struct dect_rssi_scan_result_data *new_rssi_data)
{
	if (!ctrl_data.rssi_scan_data.rssi_scan_result_last_best_stored) {
		goto update_needed;
	}

	if (new_rssi_data->all_subslots_free &&
	    ((new_rssi_data->busy_percentage <
	      ctrl_data.rssi_scan_data.rssi_scan_result_last_best.busy_percentage) ||
	     (!new_rssi_data->another_cluster_detected_in_channel &&
	      ctrl_data.rssi_scan_data.rssi_scan_result_last_best
		      .another_cluster_detected_in_channel))) {
		goto update_needed;
	}

	if (new_rssi_data->busy_subslot_cnt <
	    ctrl_data.rssi_scan_data.rssi_scan_result_last_best.busy_subslot_cnt) {
		goto update_needed;
	}
	if (new_rssi_data->free_subslot_cnt >
	    ctrl_data.rssi_scan_data.rssi_scan_result_last_best.free_subslot_cnt) {
		goto update_needed;
	}
	if (new_rssi_data->scan_suitable_percent >
	    ctrl_data.rssi_scan_data.rssi_scan_result_last_best.scan_suitable_percent) {
		goto update_needed;
	}
	if (new_rssi_data->possible_subslot_cnt <
	    ctrl_data.rssi_scan_data.rssi_scan_result_last_best.possible_subslot_cnt) {
		goto update_needed;
	}
	if (new_rssi_data->busy_percentage <
	    ctrl_data.rssi_scan_data.rssi_scan_result_last_best.busy_percentage) {
		goto update_needed;
	}
	if (!new_rssi_data->another_cluster_detected_in_channel &&
	    ctrl_data.rssi_scan_data.rssi_scan_result_last_best
		    .another_cluster_detected_in_channel) {
		goto update_needed;
	}

	return false;

update_needed:
	ctrl_data.rssi_scan_data.rssi_scan_result_last_best = *new_rssi_data;
	ctrl_data.rssi_scan_data.rssi_scan_result_last_best_stored = true;
	return true;
}

static bool dect_nrf91_ctrl_rssi_scan_data_result_data_last_best_ok(void)
{
	/* Return true if stored RSSI scan result data if possible to be used for channel access */
	if (!ctrl_data.rssi_scan_data.rssi_scan_result_last_best_stored) {
		return false;
	}
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	if (ctrl_data.rssi_scan_data
		.rssi_scan_result_last_best.scan_suitable_percent >=
	    set_ptr->net_mgmt_common.rssi_scan.scan_suitable_percent) {
		return true;
	}
	return false;
}

/**************************************************************************************************/

K_MSGQ_DEFINE(dect_nrf91_ctrl_msgq, sizeof(struct dect_mac_common_op_event_msgq_item), 1000,
	      4); /* TODO optimize sizes */

int dect_nrf91_ctrl_msgq_non_data_op_add(dect_nrf91_ctrl_op_t event_id)
{
	int ret = 0;
	struct dect_mac_common_op_event_msgq_item event;

	event.id = event_id;
	event.data = NULL;
	ret = k_msgq_put(&dect_nrf91_ctrl_msgq, &event, K_NO_WAIT);
	if (ret) {
		k_free(event.data);
		return -ENOBUFS;
	}
	return 0;
}

#ifdef RM_JH
static int dect_nrf91_ctrl_msgq_non_data_op_add_delayed(dect_nrf91_ctrl_op_t event_id,
							k_timeout_t delay)
{
	k_sleep(delay);
	return dect_nrf91_ctrl_msgq_non_data_op_add(event_id);
}
#endif

int dect_nrf91_ctrl_msgq_data_op_add(dect_nrf91_ctrl_op_t event_id, void *data, size_t data_size)
{
	int ret = 0;
	struct dect_mac_common_op_event_msgq_item event;

	event.data = k_malloc(data_size);
	if (event.data == NULL) {
		return -ENOMEM;
	}
	memcpy(event.data, data, data_size);
	event.id = event_id;

	ret = k_msgq_put(&dect_nrf91_ctrl_msgq, &event, K_NO_WAIT);
	if (ret) {
		k_free(event.data);
		return -ENOBUFS;
	}
	return 0;
}

/**************************************************************************************************/

int dect_nrf91_ctrl_cluster_channel_get(void)
{
	if (ctrl_data.ft_cluster_state == CTRL_FT_CLUSTER_STATE_STARTED) {
		return ctrl_data.configure_params.channel;
	} else {
		return -EINVAL;
	}
}

bool dect_nrf91_ctrl_nw_beacon_running(void)
{
	if (ctrl_data.ft_nw_beacon_state == CTRL_FT_NW_BEACON_STATE_STARTED) {
		return true;
	} else {
		return false;
	}
}

bool dect_nrf91_ctrl_mdm_activated(void)
{
	if (ctrl_data.mdm_activation_state == CTRL_MDM_ACTIVATED) {
		return true;
	} else {
		return false;
	}
}

static bool dect_nrf91_ctrl_connected(void) /* Cluster running or associated */
{
	if (ctrl_data.ft_cluster_state != CTRL_FT_CLUSTER_STATE_NONE ||
	    ctrl_data.ass_config.pt_association_state != CTRL_PT_ASSOCIATION_STATE_NONE) {
		return true;
	} else {
		return false;
	}
}

static int dect_nrf91_ctrl_configure_cmd(dect_nrf91_ctrl_configure_params_t *params)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	ctrl_data.configure_params = *params;

	struct nrf_modem_dect_control_configure_params config = {
		.max_tx_power = dect_nrp_utils_dbm_to_phy_tx_power(params->tx_pwr),
		.max_mcs = params->tx_mcs,
		.long_rd_id = params->long_rd_id,
		.phy_band_group_index = params->band_group_index,
		.power_save = params->power_save,
		.security.mode = set_ptr->net_mgmt_common.sec_conf.mode,
		.stats_averaging_length = 2,
	};
	int err;

	if (set_ptr->net_mgmt_common.sec_conf.mode != DECT_MAC_SECURITY_MODE_NONE) {
		memcpy(config.security.integrity_key,
		       set_ptr->net_mgmt_common.sec_conf.integrity_key,
		       NRF_MODEM_DECT_MAC_INTEGRITY_KEY_LENGTH);
		memcpy(config.security.cipher_key, set_ptr->net_mgmt_common.sec_conf.cipher_key,
		       NRF_MODEM_DECT_MAC_CIPHER_KEY_LENGTH);
	}
	err = nrf_modem_dect_control_configure(&config);
	if (err) {
		LOG_ERR("nrf_modem_dect_control_configure() failed, error: %d", err);
	}
	return err;
}

static int dect_nrf91_ctrl_modem_configure_req_from_settings(void)
{
	dect_nrf91_ctrl_configure_params_t params;
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
	int ret;

	if (ctrl_data.mdm_activation_state != CTRL_MDM_DEACTIVATED) {
		LOG_ERR("Modem is not deactivated, cannot configure");
		return -EINVAL;
	}
	params.long_rd_id = set_ptr->net_mgmt_common.identities.transmitter_long_rd_id;
	params.network_id = set_ptr->net_mgmt_common.identities.network_id;
	params.device_type = set_ptr->net_mgmt_common.device_type;
	params.band = set_ptr->net_mgmt_common.band_nbr;
	params.band_group_index = 0;
	if (params.band == 4) {
		params.band_group_index = 1;
	}

	params.debug = false;

	params.tx_mcs = set_ptr->net_mgmt_common.tx.max_mcs;
	params.auto_start = false;
	params.auto_activate = set_ptr->net_mgmt_common.auto_start.activate;
	params.channel = 0;
	params.power_save = set_ptr->net_mgmt_common.power_save;
	params.tx_pwr = set_ptr->net_mgmt_common.tx.max_power_dbm;

	ret = dect_nrf91_ctrl_configure_cmd(&params);
	if (ret != 0) {
		printk("Error in configure, error: %d\n", ret);
	}
	return ret;
}

int dect_nrf91_ctrl_configure_n_activate(void)
{
	int ret = 0;

	if (ctrl_data.mdm_activation_state != CTRL_MDM_DEACTIVATED) {
		LOG_ERR("Modem is not deactivated, cannot configure");
		return -EINVAL;
	}
	ret = dect_nrf91_ctrl_modem_configure_req_from_settings();
	if (ret != 0) {
		LOG_ERR("%s: error in configure, error: %d", (__func__), ret);
	} else {
		ctrl_data.mdm_activation_state = CTRL_MDM_ACTIVATE_REQ;
	}
	return ret;
}

/**************************************************************************************************/

static int dect_nrf91_ctrl_mdm_activate_req(void)
{
	/* Activate modem by setting full functional mode */
	char tmp_str[128] = {0};
	int err = nrf_modem_dect_control_functional_mode_set(
		NRF_MODEM_DECT_CONTROL_FUNCTIONAL_MODE_ACTIVATE);

	if (err) {
		dect_nrf91_utils_modem_phy_err_to_string(err, tmp_str);
		LOG_ERR("%s: error in CFUN set: err %s (%d)", (__func__), tmp_str, err);
	}
	return err;
}

static int dect_nrf91_ctrl_mdm_deactivate_req(void)
{
	/* Deactivate modem by setting min functional mode */
	char tmp_str[128] = {0};
	int err = nrf_modem_dect_control_functional_mode_set(
		NRF_MODEM_DECT_CONTROL_FUNCTIONAL_MODE_DEACTIVATE);

	if (err) {
		dect_nrf91_utils_modem_phy_err_to_string(err, tmp_str);
		LOG_ERR("%s: error in CFUN set: err %s (%d)", (__func__), tmp_str, err);
	}
	return err;
}

int dect_nrf91_ctrl_deactivate(void)
{
	int ret;

	ret = dect_nrf91_ctrl_mdm_deactivate_req();
	if (ret != 0) {
		LOG_ERR("%s: error in deactivate, error: %d", (__func__), ret);
	} else {
		ctrl_data.mdm_activation_state = CTRL_MDM_DEACTIVATE_REQ;
	}
	return ret;
}

/**************************************************************************************************/

int dect_nrf91_ctrl_mdm_reactivate(void)
{
	if (ctrl_data.mdm_activation_state != CTRL_MDM_ACTIVATED) {
		LOG_DBG("Modem is not activated, cannot reactivate");
		return -EINVAL;
	}

	if (dect_nrf91_ctrl_connected()) {
		LOG_DBG("Modem is connected, cannot reactivate");
		return -EINVAL;
	}
	k_sem_reset(&dect_mac_ctrl_reactivate_sema);

	int ret = dect_nrf91_ctrl_mdm_deactivate_req();

	if (ret != 0) {
		LOG_ERR("%s: error in deactivate, error: %d", (__func__), ret);
	} else {
		ctrl_data.mdm_activation_state = CTRL_MDM_REACTIVATING_DEACTIVATE;
		LOG_INF("Modem deactivation requested, waiting for configure/reactivation");
	}

	/* Wait for reactivation */
	ret = k_sem_take(&dect_mac_ctrl_reactivate_sema, K_SECONDS(3));
	if (ret != 0) {
		LOG_ERR("%s: timeout in wait for reactivation, error: %d", (__func__), ret);
	}
	return ret;
}

/**************************************************************************************************/

int dect_nrf91_ctrl_nw_scan_cmd(struct nrf_modem_dect_mac_network_scan_params *params,
				struct net_if *iface,
				dect_scan_result_cb_t cb) /* TODO: iface? kun on jo initissä */
{
	if (ctrl_data.scan_data.on_going) {
		LOG_ERR("Network scan already on going");
		return -EALREADY;
	}
	int err = nrf_modem_dect_mac_network_scan(params);

	if (!err) {
		ctrl_data.scan_data.on_going = true;
		ctrl_data.scan_data.scan_result_cb = cb;
		ctrl_data.scan_data.scan_params = *params;
		memset(ctrl_data.scan_data.cluster_channels, 0,
		     sizeof(ctrl_data.scan_data.cluster_channels));
		ctrl_data.scan_data.current_cluster_channel_index = 0;
		ctrl_data.iface = iface;
	}

	return err;
}

/**************************************************************************************************/

int dect_nrf91_ctrl_tx_cmd(dect_nrf91_ctrl_tx_cmd_params_t *params)
{
	struct nrf_modem_dect_dlc_data_tx_params tx_params = {
		.transaction_id = params->transaction_id,
		.flow_id = params->flow_id,
		.long_rd_id = params->long_rd_id,
		.data = params->data,
		.data_len = params->data_len,
	};
	int ret;

	k_mutex_lock(&dect_mac_ctrl_data_mtx, K_FOREVER);
	if (ctrl_data.tx_mdm_flow_ctrl_on) {
		LOG_DBG("Flow control is enabled, tx not allowed");
		k_mutex_unlock(&dect_mac_ctrl_data_mtx);
		return -EACCES;
	}
#if defined(CONFIG_DECT_NRP_MAC_NRF_TX_FLOW_CTRL_BASED_ON_MDM_TX_DLC_REQS)
	int arr_index = params->transaction_id - DECT_MAC_DATA_TX_HANDLE_START;

	if (ctrl_data.total_unacked_tx_data_amount + params->data_len >
	    CONFIG_NRF_MODEM_LIB_SHMEM_TX_SIZE) {
		LOG_WRN("Too much unacked TX data: %d bytes - continue but flow ctrl might occur",
			ctrl_data.total_unacked_tx_data_amount);
	}
	if (ctrl_data.total_unacked_req_amount >= DECT_MAC_DATA_TX_HANDLE_COUNT) {
		LOG_WRN("Too many unacked TX requests: %d", ctrl_data.total_unacked_req_amount);
		k_mutex_unlock(&dect_mac_ctrl_data_mtx);
		return -ENOMEM;
	}
	if (arr_index < 0 || arr_index >= DECT_NRF91_DLC_DATA_INFO_MAX_COUNT) {
		LOG_ERR("Invalid transaction ID: %d", params->transaction_id);
		k_mutex_unlock(&dect_mac_ctrl_data_mtx);
		return -EINVAL;
	}
	if (ctrl_data.dlc_data_tx_infos[arr_index].req_on_going) {
		LOG_WRN("Transaction ID %d already in use", params->transaction_id);
		k_mutex_unlock(&dect_mac_ctrl_data_mtx);
		return -EBUSY;
	}
	ctrl_data.dlc_data_tx_infos[arr_index].transaction_id = params->transaction_id;
	ctrl_data.dlc_data_tx_infos[arr_index].data_len = params->data_len;
	ctrl_data.dlc_data_tx_infos[arr_index].req_on_going = true;
#endif
	k_mutex_unlock(&dect_mac_ctrl_data_mtx);

	ret = nrf_modem_dect_dlc_data_tx(&tx_params);
	if (ret) {
		if (ret == -NRF_ENOMEM) {
			ret = -ENOMEM;
			LOG_WRN("nrf_modem_dect_dlc_data_tx returned NRF_ENOMEM");
		} else {
			LOG_ERR("nrf_modem_dect_dlc_data_tx returned error: %d", ret);
		}
#if defined(CONFIG_DECT_NRP_MAC_NRF_TX_FLOW_CTRL_BASED_ON_MDM_TX_DLC_REQS)
		ctrl_data.dlc_data_tx_infos[arr_index].req_on_going = false;
#endif
	} else {
#if defined(CONFIG_DECT_NRP_MAC_NRF_TX_FLOW_CTRL_BASED_ON_MDM_TX_DLC_REQS)
		k_mutex_lock(&dect_mac_ctrl_data_mtx, K_FOREVER);
		ctrl_data.total_unacked_tx_data_amount += params->data_len;
		ctrl_data.total_unacked_req_amount++;
		k_mutex_unlock(&dect_mac_ctrl_data_mtx);
#endif
	}

	return ret;
}

/**************************************************************************************************/

int dect_nrf91_ctrl_cluster_start_req_cmd(struct dect_cluster_start_req_params *params)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_PT) {
		LOG_ERR("Cluster start not allowed for PT");
		return -EINVAL;
	}

	if (ctrl_data.ft_cluster_state != CTRL_FT_CLUSTER_STATE_NONE) {
		LOG_ERR("Cluster already started/starting");
		return -EALREADY;
	}

	return dect_nrf91_ctrl_msgq_non_data_op_add(DECT_NRF91_CTRL_OP_CLUSTER_START_REQ);
}

int dect_nrf91_ctrl_cluster_info_req_cmd(void)
{
	int err = nrf_modem_dect_mac_cluster_info();

	if (err) {
		LOG_ERR("%s: error intitiating the request for cluster info: %d", (__func__), err);
	}

	return err;
}

/**************************************************************************************************/

static bool dect_nrf91_ctrl_nw_beacon_common_can_be_started(uint16_t channel)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_PT) {
		LOG_ERR("Network beacon can not be started for PT");
		return false;
	}

	if (dect_common_utils_channel_is_supported_by_band(set_ptr->net_mgmt_common.band_nbr,
							   channel) == false) {
		LOG_ERR("Channel %d not supported by band %d", channel,
			set_ptr->net_mgmt_common.band_nbr);
		return false;
	}

	return true;
}

int dect_nrf91_ctrl_nw_beacon_start_req_cmd(struct dect_nw_beacon_start_req_params *params)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	if (dect_nrf91_ctrl_nw_beacon_common_can_be_started(params->channel) == false) {
		return -EINVAL;
	}
	if (ctrl_data.ft_cluster_state != CTRL_FT_CLUSTER_STATE_STARTED) {
		LOG_ERR("Cluster not started");
		return false;
	}
	if (ctrl_data.ft_nw_beacon_state != CTRL_FT_NW_BEACON_STATE_NONE) {
		LOG_ERR("Network beacon already started/starting");
		return -EINVAL;
	}

	for (int i = 0; i < params->additional_ch_count; i++) {
		if (dect_common_utils_channel_is_supported_by_band(
			    set_ptr->net_mgmt_common.band_nbr, params->additional_ch_list[i]) ==
		    false) {
			LOG_ERR("Additional channel %d not supported by band %d",
				params->additional_ch_list[i], set_ptr->net_mgmt_common.band_nbr);
			return -EINVAL;
		}
	}

	return dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_NW_BEACON_START, params,
						sizeof(struct dect_nw_beacon_start_req_params));
}

int dect_nrf91_ctrl_nw_beacon_stop_req_cmd(struct dect_nw_beacon_stop_req_params *params)
{
	if (ctrl_data.ft_nw_beacon_state == CTRL_FT_NW_BEACON_STATE_NONE ||
	    ctrl_data.ft_nw_beacon_state == CTRL_FT_NW_BEACON_STATE_STOPPING) {
		LOG_ERR("Network beacon already stopped or stopping");
		return -EALREADY;
	}
	return dect_nrf91_ctrl_msgq_non_data_op_add(DECT_NRF91_CTRL_OP_MDM_NW_BEACON_STOP);
}

/**************************************************************************************************/

int dect_nrf91_ctrl_network_create_req_cmd(void)
{
	if (ctrl_data.mdm_activation_state != CTRL_MDM_ACTIVATED) {
		LOG_ERR("Modem not activated, cannot create network");
		return -EINVAL;
	}
	if (ctrl_data.ft_cluster_state != CTRL_FT_CLUSTER_STATE_NONE) {
		LOG_ERR("Cluster already started/starting");
		return -EALREADY;
	}
	if (ctrl_data.configure_params.auto_start) {
		LOG_ERR("Auto start enabled");
		return -EALREADY;
	}
	if (ctrl_data.ft_network_state != CTRL_FT_NETWORK_STATE_NONE) {
		LOG_ERR("Network already started/starting");
		return -EALREADY;
	}
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
	uint16_t nw_beacon_channel = set_ptr->net_mgmt_common.nw_beacon.channel;

	if (nw_beacon_channel != DECT_MAC_NW_BEACON_CHANNEL_NOT_USED &&
	    dect_nrf91_ctrl_nw_beacon_common_can_be_started(
		    set_ptr->net_mgmt_common.nw_beacon.channel) == false) {
		LOG_ERR("Change settings - invalid channel %d for network beacon in band %d",
			nw_beacon_channel, set_ptr->net_mgmt_common.band_nbr);
		return -EINVAL;
	}
	ctrl_data.ft_network_state = CTRL_FT_NETWORK_STATE_STARTING;

	return dect_nrf91_ctrl_msgq_non_data_op_add(DECT_NRF91_CTRL_OP_AUTO_START);
}

bool dect_nrf91_ctrl_network_remove_req_cmd_allowed(void)
{
	if (ctrl_data.ft_network_state == CTRL_FT_NETWORK_STATE_NONE) {
		return false;
	}
	return true;
}

int dect_nrf91_ctrl_network_remove_req_cmd(void)
{
	int ret = dect_nrf91_ctrl_mdm_deactivate_req();

	if (ret != 0) {
		LOG_ERR("%s: error in deactivate, error: %d", (__func__), ret);
	} else {
		ctrl_data.mdm_activation_state = CTRL_MDM_REACTIVATING_DEACTIVATE;
	}
	return ret;
}

/**************************************************************************************************/

int dect_nrf91_ctrl_network_join_req_cmd(void)
{
	if (ctrl_data.mdm_activation_state != CTRL_MDM_ACTIVATED) {
		LOG_ERR("Modem not activated, cannot join network");
		return -EINVAL;
	}
	if (ctrl_data.ass_config.pt_association_state == CTRL_PT_ASSOCIATION_STATE_ASSOCIATED) {
		LOG_ERR("Already associated");
		return -EALREADY;
	}

	return dect_nrf91_ctrl_msgq_non_data_op_add(DECT_NRF91_CTRL_OP_AUTO_START);
}

int dect_nrf91_ctrl_network_unjoin_req_cmd(void)
{
	if (ctrl_data.ass_config.pt_association_state != CTRL_PT_ASSOCIATION_STATE_ASSOCIATED) {
		LOG_ERR("Not joined");
		return -EALREADY;
	}
	int err = dect_nrf91_ctrl_associate_release_cmd(ctrl_data.ass_config.parent_long_rd_id);

	if (err) {
		LOG_ERR("dect_nrf91_ctrl_associate_release_cmd returned err: %d", err);
	}
	return err;
}

/**************************************************************************************************/

int dect_nrf91_ctrl_neighbor_list_req_cmd(void)
{
	int err = nrf_modem_dect_mac_neighbor_list();

	if (err) {
		LOG_ERR("Error intitiating the request for neighbor list: %d", err);
	}

	return err;
}

/**************************************************************************************************/

int dect_nrf91_ctrl_neighbor_info_req_cmd(struct nrf_modem_dect_mac_neighbor_info_params *params)
{
	int err = nrf_modem_dect_mac_neighbor_info(params);

	if (err) {
		LOG_ERR("%s: error intitiating the request for neighbor info: %d", (__func__), err);
	}

	return err;
}

/**************************************************************************************************/

int dect_nrf91_ctrl_associate_req_cmd(struct nrf_modem_dect_mac_association_params *params)
{
	int err = nrf_modem_dect_mac_association(params);

	if (err) {
		LOG_ERR("%s: initiation association failed, err: %d", (__func__), err);
	}

	return err;
}

int dect_nrf91_ctrl_associate_release_cmd(uint32_t long_rd_id)
{
	struct nrf_modem_dect_mac_association_release_params params = {
		.release_cause = NRF_MODEM_DECT_MAC_RELEASE_CAUSE_CONNECTION_TERMINATION,
		.long_rd_id = long_rd_id,
	};
	int err = nrf_modem_dect_mac_association_release(&params);

	if (err) {
		LOG_ERR("%s: initiation association release failed, err: %d", (__func__), err);
	}

	return err;
}
static void dect_mac_ctrl_trigger_association(void)
{
	LOG_INF("Association triggered towards:");
	LOG_INF("  network id (32bit).............................%u (0x%08x)",
		ctrl_data.ass_config.network_id, ctrl_data.ass_config.network_id);
	LOG_INF("  transmitter id (long RD ID)....................%u (0x%08x)",
		ctrl_data.ass_config.parent_long_rd_id, ctrl_data.ass_config.parent_long_rd_id);

	struct nrf_modem_dect_mac_tx_flow_config flow_config[3] = {
		{
			.flow_id = 1,
			.dlc_service_type = NRF_MODEM_DECT_DLC_SERVICE_TYPE_3,
			.num_arq_retx = 2,
			.dlc_sdu_lifetime = 255,
		},
		{
			.flow_id = 2,
			.dlc_service_type = NRF_MODEM_DECT_DLC_SERVICE_TYPE_3,
			.num_arq_retx = 2,
			.dlc_sdu_lifetime = 255,
		},
		{
			.flow_id = 3,
			.priority = 3,
			.dlc_service_type = NRF_MODEM_DECT_DLC_SERVICE_TYPE_3,
			.num_arq_retx = 2,
			.dlc_sdu_lifetime = 255,
		},
	};
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
	struct nrf_modem_dect_mac_association_params params = {
		.long_rd_id = ctrl_data.ass_config.parent_long_rd_id,
		.network_id = ctrl_data.ass_config.network_id,
		.num_flows = 3,
		.tx_flow_configs = flow_config,
		.info_triggers.num_beacon_rx_failures =
			set_ptr->net_mgmt_common.association.max_beacon_rx_failures,
	};
	int err = nrf_modem_dect_mac_association(&params);

	if (err) {
		LOG_ERR("nrf_modem_dect_mac_association failed: err %d", err);
	}
}

/**************************************************************************************************/

static void dect_nrf91_ctrl_msgq_thread_handler(void)
{
	struct dect_mac_common_op_event_msgq_item event;
	int err;
	char tmp_str[128] = {0};

	while (true) {
		k_msgq_get(&dect_nrf91_ctrl_msgq, &event, K_FOREVER);

		switch (event.id) {

		case DECT_NRF91_CTRL_OP_MDM_CAPABILITIES: {
			struct nrf_modem_dect_mac_capability_ntf_cb_params *evt_data =
				(struct nrf_modem_dect_mac_capability_ntf_cb_params *)event.data;

			LOG_INF("Modem Capabilities:");
			LOG_INF("  max_mcs: %hhu", evt_data->max_mcs);
			for (int i = 0; i < evt_data->num_band_info_elems; i++) {
				LOG_INF("    band[%hhu]:                   %hhu", i,
					evt_data->band_info_elems[i].band);
				LOG_DBG("    band_group_index[%hhu]:       %hhu", i,
					evt_data->band_info_elems[i].band_group_index);
				LOG_DBG("    power_class[%hhu]:            %hhu", i,
					evt_data->band_info_elems[i].power_class);
				LOG_INF("    min_carrier[%hhu]:            %hu", i,
					evt_data->band_info_elems[i].min_carrier);
				LOG_INF("    max_carrier[%hhu]:            %hu", i,
					evt_data->band_info_elems[i].max_carrier);
			}
			/* Store capabilities */
			ctrl_data.mdm_capas = *evt_data;
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_CONFIGURED: {
			enum nrf_modem_dect_mac_err *status =
				(enum nrf_modem_dect_mac_err *)event.data;

			if (*status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(*status, tmp_str);

				LOG_ERR("Error in configure: err %s (%d)", tmp_str, *status);
				break;
			}
			k_sem_give(&dect_mac_libmodem_api_sema);

			if (ctrl_data.configure_params.auto_activate ||
			    ctrl_data.mdm_activation_state == CTRL_MDM_ACTIVATE_REQ ||
			    ctrl_data.mdm_activation_state == CTRL_MDM_REACTIVATING_CONFIGURE) {
				LOG_INF("Modem configured: "
					"activating to full functional mode");

				if (dect_nrf91_ctrl_mdm_activate_req()) {
					ctrl_data.mdm_activation_state = CTRL_MDM_DEACTIVATED;
					LOG_ERR("Error in initiating modem activation");
				}
			} else {
				LOG_INF("Modem configured without auto start - deactivated");
				ctrl_data.mdm_activation_state = CTRL_MDM_DEACTIVATED;
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_CFUN_RESP: {
			enum nrf_modem_dect_mac_err *status =
				(enum nrf_modem_dect_mac_err *)event.data;

			if (*status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				/* Operation wasn't successful -> no change to activation state */
				dect_nrf91_utils_modem_phy_err_to_string(*status, tmp_str);
				if (ctrl_data.mdm_activation_state == CTRL_MDM_ACTIVATE_REQ) {
					dect_mgmt_activate_done_evt(
						ctrl_data.iface,
						dect_nrf91_utils_modem_status_to_net_mgmt_status(
							*status));

				} else {
					__ASSERT_NO_MSG(ctrl_data.mdm_activation_state ==
								CTRL_MDM_DEACTIVATE_REQ ||
							ctrl_data.mdm_activation_state ==
								CTRL_MDM_DEACTIVATED);
					dect_mgmt_deactivate_done_evt(
						ctrl_data.iface,
						dect_nrf91_utils_modem_status_to_net_mgmt_status(
							*status));
				}
				LOG_ERR("Error in activate/CFUN req: err %s (%d)", tmp_str,
					*status);
				break;
			}

			if ((ctrl_data.configure_params.auto_activate &&
			     (ctrl_data.mdm_activation_state != CTRL_MDM_DEACTIVATE_REQ &&
			      ctrl_data.mdm_activation_state !=
				      CTRL_MDM_REACTIVATING_DEACTIVATE)) ||
			    ctrl_data.mdm_activation_state == CTRL_MDM_ACTIVATE_REQ ||
			    ctrl_data.mdm_activation_state == CTRL_MDM_REACTIVATING_CONFIGURE) {
				dect_nrf91_ctrl_msgq_non_data_op_add(
					DECT_NRF91_CTRL_OP_MDM_ACTIVATED);
			} else {
				__ASSERT_NO_MSG(
					ctrl_data.mdm_activation_state == CTRL_MDM_DEACTIVATE_REQ ||
					ctrl_data.mdm_activation_state == CTRL_MDM_DEACTIVATED ||
					ctrl_data.mdm_activation_state ==
						CTRL_MDM_REACTIVATING_DEACTIVATE);
				dect_nrf91_ctrl_msgq_non_data_op_add(
					DECT_NRF91_CTRL_OP_MDM_DEACTIVATED);
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_ACTIVATED: {
			if (ctrl_data.mdm_activation_state != CTRL_MDM_REACTIVATING_CONFIGURE) {
				net_if_carrier_on(ctrl_data.iface);
				dect_mgmt_activate_done_evt(ctrl_data.iface, DECT_MAC_STATUS_OK);
			}
			ctrl_data.mdm_activation_state = CTRL_MDM_ACTIVATED;
			ctrl_data.tx_mdm_flow_ctrl_on = false;
#if defined(CONFIG_DECT_NRP_MAC_NRF_TX_FLOW_CTRL_BASED_ON_MDM_TX_DLC_REQS)
			ctrl_data.total_unacked_tx_data_amount = 0;
			memset(ctrl_data.dlc_data_tx_infos, 0, sizeof(ctrl_data.dlc_data_tx_infos));
#endif
			k_sem_give(&dect_mac_ctrl_reactivate_sema);
			LOG_INF("Modem activated - ready for commands");
			break;
		}

		case DECT_NRF91_CTRL_OP_MDM_DEACTIVATED: {
			bool reactivate =
				(ctrl_data.mdm_activation_state == CTRL_MDM_REACTIVATING_DEACTIVATE)
					? true
					: false;

			ctrl_data.ft_cluster_state = CTRL_FT_CLUSTER_STATE_NONE;
			ctrl_data.ft_nw_beacon_state = CTRL_FT_NW_BEACON_STATE_NONE;
			ctrl_data.configure_params.channel = 0;
			if (ctrl_data.ft_network_state != CTRL_FT_NETWORK_STATE_NONE) {
				struct dect_network_status_evt network_status_data = {
					.network_status = DECT_NETWORK_STATUS_REMOVED,
				};

				if (ctrl_data.mdm_activation_state != CTRL_MDM_DEACTIVATE_REQ) {
					/* If not deactivated by user, then we need to
					 * reactivate the modem to be able to remove network,
					 */
					reactivate = true;
				}
				dect_nrf91_child_association_all_removed();
				dect_mgmt_network_status_evt(ctrl_data.iface, network_status_data);
			}
			ctrl_data.mdm_activation_state = CTRL_MDM_DEACTIVATED;
			ctrl_data.ft_network_state = CTRL_FT_NETWORK_STATE_NONE;
			ctrl_data.configure_params.auto_start = false;

			if (reactivate) {
				LOG_INF("Deactivated. Next configure before reactivate");
				/* Reactivate modem to be able to give commands still */
				if (dect_nrf91_ctrl_configure_n_activate()) {
					LOG_ERR("Error in initiating modem configure and activate");
				} else {
					ctrl_data.mdm_activation_state =
						CTRL_MDM_REACTIVATING_CONFIGURE;
					/* we do not want to send dectivate evt when reactivating */
					break;
				}
			}
			LOG_INF("Modem state: deactivated");
			net_if_carrier_off(ctrl_data.iface);
			dect_mgmt_deactivate_done_evt(ctrl_data.iface, DECT_MAC_STATUS_OK);

			break;
		}
		case DECT_NRF91_CTRL_OP_CLUSTER_START_REQ: {
			ctrl_data.ft_cluster_state = CTRL_FT_CLUSTER_STATE_STARTING;
			dect_nrf91_ctrl_msgq_non_data_op_add(
				DECT_NRF91_CTRL_OP_RSSI_START_REQ_FROM_SETTINGS);
			break;
		}
		case DECT_NRF91_CTRL_OP_CLUSTER_CONFIG_RESP: {
			struct nrf_modem_dect_mac_cluster_configure_cb_params *params = event.data;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
			struct dect_cluster_start_resp_evt resp_evt = {
				.status = params->status,
				.cluster_channel = 0,
			};
			struct dect_network_status_evt network_status_data;

			if (params->status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				network_status_data.network_status = DECT_NETWORK_STATUS_FAILURE;
				network_status_data.dect_err_cause =
					dect_nrf91_utils_modem_status_to_net_mgmt_status(
						params->status);

				dect_nrf91_utils_modem_phy_err_to_string(params->status, tmp_str);
				LOG_ERR("Error in Cluster config: err %s (%d)", tmp_str,
					params->status);

				ctrl_data.ft_cluster_state = CTRL_FT_CLUSTER_STATE_NONE;
				ctrl_data.ft_network_state = CTRL_FT_NETWORK_STATE_NONE;
				dect_mgmt_network_status_evt(ctrl_data.iface, network_status_data);
				goto send_events;
			}
			LOG_INF("Cluster configured");

			__ASSERT_NO_MSG(set_ptr->net_mgmt_common.device_type ==
					DECT_DEVICE_TYPE_FT);

			ctrl_data.ft_cluster_state = CTRL_FT_CLUSTER_STATE_STARTED;
			resp_evt.cluster_channel = ctrl_data.configure_params.channel;
send_events:
			dect_mgmt_cluster_created_evt(ctrl_data.iface, resp_evt);
			ctrl_data.configure_params.auto_start =
				false; /* TODO: is this right if creating nw and going to start
					* network beacon also?
					*/

			if (ctrl_data.ft_network_state == CTRL_FT_NETWORK_STATE_STARTING) {
				if (set_ptr->net_mgmt_common.nw_beacon.channel !=
				    DECT_MAC_NW_BEACON_CHANNEL_NOT_USED) {
					/* Network beacon start if configured */
					struct dect_nrf91_settings *set_ptr =
						dect_nrf91_settings_ref_get();
					struct dect_nw_beacon_start_req_params params = {
						.channel =
							set_ptr->net_mgmt_common.nw_beacon.channel,
						.additional_ch_count =
							0, /* TODO: settings support to be added? */
					};

					dect_nrf91_ctrl_msgq_data_op_add(
						DECT_NRF91_CTRL_OP_MDM_NW_BEACON_START, &params,
						sizeof(struct dect_nw_beacon_start_req_params));
				} else {
					LOG_INF("Network created - network beacon not configured");
					ctrl_data.ft_network_state = CTRL_FT_NETWORK_STATE_CREATED;
					network_status_data.network_status =
						DECT_NETWORK_STATUS_CREATED;
					dect_mgmt_network_status_evt(ctrl_data.iface,
								     network_status_data);
				}
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_CLUSTER_CH_LOAD_CHANGED: {
			struct nrf_modem_dect_mac_cluster_ch_load_change_ntf_cb_params *evt_data =
				(struct nrf_modem_dect_mac_cluster_ch_load_change_ntf_cb_params *)
					event.data;

			LOG_INF("Cluster channel load changed: channel %u, busy_percentage %d",
				evt_data->rssi_result.channel,
				evt_data->rssi_result.busy_percentage);
			break;
		}
		case DECT_NRF91_CTRL_OP_NEIGHBOR_INACTIVITY: {
			struct nrf_modem_dect_mac_neighbor_inactivity_ntf_cb_params *evt_data =
				(struct nrf_modem_dect_mac_neighbor_inactivity_ntf_cb_params *)
					event.data;

			LOG_INF("Neighbor inactivity: long_rd_id %u (0x%X)", evt_data->long_rd_id,
				evt_data->long_rd_id);
			break;
		}
		case DECT_NRF91_CTRL_OP_AUTO_START: {
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

			ctrl_data.configure_params.auto_start = true;
			if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_FT) {
				struct nrf_modem_dect_mac_network_scan_params params = {
					.network_id_filter_mode =
						NRF_MODEM_DECT_MAC_NW_ID_FILTER_MODE_NONE,
					.scan_time = 2200, /* TODO: a config setting for this one?*/
					.num_channels = 0,
					.band = set_ptr->net_mgmt_common.band_nbr,
				};

				LOG_INF("FT device auto start for cluster start: "
					"starting NW scanning to see where other clusters are");

				err = nrf_modem_dect_mac_network_scan(&params);
				if (!err) {
					ctrl_data.scan_data.on_going = true;
					ctrl_data.scan_data.scan_result_cb = NULL;
					ctrl_data.scan_data.scan_params = params;
					memset(ctrl_data.scan_data.cluster_channels, 0,
					       sizeof(ctrl_data.scan_data.cluster_channels));
					ctrl_data.scan_data.current_cluster_channel_index = 0;
				} else {
					LOG_WRN("Error initiating NW scan: err %d -"
						"starting directly RSSI scan", err);
					dect_nrf91_ctrl_msgq_non_data_op_add(
						DECT_NRF91_CTRL_OP_RSSI_START_REQ_FROM_SETTINGS);
				}
			} else {
				LOG_INF("PT device auto start: starting network scan");

				struct nrf_modem_dect_mac_network_scan_params params = {
					.network_id_filter_mode =
						NRF_MODEM_DECT_MAC_NW_ID_FILTER_MODE_32BIT,
					.network_id_filter =
						set_ptr->net_mgmt_common.identities.network_id,
					.scan_time = 2200,
					.num_channels = 0,
					.band = set_ptr->net_mgmt_common.band_nbr,
				};

				err = nrf_modem_dect_mac_network_scan(&params);
				if (err) {
					dect_nrf91_utils_modem_phy_err_to_string(err, tmp_str);
					LOG_ERR("Error in Network scan: err %s (%d)", tmp_str, err);

					dect_mgmt_network_status_evt(
						ctrl_data.iface,
						(struct dect_network_status_evt){
							.network_status =
								DECT_NETWORK_STATUS_FAILURE,
							.dect_err_cause = DECT_MAC_STATUS_OS_ERROR,
							.os_err_cause = err,
						});
				}
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_RSSI_START_REQ_WITH_PARAMS: {
			struct nrf_modem_dect_mac_rssi_scan_params *params =
				(struct nrf_modem_dect_mac_rssi_scan_params *)event.data;
			int err;

			LOG_INF("DECT_NRF91_CTRL_OP_RSSI_START_REQ_WITH_PARAMS: "
				"channel_scan_length %hu, num_channels %hu, band %hhu, "
				"min_threshold %hhd, max_threshold %hhd",
				params->channel_scan_length, params->num_channels, params->band,
				params->threshold_min, params->threshold_max);

			/* Log channels */
			for (uint32_t i = 0; i < params->num_channels; i++) {
				LOG_INF("  channel[%hhu]: %d", i, params->channel_list[i]);
			}
			err = nrf_modem_dect_mac_rssi_scan(params);
			if (err) {
				dect_nrf91_utils_modem_phy_err_to_string(err, tmp_str);
				LOG_ERR("Error initiating RSSI scan by params: err %s (%d)",
					tmp_str, err);
				dect_mgmt_rssi_scan_done_evt(
					ctrl_data.iface,
					dect_nrf91_utils_modem_status_to_net_mgmt_status(err));
				if (ctrl_data.ft_network_state != CTRL_FT_NETWORK_STATE_NONE) {
					ctrl_data.ft_network_state = CTRL_FT_NETWORK_STATE_NONE;
					dect_mgmt_network_status_evt(
						ctrl_data.iface,
						(struct dect_network_status_evt){
							.network_status =
								DECT_NETWORK_STATUS_FAILURE,
							.dect_err_cause = DECT_MAC_STATUS_OS_ERROR,
							.os_err_cause = err,
						});
				}
			} else {
				LOG_INF("RSSI scan started");
				dect_nrf91_ctrl_rssi_scan_data_init(true);

			}
			break;
		}

		case DECT_NRF91_CTRL_OP_RSSI_START_REQ_FROM_SETTINGS: {
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

			LOG_INF("Starting RSSI scanning for set band #%d",
				set_ptr->net_mgmt_common.band_nbr);
			struct nrf_modem_dect_mac_rssi_scan_params params = {
				.channel_scan_length =
					set_ptr->net_mgmt_common.rssi_scan.time_per_channel_ms / 10,
				.threshold_min =
					set_ptr->net_mgmt_common.rssi_scan.free_threshold_dbm,
				.threshold_max =
					set_ptr->net_mgmt_common.rssi_scan.busy_threshold_dbm,
				.num_channels = 0, /* As a default scanning whole band,
						    * except in EU & in band 1
						    */
				.band = set_ptr->net_mgmt_common.band_nbr,
			};
			if (dect_common_utils_use_harmonized_std(
				    set_ptr->net_mgmt_common.band_nbr) == true) {
				/* Per harmonized std: only odd number channels at band #1:
				 * ETSI EN 301 406-2, V3.0.1, ch 4.3.2.3.
				 */
				bool array_filled = false;

				params.num_channels = DECT_MAC_MAX_CHANNELS_IN_RSSI_SCAN;
				array_filled = dect_common_utils_harmonized_band_channel_array_get(
					set_ptr->net_mgmt_common.band_nbr, params.channel_list,
					&params.num_channels);
				if (!array_filled) {
					LOG_ERR("RSSI scanning start: "
						"error in channel array filling");
					break;
				}
			}
			err = nrf_modem_dect_mac_rssi_scan(&params);
			if (err) {
				dect_nrf91_utils_modem_phy_err_to_string(err, tmp_str);
				LOG_ERR("Enrf_modem_dect_mac_rssi_scan failed: %s (%d)", tmp_str,
					err);
				ctrl_data.ft_cluster_state = CTRL_FT_CLUSTER_STATE_NONE;
				dect_mgmt_rssi_scan_done_evt(ctrl_data.iface,
							     DECT_MAC_STATUS_OS_ERROR);
			} else {
				dect_nrf91_ctrl_rssi_scan_data_init(false);
				LOG_INF("RSSI scan started with params: "
					"channel_scan_length %hu, num_channels %hu, band %hhu, "
					"min_threshold %hhd, max_threshold %hhd",
					params.channel_scan_length, params.num_channels,
					params.band, params.threshold_min, params.threshold_max);
			}
			break;
		}

		case DECT_NRF91_CTRL_OP_MDM_RSSI_RESULT: {
			struct dect_nrf91_ctrl_rssi_measurement_data_evt *evt_data =
				(struct dect_nrf91_ctrl_rssi_measurement_data_evt *)event.data;
			struct nrf_modem_dect_mac_rssi_result *mdm_rssi_res =
				&evt_data->rssi_result;
			bool channel_has_another_cluster = false;

			LOG_INF("RSSI scan results received");

			/* TODO: at band #1 only every other carrier -> to be done in modem? */

			bool scan_suitable_percent_ok = false;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
			struct dect_rssi_scan_result_evt l2_results_evt;
			struct dect_rssi_scan_result_data *rssi_data =
				&l2_results_evt.rssi_scan_result;
			int err = dect_nrf91_utils_mdm_rssi_results_to_l2_rssi_data(mdm_rssi_res,
										    rssi_data);

			if (err) {
				LOG_ERR("Error in converting RSSI results to L2 data: %d", err);
			}

			/* TODO: ch access rules are trying mostly to avoid collisions on
			 * legacy dect but we would like to create clusters on different channels.
			 * So, TODO: do a nw scan first and blacklist all the channels where dect
			 * nr+ clusters are found
			 */
			/* TODO: tallenna kaikki ja valitse mieluiten se jossa ei muita ja
			 * muutenkin paras?
			 */
			/* Check if we have seen another cluster in this channel */
			for (int i = 0; i < DECT_NRF91_CTRL_CLUSTER_SCAN_DATA_MAX_CHANNELS;
			     i++) {
				if (ctrl_data.scan_data.cluster_channels[i].channel ==
				    rssi_data->channel) {
					LOG_INF("RSSI results: another cluster seen in this "
						"channel %d was seen in nw scanning phase",
						rssi_data->channel);
					channel_has_another_cluster = true;
					rssi_data->another_cluster_detected_in_channel = true;
				}
			}

			/* MAC spec, ch. 5.1.2::
			 * The RD may stop the background scan process and
			 * initiate beacon transmission as defined in clause 5.2,
			 * if the number of "free" or "possible"
			 * subslots is ≥ SCAN_SUITABLE at SCAN_MEAS_DURATION
			 * at least on one channel and the number of "free"
			 * or "possible" measured subslots is sufficient for
			 * the operation of the RD.
			 */
			if (!rssi_data->all_subslots_free &&
			    rssi_data->scan_suitable_percent >=
				    set_ptr->net_mgmt_common.rssi_scan.scan_suitable_percent) {
				scan_suitable_percent_ok = true;
			}

			LOG_INF("RSSI scan results: channel %d, all_subslots_free: %s, "
				"scan_suitable_percent: %d%%, "
				"another_cluster_detected_in_channel: %s",
				rssi_data->channel,
				(rssi_data->all_subslots_free) ? "yes" : "no",
				rssi_data->scan_suitable_percent,
				(rssi_data->another_cluster_detected_in_channel) ? "yes" : "no");

			dect_mgmt_rssi_scan_result_evt(ctrl_data.iface, l2_results_evt);

			/* Store best so far scanning result */
			if (dect_nrf91_ctrl_rssi_scan_data_result_data_last_best_update(
				rssi_data)) {
				LOG_INF("best RSSI scan results so far: channel %d, "
					"all_subslots_free: %s, scan_suitable_percent: %d%%, "
					"another_cluster_detected_in_channel: %s",
					ctrl_data.rssi_scan_data.rssi_scan_result_last_best.channel,
					(ctrl_data.rssi_scan_data
						.rssi_scan_result_last_best.all_subslots_free) ?
							"yes" : "no",
					ctrl_data.rssi_scan_data
						.rssi_scan_result_last_best.scan_suitable_percent,
					(ctrl_data.rssi_scan_data.rssi_scan_result_last_best
						.another_cluster_detected_in_channel) ?
							"yes" : "no");
			}

			if (ctrl_data.rssi_scan_data.cmd_on_going) {
				/* Actual RSSI scanning command was running, let it run */
				break;
			}

			if (!channel_has_another_cluster &&
			    (rssi_data->all_subslots_free || scan_suitable_percent_ok) &&
			    (ctrl_data.configure_params.auto_start ||
			     ctrl_data.ft_cluster_state == CTRL_FT_CLUSTER_STATE_STARTING) &&
			    (ctrl_data.configure_params.channel == 0)) {
				/* MAC spec, ch. 5.1.2:
				 * if any channel where all subslots are "free", is found,
				 * then select channel for the cluster & stop the scan.
				 */
				ctrl_data.configure_params.channel = mdm_rssi_res->channel;

				err = nrf_modem_dect_mac_rssi_scan_stop();
				if (err) {
					dect_nrf91_utils_modem_phy_err_to_string(err, tmp_str);
					LOG_ERR("Error in RSSI scan stop: err %s (%d)", tmp_str,
						err);
				}
			}

			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_RSSI_COMPLETE: {
			enum nrf_modem_dect_mac_err *status =
				(enum nrf_modem_dect_mac_err *)event.data;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

			dect_mgmt_rssi_scan_done_evt(
				ctrl_data.iface,
				dect_nrf91_utils_modem_status_to_net_mgmt_status(*status));

			if (*status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(*status, tmp_str);

				LOG_ERR("Error in RSSI scan complete: err %s (%d)", tmp_str,
					*status);

				if (ctrl_data.ft_cluster_state == CTRL_FT_CLUSTER_STATE_STARTING) {
					/* TODO: check if last best stored could be used? */
					ctrl_data.ft_cluster_state = CTRL_FT_CLUSTER_STATE_NONE;
					ctrl_data.configure_params.channel = 0;
					dect_mgmt_cluster_created_evt(
						ctrl_data.iface,
						(struct dect_cluster_start_resp_evt){
							.status =
						dect_nrf91_utils_modem_status_to_net_mgmt_status(
							*status),
							.cluster_channel = 0,
						});
				}
				if (ctrl_data.ft_network_state != CTRL_FT_NETWORK_STATE_NONE) {
					struct dect_network_status_evt network_status_data = {
						.network_status = DECT_NETWORK_STATUS_FAILURE,
						.dect_err_cause =
						dect_nrf91_utils_modem_status_to_net_mgmt_status(
							*status),
					};

					ctrl_data.ft_network_state = CTRL_FT_NETWORK_STATE_NONE;
					dect_mgmt_network_status_evt(ctrl_data.iface,
								     network_status_data);
				}
				break;
			}
			LOG_INF("RSSI scan completed");
			if (ctrl_data.rssi_scan_data.cmd_on_going) {
				ctrl_data.rssi_scan_data.cmd_on_going = false;
				break;
			}

			if (ctrl_data.configure_params.auto_start ||
			    ctrl_data.ft_cluster_state == CTRL_FT_CLUSTER_STATE_STARTING) {
				if (ctrl_data.configure_params.channel == 0) {
					if (dect_nrf91_ctrl_rssi_scan_data_result_data_last_best_ok(
					    )) {
						ctrl_data.configure_params.channel =
							ctrl_data.rssi_scan_data
								.rssi_scan_result_last_best.channel;
					} else {
						LOG_ERR("No free enough channel found in RSSI scan,"
							" cannot start cluster");
						ctrl_data.ft_cluster_state =
							CTRL_FT_CLUSTER_STATE_NONE;
						dect_mgmt_cluster_created_evt(
							ctrl_data.iface,
							(struct dect_cluster_start_resp_evt){
								.status =
								DECT_MAC_STATUS_NO_RESOURCES,
								.cluster_channel = 0,
							});
						ctrl_data.ft_network_state =
							CTRL_FT_NETWORK_STATE_NONE;
						dect_mgmt_network_status_evt(
							ctrl_data.iface,
							(struct dect_network_status_evt){
								.network_status =
									DECT_NETWORK_STATUS_FAILURE,
								.dect_err_cause =
								DECT_MAC_STATUS_NO_RESOURCES,
							});
					break;
					}
				}

				/* TODO: own op evt for this: */
				LOG_INF("%s: starting a cluster on a channel %d.",
					ctrl_data.configure_params.auto_start ? "auto start"
									      : "cluster_start_req",
					ctrl_data.configure_params.channel);

				/* Start a cluster in a chosen channel */
				struct nrf_modem_dect_mac_cluster_config cluster_config = {
					.flags = {
							.has_max_tx_power = true,
							.has_rach_config = true,
						},
					.count_to_trigger = NRF_MODEM_DECT_MAC_COUNT_TO_TRIGGER_2,
					.relative_quality = NRF_MODEM_DECT_MAC_QUALITY_THRESHOLD_0,
					.min_quality = NRF_MODEM_DECT_MAC_QUALITY_THRESHOLD_0,
					.beacon_tx_power = dect_nrp_utils_dbm_to_phy_tx_power(
						set_ptr->net_mgmt_common.cluster_beacon
							.max_beacon_tx_power_dbm),
					.cluster_max_tx_power = dect_nrp_utils_dbm_to_phy_tx_power(
						set_ptr->net_mgmt_common.cluster_beacon
							.max_cluster_power_dbm),
					.cluster_beacon_period =
						set_ptr->net_mgmt_common.cluster_beacon.period,
					.cluster_channel = ctrl_data.configure_params.channel,
					.network_id = ctrl_data.configure_params.network_id,
					.rach_configuration = {
						 .policy =
							 NRF_MODEM_DECT_MAC_RACH_CONFIG_POLICY_FILL,
						 .common = {
								 .response_window_length = 8,
								 .max_transmission_length = 8,
								 .cw_min_sig = 2,
								 .cw_max_sig = 7,
							 },
						 .config = { .fill = {
									.percentage = 100,
								}}},
					.triggers = {
							.busy_threshold = 20,
						}

				};
				struct nrf_modem_dect_mac_association_config ass_config = {
					.max_num_neighbours =
						set_ptr->net_mgmt_common.cluster_beacon
							.max_num_neighbors,
					.max_num_ft_neighbours = 2,
					.neighbor_info_triggers = {
							.inactivity_timer = 10000,
						},
					.default_tx_flow_config = {
						{
							.dlc_service_type =
							NRF_MODEM_DECT_DLC_SERVICE_TYPE_3,
							.num_arq_retx = 2,
							.dlc_sdu_lifetime =
							NRF_MODEM_DECT_DLC_SDU_LIFETIME_INFINITY,
						},
						{
							.dlc_service_type =
								NRF_MODEM_DECT_DLC_SERVICE_TYPE_3,
							.num_arq_retx = 2,
							.dlc_sdu_lifetime =
							NRF_MODEM_DECT_DLC_SDU_LIFETIME_INFINITY,
						},
						{
							.priority = 3,
							.dlc_service_type =
								NRF_MODEM_DECT_DLC_SERVICE_TYPE_3,
							.num_arq_retx = 2,
							.dlc_sdu_lifetime =
							NRF_MODEM_DECT_DLC_SDU_LIFETIME_INFINITY,
						},
						{
							.priority = 4,
							.dlc_service_type =
								NRF_MODEM_DECT_DLC_SERVICE_TYPE_3,
							.num_arq_retx = 2,
							.dlc_sdu_lifetime =
							NRF_MODEM_DECT_DLC_SDU_LIFETIME_INFINITY,
						},
						{
							.priority = 5,
							.dlc_service_type =
								NRF_MODEM_DECT_DLC_SERVICE_TYPE_3,
							.num_arq_retx = 2,
							.dlc_sdu_lifetime =
							NRF_MODEM_DECT_DLC_SDU_LIFETIME_INFINITY,
						},
						{
							.priority = 6,
							.dlc_service_type =
								NRF_MODEM_DECT_DLC_SERVICE_TYPE_3,
							.num_arq_retx = 2,
							.dlc_sdu_lifetime =
							NRF_MODEM_DECT_DLC_SDU_LIFETIME_INFINITY,
						}},
				};
				struct nrf_modem_dect_mac_cluster_configure_params params = {
					.include_load_info = false,
					.include_route_info = false,
					.cluster_period_start_offset = 0,
					.association_config = &ass_config,
					.cluster_config = &cluster_config,
				};

				/* Set our IPv6 address prefix to modem to be passed to children */
				struct dect_nrf91_ipv6_prefix global_prefix;

				if (dect_nrf91_sink_ipv6_prefix_get(&global_prefix)) {
					/* Pass prefix to children */
					__ASSERT_NO_MSG(global_prefix.len == 8);
					memcpy(&cluster_config.ipv6_config.address,
					       global_prefix.prefix.s6_addr, global_prefix.len);

					cluster_config.ipv6_config.type =
						NRF_MODEM_DECT_MAC_IPV6_ADDRESS_TYPE_PREFIX;
				} else {
					/* Using link local (already set to us)*/
					cluster_config.ipv6_config.type =
						NRF_MODEM_DECT_MAC_IPV6_ADDRESS_TYPE_NONE;
					LOG_WRN("%s: no IPv6 prefix to set - using link local only",
						__func__);
				}

				err = nrf_modem_dect_mac_cluster_configure(&params);
				if (err) {
					LOG_ERR("nrf_modem_dect_mac_cluster_configure returned err "
						"%d",
						err);
					if (ctrl_data.ft_network_state !=
					    CTRL_FT_NETWORK_STATE_NONE) {
						ctrl_data.ft_network_state =
							CTRL_FT_NETWORK_STATE_NONE;
						dect_mgmt_network_status_evt(
							ctrl_data.iface,
							(struct dect_network_status_evt){
								.network_status =
									DECT_NETWORK_STATUS_FAILURE,
								.dect_err_cause =
									DECT_MAC_STATUS_OS_ERROR,
								.os_err_cause = err,
							});
					}
				}
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_RSSI_STOPPED: {
			enum nrf_modem_dect_mac_err *status =
				(enum nrf_modem_dect_mac_err *)event.data;
			if (*status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(*status, tmp_str);

				LOG_ERR("Error in RSSI scan stop: err %s (%d)", tmp_str, *status);
			} else {
				LOG_INF("RSSI scan stopped");
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_NW_BEACON_START: {
			struct dect_nw_beacon_start_req_params *params =
				(struct dect_nw_beacon_start_req_params *)event.data;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

			struct nrf_modem_dect_mac_network_beacon_configure_params beacon_params = {
				.channel = params->channel,
				.num_additional_channels = 0,
				.nw_beacon_period = set_ptr->net_mgmt_common.nw_beacon.period,
			};

			beacon_params.num_additional_channels = params->additional_ch_count;
			beacon_params.additional_channels = params->additional_ch_list;

			LOG_INF("starting network beacon on a channel %d.", params->channel);

			err = nrf_modem_dect_mac_network_beacon_configure(&beacon_params);
			if (err) {
				LOG_ERR("nrf_modem_dect_mac_network_beacon_configure failed: %d",
					err);
				ctrl_data.ft_nw_beacon_state = CTRL_FT_NW_BEACON_STATE_NONE;
				dect_mgmt_nw_beacon_start_evt(
					ctrl_data.iface,
					dect_nrf91_utils_modem_status_to_net_mgmt_status(err));

				if (ctrl_data.ft_network_state != CTRL_FT_NETWORK_STATE_NONE) {
					ctrl_data.ft_network_state = CTRL_FT_NETWORK_STATE_NONE;
					dect_mgmt_network_status_evt(
						ctrl_data.iface,
						(struct dect_network_status_evt){
							.network_status =
								DECT_NETWORK_STATUS_FAILURE,
							.dect_err_cause = DECT_MAC_STATUS_OS_ERROR,
							.os_err_cause = err,
						});
				}
			} else {
				ctrl_data.ft_nw_beacon_state = CTRL_FT_NW_BEACON_STATE_STARTING;
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_NW_BEACON_START_OR_STOP_DONE: {
			struct nrf_modem_dect_mac_network_beacon_configure_cb_params *evt_data =
				(struct nrf_modem_dect_mac_network_beacon_configure_cb_params *)
					event.data;

			if (evt_data->status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(evt_data->status, tmp_str);

				LOG_ERR("Error in network beacon start/stop: err %s (%d)", tmp_str,
					evt_data->status);
				if (ctrl_data.ft_nw_beacon_state ==
				    CTRL_FT_NW_BEACON_STATE_STOPPING) {
					dect_mgmt_nw_beacon_stop_evt(
						ctrl_data.iface,
						dect_nrf91_utils_modem_status_to_net_mgmt_status(
							evt_data->status));
				} else if (ctrl_data.ft_nw_beacon_state ==
					   CTRL_FT_NW_BEACON_STATE_STARTING) {
					ctrl_data.ft_network_state = CTRL_FT_NETWORK_STATE_CREATED;
					dect_mgmt_nw_beacon_start_evt(
						ctrl_data.iface,
						dect_nrf91_utils_modem_status_to_net_mgmt_status(
							evt_data->status));

					/* TODO: cluster is still running*/
					if (ctrl_data.ft_network_state ==
					    CTRL_FT_NETWORK_STATE_STARTING) {
						ctrl_data.ft_network_state =
							CTRL_FT_NETWORK_STATE_CREATED;
						dect_mgmt_network_status_evt(
							ctrl_data.iface,
							(struct dect_network_status_evt){
								.network_status =
									DECT_NETWORK_STATUS_CREATED,
							});
						LOG_WRN("Network beacon start failed, "
							"but cluster is still running");
					}
				}
				ctrl_data.ft_nw_beacon_state = CTRL_FT_NW_BEACON_STATE_NONE;
			} else {
				if (ctrl_data.ft_nw_beacon_state ==
				    CTRL_FT_NW_BEACON_STATE_STARTING) {
					LOG_INF("Network beacon started");
					ctrl_data.ft_nw_beacon_state =
						CTRL_FT_NW_BEACON_STATE_STARTED;
					dect_mgmt_nw_beacon_start_evt(ctrl_data.iface,
								      DECT_MAC_STATUS_OK);
					ctrl_data.ft_network_state = CTRL_FT_NETWORK_STATE_CREATED;
					dect_mgmt_network_status_evt(
						ctrl_data.iface,
						(struct dect_network_status_evt){
							.network_status =
								DECT_NETWORK_STATUS_CREATED,
						});
				} else {
					__ASSERT_NO_MSG(ctrl_data.ft_nw_beacon_state ==
							CTRL_FT_NW_BEACON_STATE_STOPPING);
					LOG_INF("Network beacon stopped");
					ctrl_data.ft_nw_beacon_state = CTRL_FT_NW_BEACON_STATE_NONE;
					dect_mgmt_nw_beacon_stop_evt(ctrl_data.iface,
								     DECT_MAC_STATUS_OK);
				}
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_NW_BEACON_STOP: {
			struct nrf_modem_dect_mac_network_beacon_configure_params beacon_params = {
				.channel = 0,
				.num_additional_channels = 0,
			};

			LOG_INF("Stopping network beacon.");

			err = nrf_modem_dect_mac_network_beacon_configure(&beacon_params);
			if (err) {
				dect_nrf91_utils_modem_phy_err_to_string(err, tmp_str);
				LOG_ERR("nrf_modem_dect_mac_network_beacon_configure failed: %s "
					"(%d)",
					tmp_str, err);
				ctrl_data.ft_nw_beacon_state = CTRL_FT_NW_BEACON_STATE_NONE;
				dect_mgmt_nw_beacon_stop_evt(ctrl_data.iface,
							     DECT_MAC_STATUS_OS_ERROR);
			} else {
				ctrl_data.ft_nw_beacon_state = CTRL_FT_NW_BEACON_STATE_STOPPING;
			}
			break;
		}

		case DECT_NRF91_CTRL_OP_MDM_CLUSTER_BEACON_RCVD: {
			struct nrf_modem_dect_mac_cluster_beacon_ntf_cb_params *params = event.data;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
			bool add_to_cluster_channels = (ctrl_data.scan_data.on_going &&
				set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_FT);

			/* Did we already mark this to cluster_channels? */
			for (int i = 0; i < DECT_NRF91_CTRL_CLUSTER_SCAN_DATA_MAX_CHANNELS &&
			     add_to_cluster_channels; i++) {
				if (ctrl_data.scan_data.cluster_channels[i].channel ==
				    params->channel) {
					add_to_cluster_channels = false;
					break;
				}
			}
			/* Store channel and RSSI */
			if (add_to_cluster_channels) {
				ctrl_data.scan_data.cluster_channels[
					ctrl_data.scan_data.current_cluster_channel_index].channel =
					params->channel;
				ctrl_data.scan_data.cluster_channels[
				ctrl_data.scan_data.current_cluster_channel_index].rssi_2 =
				params->rx_signal_info.rssi_2;
				ctrl_data.scan_data.current_cluster_channel_index++;
				__ASSERT_NO_MSG(ctrl_data.scan_data.current_cluster_channel_index <
						ARRAY_SIZE(ctrl_data.scan_data.cluster_channels));
			}
			if (ctrl_data.scan_data.scan_result_cb) {
				struct dect_scan_result_evt scan_result = {
					.beacon_type = DECT_SCAN_RESULT_TYPE_CLUSTER_BEACON,
					.channel = params->channel,
					.transmitter_short_rd_id = params->transmitter_short_rd_id,
					.transmitter_long_rd_id = params->transmitter_long_rd_id,
					.network_id = params->network_id,
					.rx_signal_info.mcs = params->rx_signal_info.mcs,
					.rx_signal_info.transmit_power =
						params->rx_signal_info.transmit_power,
					.rx_signal_info.rssi_2 = params->rx_signal_info.rssi_2,
					.rx_signal_info.snr = params->rx_signal_info.snr,
				};

				ctrl_data.scan_data.scan_result_cb(ctrl_data.iface, 0,
								   &scan_result);
			}
			if (set_ptr->net_mgmt_common.device_type != DECT_DEVICE_TYPE_PT) {
				/* Cluster beacon can be also received if FT device and
				 * if same short nw id and in same channel
				 */
				LOG_DBG("Cluster beacon received, but not PT device!!!");
				break;
			}
			if (ctrl_data.ass_config.pt_association_state !=
				    CTRL_PT_ASSOCIATION_STATE_ASSOCIATED &&
			    (set_ptr->net_mgmt_common.network_join.target_ft_long_rd_id ==
				     DECT_SETT_NETWORK_JOIN_TARGET_FT_ANY ||
			     set_ptr->net_mgmt_common.network_join.target_ft_long_rd_id ==
				     params->transmitter_long_rd_id)) {
				ctrl_data.ass_config.pt_association_state =
					CTRL_PT_ASSOCIATION_STATE_CLUSTER_BEACON_RECEIVED;
				ctrl_data.ass_config.network_id = params->network_id;
				ctrl_data.ass_config.parent_long_rd_id =
					params->transmitter_long_rd_id;

				if (ctrl_data.configure_params.auto_start) {
					if (nrf_modem_dect_mac_network_scan_stop() != 0) {
						LOG_ERR("OP_MDM_CLUSTER_BEACON_RCVD: error in "
							"Network scan stop!");
					}
				}
			}

			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_NW_BEACON_RCVD: {
			struct nrf_modem_dect_mac_network_beacon_ntf_cb_params *params = event.data;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

			LOG_DBG("Network beacon information:");
			LOG_DBG("  network id (32bit).............................%u (0x%08x)",
				params->network_id, params->network_id);
			LOG_DBG("  transmitter id (long RD ID)....................%u (0x%08x)",
				params->transmitter_long_rd_id, params->transmitter_long_rd_id);
			LOG_DBG("  short RD ID....................................%u (0x%04x)",
				params->transmitter_short_rd_id, params->transmitter_short_rd_id);
			LOG_DBG("  channel number.................................%d",
				params->channel);
			LOG_DBG("  next_cluster_channel number....................%d",
				params->beacon.next_cluster_channel);

			if (ctrl_data.scan_data.scan_result_cb) {
				struct dect_scan_result_evt scan_result = {
					.beacon_type = DECT_SCAN_RESULT_TYPE_NW_BEACON,
					.channel = params->channel,
					.transmitter_short_rd_id = params->transmitter_short_rd_id,
					.transmitter_long_rd_id = params->transmitter_long_rd_id,
					.network_id = params->network_id,
					.rx_signal_info.mcs = params->rx_signal_info.mcs,
					.rx_signal_info.transmit_power =
						params->rx_signal_info.transmit_power,
					.rx_signal_info.rssi_2 = params->rx_signal_info.rssi_2,
					.rx_signal_info.snr = params->rx_signal_info.snr,
				};
				struct dect_network_beacon_data nw_beacon = {
					.next_cluster_channel = params->beacon.next_cluster_channel,
					.current_cluster_channel =
						params->beacon.current_cluster_channel,
					.num_network_beacon_channels =
						params->beacon.num_network_beacon_channels,
				};

				for (int i = 0; i < params->beacon.num_network_beacon_channels;
				     i++) {
					nw_beacon.network_beacon_channels[i] =
						params->beacon.network_beacon_channels[i];
				}
				scan_result.network_beacon = nw_beacon;
				ctrl_data.scan_data.scan_result_cb(ctrl_data.iface, 0,
								   &scan_result);
			}

			if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_PT &&
			    ctrl_data.ass_config.pt_association_state !=
				    CTRL_PT_ASSOCIATION_STATE_ASSOCIATED) {
				ctrl_data.ass_config.pt_association_state =
					CTRL_PT_ASSOCIATION_STATE_CLUSTER_FOUND;
				ctrl_data.ass_config.network_id = params->network_id;
				ctrl_data.ass_config.parent_long_rd_id =
					params->transmitter_long_rd_id;

				if (ctrl_data.configure_params.auto_start &&
				    (set_ptr->net_mgmt_common.network_join.target_ft_long_rd_id ==
					     DECT_SETT_NETWORK_JOIN_TARGET_FT_ANY ||
				     set_ptr->net_mgmt_common.network_join.target_ft_long_rd_id ==
					     params->transmitter_long_rd_id)) {
					err = nrf_modem_dect_mac_network_scan_stop();
					if (err) {
						LOG_ERR("%s: error in stopping nw scan, err: %d",
							(__func__), err);
					}
				}
			}

			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_NW_SCAN_COMPLETE: {
			struct nrf_modem_dect_mac_network_scan_cb_params *evt_data =
				(struct nrf_modem_dect_mac_network_scan_cb_params *)event.data;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

			if (evt_data->status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(evt_data->status, tmp_str);

				LOG_ERR("Network scan completed with err %s (%d)", tmp_str,
					evt_data->status);
				/* TODO: send nw status with error if doing a join? */
			} else {
				LOG_INF("Network scan completed: %d channels scanned",
					evt_data->num_scanned_channels);
			}
			ctrl_data.scan_data.on_going = false;
			if (ctrl_data.scan_data.scan_result_cb) {
				ctrl_data.scan_data.scan_result_cb(
					ctrl_data.iface,
					dect_nrf91_utils_modem_status_to_net_mgmt_status(
						evt_data->status),
					NULL);
				ctrl_data.scan_data.scan_result_cb = NULL;
			}
			if (!ctrl_data.configure_params.auto_start) {
				/* We are done here */
				break;
			}
			if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_PT) {
				if (ctrl_data.ass_config.pt_association_state ==
				    CTRL_PT_ASSOCIATION_STATE_CLUSTER_FOUND) {
					LOG_INF("auto start: sending cluster beacon receive req, "
						"Long RD ID: 0x%X, NW ID: 0x%X",
						ctrl_data.ass_config.parent_long_rd_id,
						ctrl_data.ass_config.network_id);

					struct nrf_modem_dect_mac_cluster_beacon_config
						cluster_config = {
							.long_rd_id = ctrl_data.ass_config
									      .parent_long_rd_id,
							.network_id =
								ctrl_data.ass_config.network_id,
						};
					struct nrf_modem_dect_mac_cluster_beacon_receive_params
						params = {
							.num_configs = 1,
							.configs = &cluster_config,
						};

					err = nrf_modem_dect_mac_cluster_beacon_receive(&params);
					if (err) {
						LOG_ERR("Error in Cluster beacon receive: err %d",
							err);
					}
				} else if (ctrl_data.ass_config.pt_association_state ==
					   CTRL_PT_ASSOCIATION_STATE_CLUSTER_BEACON_RECEIVED) {
					dect_mac_ctrl_trigger_association();
				} else {
					dect_mgmt_network_status_evt(
						ctrl_data.iface,
						(struct dect_network_status_evt){
							.network_status =
								DECT_NETWORK_STATUS_FAILURE,
							.dect_err_cause =
								DECT_MAC_STATUS_RD_NOT_FOUND,
						});
					LOG_WRN("No cluster found with NW scan");
				}
			} else {
				__ASSERT_NO_MSG(
					set_ptr->net_mgmt_common.device_type ==
						DECT_DEVICE_TYPE_FT);
				dect_nrf91_ctrl_msgq_non_data_op_add(
					DECT_NRF91_CTRL_OP_RSSI_START_REQ_FROM_SETTINGS);
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_NW_SCAN_STOPPED: {
			enum nrf_modem_dect_mac_err *status =
				(enum nrf_modem_dect_mac_err *)event.data;

			if (*status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(*status, tmp_str);
				LOG_ERR("Network scan stopping failed with err %s (%d)", tmp_str,
					*status);
				break;
			}
			LOG_INF("Network scan stopped");
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_CLUSTER_RCV_COMPLETE: {
			enum nrf_modem_dect_mac_err *status =
				(enum nrf_modem_dect_mac_err *)event.data;

			if (*status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(*status, tmp_str);
				LOG_ERR("Cluster beacon rcv failed with err %s (%d)", tmp_str,
					*status);
				break;
			}
			LOG_INF("Cluster beacon reception completed");

			if (ctrl_data.configure_params.auto_start) {
				if (ctrl_data.ass_config.pt_association_state ==
				    CTRL_PT_ASSOCIATION_STATE_CLUSTER_BEACON_RECEIVED) {
					dect_mac_ctrl_trigger_association();
				} else {
					LOG_WRN("No cluster found with Cluster beacon receive");
				}
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_CLUSTER_INFO: {
			struct nrf_modem_dect_mac_cluster_info_cb_params *evt_data = event.data;
			struct dect_cluster_info_evt l2_results_evt = {
				.status = dect_nrf91_utils_modem_status_to_net_mgmt_status(
					evt_data->status),
			};
			struct dect_cluster_status_info *cluster_info = &l2_results_evt.status_info;
			struct dect_rssi_scan_result_data *rssi_result = &cluster_info->rssi_result;

			if (evt_data->status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(evt_data->status, tmp_str);
				LOG_ERR("Cluster info rcv failed with err %s (%d)", tmp_str,
					evt_data->status);
			} else {
				cluster_info->num_association_failures =
					evt_data->info.num_association_requests;
				cluster_info->num_association_requests =
					evt_data->info.num_association_failures;
				cluster_info->num_neighbors = evt_data->info.num_neighbors;
				cluster_info->num_ftpt_neighbors =
					evt_data->info.num_ftpt_neighbors;
				cluster_info->num_rach_rx_pdc = evt_data->info.num_rach_rx_pdc;
				cluster_info->num_rach_rx_pcc_crc_failures =
					evt_data->info.num_rach_rx_pcc_crc_failures;
				cluster_info->rssi_result.busy_percentage =
					evt_data->info.rssi_result.busy_percentage;

				LOG_DBG("Cluster status information:");
				LOG_DBG("  num_association_requests.......................%u",
					evt_data->info.num_association_requests);
				LOG_DBG("  num_association_failures.......................%u",
					evt_data->info.num_association_failures);
				LOG_DBG("  num_neighbors..................................%u",
					evt_data->info.num_neighbors);
				LOG_DBG("  num_ftpt_neighbors.............................%u",
					evt_data->info.num_ftpt_neighbors);
				LOG_DBG("  num_rach_rx_pdc................................%u",
					evt_data->info.num_rach_rx_pdc);
				LOG_DBG("  num_rach_rx_pcc_crc_fail.......................%u",
					evt_data->info.num_rach_rx_pcc_crc_failures);
				LOG_DBG("  channel busy percentage........................%u",
					evt_data->info.rssi_result.busy_percentage);

				struct nrf_modem_dect_mac_rssi_result *mdm_rssi_res =
					&evt_data->info.rssi_result;
				int err = dect_nrf91_utils_mdm_rssi_results_to_l2_rssi_data(
					mdm_rssi_res, rssi_result);

				if (err) {
					LOG_ERR("Error in converting RSSI results to L2 data: %d",
						err);
				}
			}
			/* Send L2 evt */
			dect_mgmt_cluster_info_evt(ctrl_data.iface, l2_results_evt);

			break;
		}
		case DECT_NRF91_CTRL_OP_CLUSTER_BEACON_RX_FAILURE: {
			struct nrf_modem_dect_mac_cluster_beacon_rx_failure_ntf_cb_params
				*evt_data =
					(struct
					 nrf_modem_dect_mac_cluster_beacon_rx_failure_ntf_cb_params
						 *)event.data;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

			LOG_INF("Max nbr of cluster beacon RX failures (%d): long_rd_id %u (0x%X) -"
				" starting releasing the association",
				set_ptr->net_mgmt_common.association.max_beacon_rx_failures,
				evt_data->long_rd_id, evt_data->long_rd_id);

			/* OK, configured max amount of cluster beacon RX failures, so we can
			 * start a dissociation process.
			 */
			if (ctrl_data.ass_config.pt_association_state ==
			    CTRL_PT_ASSOCIATION_STATE_ASSOCIATED) {
				__ASSERT_NO_MSG(
					ctrl_data.ass_config.parent_long_rd_id ==
						evt_data->long_rd_id);
				dect_nrf91_ctrl_associate_release_cmd(
					ctrl_data.ass_config.parent_long_rd_id);
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_NEIGHBOR_PAGING_FAILURE: {
			/* Message is an indication that a neighbor is tried to be paged for
			 * incoming data but there was no answer. We release the association.
			 */
			struct nrf_modem_dect_mac_neighbor_paging_failure_ntf_cb_params *params =
				(struct nrf_modem_dect_mac_neighbor_paging_failure_ntf_cb_params *)
					event.data;

			LOG_WRN("Neighbor paging failure: long_rd_id %u (0x%X) - "
				"releasing association",
				params->long_rd_id, params->long_rd_id);

			dect_nrf91_ctrl_associate_release_cmd(params->long_rd_id);
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_NEIGHBOR_INFO: {
			struct nrf_modem_dect_mac_neighbor_info_cb_params *evt_data = event.data;
			struct dect_neighbor_info_evt l2_results_evt = {
				.long_rd_id = evt_data->long_rd_id,
				.status = dect_nrf91_utils_modem_status_to_net_mgmt_status(
					evt_data->status),
			};

			if (evt_data->status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(evt_data->status, tmp_str);
				LOG_ERR("Neighbor info rcv failed with err %s (%d)", tmp_str,
					evt_data->status);
			} else {
				LOG_DBG("Neighbor status information:");
				LOG_DBG("  Neighbor (long RD ID).......................%u (0x%08x)",
					evt_data->long_rd_id, evt_data->long_rd_id);
				LOG_DBG("  Network ID..................................%u (0x%08x)",
					evt_data->network_id, evt_data->network_id);
				LOG_DBG("  Associated..................................%s",
					evt_data->associated ? "true" : "false");
				LOG_DBG("  FT mode.....................................%s",
					evt_data->ft_mode ? "true" : "false");
				LOG_DBG("  Channel.....................................%d",
					evt_data->channel);
				LOG_DBG("  Time in ms since neighbor is last seen......%u ms",
					evt_data->time_since_last_rx_ms);
				LOG_DBG("  RX mcs......................................%u",
					evt_data->last_rx_signal_info.mcs);
				LOG_DBG("  RX transmit power...........................%d",
					evt_data->last_rx_signal_info.transmit_power);
				LOG_DBG("  RX RSSI 2...................................%d",
					evt_data->last_rx_signal_info.rssi_2);
				LOG_DBG("  RX SNR.....................................%d",
					evt_data->last_rx_signal_info.snr);
				LOG_DBG("  beacon_average_rx_rssi_2..................%d",
					evt_data->beacon_average_rx_rssi_2);
				LOG_DBG("  beacon_average_rx_snr.......................%d",
					evt_data->beacon_average_rx_snr);
				l2_results_evt.network_id = evt_data->network_id;
				l2_results_evt.associated = evt_data->associated;
				l2_results_evt.ft_mode = evt_data->ft_mode;
				l2_results_evt.channel = evt_data->channel;
				l2_results_evt.time_since_last_rx_ms =
					evt_data->time_since_last_rx_ms;
				l2_results_evt.last_rx_signal_info.mcs =
					evt_data->last_rx_signal_info.mcs;
				l2_results_evt.last_rx_signal_info.transmit_power =
					evt_data->last_rx_signal_info.transmit_power;
				l2_results_evt.last_rx_signal_info.rssi_2 =
					evt_data->last_rx_signal_info.rssi_2;
				l2_results_evt.last_rx_signal_info.snr =
					evt_data->last_rx_signal_info.snr;
				l2_results_evt.beacon_average_rx_txpower =
					evt_data->beacon_average_rx_txpower;
				l2_results_evt.beacon_average_rx_rssi_2 =
					evt_data->beacon_average_rx_rssi_2;
				l2_results_evt.beacon_average_rx_snr =
					evt_data->beacon_average_rx_snr;

				LOG_DBG("  total_missed_cluster_beacons................%u",
					evt_data->status_info.total_missed_cluster_beacons);
				LOG_DBG("  current_consecutive_missed_cluster_beacons..%u",
					evt_data->status_info
						.current_consecutive_missed_cluster_beacons);
				LOG_DBG("  num_rx_paging...............................%u",
					evt_data->status_info.num_rx_paging);
				LOG_DBG("  average_rx_mcs..............................%u",
					evt_data->status_info.average_rx_mcs);
				LOG_DBG("  average_rx_txpower..........................%d",
					evt_data->status_info.average_rx_txpower);
				LOG_DBG("  average_rx_rssi_2...........................%d",
					evt_data->status_info.average_rx_rssi_2);
				LOG_DBG("  average_rx_snr..............................%d",
					evt_data->status_info.average_rx_snr);
				LOG_DBG("  average_tx_mcs..............................%u",
					evt_data->status_info.average_tx_mcs);
				LOG_DBG("  average_tx_txpower..........................%d",
					evt_data->status_info.average_tx_txpower);
				LOG_DBG("  num_tx_attempts.............................%u",
					evt_data->status_info.num_tx_attempts);
				LOG_DBG("  num_lbt_failures............................%u",
					evt_data->status_info.num_lbt_failures);
				LOG_DBG("  num_rx_pdc..................................%u",
					evt_data->status_info.num_rx_pdc);
				LOG_DBG("  num_rx_pdc_crc_fail.........................%u",
					evt_data->status_info.num_rx_pdc_crc_failures);
				LOG_DBG("  num_no_response.............................%u",
					evt_data->status_info.num_no_response);
				LOG_DBG("  num_harq_ack................................%u",
					evt_data->status_info.num_harq_ack);
				LOG_DBG("  num_harq_nack...............................%u",
					evt_data->status_info.num_harq_nack);
				LOG_DBG("  num_arq_retx................................%u",
					evt_data->status_info.num_arq_retx);
				LOG_DBG("  inactive_time_ms............................%u ms",
					evt_data->status_info.inactive_time_ms);

				l2_results_evt.status_info.total_missed_cluster_beacons =
					evt_data->status_info.total_missed_cluster_beacons;
				l2_results_evt.status_info
					.current_consecutive_missed_cluster_beacons =
					evt_data->status_info
						.current_consecutive_missed_cluster_beacons;
				l2_results_evt.status_info.num_rx_paging =
					evt_data->status_info.num_rx_paging;
				l2_results_evt.status_info.average_rx_mcs =
					evt_data->status_info.average_rx_mcs;
				l2_results_evt.status_info.average_rx_txpower =
					evt_data->status_info.average_rx_txpower;
				l2_results_evt.status_info.average_rx_rssi_2 =
					evt_data->status_info.average_rx_rssi_2;
				l2_results_evt.status_info.average_rx_snr =
					evt_data->status_info.average_rx_snr;
				l2_results_evt.status_info.average_tx_mcs =
					evt_data->status_info.average_tx_mcs;
				l2_results_evt.status_info.average_tx_txpower =
					evt_data->status_info.average_tx_txpower;
				l2_results_evt.status_info.num_tx_attempts =
					evt_data->status_info.num_tx_attempts;
				l2_results_evt.status_info.num_lbt_failures =
					evt_data->status_info.num_lbt_failures;
				l2_results_evt.status_info.num_rx_pdc =
					evt_data->status_info.num_rx_pdc;
				l2_results_evt.status_info.num_rx_pdc_crc_failures =
					evt_data->status_info.num_rx_pdc_crc_failures;
				l2_results_evt.status_info.num_no_response =
					evt_data->status_info.num_no_response;
				l2_results_evt.status_info.num_harq_ack =
					evt_data->status_info.num_harq_ack;
				l2_results_evt.status_info.num_harq_nack =
					evt_data->status_info.num_harq_nack;
				l2_results_evt.status_info.num_arq_retx =
					evt_data->status_info.num_arq_retx;
				l2_results_evt.status_info.inactive_time_ms =
					evt_data->status_info.inactive_time_ms;
			}

			/* Send L2 evt */
			dect_mgmt_neighbor_info_evt(ctrl_data.iface, l2_results_evt);
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_NEIGHBOR_LIST: {
			struct nrf_modem_dect_mac_neighbor_list_cb_params *evt_data =
				(struct nrf_modem_dect_mac_neighbor_list_cb_params *)event.data;
			struct dect_neighbor_list_evt l2_results_evt;

			l2_results_evt.neighbor_count = evt_data->num_neighbors;
			l2_results_evt.status =
				dect_nrf91_utils_modem_status_to_net_mgmt_status(evt_data->status);

			if (evt_data->status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(evt_data->status, tmp_str);
				LOG_ERR("Neighbor list rcv failed with err %s (%d)", tmp_str,
					evt_data->status);

				l2_results_evt.neighbor_count = 0;
				dect_mgmt_neighbor_list_evt(ctrl_data.iface, l2_results_evt);
				break;
			}

			LOG_DBG("Neighbors:");
			LOG_DBG("  num_neighbors..................................%u",
				evt_data->num_neighbors);
			for (uint32_t i = 0; i < evt_data->num_neighbors; i++) {
				LOG_DBG("  Neighbor (long RD ID)..........................%u "
					"(0x%08x)",
					evt_data->long_rd_ids[i], evt_data->long_rd_ids[i]);
				if (i >= DECT_NRF91_MAX_NEIGHBOR_LIST_COUNT) {
					LOG_ERR("Neighbor list too long, max %d, rd id %d ignored",
						DECT_NRF91_MAX_NEIGHBOR_LIST_COUNT,
						evt_data->long_rd_ids[i]);
					continue;
				}
				l2_results_evt.neighbor_long_rd_ids[i] = evt_data->long_rd_ids[i];
			}
			dect_mgmt_neighbor_list_evt(ctrl_data.iface, l2_results_evt);
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_FLOW_CONTROL: {
			struct nrf_modem_dect_dlc_flow_control_ntf_cb_params *evt_data = event.data;

			k_mutex_lock(&dect_mac_ctrl_data_mtx, K_FOREVER);
			ctrl_data.tx_mdm_flow_ctrl_on =
				evt_data->status == NRF_MODEM_DECT_DLC_FLOW_CTRL_STATUS_ON ? true
											   : false;
			k_mutex_unlock(&dect_mac_ctrl_data_mtx);

			LOG_DBG("Flow control %s:",
				evt_data->status == NRF_MODEM_DECT_DLC_FLOW_CTRL_STATUS_ON ? "ON"
											   : "OFF");
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_DLC_DATA_RESP: {
#if defined(CONFIG_DECT_NRP_MAC_NRF_TX_FLOW_CTRL_BASED_ON_MDM_TX_DLC_REQS)
			struct dect_nrf91_ctrl_dlc_data_tx_resp_evt *evt_data = event.data;
			int arr_index;

			if (evt_data->status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				dect_nrf91_utils_modem_phy_err_to_string(evt_data->status, tmp_str);
				LOG_ERR("DLC data response failed to rd id %d with err %s (%d), "
					"transaction id %d",
					evt_data->long_rd_id, tmp_str, evt_data->status,
					evt_data->acked_data[0].transaction_id);
			}

			/* Update total_unacked_tx_data_amount (even if is error) */
			k_mutex_lock(&dect_mac_ctrl_data_mtx, K_FOREVER);
			for (int i = 0; i < evt_data->num_acked_data; i++) {
				arr_index = evt_data->acked_data[i].transaction_id -
					    DECT_MAC_DATA_TX_HANDLE_START;
				if (arr_index < 0 ||
				    arr_index >= DECT_NRF91_DLC_DATA_INFO_MAX_COUNT) {
					LOG_ERR("Transaction ID %d not found in array",
						evt_data->acked_data[i].transaction_id);
					continue;
				}
				ctrl_data.dlc_data_tx_infos[arr_index].req_on_going = false;
				ctrl_data.total_unacked_tx_data_amount -=
					ctrl_data.dlc_data_tx_infos[arr_index].data_len;
				ctrl_data.total_unacked_req_amount--;
			}
			k_mutex_unlock(&dect_mac_ctrl_data_mtx);
			LOG_DBG("DLC data response (towards RD ID %d): "
				"total %d bytes unacked left, total req count %d",
				evt_data->long_rd_id,
				ctrl_data.total_unacked_tx_data_amount,
				ctrl_data.total_unacked_req_amount);
#endif
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_IND: {
			struct nrf_modem_dect_mac_association_ntf_cb_params *params = event.data;

			if (params->status !=
			    NRF_MODEM_DECT_MAC_ASSOCIATION_INDICATION_STATUS_SUCCESS) {
				dect_nrf91_utils_modem_association_ind_err_to_string(params->status,
										     tmp_str);
				LOG_ERR("Association indication with err %s (%d) with long RD ID: "
					"0x%X",
					tmp_str, params->status, params->long_rd_id);
				break;
			}
			LOG_INF("New child: association with long RD ID: %d (0x%X)",
				params->long_rd_id, params->long_rd_id);

			dect_nrf91_child_association_created(params->long_rd_id);
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_RESP: {
			struct nrf_modem_dect_mac_association_cb_params *params = event.data;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
			struct dect_network_status_evt network_status_data = {
				.network_status = DECT_NETWORK_STATUS_FAILURE,
				.dect_err_cause = dect_nrf91_utils_modem_status_to_net_mgmt_status(
					params->status),
			};

			/* Association response only with PT devices */
			__ASSERT_NO_MSG(set_ptr->net_mgmt_common.device_type ==
					DECT_DEVICE_TYPE_PT);
			if (params->status != NRF_MODEM_DECT_MAC_STATUS_OK) {
				/* Modem operation failed */
				struct dect_association_req_result_evt resp = {
					.transmitter_long_rd_id = params->long_rd_id,
					.accepted = false,
					.reject_cause =
						DECT_MAC_ASSOCIATION_REJECT_CAUSE_OTHER_REASON,
				};

				dect_mgmt_association_req_result_evt(ctrl_data.iface, resp);
				dect_mgmt_network_status_evt(ctrl_data.iface, network_status_data);

				ctrl_data.configure_params.auto_start = false;
				ctrl_data.ass_config.pt_association_state =
					CTRL_PT_ASSOCIATION_STATE_NONE;

				dect_nrf91_utils_modem_phy_err_to_string(params->status, tmp_str);
				LOG_ERR("Modem operation failed for Association Response with "
					"err %s (%d) with long RD ID: 0x%X",
					tmp_str, params->status, params->long_rd_id);
				break;
			}

			if (!params->flags.has_association_response) {
				struct dect_association_req_result_evt resp = {
					.transmitter_long_rd_id = params->long_rd_id,
					.accepted = false,
					.reject_cause = DECT_MAC_ASSOCIATION_NO_RESPONSE,
				};

				dect_mgmt_network_status_evt(ctrl_data.iface, network_status_data);
				dect_mgmt_association_req_result_evt(ctrl_data.iface, resp);
				LOG_ERR("Association response with no actual response with long RD "
					"ID: 0x%X",
					params->long_rd_id);
			} else if (!params->association_response.ack_status) {
				/* Association rejected */
				struct dect_association_req_result_evt resp = {
					.transmitter_long_rd_id = params->long_rd_id,
					.accepted = false,
					.reject_cause = params->association_response.reject_cause,
				};

				LOG_INF("Association rejected with long RD ID: %d (0x%X), cause %d",
					params->long_rd_id, params->long_rd_id,
					params->association_response.reject_cause);

				dect_mgmt_association_req_result_evt(ctrl_data.iface, resp);
			} else {
				struct dect_association_req_result_evt resp = {
					.transmitter_long_rd_id = params->long_rd_id,
					.accepted = true,
				};

				LOG_INF("New parent: association with long RD ID: %d (0x%X)",
					params->long_rd_id, params->long_rd_id);

				ctrl_data.configure_params.auto_start = false;
				ctrl_data.ass_config.pt_association_state =
					CTRL_PT_ASSOCIATION_STATE_ASSOCIATED;
				dect_mgmt_association_req_result_evt(ctrl_data.iface, resp);
				dect_nrf91_parent_association_created(params->long_rd_id,
								      params->ipv6_config);
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_RELEASE_RESP: {
			struct nrf_modem_dect_mac_association_release_cb_params *params =
				event.data;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

			LOG_INF("Association released with long RD ID: %u (0x%08x)",
				params->long_rd_id, params->long_rd_id);

			if (ctrl_data.ass_config.pt_association_state ==
			    CTRL_PT_ASSOCIATION_STATE_ASSOCIATED) {
				ctrl_data.ass_config.pt_association_state =
					CTRL_PT_ASSOCIATION_STATE_NONE;
				dect_nrf91_parent_association_removed(params->long_rd_id);
			} else if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_FT) {
				dect_nrf91_child_association_removed(params->long_rd_id);
			}
			break;
		}
		case DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_RELEASE_IND: {
			struct nrf_modem_dect_mac_association_release_ntf_cb_params *params =
				event.data;
			struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

			LOG_INF("Release req received and association released with long RD ID: "
				"%u (0x%X)",
				params->long_rd_id, params->long_rd_id);

			if (set_ptr->net_mgmt_common.device_type == DECT_DEVICE_TYPE_FT) {
				dect_nrf91_child_association_removed(params->long_rd_id);
			} else {
				/* Network kicked us out? */
				ctrl_data.ass_config.pt_association_state =
					CTRL_PT_ASSOCIATION_STATE_NONE;
				dect_nrf91_parent_association_removed(params->long_rd_id);
			}
			break;
		}

		default:
			LOG_WRN("DECT NRF91 CTRL: Unknown event %u received", event.id);
			break;
		}
		k_free(event.data);
	}
}

#define DECT_NRF91_CTRL_STACK_SIZE CONFIG_DECT_NRP_MAC_NRF_CTRL_THREAD_STACK_SIZE
#define DECT_NRF91_CTRL_PRIORITY   K_PRIO_COOP(9) /* -7 */

K_THREAD_DEFINE(dect_nrf91_ctrl_msgq_th, DECT_NRF91_CTRL_STACK_SIZE,
		dect_nrf91_ctrl_msgq_thread_handler, NULL, NULL, NULL, DECT_NRF91_CTRL_PRIORITY, 0,
		0);

/**************************************************************************************************/

static void
dect_nrf91_ctrl_mdm_cfun_cb(struct nrf_modem_dect_mac_control_functional_mode_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_CFUN_RESP, &params->status,
					 sizeof(params->status));
}

static void
dect_nrf91_ctrl_mdm_configure_cb(struct nrf_modem_dect_mac_control_configure_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_CONFIGURED, &params->status,
					 sizeof(params->status));
}

static void
dect_nrf91_ctrl_mdm_systemmode_cb(struct nrf_modem_dect_mac_control_systemmode_cb_params *params)
{
	k_sem_give(&dect_mac_libmodem_api_sema);
}

static void
dect_nrf91_ctrl_mdm_capability_ntf_cb(struct nrf_modem_dect_mac_capability_ntf_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_MDM_CAPABILITIES, params,
		sizeof(struct nrf_modem_dect_mac_capability_ntf_cb_params));
}

static void dect_nrf91_ctrl_mdm_rssi_scan_cb(struct nrf_modem_dect_mac_rssi_scan_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_RSSI_COMPLETE, &params->status,
					 sizeof(params->status));
}

static void
dect_nrf91_ctrl_mdm_rssi_scan_ntf_cb(struct nrf_modem_dect_mac_rssi_scan_ntf_cb_params *params)
{
	struct dect_nrf91_ctrl_rssi_measurement_data_evt evt_data;

	__ASSERT_NO_MSG(params->rssi_meas_array_size == DECT_NRF91_RSSI_MEAS_ARR_SIZE);
	evt_data.rssi_result.channel = params->channel;
	evt_data.rssi_result.busy_percentage = params->busy_percentage;

	for (int i = 0; i < params->rssi_meas_array_size; i++) {
		if (i >= DECT_NRF91_RSSI_MEAS_ARR_SIZE) {
			printk("Too many RSSI measurements, max %d", DECT_NRF91_RSSI_MEAS_ARR_SIZE);
			break;
		}
		evt_data.rssi_result.busy[i] = params->busy[i];
		evt_data.rssi_result.possible[i] = params->possible[i];
		evt_data.rssi_result.free[i] = params->free[i];
	}

	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_RSSI_RESULT, &evt_data,
					 sizeof(struct dect_nrf91_ctrl_rssi_measurement_data_evt));
}

static void
dect_nrf91_ctrl_mdm_rssi_scan_stop_cb(struct nrf_modem_dect_mac_rssi_scan_stop_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_RSSI_STOPPED, &params->status,
					 sizeof(params->status));
}

static void dect_nrf91_ctrl_mdm_cluster_configure_cb(
	struct nrf_modem_dect_mac_cluster_configure_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_CLUSTER_CONFIG_RESP, &params->status,
					 sizeof(params->status));
}

static void dect_nrf91_ctrl_mdm_cluster_ch_load_change_ntf_cb(
	struct nrf_modem_dect_mac_cluster_ch_load_change_ntf_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_CLUSTER_CH_LOAD_CHANGED, params,
		sizeof(struct nrf_modem_dect_mac_cluster_ch_load_change_ntf_cb_params));
}

static void dect_nrf91_ctrl_mdm_neighbor_inactivity_ntf_cb(
	struct nrf_modem_dect_mac_neighbor_inactivity_ntf_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_NEIGHBOR_INACTIVITY, params,
		sizeof(struct nrf_modem_dect_mac_neighbor_inactivity_ntf_cb_params));
}

static void dect_nrf91_ctrl_mdm_network_beacon_configure_cb(
	struct nrf_modem_dect_mac_network_beacon_configure_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_NW_BEACON_START_OR_STOP_DONE,
					 &params->status, sizeof(params->status));
}

static void
dect_nrf91_ctrl_mdm_association_ntf_cb(struct nrf_modem_dect_mac_association_ntf_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_IND, params,
		sizeof(struct nrf_modem_dect_mac_association_ntf_cb_params));
}

static void dect_nrf91_ctrl_mdm_association_release_ntf_cb(
	struct nrf_modem_dect_mac_association_release_ntf_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_RELEASE_IND, params,
		sizeof(struct nrf_modem_dect_mac_association_release_ntf_cb_params));
}

static void
dect_nrf91_ctrl_mdm_network_scan_cb(struct nrf_modem_dect_mac_network_scan_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_NW_SCAN_COMPLETE, params,
					 sizeof(struct nrf_modem_dect_mac_network_scan_cb_params));
}

static void dect_nrf91_ctrl_mdm_cluster_beacon_ntf_cb(
	struct nrf_modem_dect_mac_cluster_beacon_ntf_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_MDM_CLUSTER_BEACON_RCVD, params,
		sizeof(struct nrf_modem_dect_mac_cluster_beacon_ntf_cb_params));
}

static void dect_nrf91_ctrl_mdm_network_beacon_ntf_cb(
	struct nrf_modem_dect_mac_network_beacon_ntf_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_MDM_NW_BEACON_RCVD, params,
		sizeof(struct nrf_modem_dect_mac_network_beacon_ntf_cb_params));
}

static void dect_nrf91_ctrl_mdm_network_scan_stop_cb(
	struct nrf_modem_dect_mac_network_scan_stop_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_NW_SCAN_STOPPED, &params->status,
					 sizeof(params->status));
}

static void dect_nrf91_ctrl_mdm_cluster_beacon_receive_cb(
	struct nrf_modem_dect_mac_cluster_beacon_receive_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_CLUSTER_RCV_COMPLETE,
					 &params->cluster_status[0],
					 sizeof(params->cluster_status[0]));
}

static void
dect_nrf91_ctrl_mdm_association_cb(struct nrf_modem_dect_mac_association_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_RESP, params,
					 sizeof(struct nrf_modem_dect_mac_association_cb_params));
}

static void dect_nrf91_ctrl_mdm_association_release_cb(
	struct nrf_modem_dect_mac_association_release_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_RELEASE_RESP, params,
		sizeof(struct nrf_modem_dect_mac_association_release_cb_params));
}

static void
dect_nrf91_ctrl_mdm_dlc_data_rx_ntf_cb(struct nrf_modem_dect_dlc_data_rx_ntf_cb_params *params)
{
	struct net_pkt *rcv_pkt;

	rcv_pkt = net_pkt_rx_alloc_with_buffer(ctrl_data.iface, params->data_len, AF_UNSPEC, 0,
					       K_NO_WAIT);
	if (!rcv_pkt) {
		printk("%s: cannot allocate rcv packet of len %d\n", (__func__), params->data_len);
		return;
	}
	if (net_pkt_write(rcv_pkt, params->data, params->data_len) < 0) {
		printk("%s: cannot write rcv packet of len %d\n", (__func__), params->data_len);
		net_pkt_unref(rcv_pkt);
		return;
	}
	struct dect_nrf91_ctrl_dlc_rx_data_with_pkt_ptr mdm_dlc_data_with_pkt_ptr_params;

	mdm_dlc_data_with_pkt_ptr_params.mdm_params = *params;
	mdm_dlc_data_with_pkt_ptr_params.data_len = params->data_len;
	mdm_dlc_data_with_pkt_ptr_params.iface = ctrl_data.iface;
	mdm_dlc_data_with_pkt_ptr_params.pkt = rcv_pkt;
	dect_nrf91_rx_msgq_data_op_add(DECT_NRF91_RX_OP_RX_DATA_WITH_PKT_PTR,
				       (void *)&mdm_dlc_data_with_pkt_ptr_params,
				       sizeof(struct dect_nrf91_ctrl_dlc_rx_data_with_pkt_ptr));
}

static void dect_nrf91_ctrl_mdm_dlc_data_tx_cb(struct nrf_modem_dect_dlc_data_tx_cb_params *params)
{
	struct dect_nrf91_ctrl_dlc_data_tx_resp_evt evt_data;
#if RM_JH
	printk("DLC Data TX completed, receiver: 0x%X, flow ID: %hhu, status: %u\n",
	       params->long_rd_id, params->flow_id, params->status);
	for (int i = 0; i < params->num_transaction_ids; i++) {
		printk("  ID #%d: %d\n", i + 1, params->transaction_ids[i]);
	}
#endif
#if defined(CONFIG_DECT_NRP_MAC_NRF_MDM_BUNDLED_TX_RESPS)
	evt_data.status = params->status;
	evt_data.long_rd_id = params->long_rd_id;
	evt_data.flow_id = params->flow_id;
	evt_data.num_acked_data = params->num_transaction_ids;
	for (int i = 0; i < params->num_transaction_ids; i++) {
		if (i >= DECT_NRF91_DLC_DATA_INFO_MAX_COUNT) {
			printk("Too many transaction IDs, only first %d will be used\n",
			       DECT_NRF91_DLC_DATA_INFO_MAX_COUNT);
			break;
		}
		evt_data.acked_data[i].transaction_id = params->transaction_ids[i];
	}
#else
	evt_data.status = params->status;
	evt_data.long_rd_id = params->long_rd_id;
	evt_data.flow_id = params->flow_id;
	evt_data.num_acked_data = 1;
	evt_data.acked_data[0].transaction_id = params->transaction_id;
#endif
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_DLC_DATA_RESP, &evt_data,
					 sizeof(struct dect_nrf91_ctrl_dlc_data_tx_resp_evt));
}

static void
dect_nrf91_ctrl_mdm_cluster_info_ntf_cb(struct nrf_modem_dect_mac_cluster_info_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_CLUSTER_INFO, params,
					 sizeof(struct nrf_modem_dect_mac_cluster_info_cb_params));
}

static void
dect_nrf91_ctrl_mdm_neighbor_info_cb(struct nrf_modem_dect_mac_neighbor_info_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_NEIGHBOR_INFO, params,
					 sizeof(struct nrf_modem_dect_mac_neighbor_info_cb_params));
}

BUILD_ASSERT(DECT_NRF91_MAX_NEIGHBOR_LIST_COUNT == DECT_L2_MAX_NEIGHBOR_LIST_ITEM_COUNT,
	     "Neighbor list size mismatch between DECT L2 and DECT NRF91");
static void
dect_nrf91_ctrl_mdm_neighbor_list_cb(struct nrf_modem_dect_mac_neighbor_list_cb_params *params)
{
	struct dect_nrf91_ctrl_neighbor_list_resp_evt evt_data;

	evt_data.status = params->status;
	evt_data.num_neighbors = params->num_neighbors;
	for (int i = 0; i < params->num_neighbors; i++) {
		if (i >= DECT_L2_MAX_NEIGHBOR_LIST_ITEM_COUNT) {
			printk("Too many neighbors, only first %d will be used\n",
			       DECT_L2_MAX_NEIGHBOR_LIST_ITEM_COUNT);
			break;
		}
		evt_data.neighbor_long_rd_ids[i] = params->long_rd_ids[i];
	}

	dect_nrf91_ctrl_msgq_data_op_add(DECT_NRF91_CTRL_OP_MDM_NEIGHBOR_LIST, params,
					 sizeof(struct nrf_modem_dect_mac_neighbor_list_cb_params));
}

static void dect_nrf91_ctrl_mdm_flow_control_ntf_cb(
	struct nrf_modem_dect_dlc_flow_control_ntf_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_MDM_FLOW_CONTROL, params,
		sizeof(struct nrf_modem_dect_dlc_flow_control_ntf_cb_params));
}

static void
dect_nrf91_ctrl_mdm_dlc_data_discard_cb(struct nrf_modem_dect_dlc_data_discard_cb_params *params)
{
	printk("Data discard completed, status: %hu\n", params->status);
}

static void dect_nrf91_ctrl_mdm_cluster_beacon_receive_stop_cb(
	struct nrf_modem_dect_mac_cluster_beacon_receive_stop_cb_params *params)
{
	printk("Cluster beacon stop completed, status: %hu\n", params->status);
}

static void dect_nrf91_ctrl_mdm_neighbor_paging_failure_ntf_cb(
	struct nrf_modem_dect_mac_neighbor_paging_failure_ntf_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_NEIGHBOR_PAGING_FAILURE, params,
		sizeof(struct nrf_modem_dect_mac_neighbor_paging_failure_ntf_cb_params));

}

static void dect_nrf91_ctrl_mdm_cluster_beacon_rx_fail_ntf_cb(
	struct nrf_modem_dect_mac_cluster_beacon_rx_failure_ntf_cb_params *params)
{
	dect_nrf91_ctrl_msgq_data_op_add(
		DECT_NRF91_CTRL_OP_CLUSTER_BEACON_RX_FAILURE, params,
		sizeof(struct nrf_modem_dect_mac_cluster_beacon_rx_failure_ntf_cb_params));
}

static const struct nrf_modem_dect_mac_ntf_callbacks mac_ntf_callbacks = {
	.association_ntf = dect_nrf91_ctrl_mdm_association_ntf_cb,
	.association_release_ntf = dect_nrf91_ctrl_mdm_association_release_ntf_cb,
	.cluster_ch_load_change_ntf = dect_nrf91_ctrl_mdm_cluster_ch_load_change_ntf_cb,
	.neighbor_inactivity_ntf = dect_nrf91_ctrl_mdm_neighbor_inactivity_ntf_cb,
	.neighbor_paging_failure_ntf = dect_nrf91_ctrl_mdm_neighbor_paging_failure_ntf_cb,
	.rssi_scan_ntf = dect_nrf91_ctrl_mdm_rssi_scan_ntf_cb,
	.cluster_beacon_ntf = dect_nrf91_ctrl_mdm_cluster_beacon_ntf_cb,
	.network_beacon_ntf = dect_nrf91_ctrl_mdm_network_beacon_ntf_cb,
	.dlc_data_rx_ntf = dect_nrf91_ctrl_mdm_dlc_data_rx_ntf_cb,
	.capability_ntf = dect_nrf91_ctrl_mdm_capability_ntf_cb,
	.cluster_beacon_rx_failure_ntf = dect_nrf91_ctrl_mdm_cluster_beacon_rx_fail_ntf_cb,
	.dlc_flow_control_ntf = dect_nrf91_ctrl_mdm_flow_control_ntf_cb,
};

static const struct nrf_modem_dect_mac_op_callbacks mac_op_callbacks = {

	.control_functional_mode = dect_nrf91_ctrl_mdm_cfun_cb,
	.control_configure = dect_nrf91_ctrl_mdm_configure_cb,
	.control_systemmode = dect_nrf91_ctrl_mdm_systemmode_cb,
	.association = dect_nrf91_ctrl_mdm_association_cb,
	.association_release = dect_nrf91_ctrl_mdm_association_release_cb,
	.cluster_beacon_receive = dect_nrf91_ctrl_mdm_cluster_beacon_receive_cb,
	.cluster_beacon_receive_stop = dect_nrf91_ctrl_mdm_cluster_beacon_receive_stop_cb,
	.cluster_configure = dect_nrf91_ctrl_mdm_cluster_configure_cb,
	.cluster_info = dect_nrf91_ctrl_mdm_cluster_info_ntf_cb,
	.neighbor_info = dect_nrf91_ctrl_mdm_neighbor_info_cb,
	.neighbor_list = dect_nrf91_ctrl_mdm_neighbor_list_cb,
	.dlc_data_tx = dect_nrf91_ctrl_mdm_dlc_data_tx_cb,
	.dlc_data_discard = dect_nrf91_ctrl_mdm_dlc_data_discard_cb,
	.network_beacon_configure = dect_nrf91_ctrl_mdm_network_beacon_configure_cb,
	.network_scan = dect_nrf91_ctrl_mdm_network_scan_cb,
	.network_scan_stop = dect_nrf91_ctrl_mdm_network_scan_stop_cb,
	.rssi_scan = dect_nrf91_ctrl_mdm_rssi_scan_cb,
	.rssi_scan_stop = dect_nrf91_ctrl_mdm_rssi_scan_stop_cb,
};

static void dect_nrf91_ctrl_mac_init(void)
{
	LOG_INF("Starting DECT NR+ Stack initialization");

	int ret = nrf_modem_dect_mac_callback_set(&mac_op_callbacks, &mac_ntf_callbacks);

	if (ret != 0) {
		printk("Error in callback set, error: %d\n", ret);
	}

	/* TODO: init need to be changed, we will need to enable also MODE_PHY with dect_shell for
	 * certification
	 */
	ret = nrf_modem_dect_control_systemmode_set(NRF_MODEM_DECT_MODE_MAC);
	if (ret != 0) {
		printk("Error in systemmode set, error: %d\n", ret);
	}

	/* Wait that sysmode is set */
	ret = k_sem_take(&dect_mac_libmodem_api_sema, K_SECONDS(2));

	if (ret) {
		printk("(%s): nrf_modem_dect_control_systemmode_set() timeout.\n", (__func__));
	}
	ctrl_data.mdm_activation_state = CTRL_MDM_DEACTIVATED;
	net_if_carrier_off(ctrl_data.iface);
	(void)dect_nrf91_ctrl_modem_configure_req_from_settings();

	ret = k_sem_take(&dect_mac_libmodem_api_sema, K_SECONDS(2));
	if (ret) {
		printk("(%s): modem configure timeout.\n", (__func__));
	}
}

/**************************************************************************************************/

struct nrf_modem_dect_mac_capability_ntf_cb_params *dect_nrf91_ctrl_mdm_capabilities_ref_get(void)
{
	return &ctrl_data.mdm_capas;
}

static void dect_nrf91_ctrl_on_modem_lib_init(int ret, void *ctx)
{
	ARG_UNUSED(ret);
	ARG_UNUSED(ctx);

	dect_nrf91_ctrl_mac_init();
}

NRF_MODEM_LIB_ON_INIT(dect_nrf91_ctrl_init_hook, dect_nrf91_ctrl_on_modem_lib_init, NULL);

int dect_nrf91_ctrl_init(struct net_if *iface)
{
	memset(&ctrl_data, 0, sizeof(struct dect_nrf91_ctrl_data));

	ctrl_data.iface = iface;
	net_if_carrier_off(ctrl_data.iface);
	LOG_DBG("dect_nrf91_ctrl_init");

	return 0;
}
