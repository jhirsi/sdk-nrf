/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/net/net_if.h>
#include <zephyr/net/dect_mgmt.h>
#include <zephyr/logging/log.h>
#include <nrf_rpc/nrf_rpc_serialize.h>
#include <nrf_rpc_cbor.h>

#include "dect_rpc_client.h"
#include "dect_rpc_common.h"
#include "dect_rpc_serialize.h"
#include "dect_rpc_group.h"
#include "dect_rpc_ids.h"

LOG_MODULE_REGISTER(dect_rpc_client, CONFIG_DECT_NRP_RPC_LOG_LEVEL);

/* Event callback */
static void (*event_callback)(int iface_index, uint32_t event,
			      const void *event_data, size_t event_data_len);
static void *event_callback_user_data;

int dect_rpc_client_init(void)
{
	/* Client initialization - can be extended if needed */
	return 0;
}

int dect_rpc_activate(int iface_index)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;
	int err;

	if (iface_index < 0) {
		return -EINVAL;
	}

	LOG_DBG("Sending DECT activate command for iface %d", iface_index);

	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 4);
	nrf_rpc_encode_int(&ctx, iface_index);

	err = nrf_rpc_cbor_cmd_rsp(&dect_rpc_group, DECT_RPC_CMD_ACTIVATE, &ctx);
	if (err < 0) {
		LOG_ERR("Failed to send/receive activate command: %d", err);
		return err;
	}

	LOG_DBG("Received activate response, decoding...");

	result = nrf_rpc_decode_int(&ctx);

	if (!nrf_rpc_decoding_done_and_check(&dect_rpc_group, &ctx)) {
		LOG_ERR("Failed to decode activate response");
		dect_rpc_report_rsp_decoding_error(DECT_RPC_CMD_ACTIVATE);
		return -EIO;
	}

	LOG_DBG("DECT activate completed with result: %d", result);
	return result;
}

int dect_rpc_deactivate(int iface_index)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;
	int err;

	if (iface_index < 0) {
		return -EINVAL;
	}

	LOG_DBG("Sending DECT deactivate command for iface %d", iface_index);

	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 4);
	nrf_rpc_encode_int(&ctx, iface_index);

	err = nrf_rpc_cbor_cmd_rsp(&dect_rpc_group, DECT_RPC_CMD_DEACTIVATE, &ctx);
	if (err < 0) {
		LOG_ERR("Failed to send/receive deactivate command: %d", err);
		return err;
	}

	LOG_DBG("Received deactivate response, decoding...");

	result = nrf_rpc_decode_int(&ctx);

	if (!nrf_rpc_decoding_done_and_check(&dect_rpc_group, &ctx)) {
		LOG_ERR("Failed to decode deactivate response");
		dect_rpc_report_rsp_decoding_error(DECT_RPC_CMD_DEACTIVATE);
		return -EIO;
	}

	LOG_DBG("DECT deactivate completed with result: %d", result);
	return result;
}

int dect_rpc_activate_iface(struct net_if *iface)
{
	int iface_index = iface ? dect_rpc_get_iface_index(iface) : 0;
	return dect_rpc_activate(iface_index);
}

int dect_rpc_deactivate_iface(struct net_if *iface)
{
	int iface_index = iface ? dect_rpc_get_iface_index(iface) : 0;
	return dect_rpc_deactivate(iface_index);
}

int dect_rpc_status_info_get(int iface_index, struct dect_status_info *status_info)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;
	int err;

	if (iface_index < 0 || !status_info) {
		return -EINVAL;
	}

	LOG_DBG("Sending DECT status_info_get command for iface %d", iface_index);

	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 4);
	nrf_rpc_encode_int(&ctx, iface_index);

	err = nrf_rpc_cbor_cmd_rsp(&dect_rpc_group, DECT_RPC_CMD_STATUS_INFO_GET, &ctx);
	if (err < 0) {
		LOG_ERR("Failed to send/receive status_info_get command: %d", err);
		return err;
	}

	LOG_DBG("Received status_info_get response, decoding...");

	result = nrf_rpc_decode_int(&ctx);

	if (result == 0 && nrf_rpc_decode_valid(&ctx)) {
		if (!dect_rpc_decode_status_info(&ctx, status_info)) {
			LOG_ERR("Failed to decode status_info response");
			dect_rpc_report_rsp_decoding_error(DECT_RPC_CMD_STATUS_INFO_GET);
			return -EIO;
		}
		LOG_DBG("Status_info decoded successfully");
	}

	if (!nrf_rpc_decoding_done_and_check(&dect_rpc_group, &ctx)) {
		LOG_ERR("Failed to complete status_info response decoding");
		dect_rpc_report_rsp_decoding_error(DECT_RPC_CMD_STATUS_INFO_GET);
		return -EIO;
	}

	LOG_DBG("DECT status_info_get completed with result: %d", result);
	return result;
}

