/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * DECT NR+ RPC server: Receives IF_SEND (raw IPv6 from DECT NR+ RPC client), builds net_pkt, sets
 * lladdr from IPv6 dst, calls net_if_send(server_dect_if, pkt).
 * Packets received from the DECT NR+ modem are forwarded to the DECT NR+ RPC client via
 * IF_RECEIVE using the hook in DECT NR+ L2 (dect_net_l2_rpc_forward_register).
 * GET_ADDRS: reads DECT NR+ RPC server DECT iface IPv6 addrs/prefixes and returns them
 * so the DECT NR+ RPC client can mirror them.
 */

#include "dect_rpc_ids.h"
#include "dect_rpc_common.h"
#include <nrf_rpc.h>
#include <nrf_rpc_cbor.h>
#include <nrf_rpc/nrf_rpc_serialize.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/init.h>

#include <net/dect/dect_net_l2.h>
#include <net/dect/dect_net_l2_mgmt.h>
#include <net/dect/dect_utils.h>

#if defined(CONFIG_DECT_NR_RPC_SHELL)
#include <net/dect/dect_net_l2_shell_util.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(dect_rpc, CONFIG_NET_DECT_RPC_LOG_LEVEL);

#define DECT_RPC_MAX_ADDRS   8
#define DECT_RPC_MAX_PREFIXES 4
#define DECT_RPC_MAX_STATUS_CHILDREN 16

static struct net_if *server_dect_if;

enum dect_rpc_evt_type {
	DECT_RPC_EVT_IF_RECEIVE,
	DECT_RPC_EVT_IF_LINK_STATE,
	DECT_RPC_EVT_IF_ADDRS_CHANGED,
};

struct dect_rpc_evt_msg {
	enum dect_rpc_evt_type type;
	union {
		struct {
			uint8_t *data;
			size_t len;
		} if_receive;
		struct {
			bool carrier_ok;
			bool dormant;
		} if_link_state;
	};
};

#define DECT_RPC_EVT_QUEUE_LEN 8
K_MSGQ_DEFINE(dect_rpc_evt_msgq, sizeof(struct dect_rpc_evt_msg), DECT_RPC_EVT_QUEUE_LEN, 4);

static void dect_rpc_evt_work_fn(struct k_work *work);

static K_WORK_DEFINE(dect_rpc_evt_work, dect_rpc_evt_work_fn);

static void dect_rpc_evt_work_fn(struct k_work *work)
{
	struct dect_rpc_evt_msg msg;
	struct nrf_rpc_cbor_ctx ctx;

	ARG_UNUSED(work);

	/* Process one event per run */
	if (k_msgq_get(&dect_rpc_evt_msgq, &msg, K_NO_WAIT) != 0) {
		return;
	}

	if (!NRF_RPC_GROUP_STATUS(dect_rpc_group)) {
		if (msg.type == DECT_RPC_EVT_IF_RECEIVE && msg.if_receive.data != NULL) {
			k_free(msg.if_receive.data);
		}
		goto resubmit;
	}

	switch (msg.type) {
	case DECT_RPC_EVT_IF_RECEIVE:
		if (msg.if_receive.data != NULL && msg.if_receive.len > 0) {
			const size_t cbor_size = 10 + msg.if_receive.len;

			NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, cbor_size);
			if (zcbor_bstr_start_encode(ctx.zs)) {
				memcpy(ctx.zs[0].payload_mut, msg.if_receive.data,
				       msg.if_receive.len);
				ctx.zs->payload_mut += msg.if_receive.len;
				if (zcbor_bstr_end_encode(ctx.zs, NULL)) {
					nrf_rpc_cbor_evt_no_err(&dect_rpc_group,
								DECT_RPC_CMD_IF_RECEIVE, &ctx);
				}
			}
			k_free(msg.if_receive.data);
		}
		break;
	case DECT_RPC_EVT_IF_LINK_STATE:
		NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 4);
		nrf_rpc_encode_bool(&ctx, msg.if_link_state.carrier_ok);
		nrf_rpc_encode_bool(&ctx, msg.if_link_state.dormant);
		nrf_rpc_cbor_evt_no_err(&dect_rpc_group, DECT_RPC_CMD_IF_LINK_STATE, &ctx);
		LOG_DBG("Forwarded link state (carrier=%d dormant=%d) to RPC client",
			msg.if_link_state.carrier_ok, msg.if_link_state.dormant);
		break;
	case DECT_RPC_EVT_IF_ADDRS_CHANGED:
		NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 4);
		nrf_rpc_encode_uint(&ctx, 0U);
		nrf_rpc_cbor_evt_no_err(&dect_rpc_group, DECT_RPC_CMD_IF_ADDRS_CHANGED, &ctx);
		LOG_DBG("Notified RPC client: addresses changed");
		break;
	}

	if (CONFIG_DECT_NR_RPC_SERVER_EVT_PACING_MS > 0) {
		if (IS_ENABLED(CONFIG_NRF_RPC_UART_RELIABLE) ||
		    msg.type != DECT_RPC_EVT_IF_RECEIVE) {
			k_msleep(CONFIG_DECT_NR_RPC_SERVER_EVT_PACING_MS);
		}
	}

