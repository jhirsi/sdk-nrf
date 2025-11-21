/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/dect_mgmt.h>
#include <nrf_rpc/nrf_rpc_serialize.h>
#include "dect_rpc_serialize.h"

void dect_rpc_encode_ipv6_addr(struct nrf_rpc_cbor_ctx *ctx,
				const struct in6_addr *addr)
{
	if (addr) {
		nrf_rpc_encode_buffer(ctx, addr, sizeof(struct in6_addr));
	} else {
		nrf_rpc_encode_null(ctx);
	}
}

bool dect_rpc_decode_ipv6_addr(struct nrf_rpc_cbor_ctx *ctx,
				struct in6_addr *addr)
{
	if (!addr) {
		return false;
	}

	return nrf_rpc_decode_buffer(ctx, addr, sizeof(struct in6_addr)) != NULL;
}

void dect_rpc_encode_settings(struct nrf_rpc_cbor_ctx *ctx,
			       const struct dect_settings *settings)
{
	if (!settings) {
		nrf_rpc_encode_null(ctx);
		return;
	}

	/* Command params */
	nrf_rpc_encode_bool(ctx, settings->cmd_params.reset_to_driver_defaults);
	nrf_rpc_encode_uint(ctx, settings->cmd_params.write_scope_bitmap);

	/* Auto-start */
	nrf_rpc_encode_bool(ctx, settings->auto_start.activate);

	/* Region */
	nrf_rpc_encode_uint(ctx, (uint32_t)settings->region);

	/* Device type */
	nrf_rpc_encode_uint(ctx, settings->device_type);

	/* Identities */
	nrf_rpc_encode_uint(ctx, settings->identities.network_id);
	nrf_rpc_encode_uint(ctx, settings->identities.transmitter_long_rd_id);

	/* TX settings */
	nrf_rpc_encode_int(ctx, settings->tx.max_power_dbm);
	nrf_rpc_encode_uint(ctx, settings->tx.max_mcs);

	/* Power save */
	nrf_rpc_encode_bool(ctx, settings->power_save);

	/* Band */
	nrf_rpc_encode_uint(ctx, settings->band_nbr);

	/* RSSI scan */
	nrf_rpc_encode_uint(ctx, settings->rssi_scan.scan_suitable_percent);
	nrf_rpc_encode_uint(ctx, settings->rssi_scan.time_per_channel_ms);
	nrf_rpc_encode_int(ctx, settings->rssi_scan.free_threshold_dbm);
	nrf_rpc_encode_int(ctx, settings->rssi_scan.busy_threshold_dbm);

	/* Cluster settings */
	nrf_rpc_encode_int(ctx, settings->cluster.max_beacon_tx_power_dbm);
	nrf_rpc_encode_int(ctx, settings->cluster.max_cluster_power_dbm);
	nrf_rpc_encode_uint(ctx, (uint32_t)settings->cluster.beacon_period);
	nrf_rpc_encode_uint(ctx, settings->cluster.max_num_neighbors);
	nrf_rpc_encode_uint(ctx, settings->cluster.neighbor_inactivity_disconnect_timer_ms);
	nrf_rpc_encode_uint(ctx, settings->cluster.channel_loaded_percent);

	/* Network beacon */
	nrf_rpc_encode_uint(ctx, settings->nw_beacon.channel);
	nrf_rpc_encode_uint(ctx, (uint32_t)settings->nw_beacon.beacon_period);

	/* Association */
	nrf_rpc_encode_uint(ctx, settings->association.max_beacon_rx_failures);
	nrf_rpc_encode_int(ctx, settings->association.min_sensitivity_dbm);

	/* Network join */
	nrf_rpc_encode_uint(ctx, settings->network_join.target_ft_long_rd_id);

	/* Security */
	nrf_rpc_encode_uint(ctx, (uint32_t)settings->sec_conf.mode);
	nrf_rpc_encode_buffer(ctx, settings->sec_conf.integrity_key,
			      DECT_MAC_INTEGRITY_KEY_LENGTH);
	nrf_rpc_encode_buffer(ctx, settings->sec_conf.cipher_key,
			      DECT_MAC_CIPHER_KEY_LENGTH);
}

bool dect_rpc_decode_settings(struct nrf_rpc_cbor_ctx *ctx,
			      struct dect_settings *settings)
{
	if (!settings) {
		return false;
	}

	/* Check for null - use nrf_rpc_decode_is_null which properly handles errors */
	if (nrf_rpc_decode_is_null(ctx)) {
		return false;
	}

	/* Command params */
	settings->cmd_params.reset_to_driver_defaults = nrf_rpc_decode_bool(ctx);
	settings->cmd_params.write_scope_bitmap = nrf_rpc_decode_uint(ctx);

