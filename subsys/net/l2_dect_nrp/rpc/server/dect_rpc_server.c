/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/net/net_if.h>
#include <zephyr/net/dect_mgmt.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <nrf_rpc/nrf_rpc_serialize.h>
#include <nrf_rpc_cbor.h>

#include "dect_rpc_server.h"
#include "dect_rpc_common.h"
#include "dect_rpc_serialize.h"
#include "dect_rpc_group.h"
#include "dect_rpc_ids.h"

LOG_MODULE_REGISTER(dect_rpc_server, CONFIG_DECT_NRP_RPC_LOG_LEVEL);

/* Event subscription mask - for MVP, subscribe to all events */
static uint64_t event_subscription_mask = 0xFFFFFFFF;

/* Pending command tracking for async operations */
struct pending_cmd {
	struct net_if *iface;
	const struct nrf_rpc_group *group;
	struct k_sem completion_sem;
	int result;
	bool active;
};

static struct pending_cmd pending_activate;
static struct pending_cmd pending_deactivate;

/* RPC command handlers */

static void dect_rpc_cmd_activate(const struct nrf_rpc_group *group,
				  struct nrf_rpc_cbor_ctx *ctx,
				  void *handler_data)
{
	struct net_if *iface;
	int iface_index;
	int ret;

	iface_index = nrf_rpc_decode_int(ctx);

	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_ACTIVATE);
		return;
	}

	iface = dect_rpc_get_iface_by_index(iface_index);
	if (!iface) {
		nrf_rpc_rsp_send_int(group, -EINVAL);
		return;
	}

	/* Set up pending command */
	pending_activate.iface = iface;
	pending_activate.group = group;
	pending_activate.result = -ETIMEDOUT; /* Default to timeout */
	pending_activate.active = true;

	/* Queue the async operation */
	ret = net_mgmt(NET_REQUEST_DECT_ACTIVATE, iface, NULL, 0);
	if (ret != 0) {
		/* If net_mgmt returns error immediately, send response now */
		pending_activate.active = false;
		nrf_rpc_rsp_send_int(group, ret);
		return;
	}

	LOG_DBG("DECT activate queued, waiting for completion event");

	/* Wait for completion event (with timeout) */
	if (k_sem_take(&pending_activate.completion_sem, K_SECONDS(30)) != 0) {
		LOG_ERR("DECT activate timeout waiting for completion");
		pending_activate.active = false;
		nrf_rpc_rsp_send_int(group, -ETIMEDOUT);
		return;
	}

	/* Send response with result from completion event */
	ret = pending_activate.result;
	pending_activate.active = false;
	LOG_DBG("DECT activate completed with result: %d", ret);
	nrf_rpc_rsp_send_int(group, ret);
}

static void dect_rpc_cmd_deactivate(const struct nrf_rpc_group *group,
				    struct nrf_rpc_cbor_ctx *ctx,
				    void *handler_data)
{
	struct net_if *iface;
	int iface_index;
	int ret;

	iface_index = nrf_rpc_decode_int(ctx);

	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_DEACTIVATE);
		return;
	}

	iface = dect_rpc_get_iface_by_index(iface_index);
	if (!iface) {
		nrf_rpc_rsp_send_int(group, -EINVAL);
		return;
	}

	/* Set up pending command */
	pending_deactivate.iface = iface;
	pending_deactivate.group = group;
	pending_deactivate.result = -ETIMEDOUT; /* Default to timeout */
	pending_deactivate.active = true;

	/* Queue the async operation */
	ret = net_mgmt(NET_REQUEST_DECT_DEACTIVATE, iface, NULL, 0);
	if (ret != 0) {
		/* If net_mgmt returns error immediately, send response now */
		pending_deactivate.active = false;
		nrf_rpc_rsp_send_int(group, ret);
		return;
	}

	LOG_DBG("DECT deactivate queued, waiting for completion event");

	/* Wait for completion event (with timeout) */
	if (k_sem_take(&pending_deactivate.completion_sem, K_SECONDS(30)) != 0) {
		LOG_ERR("DECT deactivate timeout waiting for completion");
		pending_deactivate.active = false;
		nrf_rpc_rsp_send_int(group, -ETIMEDOUT);
		return;
	}

	/* Send response with result from completion event */
	ret = pending_deactivate.result;
	pending_deactivate.active = false;
	LOG_DBG("DECT deactivate completed with result: %d", ret);
	nrf_rpc_rsp_send_int(group, ret);
}

