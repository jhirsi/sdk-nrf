/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/net/net_if.h>
#include <zephyr/logging/log.h>
#include <nrf_rpc_cbor.h>
#include "dect_rpc_common.h"
#include "dect_rpc_ids.h"

LOG_MODULE_REGISTER(dect_rpc_common, CONFIG_DECT_NRP_RPC_LOG_LEVEL);

struct net_if *dect_rpc_get_iface_by_index(int iface_index)
{
	struct net_if *iface;

	if (iface_index < 0) {
		return NULL;
	}

	/* If index is 0, find the DECT interface by name (same pattern as dect_l2_shell.c) */
	if (iface_index == 0) {
		#ifdef CONFIG_DECT_NRP_MAC_DEVICE_NAME
		int dect_iface_idx = net_if_get_by_name(CONFIG_DECT_NRP_MAC_DEVICE_NAME);
		if (dect_iface_idx >= 0) {
			iface = net_if_get_by_index(dect_iface_idx);
			if (iface) {
				LOG_DBG("Found DECT interface '%s' at index %d",
					CONFIG_DECT_NRP_MAC_DEVICE_NAME, dect_iface_idx);
				return iface;
			}
		}
		LOG_WRN("DECT interface '%s' not found", CONFIG_DECT_NRP_MAC_DEVICE_NAME);
		#else
		LOG_WRN("CONFIG_DECT_NRP_MAC_DEVICE_NAME not defined");
		#endif
		return NULL;
	}

	/* For non-zero index, use the index directly */
	iface = net_if_get_by_index(iface_index);
	if (!iface) {
		LOG_WRN("Interface with index %d not found", iface_index);
		return NULL;
	}

	return iface;
}

int dect_rpc_get_iface_index(struct net_if *iface)
{
	if (!iface) {
		return -1;
	}

	return net_if_get_by_iface(iface);
}

void dect_rpc_report_cmd_decoding_error(enum dect_rpc_cmd_server cmd_id)
{
	LOG_ERR("Failed to decode DECT RPC command: %d", cmd_id);
}

void dect_rpc_report_rsp_decoding_error(enum dect_rpc_cmd_server cmd_id)
{
	LOG_ERR("Failed to decode DECT RPC response: %d", cmd_id);
}