	/* Auto-start */
	settings->auto_start.activate = nrf_rpc_decode_bool(ctx);

	/* Region */
	settings->region = (enum dect_settings_region)nrf_rpc_decode_uint(ctx);

	/* Device type */
	settings->device_type = nrf_rpc_decode_uint(ctx);

	/* Identities */
	settings->identities.network_id = nrf_rpc_decode_uint(ctx);
	settings->identities.transmitter_long_rd_id = nrf_rpc_decode_uint(ctx);

	/* TX settings */
	settings->tx.max_power_dbm = nrf_rpc_decode_int(ctx);
	settings->tx.max_mcs = nrf_rpc_decode_uint(ctx);

	/* Power save */
	settings->power_save = nrf_rpc_decode_bool(ctx);

	/* Band */
	settings->band_nbr = nrf_rpc_decode_uint(ctx);

	/* RSSI scan */
	settings->rssi_scan.scan_suitable_percent = nrf_rpc_decode_uint(ctx);
	settings->rssi_scan.time_per_channel_ms = nrf_rpc_decode_uint(ctx);
	settings->rssi_scan.free_threshold_dbm = nrf_rpc_decode_int(ctx);
	settings->rssi_scan.busy_threshold_dbm = nrf_rpc_decode_int(ctx);

	/* Cluster settings */
	settings->cluster.max_beacon_tx_power_dbm = nrf_rpc_decode_int(ctx);
	settings->cluster.max_cluster_power_dbm = nrf_rpc_decode_int(ctx);
	settings->cluster.beacon_period = (enum dect_cluster_beacon_period)nrf_rpc_decode_uint(ctx);
	settings->cluster.max_num_neighbors = nrf_rpc_decode_uint(ctx);
	settings->cluster.neighbor_inactivity_disconnect_timer_ms = nrf_rpc_decode_uint(ctx);
	settings->cluster.channel_loaded_percent = nrf_rpc_decode_uint(ctx);

	/* Network beacon */
	settings->nw_beacon.channel = nrf_rpc_decode_uint(ctx);
	settings->nw_beacon.beacon_period = (enum dect_nw_beacon_period)nrf_rpc_decode_uint(ctx);

	/* Association */
	settings->association.max_beacon_rx_failures = nrf_rpc_decode_uint(ctx);
	settings->association.min_sensitivity_dbm = nrf_rpc_decode_int(ctx);

	/* Network join */
	settings->network_join.target_ft_long_rd_id = nrf_rpc_decode_uint(ctx);

	/* Security */
	settings->sec_conf.mode = (enum dect_mac_security_mode)nrf_rpc_decode_uint(ctx);
	if (!nrf_rpc_decode_buffer(ctx, settings->sec_conf.integrity_key,
				   DECT_MAC_INTEGRITY_KEY_LENGTH)) {
		return false;
	}
	if (!nrf_rpc_decode_buffer(ctx, settings->sec_conf.cipher_key,
				   DECT_MAC_CIPHER_KEY_LENGTH)) {
		return false;
	}

	return true;
}

void dect_rpc_encode_status_info(struct nrf_rpc_cbor_ctx *ctx,
				  const struct dect_status_info *status_info)
{
	if (!status_info) {
		nrf_rpc_encode_null(ctx);
		return;
	}

	nrf_rpc_encode_bool(ctx, status_info->mdm_activated);
	nrf_rpc_encode_bool(ctx, status_info->cluster_running);
	nrf_rpc_encode_uint(ctx, status_info->cluster_channel);
	nrf_rpc_encode_bool(ctx, status_info->nw_beacon_running);

	/* Parent associations */
	/* Encode the actual number of associations we will encode (max 1) */
	uint8_t parent_count_to_encode = (status_info->parent_count > 1) ? 1 : status_info->parent_count;
	nrf_rpc_encode_uint(ctx, parent_count_to_encode);
	for (uint8_t i = 0; i < parent_count_to_encode; i++) {
		nrf_rpc_encode_uint(ctx, status_info->parent_associations[i].long_rd_id);
		dect_rpc_encode_ipv6_addr(ctx, &status_info->parent_associations[i].local_ipv6_addr);
		nrf_rpc_encode_bool(ctx, status_info->parent_associations[i].global_ipv6_addr_set);
		if (status_info->parent_associations[i].global_ipv6_addr_set) {
			dect_rpc_encode_ipv6_addr(ctx,
						   &status_info->parent_associations[i].global_ipv6_addr);
		}
	}

	/* Child associations */
	nrf_rpc_encode_uint(ctx, status_info->child_count);
	for (uint8_t i = 0; i < status_info->child_count; i++) {
		nrf_rpc_encode_uint(ctx, status_info->child_associations[i].long_rd_id);
		dect_rpc_encode_ipv6_addr(ctx, &status_info->child_associations[i].local_ipv6_addr);
		nrf_rpc_encode_bool(ctx, status_info->child_associations[i].global_ipv6_addr_set);
		if (status_info->child_associations[i].global_ipv6_addr_set) {
			dect_rpc_encode_ipv6_addr(ctx,
						   &status_info->child_associations[i].global_ipv6_addr);
		}
	}

	/* Border router info */
	/* Note: net_if pointer cannot be serialized, so we encode iface index */
	int br_iface_index = status_info->br_net_iface ?
			     net_if_get_by_iface(status_info->br_net_iface) : -1;
	nrf_rpc_encode_int(ctx, br_iface_index);
	nrf_rpc_encode_bool(ctx, status_info->br_global_ipv6_addr_prefix_set);
	if (status_info->br_global_ipv6_addr_prefix_set) {
		dect_rpc_encode_ipv6_addr(ctx, &status_info->br_global_ipv6_addr_prefix);
		nrf_rpc_encode_int(ctx, status_info->br_global_ipv6_addr_prefix_len);
	}

	/* Firmware version string */
	nrf_rpc_encode_str(ctx, status_info->fw_version_str,
			   strlen(status_info->fw_version_str));
}

