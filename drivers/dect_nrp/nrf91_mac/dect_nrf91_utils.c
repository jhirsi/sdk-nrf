/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "nrf_modem_dect_mac.h"

#include "dect_nrf91_settings.h"
#include "dect_nrf91_ctrl.h"
#include "dect_nrf91_utils.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(DECT_NRP_MAC, CONFIG_DECT_NRP_MAC_LOG_LEVEL);

static const char *
dect_nrf91_utils_map_to_string(struct dect_nrf91_utils_mapping_tbl_item const *mapping_table,
			       int mode, char *out_str_buff)
{
	bool found = false;
	int i;

	for (i = 0; mapping_table[i].key != -1; i++) {
		if (mapping_table[i].key == mode) {
			found = true;
			break;
		}
	}

	if (!found) {
		sprintf(out_str_buff, "%d (unknown value, not converted to string)", mode);
	} else {
		strcpy(out_str_buff, mapping_table[i].value_str);
	}
	return out_str_buff;
}
const char *dect_nrf91_utils_modem_phy_err_to_string(enum nrf_modem_dect_mac_err err,
						     char *out_str_buff)
{
	struct dect_nrf91_utils_mapping_tbl_item const mapping_table[] = {
		{NRF_MODEM_DECT_MAC_STATUS_OK, "MAC_STATUS_OK"},
		{NRF_MODEM_DECT_MAC_STATUS_FAIL, "MAC_STATUS_FAIL"},
		{NRF_MODEM_DECT_MAC_STATUS_INVALID_PARAM, "MAC_STATUS_INVALID_PARAM"},
		{NRF_MODEM_DECT_MAC_STATUS_NOT_ALLOWED, "MAC_STATUS_NOT_ALLOWED"},
		{NRF_MODEM_DECT_MAC_STATUS_NO_CONFIG, "MAC_STATUS_NO_CONFIG"},
		{NRF_MODEM_DECT_MAC_STATUS_RD_NOT_FOUND, "MAC_STATUS_RD_NOT_FOUND"},
		{NRF_MODEM_DECT_MAC_STATUS_TEMP_FAILURE, "MAC_STATUS_TEMP_FAILURE"},
		{NRF_MODEM_DECT_MAC_STATUS_NO_RESOURCES, "MAC_STATUS_NO_RESOURCES"},
		{NRF_MODEM_DECT_MAC_STATUS_NO_RESPONSE, "MAC_STATUS_NO_RESPONSE"},
		{NRF_MODEM_DECT_MAC_STATUS_NW_REJECT, "MAC_STATUS_NW_REJECT"},
		{NRF_MODEM_DECT_MAC_STATUS_NO_MEMORY, "MAC_STATUS_NO_MEMORY"},
		{NRF_MODEM_DECT_MAC_STATUS_NO_RSSI_RESULTS, "MAC_STATUS_NO_RSSI_RESULTS"},
		{-1, NULL}};

	dect_nrf91_utils_map_to_string(mapping_table, err, out_str_buff);

	return out_str_buff;
}

enum dect_status_values
dect_nrf91_utils_modem_status_to_net_mgmt_status(enum nrf_modem_dect_mac_err mdm_status)
{
	switch (mdm_status) {
	case NRF_MODEM_DECT_MAC_STATUS_OK:
		return DECT_MAC_STATUS_OK;
	case NRF_MODEM_DECT_MAC_STATUS_FAIL:
		return DECT_MAC_STATUS_FAIL;
	case NRF_MODEM_DECT_MAC_STATUS_INVALID_PARAM:
		return DECT_MAC_STATUS_INVALID_PARAM;
	case NRF_MODEM_DECT_MAC_STATUS_NOT_ALLOWED:
		return DECT_MAC_STATUS_NOT_ALLOWED;
	case NRF_MODEM_DECT_MAC_STATUS_NO_CONFIG:
		return DECT_MAC_STATUS_NO_CONFIG;
	case NRF_MODEM_DECT_MAC_STATUS_RD_NOT_FOUND:
		return DECT_MAC_STATUS_RD_NOT_FOUND;
	case NRF_MODEM_DECT_MAC_STATUS_TEMP_FAILURE:
		return DECT_MAC_STATUS_TEMP_FAILURE;
	case NRF_MODEM_DECT_MAC_STATUS_NO_RESOURCES:
		return DECT_MAC_STATUS_NO_RESOURCES;
	case NRF_MODEM_DECT_MAC_STATUS_NO_RESPONSE:
		return DECT_MAC_STATUS_NO_RESPONSE;
	case NRF_MODEM_DECT_MAC_STATUS_NW_REJECT:
		return DECT_MAC_STATUS_NW_REJECT;
	case NRF_MODEM_DECT_MAC_STATUS_NO_MEMORY:
		return DECT_MAC_STATUS_NO_MEMORY;
	case NRF_MODEM_DECT_MAC_STATUS_NO_RSSI_RESULTS:
		return DECT_MAC_STATUS_NO_RSSI_RESULTS;
	default:
		return DECT_MAC_STATUS_UNKNOWN;
	}
}