resubmit:
	/* If more events are queued, run work again. */
	if (k_msgq_num_used_get(&dect_rpc_evt_msgq) > 0) {
		k_work_submit(&dect_rpc_evt_work);
	}
}

/** Resolve DECT NR+ interface by device name (CONFIG_DECT_MDM_DEVICE_NAME, e.g. "dect0"). */
static struct net_if *get_server_dect_if(void)
{
	int idx = net_if_get_by_name(CONFIG_DECT_MDM_DEVICE_NAME);

	if (idx < 0) {
		return NULL;
	}
	return net_if_get_by_index(idx);
}

#if defined(CONFIG_DECT_NR_RPC_SHELL)
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

/* Session mutex: local shell holds it while a DECT command runs; RPC shell uses try_lock.
 * Server shell always wins (local shell gets the lock first when user types a command).
 */
K_MUTEX_DEFINE(dect_rpc_shell_session_mutex);

/* Flush each line to RPC immediately via SHELL_LINE (work queue avoids blocking handler). */
#define DECT_RPC_SHELL_LINE_LEN 256
#define DECT_RPC_SHELL_LINE_QUEUE_LEN 48
#define DECT_RPC_SHELL_LINE_PUT_TIMEOUT_MS 2000

struct dect_rpc_shell_line_msg {
	char buf[DECT_RPC_SHELL_LINE_LEN];
	size_t len;
};

K_MSGQ_DEFINE(dect_rpc_shell_line_msgq, sizeof(struct dect_rpc_shell_line_msg),
	      DECT_RPC_SHELL_LINE_QUEUE_LEN, 4);

static void dect_rpc_shell_line_work_fn(struct k_work *work)
{
	struct dect_rpc_shell_line_msg msg;
	struct nrf_rpc_cbor_ctx ctx;

	ARG_UNUSED(work);
	while (k_msgq_get(&dect_rpc_shell_line_msgq, &msg, K_NO_WAIT) == 0) {
		if (msg.len == 0 || !NRF_RPC_GROUP_STATUS(dect_rpc_group)) {
			continue;
		}
		NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 4 + msg.len);
		nrf_rpc_encode_buffer(&ctx, (const uint8_t *)msg.buf, msg.len);
		nrf_rpc_cbor_evt_no_err(&dect_rpc_group, DECT_RPC_CMD_SHELL_LINE, &ctx);
		if (CONFIG_DECT_NR_RPC_SERVER_EVT_PACING_MS > 0) {
			k_msleep(CONFIG_DECT_NR_RPC_SERVER_EVT_PACING_MS);
		}
	}
}

static K_WORK_DEFINE(dect_rpc_shell_line_work, dect_rpc_shell_line_work_fn);

