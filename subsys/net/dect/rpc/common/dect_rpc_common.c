/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "dect_rpc_common.h"
#include "dect_rpc_ids.h"
#include <nrf_rpc/nrf_rpc_serialize.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dect_rpc, CONFIG_NET_DECT_RPC_LOG_LEVEL);

void dect_rpc_decode_void(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
			  void *handler_data)
{
	nrf_rpc_rsp_decode_void(group, ctx, handler_data);
}

void dect_rpc_report_cmd_decoding_error(uint8_t cmd_evt_id)
{
	LOG_ERR("DECT NR+ RPC command decoding error: %u", cmd_evt_id);
}