const char *dect_nrf91_utils_modem_association_ind_err_to_string(
	enum nrf_modem_dect_mac_association_indication_status err, char *out_str_buff)
{
	struct dect_nrf91_utils_mapping_tbl_item const mapping_table[] = {
		{NRF_MODEM_DECT_MAC_ASSOCIATION_INDICATION_STATUS_SUCCESS, "SUCCESS"},
		{NRF_MODEM_DECT_MAC_ASSOCIATION_INDICATION_STATUS_SHORT_ID_CONFLICT,
		 "SHORT_ID_CONFLICT"},
		{NRF_MODEM_DECT_MAC_ASSOCIATION_INDICATION_STATUS_LONG_ID_CONFLICT,
		 "LONG_ID_CONFLICT"},
		{NRF_MODEM_DECT_MAC_ASSOCIATION_INDICATION_STATUS_MAX_NUM_NEIGHBOURS,
		 "MAX_NUM_NEIGHBOURS"},
		{NRF_MODEM_DECT_MAC_ASSOCIATION_INDICATION_STATUS_RD_CAPA_MISMATCH,
		 "RD_CAPA_MISMATCH"},
		{NRF_MODEM_DECT_MAC_ASSOCIATION_INDICATION_STATUS_NO_RESOURCES_FOR_RESPONSE,
		 "NO_RESOURCES_FOR_RESPONSE"},
		{-1, NULL}};

	dect_nrf91_utils_map_to_string(mapping_table, err, out_str_buff);

	return out_str_buff;
}

int dect_nrf91_utils_mdm_rssi_results_to_l2_rssi_data(
	const struct nrf_modem_dect_mac_rssi_result *rssi_scan_results,
	struct dect_rssi_scan_result_data *rssi_data_out)
{
	enum dect_rssi_scan_result_verdict current_verdict = DECT_RSSI_SCAN_VERDICT_UNKNOWN;
	bool all_subslots_free = true;
	uint8_t free_subslot_cnt = 0;
	uint8_t possible_subslot_cnt = 0;
	uint8_t busy_subslot_cnt = 0;
	uint8_t l2_results_evt_verdict_arr_index = 0;
	size_t rssi_meas_array_size = sizeof(rssi_scan_results->busy);

	__ASSERT_NO_MSG(rssi_scan_results != NULL && rssi_data_out != NULL);
	__ASSERT_NO_MSG(rssi_meas_array_size == DECT_NRF91_RSSI_MEAS_ARR_SIZE);

	memset(rssi_data_out, 0, sizeof(*rssi_data_out));

	LOG_INF("RSSI scan results:");
	LOG_INF("  channel                             %hu", rssi_scan_results->channel);
	LOG_INF("  busy_percentage                     %d", rssi_scan_results->busy_percentage);