static void dect_rpc_shell_line_flush(const struct shell *shell, const char *fmt, va_list ap)
{
	struct dect_rpc_shell_line_msg msg;
	int n;
	const struct shell *shell_display;

	n = vsnprintf(msg.buf, sizeof(msg.buf), fmt, ap);
	if (n <= 0) {
		return;
	}
	msg.len = (size_t)n;
	if (msg.len >= sizeof(msg.buf)) {
		msg.len = sizeof(msg.buf) - 1;
	}
	if (msg.len > 0 && msg.buf[msg.len - 1] != '\n') {
		if (msg.len + 1 < sizeof(msg.buf)) {
			msg.buf[msg.len++] = '\n';
			msg.buf[msg.len] = '\0';
		}
	}

	/* Queue for RPC (work thread sends); block briefly so bursts do not drop lines. */
	if (k_msgq_put(
		&dect_rpc_shell_line_msgq, &msg, K_MSEC(DECT_RPC_SHELL_LINE_PUT_TIMEOUT_MS)) != 0) {
		LOG_WRN("Shell line RPC queue full after %d ms; client may miss output",
			DECT_RPC_SHELL_LINE_PUT_TIMEOUT_MS);
	} else {
		k_work_submit(&dect_rpc_shell_line_work);
	}

	/* Always show on server. */
	shell_display = (shell != NULL) ? shell : shell_backend_uart_get_ptr();
	if (shell_display != NULL) {
		shell_fprintf(shell_display, SHELL_NORMAL, "%.*s", (int)msg.len, msg.buf);
		if (msg.len > 0 && msg.buf[msg.len - 1] != '\n') {
			shell_fprintf(shell_display, SHELL_NORMAL, "\n");
		}
	}
}

static void dect_rpc_shell_print_fn(const struct shell *shell, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	dect_rpc_shell_line_flush(shell, fmt, ap);
	va_end(ap);
}

static void dect_rpc_shell_error_fn(const struct shell *shell, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	dect_rpc_shell_line_flush(shell, fmt, ap);
	va_end(ap);
}

static void dect_rpc_shell_warn_fn(const struct shell *shell, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	dect_rpc_shell_line_flush(shell, fmt, ap);
	va_end(ap);
}

void dect_rpc_shell_session_lock(void)
{
	k_mutex_lock(&dect_rpc_shell_session_mutex, K_FOREVER);
}

void dect_rpc_shell_session_unlock(void)
{
	k_mutex_unlock(&dect_rpc_shell_session_mutex);
}

static int dect_rpc_shell_session_try_lock(void)
{
	return k_mutex_lock(&dect_rpc_shell_session_mutex, K_NO_WAIT);
}
#endif

/* Called from l2_dect (dect_net_l2_recv) for every received packet. Queue send (event);
 * server-originated command would arrive as event on client (dst=UNKNOWN) and never get RSP.
 */
void dect_rpc_server_forward_recv(struct net_if *iface, struct net_pkt *pkt)
{
	size_t len;
	uint8_t *copy;
	struct dect_rpc_evt_msg msg;

	ARG_UNUSED(iface);

	if (!pkt) {
		return;
	}

	len = net_pkt_get_len(pkt);
	if (len == 0) {
		return;
	}

	copy = k_malloc(len);
	if (!copy) {
		LOG_WRN("Failed to alloc for RPC forward");
		return;
	}
	if (net_pkt_read(pkt, copy, len) < 0) {
		k_free(copy);
		return;
	}

	msg.type = DECT_RPC_EVT_IF_RECEIVE;
	msg.if_receive.data = copy;
	msg.if_receive.len = len;
	if (k_msgq_put(&dect_rpc_evt_msgq, &msg, K_NO_WAIT) == 0) {
		k_work_submit(&dect_rpc_evt_work);
		LOG_DBG("DECT recv %zu bytes -> RPC client", len);
		LOG_DBG("Queued IPv6 packet for RPC client (%zu bytes)", len);
	} else {
		k_free(copy);
		LOG_WRN("DECT recv %zu bytes: event queue full, drop", len);
	}
}

/* Notify client that addresses/prefixes/link may have changed; client will re-sync. */
void dect_rpc_server_notify_addrs_changed(void)
{
	struct dect_rpc_evt_msg msg = {
		.type = DECT_RPC_EVT_IF_ADDRS_CHANGED,
	};

	if (k_msgq_put(&dect_rpc_evt_msgq, &msg, K_NO_WAIT) == 0) {
		k_work_submit(&dect_rpc_evt_work);
	}
}

