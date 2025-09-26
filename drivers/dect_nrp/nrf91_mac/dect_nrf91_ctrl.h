/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_NRF91_CTRL_H
#define DECT_NRF91_CTRL_H

#include <zephyr/kernel.h>
#include <stdint.h>

#include "dect_nrf91_common.h"

#define DECT_MAC_DATA_TX_HANDLE_START 1000
#define DECT_MAC_DATA_TX_HANDLE_END   1039
#define DECT_MAC_DATA_TX_HANDLE_IN_RANGE(x)                                                        \
	(x >= DECT_MAC_DATA_TX_HANDLE_START && x <= DECT_MAC_DATA_TX_HANDLE_END)
#define DECT_MAC_DATA_TX_HANDLE_COUNT                                                              \
	(DECT_MAC_DATA_TX_HANDLE_END - DECT_MAC_DATA_TX_HANDLE_START + 1)

typedef enum {
	DECT_NRF91_CTRL_OP_MDM_CAPABILITIES,
	DECT_NRF91_CTRL_OP_MDM_CONFIGURED,
	DECT_NRF91_CTRL_OP_MDM_CFUN_RESP,
	DECT_NRF91_CTRL_OP_MDM_ACTIVATED,
	DECT_NRF91_CTRL_OP_MDM_DEACTIVATED,
	DECT_NRF91_CTRL_OP_AUTO_START,
	DECT_NRF91_CTRL_OP_CLUSTER_START_REQ,
	DECT_NRF91_CTRL_OP_CLUSTER_RECONFIG_REQ,
	DECT_NRF91_CTRL_OP_CLUSTER_CONFIG_RESP,
	DECT_NRF91_CTRL_OP_CLUSTER_CH_LOAD_CHANGED,
	DECT_NRF91_CTRL_OP_NEIGHBOR_INACTIVITY,
	DECT_NRF91_CTRL_OP_RSSI_START_REQ_CMD,
	DECT_NRF91_CTRL_OP_RSSI_START_REQ_CH_SELECTION,
	DECT_NRF91_CTRL_OP_MDM_RSSI_RESULT,
	DECT_NRF91_CTRL_OP_MDM_RSSI_COMPLETE,
	DECT_NRF91_CTRL_OP_MDM_RSSI_STOPPED,
	DECT_NRF91_CTRL_OP_MDM_NW_BEACON_START,
	DECT_NRF91_CTRL_OP_MDM_NW_BEACON_START_OR_STOP_DONE,
	DECT_NRF91_CTRL_OP_MDM_NW_BEACON_STOP,
	DECT_NRF91_CTRL_OP_MDM_NW_SCAN_COMPLETE,
	DECT_NRF91_CTRL_OP_MDM_NW_SCAN_STOPPED,
	DECT_NRF91_CTRL_OP_MDM_CLUSTER_BEACON_RCVD,
	DECT_NRF91_CTRL_OP_MDM_NW_BEACON_RCVD,
	DECT_NRF91_CTRL_OP_MDM_CLUSTER_RCV_COMPLETE,
	DECT_NRF91_CTRL_OP_MDM_CLUSTER_INFO,
	DECT_NRF91_CTRL_OP_CLUSTER_BEACON_RX_FAILURE,
	DECT_NRF91_CTRL_OP_NEIGHBOR_PAGING_FAILURE,
	DECT_NRF91_CTRL_OP_MDM_NEIGHBOR_INFO,
	DECT_NRF91_CTRL_OP_MDM_NEIGHBOR_LIST,
	DECT_NRF91_CTRL_OP_MDM_FLOW_CONTROL,
	DECT_NRF91_CTRL_OP_MDM_DLC_DATA_RESP,
	DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_IND,
	DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_RESP,
	DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_RELEASE_IND,
	DECT_NRF91_CTRL_OP_MDM_ASSOCIATION_RELEASE_RESP,
	DECT_NRF91_CTRL_OP_MDM_IPV6_CONFIG_CHANGED,
} dect_nrf91_ctrl_op_t;

typedef enum {
	DECT_NRF91_CTRL_DEVICE_TYPE_FT,
	DECT_NRF91_CTRL_DEVICE_TYPE_PT,
	DECT_NRF91_CTRL_DEVICE_TYPE_NA,
} dect_nrf91_ctrl_device_type_t;

/* TODO: use nrf_modem_dect_control_configure_params directly? */
typedef struct {
	dect_nrf91_ctrl_device_type_t device_type;
	bool debug;

	bool power_save;
	bool auto_start;
	bool auto_activate;

	int8_t expected_mcs1_rx_rssi_level;

	int8_t tx_pwr;
	uint8_t tx_mcs;
	uint8_t band;
	uint8_t band_group_index;

	uint16_t channel;

	uint32_t long_rd_id;
	uint32_t network_id;
} dect_nrf91_ctrl_configure_params_t;