	for (uint32_t i = 0; i < rssi_meas_array_size; i++) {
		LOG_DBG("  free[%hhu]                       %hhu, 0x%hhX", i,
			rssi_scan_results->free[i], rssi_scan_results->free[i]);
		LOG_DBG("  possible[%hhu]                   %hhu, 0x%hhX", i,
			rssi_scan_results->possible[i], rssi_scan_results->possible[i]);
		LOG_DBG("  busy[%hhu]]                      %hhu, 0x%hhX", i,
			rssi_scan_results->busy[i], rssi_scan_results->busy[i]);

		for (uint32_t j = 0; j < 8; j++) {
			/* Go through the bits in this byte */
			if (rssi_scan_results->busy[i] & (1 << j)) {
				busy_subslot_cnt++;
				current_verdict = DECT_RSSI_SCAN_VERDICT_BUSY;
			} else if (rssi_scan_results->possible[i] & (1 << j)) {
				possible_subslot_cnt++;
				current_verdict = DECT_RSSI_SCAN_VERDICT_POSSIBLE;
			} else if (rssi_scan_results->free[i] & (1 << j)) {
				free_subslot_cnt++;
				current_verdict = DECT_RSSI_SCAN_VERDICT_FREE;
			} else {
				/* No bits set */
				__ASSERT_NO_MSG(0);
			}
			__ASSERT_NO_MSG(l2_results_evt_verdict_arr_index <
					DECT_NRP_RSSI_MEAS_SUBSLOT_COUNT);
			rssi_data_out->frame_subslot_verdicts[l2_results_evt_verdict_arr_index++] =
				current_verdict;
		}
		if (rssi_scan_results->free[i] != 0xff) {
			all_subslots_free = false;
		}
	}

	LOG_INF("  free_subslot_cnt                    %hhu", free_subslot_cnt);
	LOG_INF("  possible_subslot_cnt                %hhu", possible_subslot_cnt);
	LOG_INF("  busy_subslot_cnt                    %hhu", busy_subslot_cnt);

	uint8_t free_possible_percent = ((free_subslot_cnt + possible_subslot_cnt) * 100) /
					DECT_NRP_RSSI_MEAS_SUBSLOT_COUNT;

	rssi_data_out->channel = rssi_scan_results->channel;
	rssi_data_out->busy_percentage = rssi_scan_results->busy_percentage;
	rssi_data_out->all_subslots_free = all_subslots_free;
	rssi_data_out->scan_suitable_percent = free_possible_percent;
	rssi_data_out->free_subslot_cnt = free_subslot_cnt;
	rssi_data_out->possible_subslot_cnt = possible_subslot_cnt;
	rssi_data_out->busy_subslot_cnt = busy_subslot_cnt;

	return 0;
}

/**************************************************************************************************/

bool dect_common_utils_use_harmonized_std(uint8_t band_nbr)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();
	bool use_harmonize_std = false;

	if (band_nbr == 1 && set_ptr->net_mgmt_common.region == DECT_SETTINGS_REGION_EU) {
		/* Per harmonized std: only odd number channels at band #1:
		 * ETSI EN 301 406-2, V3.0.1, ch 4.3.2.3.
		 */
		use_harmonize_std = true;
	}
	return use_harmonize_std;
}

static bool dect_common_utils_is_odd_number(uint16_t number)
{
	if (number % 2) {
		return true; /* Odd number */
	} else {
		return false;
	}
}

static struct nrf_modem_dect_mac_phy_band_info *
dect_common_utils_channel_mdm_band_info_get(uint16_t band_nbr)
{
	struct nrf_modem_dect_mac_capability_ntf_cb_params *mdm_capas_ptr =
		dect_nrf91_ctrl_mdm_capabilities_ref_get();

	for (int i = 0; i < mdm_capas_ptr->num_band_info_elems; i++) {
		if (mdm_capas_ptr->band_info_elems[i].band == band_nbr) {
			return &mdm_capas_ptr->band_info_elems[i];
		}
	}
	return NULL;
}

bool dect_common_utils_harmonized_band_channel_array_get(uint8_t band_nbr, uint16_t *channel_array,
							 uint8_t *channel_array_size)
{
	bool use_harmonized_std = dect_common_utils_use_harmonized_std(band_nbr);
	struct nrf_modem_dect_mac_phy_band_info *band_info_ptr =
		dect_common_utils_channel_mdm_band_info_get(band_nbr);

	__ASSERT_NO_MSG(use_harmonized_std);

	if (band_info_ptr == NULL) {
		printk("Band %d not supported\n", band_nbr);
		return false;
	}
	uint16_t channel_array_size_temp =
		(band_info_ptr->max_carrier - band_info_ptr->min_carrier + 1) / 2;

	if (channel_array == NULL || channel_array_size == NULL ||
	    *channel_array_size < channel_array_size_temp) {
		printk("Channel array size %d is too small, needed %d\n", *channel_array_size,
		       channel_array_size_temp);

		return false;
	}

	channel_array_size_temp = 0;
	*channel_array_size = band_info_ptr->max_carrier - band_info_ptr->min_carrier + 1;
	for (uint16_t i = 0; i < *channel_array_size; i++) {
		if (use_harmonized_std) {
			if (dect_common_utils_is_odd_number(band_info_ptr->min_carrier + i)) {
				channel_array[channel_array_size_temp] =
					band_info_ptr->min_carrier + i;
				channel_array_size_temp++;
			} /* else skip */
		} else {
			channel_array[channel_array_size_temp] = band_info_ptr->min_carrier + i;
			channel_array_size_temp++;
		}
	}
	*channel_array_size = channel_array_size_temp;
	return true;
}