/* Called from L2 when carrier/dormant state may have changed. Queue send to client. */
static void dect_rpc_server_link_state_changed(struct net_if *iface)
{
	struct dect_rpc_evt_msg msg;

	if (!iface) {
		return;
	}

	msg.type = DECT_RPC_EVT_IF_LINK_STATE;
	msg.if_link_state.carrier_ok = net_if_is_carrier_ok(iface);
	msg.if_link_state.dormant = net_if_is_dormant(iface);
	if (k_msgq_put(&dect_rpc_evt_msgq, &msg, K_NO_WAIT) == 0) {
		k_work_submit(&dect_rpc_evt_work);
	}

	/* Tell client to re-sync addresses/prefixes so it gets full state. */
	dect_rpc_server_notify_addrs_changed();
}

static void dect_rpc_cmd_if_send(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
				  void *handler_data)
{
	const uint8_t *pkt_data;
	size_t pkt_data_len;
	struct net_pkt *pkt = NULL;
	struct in6_addr dst;
	uint32_t target_long_rd_id;
	int ret;

	ARG_UNUSED(handler_data);

	dect_net_l2_rpc_client_set_connected(true);
	LOG_DBG("Received IPv6 packet from RPC client");

	pkt_data = nrf_rpc_decode_buffer_ptr_and_size(ctx, &pkt_data_len);
	if (!pkt_data || pkt_data_len < sizeof(struct in6_addr)) {
		goto out;
	}

	if (!server_dect_if) {
		server_dect_if = get_server_dect_if();
	}
	if (!server_dect_if) {
		LOG_ERR("No DECT interface for RPC send");
		goto out;
	}

	/* Parse IPv6 header to get destination for lladdr. */
	memcpy(&dst, pkt_data + 24, sizeof(struct in6_addr));
	target_long_rd_id = dect_utils_lib_long_rd_id_from_ipv6_addr(&dst);
	if (!target_long_rd_id) {
		LOG_ERR("No long_rd_id from IPv6 dst");
		goto out;
	}

	pkt = net_pkt_alloc_with_buffer(server_dect_if, pkt_data_len, AF_INET6, 0, K_NO_WAIT);
	if (!pkt) {
		LOG_ERR("Failed to allocate net_pkt");
		goto out;
	}

	ret = net_pkt_write(pkt, pkt_data, pkt_data_len);
	if (ret < 0) {
		LOG_ERR("net_pkt_write failed: %d", ret);
		net_pkt_unref(pkt);
		goto out;
	}

	net_pkt_set_family(pkt, AF_INET6);
	target_long_rd_id = htonl(target_long_rd_id);
	net_pkt_lladdr_dst(pkt)->len = sizeof(target_long_rd_id);
	memcpy(net_pkt_lladdr_dst(pkt)->addr, &target_long_rd_id, sizeof(target_long_rd_id));

	/* Reset cursor so driver's net_pkt_read() reads from the start of the packet. */
	net_pkt_cursor_init(pkt);

	LOG_DBG("RPC recv %zu bytes -> DECT send", pkt_data_len);
	if (net_if_send_data(server_dect_if, pkt) != NET_OK) {
		LOG_WRN("RPC recv %zu bytes: DECT send failed", pkt_data_len);
		net_pkt_unref(pkt);
	}

out:
	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_IF_SEND);
		return;
	}
	nrf_rpc_rsp_send_void(group);
}

NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_if_send, DECT_RPC_CMD_IF_SEND,
			 dect_rpc_cmd_if_send, NULL);

static void dect_rpc_cmd_if_enable(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
				   void *handler_data)
{
	ARG_UNUSED(handler_data);

	dect_net_l2_rpc_client_set_connected(true);
	/* Optional: decode bool enable and sync with server state. */
	(void)nrf_rpc_decode_bool(ctx);
	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_IF_ENABLE);
		return;
	}
	nrf_rpc_rsp_send_void(group);
}

NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_if_enable, DECT_RPC_CMD_IF_ENABLE,
			 dect_rpc_cmd_if_enable, NULL);

/* GET_ADDRS: read-only mirror of dect0. We never modify server dect0 (no put, no down, no set_mtu).
 * Response: n_addrs, addrs[], n_prefixes, prefixes[], mtu, carrier_ok, dormant.
 */