static void dect_rpc_cmd_status_info_get(const struct nrf_rpc_group *group,
					 struct nrf_rpc_cbor_ctx *ctx,
					 void *handler_data)
{
	struct net_if *iface;
	struct dect_status_info status_info;
	int iface_index;
	int ret;

	iface_index = nrf_rpc_decode_int(ctx);

	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_STATUS_INFO_GET);
		return;
	}

	LOG_DBG("DECT status_info_get command received for iface %d", iface_index);

	iface = dect_rpc_get_iface_by_index(iface_index);
	if (!iface) {
		LOG_ERR("DECT interface not found for index %d", iface_index);
		nrf_rpc_rsp_send_int(group, -EINVAL);
		return;
	}

	ret = net_mgmt(NET_REQUEST_DECT_STATUS_INFO_GET, iface, &status_info,
		       sizeof(status_info));

	LOG_DBG("DECT status_info_get result: %d", ret);

	if (ret == 0) {
		struct nrf_rpc_cbor_ctx rsp_ctx;

		NRF_RPC_CBOR_ALLOC(group, rsp_ctx, 4 + 300);
		nrf_rpc_encode_int(&rsp_ctx, ret);
		dect_rpc_encode_status_info(&rsp_ctx, &status_info);
		LOG_DBG("Sending status_info response");
		nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
		LOG_DBG("Status_info response sent");
	} else {
		LOG_ERR("Failed to get status info: %d", ret);
		nrf_rpc_rsp_send_int(group, ret);
	}
}

static void dect_rpc_cmd_settings_read(const struct nrf_rpc_group *group,
					struct nrf_rpc_cbor_ctx *ctx,
					void *handler_data)
{
	struct net_if *iface;
	struct dect_settings settings;
	int iface_index;
	int ret;

	iface_index = nrf_rpc_decode_int(ctx);

	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_SETTINGS_READ);
		return;
	}

	iface = dect_rpc_get_iface_by_index(iface_index);
	if (!iface) {
		nrf_rpc_rsp_send_int(group, -EINVAL);
		return;
	}

	ret = net_mgmt(NET_REQUEST_DECT_SETTINGS_READ, iface, &settings,
		       sizeof(settings));

	if (ret == 0) {
		struct nrf_rpc_cbor_ctx rsp_ctx;

		NRF_RPC_CBOR_ALLOC(group, rsp_ctx, 4 + 200);
		nrf_rpc_encode_int(&rsp_ctx, ret);
		dect_rpc_encode_settings(&rsp_ctx, &settings);
		nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
	} else {
		nrf_rpc_rsp_send_int(group, ret);
	}
}

static void dect_rpc_cmd_settings_write(const struct nrf_rpc_group *group,
					struct nrf_rpc_cbor_ctx *ctx,
					void *handler_data)
{
	struct net_if *iface;
	struct dect_settings settings;
	int iface_index;
	int ret;

	iface_index = nrf_rpc_decode_int(ctx);

	if (!dect_rpc_decode_settings(ctx, &settings)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_SETTINGS_WRITE);
		return;
	}

	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_SETTINGS_WRITE);
		return;
	}

	iface = dect_rpc_get_iface_by_index(iface_index);
	if (!iface) {
		nrf_rpc_rsp_send_int(group, -EINVAL);
		return;
	}

	ret = net_mgmt(NET_REQUEST_DECT_SETTINGS_WRITE, iface, &settings,
		       sizeof(settings));

	nrf_rpc_rsp_send_int(group, ret);
}

