/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_NRF91_UTILS_H
#define DECT_NRF91_UTILS_H

#include <nrf_modem_dect_mac.h>

struct dect_nrf91_utils_mapping_tbl_item {
	int key;
	char *value_str;
};

/******************************************************************************/

const char *dect_nrf91_utils_modem_phy_err_to_string(enum nrf_modem_dect_mac_err err,
						     char *out_str_buff);
const char *dect_nrf91_utils_modem_association_ind_err_to_string(
	enum nrf_modem_dect_mac_association_indication_status err, char *out_str_buff);
enum dect_status_values
dect_nrf91_utils_modem_status_to_net_mgmt_status(enum nrf_modem_dect_mac_err mdm_status);

/******************************************************************************/

bool dect_common_utils_use_harmonized_std(uint8_t band_nbr);
bool dect_common_utils_harmonized_band_channel_array_get(uint8_t band_nbr, uint16_t *channel_array,
							 uint8_t *channel_array_size);
bool dect_common_utils_channel_is_supported_by_band(uint16_t band_nbr, uint16_t channel);

#include "dect_net_l2.h"
#include "dect_nrf91_ctrl.h"

int dect_nrf91_utils_mdm_rssi_results_to_l2_rssi_data(
	const struct nrf_modem_dect_mac_rssi_result *rssi_scan_results,
	struct dect_rssi_scan_result_data *rssi_data_out);

bool dect_nrf91_utils_cluster_acceptable_for_association(
	struct nrf_modem_dect_mac_cluster_beacon_ntf_cb_params *cluster_beacon);
#endif /* DECT_NRF91_UTILS_H */