int dect_rpc_settings_read(int iface_index, struct dect_settings *settings)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;

	if (iface_index < 0 || !settings) {
		return -EINVAL;
	}

	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 4);
	nrf_rpc_encode_int(&ctx, iface_index);

	nrf_rpc_cbor_cmd_rsp_no_err(&dect_rpc_group, DECT_RPC_CMD_SETTINGS_READ, &ctx);

	result = nrf_rpc_decode_int(&ctx);

	if (result == 0 && nrf_rpc_decode_valid(&ctx)) {
		if (!dect_rpc_decode_settings(&ctx, settings)) {
			dect_rpc_report_rsp_decoding_error(DECT_RPC_CMD_SETTINGS_READ);
			return -EIO;
		}
	}

	if (!nrf_rpc_decoding_done_and_check(&dect_rpc_group, &ctx)) {
		dect_rpc_report_rsp_decoding_error(DECT_RPC_CMD_SETTINGS_READ);
		return -EIO;
	}

	return result;
}

int dect_rpc_settings_write(int iface_index, const struct dect_settings *settings)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;

	if (iface_index < 0 || !settings) {
		return -EINVAL;
	}

	/* Estimate buffer size: iface_index + settings structure */
	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 4 + 200);
	nrf_rpc_encode_int(&ctx, iface_index);
	dect_rpc_encode_settings(&ctx, settings);

	nrf_rpc_cbor_cmd_rsp_no_err(&dect_rpc_group, DECT_RPC_CMD_SETTINGS_WRITE, &ctx);

	result = nrf_rpc_decode_int(&ctx);

	if (!nrf_rpc_decoding_done_and_check(&dect_rpc_group, &ctx)) {
		dect_rpc_report_rsp_decoding_error(DECT_RPC_CMD_SETTINGS_WRITE);
		return -EIO;
	}

	return result;
}

int dect_rpc_event_subscribe(int iface_index, uint32_t event_mask)
{
	struct nrf_rpc_cbor_ctx ctx;
	int result;

	if (iface_index < 0) {
		return -EINVAL;
	}

	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 8);
	nrf_rpc_encode_int(&ctx, iface_index);
	nrf_rpc_encode_uint(&ctx, event_mask);

	nrf_rpc_cbor_cmd_rsp_no_err(&dect_rpc_group, DECT_RPC_CMD_EVENT_SUBSCRIBE, &ctx);

	result = nrf_rpc_decode_int(&ctx);

	if (!nrf_rpc_decoding_done_and_check(&dect_rpc_group, &ctx)) {
		dect_rpc_report_rsp_decoding_error(DECT_RPC_CMD_EVENT_SUBSCRIBE);
		return -EIO;
	}

	return result;
}

void dect_rpc_register_event_callback(
	void (*callback)(int iface_index, uint32_t event,
			 const void *event_data, size_t event_data_len),
	void *user_data)
{
	event_callback = callback;
	event_callback_user_data = user_data;
}

/* Event handler - will be called by server-side event forwarding */
static void dect_rpc_evt_handler(const struct nrf_rpc_group *group,
				 struct nrf_rpc_cbor_ctx *ctx,
				 void *handler_data)
{
	uint64_t mgmt_event;
	int iface_index;

	iface_index = nrf_rpc_decode_int(ctx);
	mgmt_event = nrf_rpc_decode_uint64(ctx);

	/* Decode event data based on event type - simplified for MVP */
	/* For full implementation, decode specific event structures */

	if (event_callback) {
		/* For MVP, pass NULL event_data - can be extended later */
		/* Cast to uint32_t for callback compatibility (callback uses uint32_t) */
		event_callback(iface_index, (uint32_t)mgmt_event, NULL, 0);
	}
}

/* Register event decoders for all DECT events */
NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_evt_activate_done,
			 DECT_RPC_EVT_ACTIVATE_DONE, dect_rpc_evt_handler, NULL);
NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_evt_deactivate_done,
			 DECT_RPC_EVT_DEACTIVATE_DONE, dect_rpc_evt_handler, NULL);
NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_evt_scan_result,
			 DECT_RPC_EVT_SCAN_RESULT, dect_rpc_evt_handler, NULL);
NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_evt_scan_done,
			 DECT_RPC_EVT_SCAN_DONE, dect_rpc_evt_handler, NULL);
NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_evt_association_changed,
			 DECT_RPC_EVT_ASSOCIATION_CHANGED, dect_rpc_evt_handler, NULL);
NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_evt_network_status,
			 DECT_RPC_EVT_NETWORK_STATUS, dect_rpc_evt_handler, NULL);
NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_evt_sink_status,
			 DECT_RPC_EVT_SINK_STATUS, dect_rpc_evt_handler, NULL);