typedef struct {
	uint8_t flow_id;

	uint8_t data[1280];
	uint32_t data_len;
	uint32_t long_rd_id;
	uint32_t transaction_id;
} dect_nrf91_ctrl_tx_cmd_params_t;

#include <zephyr/net/net_if.h>
struct dect_nrf91_ctrl_dlc_rx_data_with_pkt_ptr {
	struct net_if *iface;
	struct nrf_modem_dect_dlc_data_rx_ntf_cb_params mdm_params;
	size_t data_len;
	struct net_pkt *pkt;
};

struct dect_nrf91_ctrl_dlc_data_tx_evt_data_item {
	uint32_t transaction_id;
};

#define DECT_NRF91_DLC_DATA_INFO_MAX_COUNT 40

struct dect_nrf91_ctrl_dlc_data_tx_resp_evt {
	enum nrf_modem_dect_mac_err status;
	uint8_t flow_id;
	uint32_t long_rd_id;

	uint8_t num_acked_data;
#if defined(CONFIG_DECT_NRP_MAC_MDM_BUNDLED_TX_RESPS)
	struct dect_nrf91_ctrl_dlc_data_tx_evt_data_item
		acked_data[DECT_NRF91_DLC_DATA_INFO_MAX_COUNT];
#else
	struct dect_nrf91_ctrl_dlc_data_tx_evt_data_item acked_data[1];
#endif
};

#define DECT_NRF91_MAX_NEIGHBOR_LIST_COUNT 50
struct dect_nrf91_ctrl_neighbor_list_resp_evt {
	int status;
	uint8_t num_neighbors;
	uint32_t neighbor_long_rd_ids[DECT_NRF91_MAX_NEIGHBOR_LIST_COUNT];
};

#define DECT_NRF91_RSSI_MEAS_ARR_SIZE (DECT_NRP_RSSI_MEAS_SUBSLOT_COUNT / 8)
struct dect_nrf91_ctrl_rssi_measurement_data_evt {
	struct nrf_modem_dect_mac_rssi_result rssi_result;
};

struct dect_mac_common_op_event_msgq_item {
	dect_nrf91_ctrl_op_t id;
	void *data;
};

#include "dect_net_l2_mgmt.h"

/* All of these that return integer returns 0 if success and negative on error */

int dect_nrf91_ctrl_configure_n_activate(void);
int dect_nrf91_ctrl_deactivate(void);

int dect_nrf91_ctrl_cluster_channel_get(void);
bool dect_nrf91_ctrl_mdm_activated(void);

int dect_nrf91_ctrl_mdm_reactivate(void);

int dect_nrf91_ctrl_nw_scan_cmd(struct nrf_modem_dect_mac_network_scan_params *params,
				struct net_if *iface, dect_scan_result_cb_t cb);
int dect_nrf91_ctrl_tx_cmd(dect_nrf91_ctrl_tx_cmd_params_t *params);
int dect_nrf91_ctrl_associate_req_cmd(struct nrf_modem_dect_mac_association_params *params);
int dect_nrf91_ctrl_associate_release_cmd(
	uint32_t long_rd_id, enum nrf_modem_dect_mac_release_cause rel_cause);

int dect_nrf91_ctrl_neighbor_info_req_cmd(struct nrf_modem_dect_mac_neighbor_info_params *params);

int dect_nrf91_ctrl_cluster_start_req_cmd(struct dect_cluster_start_req_params *params);
int dect_nrf91_ctrl_cluster_reconfig_req_cmd(struct dect_cluster_reconfig_req_params *params);
int dect_nrf91_ctrl_cluster_info_req_cmd(void);

int dect_nrf91_ctrl_network_create_req_cmd(void);
int dect_nrf91_ctrl_network_remove_req_cmd(void);
bool dect_nrf91_ctrl_network_remove_req_cmd_allowed(void);

int dect_nrf91_ctrl_network_join_req_cmd(void);
int dect_nrf91_ctrl_network_unjoin_req_cmd(void);

int dect_nrf91_ctrl_nw_beacon_start_req_cmd(struct dect_nw_beacon_start_req_params *params);
int dect_nrf91_ctrl_nw_beacon_stop_req_cmd(struct dect_nw_beacon_stop_req_params *params);
bool dect_nrf91_ctrl_nw_beacon_running(void);

int dect_nrf91_ctrl_neighbor_list_req_cmd(void);

int dect_nrf91_ctrl_init(struct net_if *iface);

struct nrf_modem_dect_mac_capability_ntf_cb_params *dect_nrf91_ctrl_mdm_capabilities_ref_get(void);

int dect_nrf91_ctrl_msgq_data_op_add(dect_nrf91_ctrl_op_t event_id, void *data, size_t data_size);

#endif /* DECT_NRF91_CTRL_H */