static void dect_rpc_cmd_event_subscribe(const struct nrf_rpc_group *group,
					 struct nrf_rpc_cbor_ctx *ctx,
					 void *handler_data)
{
	int iface_index;
	uint32_t event_mask;

	iface_index = nrf_rpc_decode_int(ctx);
	event_mask = nrf_rpc_decode_uint(ctx);

	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_EVENT_SUBSCRIBE);
		return;
	}

	/* For MVP, store subscription mask - can be extended per-client later */
	/* Convert uint32_t mask to uint64_t for net_mgmt event comparison */
	event_subscription_mask = (uint64_t)event_mask;

	nrf_rpc_rsp_send_int(group, 0);
}

/* Register command decoders */
NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_activate,
			 DECT_RPC_CMD_ACTIVATE, dect_rpc_cmd_activate, NULL);
NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_deactivate,
			 DECT_RPC_CMD_DEACTIVATE, dect_rpc_cmd_deactivate, NULL);
NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_status_info_get,
			 DECT_RPC_CMD_STATUS_INFO_GET, dect_rpc_cmd_status_info_get, NULL);
NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_settings_read,
			 DECT_RPC_CMD_SETTINGS_READ, dect_rpc_cmd_settings_read, NULL);
NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_settings_write,
			 DECT_RPC_CMD_SETTINGS_WRITE, dect_rpc_cmd_settings_write, NULL);
NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_event_subscribe,
			 DECT_RPC_CMD_EVENT_SUBSCRIBE, dect_rpc_cmd_event_subscribe, NULL);

/* Event forwarding from net_mgmt to RPC */
static void dect_rpc_net_mgmt_event_cb(struct net_mgmt_event_callback *cb,
					uint64_t mgmt_event,
					struct net_if *iface)
{
	ARG_UNUSED(cb);
	struct nrf_rpc_cbor_ctx ctx;
	enum dect_rpc_cmd_client event_id;
	int iface_index;
	struct dect_common_resp_evt *evt_data;

	/* Handle completion events for pending commands first */
	if (mgmt_event == NET_EVENT_DECT_ACTIVATE_DONE) {
		if (pending_activate.active && pending_activate.iface == iface) {
			/* Extract status from event data */
			evt_data = (struct dect_common_resp_evt *)cb->info;
			if (evt_data && evt_data->status == DECT_MAC_STATUS_OK) {
				pending_activate.result = 0;
			} else {
				/* Convert dect_status_values to error code */
				pending_activate.result = -EIO;
			}
			k_sem_give(&pending_activate.completion_sem);
			LOG_DBG("DECT activate completion event received, result: %d",
				pending_activate.result);
		}
	} else if (mgmt_event == NET_EVENT_DECT_DEACTIVATE_DONE) {
		if (pending_deactivate.active && pending_deactivate.iface == iface) {
			/* Extract status from event data */
			evt_data = (struct dect_common_resp_evt *)cb->info;
			if (evt_data && evt_data->status == DECT_MAC_STATUS_OK) {
				pending_deactivate.result = 0;
			} else {
				/* Convert dect_status_values to error code */
				pending_deactivate.result = -EIO;
			}
			k_sem_give(&pending_deactivate.completion_sem);
			LOG_DBG("DECT deactivate completion event received, result: %d",
				pending_deactivate.result);
		}
	}

	/* Check if event is subscribed - use uint64_t for mask comparison */
	if (!(event_subscription_mask & mgmt_event)) {
		return;
	}