static void dect_rpc_cmd_if_get_addrs(const struct nrf_rpc_group *group,
				      struct nrf_rpc_cbor_ctx *ctx, void *handler_data)
{
	struct net_if_ipv6 *ipv6;
	struct nrf_rpc_cbor_ctx rsp_ctx;
	int n_addrs = 0;
	int n_prefixes = 0;
	int j;
	size_t rsp_size = 4 + DECT_RPC_MAX_ADDRS * (2 + 16) + 4
		+ DECT_RPC_MAX_PREFIXES * (2 + 16 + 1) + 4;

	ARG_UNUSED(handler_data);

	dect_net_l2_rpc_client_set_connected(true);
	/* Consume optional request payload (client may send null) */
	(void)nrf_rpc_decode_skip(ctx);
	nrf_rpc_decoding_done(group, ctx->in_packet);

	if (!server_dect_if) {
		server_dect_if = get_server_dect_if();
	}
	if (!server_dect_if || net_if_config_ipv6_get(server_dect_if, &ipv6) < 0) {
		NRF_RPC_CBOR_ALLOC(group, rsp_ctx, 16);
		nrf_rpc_encode_uint(&rsp_ctx, 0);
		nrf_rpc_encode_uint(&rsp_ctx, 0);
		nrf_rpc_encode_uint(&rsp_ctx, (uint32_t)NET_IPV6_MTU);
		nrf_rpc_encode_bool(&rsp_ctx, false);
		nrf_rpc_encode_bool(&rsp_ctx, true);
		nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
		return;
	}

	for (int i = 0; i < NET_IF_MAX_IPV6_ADDR; i++) {
		if (ipv6->unicast[i].is_used) {
			n_addrs++;
		}
	}
	for (int i = 0; i < NET_IF_MAX_IPV6_PREFIX; i++) {
		if (ipv6->prefix[i].is_used) {
			n_prefixes++;
		}
	}
	if (n_addrs > DECT_RPC_MAX_ADDRS) {
		n_addrs = DECT_RPC_MAX_ADDRS;
	}
	if (n_prefixes > DECT_RPC_MAX_PREFIXES) {
		n_prefixes = DECT_RPC_MAX_PREFIXES;
	}

	NRF_RPC_CBOR_ALLOC(group, rsp_ctx, rsp_size);
	nrf_rpc_encode_uint(&rsp_ctx, (uint32_t)n_addrs);
	j = 0;
	for (int i = 0; i < NET_IF_MAX_IPV6_ADDR && j < n_addrs; i++) {
		if (ipv6->unicast[i].is_used) {
			nrf_rpc_encode_buffer(&rsp_ctx, &ipv6->unicast[i].address.in6_addr,
					     sizeof(struct in6_addr));
			j++;
		}
	}
	nrf_rpc_encode_uint(&rsp_ctx, (uint32_t)n_prefixes);
	j = 0;
	for (int i = 0; i < NET_IF_MAX_IPV6_PREFIX && j < n_prefixes; i++) {
		if (ipv6->prefix[i].is_used) {
			nrf_rpc_encode_buffer(&rsp_ctx, &ipv6->prefix[i].prefix,
					     sizeof(struct in6_addr));
			nrf_rpc_encode_uint(&rsp_ctx, (uint32_t)ipv6->prefix[i].len);
			j++;
		}
	}
	nrf_rpc_encode_uint(&rsp_ctx, (uint32_t)net_if_get_mtu(server_dect_if));
	nrf_rpc_encode_bool(&rsp_ctx, net_if_is_carrier_ok(server_dect_if));
	nrf_rpc_encode_bool(&rsp_ctx, net_if_is_dormant(server_dect_if));
	/* Do not modify server dect0: no put(), down(), or set_mtu(). */
	nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
}

NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_if_get_addrs, DECT_RPC_CMD_IF_GET_ADDRS,
			 dect_rpc_cmd_if_get_addrs, NULL);

/* DECT_STATUS: optional seq (uint); response = status + associations + fw_version. */
static void dect_rpc_cmd_if_status(const struct nrf_rpc_group *group,
				   struct nrf_rpc_cbor_ctx *ctx, void *handler_data)
{
	struct dect_status_info status;
	struct nrf_rpc_cbor_ctx rsp_ctx;
	int ret;
	size_t rsp_size;
	int i, n_child;
	size_t fw_len;