bool dect_common_utils_channel_is_supported_by_band(uint16_t band_nbr, uint16_t channel)
{
	struct nrf_modem_dect_mac_phy_band_info *band_info_ptr =
		dect_common_utils_channel_mdm_band_info_get(band_nbr);

	if (band_info_ptr == NULL) {
		printk("Band %d not supported\n", band_nbr);
		return false;
	}

	if (channel < band_info_ptr->min_carrier || channel > band_info_ptr->max_carrier) {
		printk("Channel %d not supported in band %d\n", channel, band_nbr);
		return false;
	}
	/* Check harmonized standard: Only odd number channels: ETSI EN 301 406-2,
	 * V3.0.1, ch 4.3.2.3
	 */
	if (dect_common_utils_use_harmonized_std(band_nbr) &&
	    !dect_common_utils_is_odd_number(channel)) {
		printk("Channel %d is not odd number, not allowed in harmonized std\n", channel);
		return false;
	}
	return true;
}

static int8_t dect_nrf91_utils_cluster_min_quality_to_db(
	enum nrf_modem_dect_mac_quality_threshold min_quality)
{
	switch (min_quality) {
	case NRF_MODEM_DECT_MAC_QUALITY_THRESHOLD_0:
		return 0;
	case NRF_MODEM_DECT_MAC_QUALITY_THRESHOLD_3:
		return 3;
	case NRF_MODEM_DECT_MAC_QUALITY_THRESHOLD_6:
		return 6;
	case NRF_MODEM_DECT_MAC_QUALITY_THRESHOLD_9:
	default:
		return 9;
	}
}

bool dect_nrf91_utils_cluster_acceptable_for_association(
	struct nrf_modem_dect_mac_cluster_beacon_ntf_cb_params *cluster_beacon)
{
	struct dect_nrf91_settings *set_ptr = dect_nrf91_settings_ref_get();

	__ASSERT_NO_MSG(set_ptr && cluster_beacon);

	if (cluster_beacon->network_id != set_ptr->net_mgmt_common.identities.network_id) {
		return false;
	}
	int8_t min_quality_db =
		dect_nrf91_utils_cluster_min_quality_to_db(cluster_beacon->beacon.min_quality);

	/* MAC spec, ch. 5.1.4 */
	if (cluster_beacon->rx_signal_info.rssi_2 <
	    (set_ptr->net_mgmt_common.association.min_sensitivity_dbm + min_quality_db)) {
		LOG_INF("nw_scan: cluster beacon RSSI too low: %d < %d "
			"(long rd id %u (0x%08X), min_quality %d dB)",
			cluster_beacon->rx_signal_info.rssi_2,
			set_ptr->net_mgmt_common.association.min_sensitivity_dbm + min_quality_db,
			cluster_beacon->transmitter_long_rd_id,
			cluster_beacon->transmitter_long_rd_id, min_quality_db);
		return false;
	}
	if (!(set_ptr->net_mgmt_common.network_join.target_ft_long_rd_id ==
		      DECT_SETT_NETWORK_JOIN_TARGET_FT_ANY ||
	      set_ptr->net_mgmt_common.network_join.target_ft_long_rd_id ==
		      cluster_beacon->transmitter_long_rd_id)) {
		LOG_INF("nw_scan: cluster beacon long rd id mismatch: %d != %d",
			set_ptr->net_mgmt_common.network_join.target_ft_long_rd_id,
			cluster_beacon->transmitter_long_rd_id);
		return false;
	}
	return true;
}