	/* Map net_mgmt event to RPC event ID - use uint64_t for switch */
	switch (mgmt_event) {
	case NET_EVENT_DECT_ACTIVATE_DONE:
		event_id = DECT_RPC_EVT_ACTIVATE_DONE;
		break;
	case NET_EVENT_DECT_DEACTIVATE_DONE:
		event_id = DECT_RPC_EVT_DEACTIVATE_DONE;
		break;
	case NET_EVENT_DECT_SCAN_RESULT:
		event_id = DECT_RPC_EVT_SCAN_RESULT;
		break;
	case NET_EVENT_DECT_SCAN_DONE:
		event_id = DECT_RPC_EVT_SCAN_DONE;
		break;
	case NET_EVENT_DECT_ASSOCIATION_CHANGED:
		event_id = DECT_RPC_EVT_ASSOCIATION_CHANGED;
		break;
	case NET_EVENT_DECT_NETWORK_STATUS:
		event_id = DECT_RPC_EVT_NETWORK_STATUS;
		break;
	case NET_EVENT_DECT_SINK_STATUS:
		event_id = DECT_RPC_EVT_SINK_STATUS;
		break;
	default:
		/* Unknown event - skip */
		return;
	}

	iface_index = dect_rpc_get_iface_index(iface);

	/* Serialize and send event */
	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 8);
	nrf_rpc_encode_int(&ctx, iface_index);
	nrf_rpc_encode_uint64(&ctx, mgmt_event);

	/* For MVP, send basic event info only */
	/* Can be extended to serialize full event data structures */

	nrf_rpc_cbor_evt(&dect_rpc_group, event_id, &ctx);
}

static struct net_mgmt_event_callback dect_rpc_event_cb;

int dect_rpc_server_init(void)
{
	/* Initialize semaphores for pending commands */
	k_sem_init(&pending_activate.completion_sem, 0, 1);
	k_sem_init(&pending_deactivate.completion_sem, 0, 1);
	pending_activate.active = false;
	pending_deactivate.active = false;

	/* Register net_mgmt event callback */
	net_mgmt_init_event_callback(&dect_rpc_event_cb,
				     dect_rpc_net_mgmt_event_cb,
				     NET_EVENT_DECT_ACTIVATE_DONE |
				     NET_EVENT_DECT_DEACTIVATE_DONE |
				     NET_EVENT_DECT_SCAN_RESULT |
				     NET_EVENT_DECT_SCAN_DONE |
				     NET_EVENT_DECT_RSSI_SCAN_RESULT |
				     NET_EVENT_DECT_RSSI_SCAN_DONE |
				     NET_EVENT_DECT_ASSOCIATION_CHANGED |
				     NET_EVENT_DECT_NETWORK_STATUS |
				     NET_EVENT_DECT_SINK_STATUS |
				     NET_EVENT_DECT_CLUSTER_CREATED_RESULT |
				     NET_EVENT_DECT_CLUSTER_STOPPED_RESULT |
				     NET_EVENT_DECT_NW_BEACON_START_RESULT |
				     NET_EVENT_DECT_NW_BEACON_STOP_RESULT |
				     NET_EVENT_DECT_NEIGHBOR_LIST |
				     NET_EVENT_DECT_NEIGHBOR_INFO |
				     NET_EVENT_DECT_CLUSTER_INFO);

	net_mgmt_add_event_callback(&dect_rpc_event_cb);

	LOG_INF("DECT RPC server initialized");
	LOG_DBG("Registered command handlers: ACTIVATE=%d, DEACTIVATE=%d, STATUS_INFO_GET=%d, SETTINGS_READ=%d, SETTINGS_WRITE=%d, EVENT_SUBSCRIBE=%d",
		DECT_RPC_CMD_ACTIVATE, DECT_RPC_CMD_DEACTIVATE, DECT_RPC_CMD_STATUS_INFO_GET,
		DECT_RPC_CMD_SETTINGS_READ, DECT_RPC_CMD_SETTINGS_WRITE, DECT_RPC_CMD_EVENT_SUBSCRIBE);

	return 0;
}