	ARG_UNUSED(handler_data);

	dect_net_l2_rpc_client_set_connected(true);
	(void)nrf_rpc_decode_skip(ctx);
	/* Signal decode done so transport RX can receive next packet (ref b5dfb04). */
	nrf_rpc_decoding_done(group, ctx->in_packet);

	if (!server_dect_if) {
		server_dect_if = get_server_dect_if();
	}
	if (!server_dect_if) {
		static const uint8_t zero_addr[16];

		rsp_size = 32;
		NRF_RPC_CBOR_ALLOC(group, rsp_ctx, rsp_size);
		nrf_rpc_encode_bool(&rsp_ctx, false);
		nrf_rpc_encode_bool(&rsp_ctx, false);
		nrf_rpc_encode_uint(&rsp_ctx, 0);
		nrf_rpc_encode_bool(&rsp_ctx, false);
		nrf_rpc_encode_uint(&rsp_ctx, 0);
		nrf_rpc_encode_uint(&rsp_ctx, 0);
		nrf_rpc_encode_bool(&rsp_ctx, false);
		nrf_rpc_encode_buffer(&rsp_ctx, zero_addr, 16);
		nrf_rpc_encode_uint(&rsp_ctx, 0);
		nrf_rpc_encode_buffer(&rsp_ctx, (const uint8_t *)"", 1);
		nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
		return;
	}

	ret = net_mgmt(NET_REQUEST_DECT_STATUS_INFO_GET, server_dect_if, &status, sizeof(status));
	if (ret < 0) {
		static const uint8_t zero_addr[16];

		rsp_size = 32;
		NRF_RPC_CBOR_ALLOC(group, rsp_ctx, rsp_size);
		nrf_rpc_encode_bool(&rsp_ctx, false);
		nrf_rpc_encode_bool(&rsp_ctx, false);
		nrf_rpc_encode_uint(&rsp_ctx, 0);
		nrf_rpc_encode_bool(&rsp_ctx, false);
		nrf_rpc_encode_uint(&rsp_ctx, 0);
		nrf_rpc_encode_uint(&rsp_ctx, 0);
		nrf_rpc_encode_bool(&rsp_ctx, false);
		nrf_rpc_encode_buffer(&rsp_ctx, zero_addr, 16);
		nrf_rpc_encode_uint(&rsp_ctx, 0);
		nrf_rpc_encode_buffer(&rsp_ctx, (const uint8_t *)"", 1);
		nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
		return;
	}

	fw_len = strnlen(status.fw_version_str, sizeof(status.fw_version_str));
	n_child = status.child_count;
	if (n_child > DECT_RPC_MAX_STATUS_CHILDREN) {
		n_child = DECT_RPC_MAX_STATUS_CHILDREN;
	}
	rsp_size = 4 + 2 + 4 + 2 + 2  /* bools + channel + counts */
		   + (status.parent_count > 0 ? (4 + 16 + 1 + 16) : 0)
		   + n_child * (4 + 16 + 1 + 16)
		   + 1 + 16 + 4
		   + 4 + fw_len;

	NRF_RPC_CBOR_ALLOC(group, rsp_ctx, rsp_size);

	nrf_rpc_encode_bool(&rsp_ctx, status.mdm_activated);
	nrf_rpc_encode_bool(&rsp_ctx, status.cluster_running);
	nrf_rpc_encode_uint(&rsp_ctx, (uint32_t)status.cluster_channel);
	nrf_rpc_encode_bool(&rsp_ctx, status.nw_beacon_running);
	nrf_rpc_encode_uint(&rsp_ctx, (uint32_t)status.parent_count);

	for (i = 0; i < status.parent_count && i < 1; i++) {
		nrf_rpc_encode_uint(&rsp_ctx, status.parent_associations[i].long_rd_id);
		nrf_rpc_encode_buffer(&rsp_ctx, &status.parent_associations[i].local_ipv6_addr,
				     sizeof(struct in6_addr));
		nrf_rpc_encode_bool(&rsp_ctx, status.parent_associations[i].global_ipv6_addr_set);
		nrf_rpc_encode_buffer(&rsp_ctx, &status.parent_associations[i].global_ipv6_addr,
				     sizeof(struct in6_addr));
	}