bool dect_rpc_decode_status_info(struct nrf_rpc_cbor_ctx *ctx,
				 struct dect_status_info *status_info)
{
	if (!status_info) {
		return false;
	}

	/* Check for null - use nrf_rpc_decode_is_null which properly handles errors */
	if (nrf_rpc_decode_is_null(ctx)) {
		return false;
	}

	memset(status_info, 0, sizeof(*status_info));

	status_info->mdm_activated = nrf_rpc_decode_bool(ctx);
	status_info->cluster_running = nrf_rpc_decode_bool(ctx);
	status_info->cluster_channel = nrf_rpc_decode_uint(ctx);
	status_info->nw_beacon_running = nrf_rpc_decode_bool(ctx);

	/* Parent associations */
	status_info->parent_count = nrf_rpc_decode_uint(ctx);
	if (status_info->parent_count > 1) {
		status_info->parent_count = 1;
	}
	for (uint8_t i = 0; i < status_info->parent_count; i++) {
		status_info->parent_associations[i].long_rd_id = nrf_rpc_decode_uint(ctx);
		if (!dect_rpc_decode_ipv6_addr(ctx, &status_info->parent_associations[i].local_ipv6_addr)) {
			return false;
		}
		status_info->parent_associations[i].global_ipv6_addr_set = nrf_rpc_decode_bool(ctx);
		if (status_info->parent_associations[i].global_ipv6_addr_set) {
			if (!dect_rpc_decode_ipv6_addr(ctx,
						       &status_info->parent_associations[i].global_ipv6_addr)) {
				return false;
			}
		}
	}

	/* Child associations */
	status_info->child_count = nrf_rpc_decode_uint(ctx);
	for (uint8_t i = 0; i < status_info->child_count; i++) {
		status_info->child_associations[i].long_rd_id = nrf_rpc_decode_uint(ctx);
		if (!dect_rpc_decode_ipv6_addr(ctx, &status_info->child_associations[i].local_ipv6_addr)) {
			return false;
		}
		status_info->child_associations[i].global_ipv6_addr_set = nrf_rpc_decode_bool(ctx);
		if (status_info->child_associations[i].global_ipv6_addr_set) {
			if (!dect_rpc_decode_ipv6_addr(ctx,
						       &status_info->child_associations[i].global_ipv6_addr)) {
				return false;
			}
		}
	}

	/* Border router info */
	int br_iface_index = nrf_rpc_decode_int(ctx);
	status_info->br_net_iface = (br_iface_index >= 0) ?
				     net_if_get_by_index(br_iface_index) : NULL;
	status_info->br_global_ipv6_addr_prefix_set = nrf_rpc_decode_bool(ctx);
	if (status_info->br_global_ipv6_addr_prefix_set) {
		if (!dect_rpc_decode_ipv6_addr(ctx, &status_info->br_global_ipv6_addr_prefix)) {
			return false;
		}
		status_info->br_global_ipv6_addr_prefix_len = nrf_rpc_decode_int(ctx);
	}

	/* Firmware version string */
	const char *fw_version = nrf_rpc_decode_str(ctx, status_info->fw_version_str,
						    sizeof(status_info->fw_version_str));
	if (!fw_version) {
		/* Check if decoder is still valid (NULL is OK, error is not) */
		if (!nrf_rpc_decode_valid(ctx)) {
			return false;
		}
		status_info->fw_version_str[0] = '\0';
	}

	return true;
}