	nrf_rpc_encode_uint(&rsp_ctx, (uint32_t)n_child);
	for (i = 0; i < n_child; i++) {
		nrf_rpc_encode_uint(&rsp_ctx, status.child_associations[i].long_rd_id);
		nrf_rpc_encode_buffer(&rsp_ctx, &status.child_associations[i].local_ipv6_addr,
				     sizeof(struct in6_addr));
		nrf_rpc_encode_bool(&rsp_ctx, status.child_associations[i].global_ipv6_addr_set);
		nrf_rpc_encode_buffer(&rsp_ctx, &status.child_associations[i].global_ipv6_addr,
				     sizeof(struct in6_addr));
	}

	nrf_rpc_encode_bool(&rsp_ctx, status.br_global_ipv6_addr_prefix_set);
	nrf_rpc_encode_buffer(&rsp_ctx, &status.br_global_ipv6_addr_prefix,
			     sizeof(struct in6_addr));
	nrf_rpc_encode_uint(&rsp_ctx, (uint32_t)status.br_global_ipv6_addr_prefix_len);
	nrf_rpc_encode_buffer(&rsp_ctx, status.fw_version_str, fw_len);

	nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
}

NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_if_status, DECT_RPC_CMD_IF_STATUS,
			 dect_rpc_cmd_if_status, NULL);

/* DECT_RPC_CMD_PING: UART test. No request payload; response = 0. */
static void dect_rpc_cmd_ping(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
			      void *handler_data)
{
	struct nrf_rpc_cbor_ctx rsp_ctx;

	ARG_UNUSED(handler_data);

	dect_net_l2_rpc_client_set_connected(true);
	(void)nrf_rpc_decode_skip(ctx);
	nrf_rpc_decoding_done(group, ctx->in_packet);

	NRF_RPC_CBOR_ALLOC(group, rsp_ctx, 4);
	nrf_rpc_encode_uint(&rsp_ctx, 0U);
	nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
}

NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_ping, DECT_RPC_CMD_PING,
			 dect_rpc_cmd_ping, NULL);

/* DECT_RPC_CMD_SHELL: request = argc (uint), argv[0]..argv[argc-1] (bstr).
 * Response = status (uint: 0 OK, 1 error/BUSY), output (bstr).
 * If CONFIG_DECT_NR_RPC_SHELL is not set, returns "DECT L2 shell not configured on server".
 * Otherwise server shell wins: if local shell holds the session lock, RPC returns BUSY.
 *
 * Stack: Runs on the nRF RPC thread pool ("rpc" threads), not on "rpc uart rx". Set
 * CONFIG_NRF_RPC_THREAD_STACK_SIZE. With CONFIG_DECT_CLUSTER_MAX_CHILD_ASSOCIATION_COUNT=10,
 * Output is flushed to RPC per line via DECT_RPC_CMD_SHELL_LINE; response is status only.
 */
static void dect_rpc_cmd_shell(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
			       void *handler_data)
{
	struct nrf_rpc_cbor_ctx rsp_ctx;
	uint32_t argc;
	const uint8_t *p;
	size_t sz;
	int i;
#if defined(CONFIG_DECT_NR_RPC_SHELL)
	char argv_bufs[DECT_RPC_SHELL_MAX_ARGC][DECT_RPC_SHELL_ARG_LEN];
	char *argv_ptrs[DECT_RPC_SHELL_MAX_ARGC];
	int ret;
#endif

	ARG_UNUSED(handler_data);

	dect_net_l2_rpc_client_set_connected(true);
	if (!nrf_rpc_decode_valid(ctx)) {
		goto err_decode;
	}
	argc = nrf_rpc_decode_uint(ctx);
	if (argc == 0 || argc > DECT_RPC_SHELL_MAX_ARGC) {
		goto err_decode;
	}
	for (i = 0; i < (int)argc; i++) {
		p = nrf_rpc_decode_buffer_ptr_and_size(ctx, &sz);
		if (!p || sz >= DECT_RPC_SHELL_ARG_LEN) {
			goto err_decode;
		}
#if defined(CONFIG_DECT_NR_RPC_SHELL)
		memcpy(argv_bufs[i], p, sz);
		argv_bufs[i][sz] = '\0';
		argv_ptrs[i] = argv_bufs[i];
#endif
	}
	nrf_rpc_decoding_done(group, ctx->in_packet);
	/* Request fully decoded; run shell (may block) then send response. */

#if !defined(CONFIG_DECT_NR_RPC_SHELL)
	/* Server not built with DECT L2 shell over RPC; return clear error. */
	NRF_RPC_CBOR_ALLOC(group, rsp_ctx, 64);
	nrf_rpc_encode_uint(&rsp_ctx, 1U);
	nrf_rpc_encode_buffer(&rsp_ctx,
			      (const uint8_t *)"DECT L2 shell not configured on server", 38);
	nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
	return;
#else
	if (dect_rpc_shell_session_try_lock() != 0) {
		/* Server (local) shell is in use; RPC returns BUSY. */
		NRF_RPC_CBOR_ALLOC(group, rsp_ctx, 32);
		nrf_rpc_encode_uint(&rsp_ctx, 1);
		nrf_rpc_encode_buffer(&rsp_ctx, (const uint8_t *)"BUSY", 4);
		nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
		return;
	}
	ret = dect_shell_exec_by_name(argv_ptrs[0], (int)argc, argv_ptrs, NULL, 0);
	dect_rpc_shell_session_unlock();
	/* Response is status only; output flushed per line via DECT_RPC_CMD_SHELL_LINE. */
	NRF_RPC_CBOR_ALLOC(group, rsp_ctx, 8);
	nrf_rpc_encode_uint(&rsp_ctx, ret == 0 ? 0U : 1U);
	nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
	return;
#endif

err_decode:
	nrf_rpc_decoding_done(group, ctx->in_packet);
	NRF_RPC_CBOR_ALLOC(group, rsp_ctx, 24);
	nrf_rpc_encode_uint(&rsp_ctx, 1U);
	nrf_rpc_encode_buffer(&rsp_ctx, (const uint8_t *)"decode error", 12);
	nrf_rpc_cbor_rsp_no_err(group, &rsp_ctx);
}

NRF_RPC_CBOR_CMD_DECODER(dect_rpc_group, dect_rpc_cmd_shell, DECT_RPC_CMD_SHELL,
			 dect_rpc_cmd_shell, NULL);

/* When IPv6 addr/prefix changes on DECT iface, notify client to re-sync. */
static void dect_rpc_server_ipv6_event_cb(struct net_mgmt_event_callback *cb,
					 uint64_t mgmt_event, struct net_if *iface)
{
	struct net_if *dect_if = server_dect_if ? server_dect_if : get_server_dect_if();

	ARG_UNUSED(cb);
	ARG_UNUSED(mgmt_event);

	if (dect_if && iface == dect_if) {
		LOG_DBG("DECT iface IPv6 change, notifying client");
		dect_rpc_server_notify_addrs_changed();
	}
}

static struct net_mgmt_event_callback dect_rpc_server_ipv6_cb;

static int dect_rpc_server_init(void)
{
	dect_net_l2_rpc_forward_register(dect_rpc_server_forward_recv);
	dect_net_l2_link_state_register(dect_rpc_server_link_state_changed);

	server_dect_if = get_server_dect_if();
	net_mgmt_init_event_callback(&dect_rpc_server_ipv6_cb,
				     dect_rpc_server_ipv6_event_cb,
				     NET_EVENT_IPV6_ADDR_ADD | NET_EVENT_IPV6_ADDR_DEL
				     | NET_EVENT_IPV6_PREFIX_ADD | NET_EVENT_IPV6_PREFIX_DEL);
	net_mgmt_add_event_callback(&dect_rpc_server_ipv6_cb);

#if defined(CONFIG_DECT_NR_RPC_SHELL)
	{
		struct dect_net_l2_shell_print_fns rpc_print_fns = {
			.print_fn = dect_rpc_shell_print_fn,
			.error_fn = dect_rpc_shell_error_fn,
			.warn_fn  = dect_rpc_shell_warn_fn,
		};

		(void)dect_net_l2_shell_init(&rpc_print_fns);
	}
#endif
	return 0;
}

SYS_INIT(dect_rpc_server_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
