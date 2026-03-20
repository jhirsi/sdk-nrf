/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Client: Zephyr net_if that forwards raw IPv6
 * over RPC. Send encodes net_pkt and calls IF_SEND; IF_RECEIVE decoder
 * pushes packets into the stack via net_recv_data(). IPv6 addresses and
 * prefixes are synced from the server (GET_ADDRS) so the client iface
 * mirrors the server DECT iface; no L2 changes required.
 */

#include "dect_rpc_ids.h"
#include "dect_rpc_common.h"
#include <nrf_rpc.h>
#include <nrf_rpc/nrf_rpc_serialize.h>
#include <nrf_rpc_cbor.h>

#include <stdbool.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>
#include <errno.h>
#include <zephyr/net/net_l2.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_linkaddr.h>

#include <zephyr/net/mld.h>
#include "dect_rpc_client_net.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(dect_rpc, CONFIG_NET_DECT_RPC_LOG_LEVEL);

#define DECT_RPC_MAX_ADDRS   8
#define DECT_RPC_MAX_PREFIXES 4

struct dect_rpc_l2_data {
};

#if defined(CONFIG_DECT_NR_RPC_ADDR_SYNC_INTERVAL_SEC)
static void dect_rpc_addr_sync_work_fn(struct k_work *work);
#endif

static enum net_verdict dect_rpc_l2_recv(struct net_if *iface, struct net_pkt *pkt)
{
	ARG_UNUSED(iface);
	ARG_UNUSED(pkt);
	return NET_CONTINUE;
}

static int dect_rpc_l2_send(struct net_if *iface, struct net_pkt *pkt)
{
	ARG_UNUSED(iface);

	bool encoded_ok = false;
	const size_t len = net_pkt_get_len(pkt);
	const size_t cbor_buffer_size = 10 + len;
	struct nrf_rpc_cbor_ctx ctx;

	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, cbor_buffer_size);
	if (!zcbor_bstr_start_encode(ctx.zs)) {
		goto out;
	}

	for (struct net_buf *buf = pkt->buffer; buf; buf = buf->frags) {
		memcpy(ctx.zs[0].payload_mut, buf->data, buf->len);
		ctx.zs->payload_mut += buf->len;
	}

	if (!zcbor_bstr_end_encode(ctx.zs, NULL)) {
		goto out;
	}

	LOG_DBG("RPC send %zu bytes -> server", len);
	LOG_DBG("Sending IPv6 packet to RPC server (%zu bytes)", len);
	encoded_ok = true;
	nrf_rpc_cbor_cmd_no_err(&dect_rpc_group, DECT_RPC_CMD_IF_SEND, &ctx,
				dect_rpc_decode_void, NULL);

out:
	if (!encoded_ok) {
		LOG_ERR("RPC send: failed to encode packet");
	}
	net_pkt_unref(pkt);
	return (int)len;
}

static int dect_rpc_l2_enable(struct net_if *iface, bool state)
{
	if (state && iface) {
		/* No GET_ADDRS here (OT pattern: L2 enable does not block on server RPC). */
#if defined(CONFIG_DECT_NR_RPC_ADDR_SYNC_INTERVAL_SEC)
		{
			static struct k_work_delayable addr_sync_work;
			static bool work_initialized;

			if (!work_initialized) {
				k_work_init_delayable(&addr_sync_work, dect_rpc_addr_sync_work_fn);
				work_initialized = true;
			}
			k_work_reschedule(&addr_sync_work,
					 K_SECONDS(CONFIG_DECT_NR_RPC_ADDR_SYNC_INTERVAL_SEC));
		}
#endif
	}
	return 0;
}

static enum net_l2_flags dect_rpc_l2_flags(struct net_if *iface)
{
	ARG_UNUSED(iface);
	return NET_L2_MULTICAST | NET_L2_MULTICAST_SKIP_JOIN_SOLICIT_NODE;
}

#if IS_ENABLED(CONFIG_MDNS_RESPONDER) || IS_ENABLED(CONFIG_MDNS_RESOLVER)
static void dect_rpc_mdns_join_schedule_if_ready(bool carrier_ok, bool dormant);
#endif
/* Decode GET_ADDRS response and mirror to client iface (addrs, prefixes, MTU, carrier, dormant). */
static bool apply_addrs_from_ctx(struct net_if *iface, struct nrf_rpc_cbor_ctx *ctx)
{
	struct net_if_ipv6 *ipv6;
	uint32_t n_addrs, n_prefixes;
	const uint8_t *p;
	size_t sz;
	struct in6_addr addr;
	uint32_t prefix_len;
	int i;

	if (!iface || net_if_config_ipv6_get(iface, &ipv6) < 0) {
		return false;
	}

	/* Remove all current unicast addresses and prefixes */
	for (i = 0; i < NET_IF_MAX_IPV6_ADDR; i++) {
		if (ipv6->unicast[i].is_used) {
			net_if_ipv6_addr_rm(iface, &ipv6->unicast[i].address.in6_addr);
		}
	}
	for (i = 0; i < NET_IF_MAX_IPV6_PREFIX; i++) {
		if (ipv6->prefix[i].is_used) {
			net_if_ipv6_prefix_rm(iface, &ipv6->prefix[i].prefix, ipv6->prefix[i].len);
		}
	}

	if (!nrf_rpc_decode_valid(ctx)) {
		/* Do not put(): keep config attached so interface retains IPv6 */
		return false;
	}

	n_addrs = nrf_rpc_decode_uint(ctx);
	for (i = 0; i < (int)n_addrs && i < DECT_RPC_MAX_ADDRS; i++) {
		p = nrf_rpc_decode_buffer_ptr_and_size(ctx, &sz);
		if (p && sz == sizeof(struct in6_addr)) {
			memcpy(&addr, p, sizeof(addr));
			net_if_ipv6_addr_add(iface, &addr, NET_ADDR_AUTOCONF, 0);
		}
	}
	n_prefixes = nrf_rpc_decode_uint(ctx);
	for (i = 0; i < (int)n_prefixes && i < DECT_RPC_MAX_PREFIXES; i++) {
		p = nrf_rpc_decode_buffer_ptr_and_size(ctx, &sz);
		if (p && sz == sizeof(struct in6_addr)) {
			memcpy(&addr, p, sizeof(addr));
			prefix_len = nrf_rpc_decode_uint(ctx);
			if (prefix_len <= 128) {
				net_if_ipv6_prefix_add(iface, &addr, (uint8_t)prefix_len, 0);
			}
		}
	}
	/* MTU: mirror server dect0 to client iface only */
	if (nrf_rpc_decode_valid(ctx)) {
		uint32_t mtu = nrf_rpc_decode_uint(ctx);

		if (mtu > 0 && mtu <= 0xFFFF) {
			net_if_set_mtu(iface, (uint16_t)mtu);
		}
	}
	/* Carrier and dormant: mirror server link state */
	if (nrf_rpc_decode_valid(ctx)) {
		bool carrier_ok = nrf_rpc_decode_bool(ctx);

		if (nrf_rpc_decode_valid(ctx)) {
			bool dormant = nrf_rpc_decode_bool(ctx);

			if (carrier_ok) {
				net_if_carrier_on(iface);
			} else {
				net_if_carrier_off(iface);
			}
			if (dormant) {
				net_if_dormant_on(iface);
			} else {
				net_if_dormant_off(iface);
			}
#if IS_ENABLED(CONFIG_MDNS_RESPONDER) || IS_ENABLED(CONFIG_MDNS_RESOLVER)
			dect_rpc_mdns_join_schedule_if_ready(carrier_ok, dormant);
#endif
		}
	}

	/* Do not net_if_config_ipv6_put(): put() detaches config and clears iface->config.ip.ipv6,
	 * which makes the interface show "IPv6 not enabled". Keep config attached with new addrs.
	 */
	LOG_DBG("Synced %u addrs, %u prefixes from server", n_addrs, n_prefixes);
	return true;
}

/* Unique request payload per GET_ADDRS so CRC differs (avoids UART duplicate drop). */
static uint32_t dect_rpc_get_addrs_seq;

/* Ref pattern: synchronous cmd_rsp, decode in place, then apply (like status). */
int sync_addrs_from_server(struct net_if *iface)
{
	struct nrf_rpc_cbor_ctx ctx;
	int err;

	if (!iface) {
		return -EINVAL;
	}

	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 8);
	nrf_rpc_encode_uint(&ctx, dect_rpc_get_addrs_seq++);

	err = nrf_rpc_cbor_cmd_rsp(&dect_rpc_group, DECT_RPC_CMD_IF_GET_ADDRS, &ctx);
	if (err < 0) {
		return err;
	}

	if (!nrf_rpc_decode_valid(&ctx)) {
		return -EIO;
	}

	if (!apply_addrs_from_ctx(iface, &ctx)) {
		return -EIO;
	}

	if (!nrf_rpc_decoding_done_and_check(&dect_rpc_group, &ctx)) {
		return -EIO;
	}

	return 0;
}

#if defined(CONFIG_DECT_NR_RPC_AUTO_SYNC)
static void dect_rpc_auto_sync_work_fn(struct k_work *work)
{
	struct net_if *iface = net_if_get_first_by_type(&NET_L2_GET_NAME(DECT_RPC_L2));

	ARG_UNUSED(work);
	if (!iface) {
		LOG_INF("DECT RPC: no interface for auto-sync");
		return;
	}
	/* Sync even when interface is admin down (NO_AUTO_START); addresses/MTU/status apply.
	 * After first successful sync, bring the interface up so it effectively "auto-starts"
	 * once the server has synced (no need for the user to run "net iface up").
	 */
	if (sync_addrs_from_server(iface) == 0) {
		struct net_if_ipv6 *ipv6;
		char buf[NET_INET6_ADDRSTRLEN];
		bool logged_first = false;

		if (net_if_config_ipv6_get(iface, &ipv6) >= 0) {
			for (int i = 0; i < NET_IF_MAX_IPV6_ADDR; i++) {
				if (ipv6->unicast[i].is_used &&
				    net_addr_ntop(NET_AF_INET6,
						  &ipv6->unicast[i].address.in6_addr,
						  buf, sizeof(buf))) {
					LOG_DBG("DECT RPC: auto-synced addresses from server, "
						"first: %s",
						buf);
					logged_first = true;
					break;
				}
			}
			/* do not put(): keep config attached */
		}
		if (!logged_first) {
			LOG_DBG("DECT RPC: auto-synced addresses from server");
		}
		/* Bring iface up after first successful sync (idempotent if already up). */
		if (net_if_up(iface) == 0) {
			LOG_DBG("DECT RPC: interface up after sync");
		}
	} else {
		LOG_WRN("DECT RPC: auto-sync failed, run 'dect sync' manually");
	}
}

static K_WORK_DELAYABLE_DEFINE(dect_rpc_auto_sync_work, dect_rpc_auto_sync_work_fn);
#endif /* CONFIG_DECT_NR_RPC_AUTO_SYNC */

#if defined(CONFIG_DECT_NR_RPC_ADDR_SYNC_INTERVAL_SEC)
static void dect_rpc_addr_sync_work_fn(struct k_work *work)
{
	struct net_if *iface = net_if_get_first_by_type(&NET_L2_GET_NAME(DECT_RPC_L2));

	if (iface && net_if_is_up(iface)) {
		sync_addrs_from_server(iface);
	}
	if (CONFIG_DECT_NR_RPC_ADDR_SYNC_INTERVAL_SEC > 0) {
		k_work_reschedule((struct k_work_delayable *)work,
				  K_SECONDS(CONFIG_DECT_NR_RPC_ADDR_SYNC_INTERVAL_SEC));
	}
}
#endif /* CONFIG_DECT_NR_RPC_ADDR_SYNC_INTERVAL_SEC */

NET_L2_INIT(DECT_RPC_L2, dect_rpc_l2_recv, dect_rpc_l2_send, dect_rpc_l2_enable, dect_rpc_l2_flags);

static int dect_rpc_dev_init(const struct device *dev)
{
	ARG_UNUSED(dev);
	return 0;
}

/* Dummy link address so net_if_up() does not assert (link_addr->len > 0). */
static const uint8_t dect_rpc_lladdr[6] = { 0x02, 0x00, 0x5e, 0x00, 0x52, 0x01 };

static void dect_rpc_if_init(struct net_if *iface)
{
	net_if_flag_set(iface, NET_IF_NO_AUTO_START);
	net_if_flag_set(iface, NET_IF_IPV6_NO_ND);
#if !defined(CONFIG_NET_IPV6_MLD)
	net_if_flag_set(iface, NET_IF_IPV6_NO_MLD);
#endif
	net_if_set_link_addr(iface, (uint8_t *)dect_rpc_lladdr, sizeof(dect_rpc_lladdr),
			     NET_LINK_DUMMY);
#if defined(CONFIG_NET_INTERFACE_NAME)
	{
		int name_err = net_if_set_name(iface, CONFIG_DECT_NR_RPC_CLIENT_IFACE_NAME);

		if (name_err < 0) {
			LOG_ERR("net_if_set_name(\"%s\") failed (%d); "
				"check CONFIG_NET_INTERFACE_NAME_LEN",
				CONFIG_DECT_NR_RPC_CLIENT_IFACE_NAME, name_err);
		}
	}
#endif
}

static struct net_if_api dect_rpc_if_api = {
	.init = dect_rpc_if_init,
};

NET_DEVICE_INIT(dect_rpc, CONFIG_DECT_NR_RPC_CLIENT_IFACE_NAME, dect_rpc_dev_init, NULL, NULL, NULL,
		CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &dect_rpc_if_api, DECT_RPC_L2,
		struct dect_rpc_l2_data, 1280);

#if !defined(CONFIG_NRF_RPC_INIT)
static K_SEM_DEFINE(rpc_init_done_sem, 0, 1);
#endif

const struct nrf_rpc_group *dect_rpc_client_get_group(void)
{
	return &dect_rpc_group;
}

void dect_rpc_client_notify_rpc_init_done(void)
{
#if !defined(CONFIG_NRF_RPC_INIT)
	k_sem_give(&rpc_init_done_sem);
#endif
#if defined(CONFIG_DECT_NR_RPC_AUTO_SYNC)
	k_work_schedule(&dect_rpc_auto_sync_work,
			K_SECONDS(CONFIG_DECT_NR_RPC_AUTO_SYNC_DELAY_SEC));
#endif
}

int dect_rpc_client_wait_rpc_init(k_timeout_t timeout)
{
#if !defined(CONFIG_NRF_RPC_INIT)
	return k_sem_take(&rpc_init_done_sem, timeout);
#else
	ARG_UNUSED(timeout);
	return 0;
#endif
}

static K_SEM_DEFINE(ping_rsp_sem, 0, 1);

static void ping_rsp_handler(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
			     void *handler_data)
{
	ARG_UNUSED(handler_data);
	if (nrf_rpc_decode_valid(ctx)) {
		(void)nrf_rpc_decode_skip(ctx);
	}
	k_sem_give(&ping_rsp_sem);
}

static uint32_t dect_rpc_ping_seq;

#define DECT_RPC_PING_PRE_MS   250
#define DECT_RPC_PING_ATTEMPTS 3
#define DECT_RPC_PING_RETRY_MS 400

int dect_rpc_client_ping(k_timeout_t timeout)
{
	struct nrf_rpc_cbor_ctx req_ctx;
	int ret;
	int attempt;

	k_sem_take(&ping_rsp_sem, K_NO_WAIT);
	k_msleep(DECT_RPC_PING_PRE_MS);
	for (attempt = 0; attempt < DECT_RPC_PING_ATTEMPTS; attempt++) {
		if (attempt > 0) {
			k_msleep(DECT_RPC_PING_RETRY_MS);
		}
		NRF_RPC_CBOR_ALLOC(&dect_rpc_group, req_ctx, 8);
		nrf_rpc_encode_uint(&req_ctx, dect_rpc_ping_seq++);
		ret = nrf_rpc_cbor_cmd(&dect_rpc_group, DECT_RPC_CMD_PING, &req_ctx,
				       ping_rsp_handler, NULL);
		if (ret == 0) {
			return k_sem_take(&ping_rsp_sem, timeout) == 0 ? 0 : -ETIMEDOUT;
		}
	}
	return -EIO;
}

/* Decoder for IF_RECEIVE: server sends raw IPv6 packet to client (event; server uses event
 * because command would have dst=UNKNOWN and client would run event path, so no RSP).
 */
static void dect_rpc_cmd_if_receive(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
				     void *handler_data)
{
	const uint8_t *pkt_data;
	size_t pkt_data_len = 0;
	struct net_if *iface;
	struct net_pkt *pkt = NULL;

	pkt_data = nrf_rpc_decode_buffer_ptr_and_size(ctx, &pkt_data_len);
	if (pkt_data && pkt_data_len > 0) {
		iface = net_if_get_first_by_type(&NET_L2_GET_NAME(DECT_RPC_L2));
		if (iface) {
			pkt = net_pkt_rx_alloc_with_buffer(iface, pkt_data_len, NET_AF_UNSPEC, 0,
							   K_NO_WAIT);
		} else {
			LOG_ERR("No DECT RPC net interface");
		}

		if (pkt) {
			net_pkt_write(pkt, pkt_data, pkt_data_len);
		} else {
			LOG_ERR("Failed to allocate net_pkt for RPC receive");
		}
	}

	/* Release RX buffer early so UART rx_work can process the next frame (ACK when reliable) */
	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_IF_RECEIVE);
		nrf_rpc_decoding_done(group, ctx->in_packet);
		if (pkt) {
			net_pkt_unref(pkt);
		}
		return;
	}

	if (pkt) {
		LOG_DBG("RPC recv %zu bytes -> net stack", pkt_data_len);
		LOG_DBG("Passing IPv6 packet to Zephyr net stack (%zu bytes)", pkt_data_len);
		if (net_recv_data(iface, pkt) < 0) {
			LOG_ERR("RPC recv %zu bytes: net_recv_data failed (drop)", pkt_data_len);
			net_pkt_unref(pkt);
		}
	}
}

NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_cmd_if_receive, DECT_RPC_CMD_IF_RECEIVE,
			 dect_rpc_cmd_if_receive, NULL);

static void dect_rpc_net_join_ipv6_mdns_group(struct net_if *iface)
{
	struct in6_addr ipv6mr_multiaddr;
	int ret;

#if !IS_ENABLED(CONFIG_MDNS_RESPONDER) && !IS_ENABLED(CONFIG_MDNS_RESOLVER)
	return;
#endif

	/* Well known IPv6 ff02::fb address */
	net_ipv6_addr_create(&ipv6mr_multiaddr, 0xff02, 0, 0, 0, 0, 0, 0, 0x00fb);

	ret = net_ipv6_mld_join(iface, &ipv6mr_multiaddr);
	if (ret < 0 && ret != -EALREADY) {
		LOG_ERR("Iface %p, cannot add/join mDNS (ff02::fb) (%d)", iface, ret);
	}
}

#if IS_ENABLED(CONFIG_MDNS_RESPONDER) || IS_ENABLED(CONFIG_MDNS_RESOLVER)
static void dect_rpc_mdns_join_work_fn(struct k_work *work)
{
	struct net_if *iface = net_if_get_first_by_type(&NET_L2_GET_NAME(DECT_RPC_L2));

	ARG_UNUSED(work);

	if (iface == NULL) {
		return;
	}

	/* Defer MLD join out of the nRF RPC handler; net_if_is_up() is set after carrier/dormant
	 * updates have run through update_operational_state().
	 */
	if (net_if_is_up(iface)) {
		dect_rpc_net_join_ipv6_mdns_group(iface);
	}
}

static K_WORK_DELAYABLE_DEFINE(dect_rpc_mdns_join_work, dect_rpc_mdns_join_work_fn);

static void dect_rpc_mdns_join_schedule_if_ready(bool carrier_ok, bool dormant)
{
	if (carrier_ok && !dormant) {
		(void)k_work_schedule(&dect_rpc_mdns_join_work, K_NO_WAIT);
	} else {
		(void)k_work_cancel_delayable(&dect_rpc_mdns_join_work);
	}
}
#endif /* MDNS_RESPONDER || MDNS_RESOLVER */

/* IF_LINK_STATE: server pushes carrier/dormant so client iface mirrors server (event). */
static void dect_rpc_cmd_if_link_state(const struct nrf_rpc_group *group,
				       struct nrf_rpc_cbor_ctx *ctx, void *handler_data)
{
	struct net_if *iface;
	bool carrier_ok;
	bool dormant;

	ARG_UNUSED(handler_data);
	if (!nrf_rpc_decode_valid(ctx)) {
		nrf_rpc_decoding_done(group, ctx->in_packet);
		return;
	}
	carrier_ok = nrf_rpc_decode_bool(ctx);
	if (!nrf_rpc_decode_valid(ctx)) {
		nrf_rpc_decoding_done(group, ctx->in_packet);
		return;
	}
	dormant = nrf_rpc_decode_bool(ctx);
	/* Release RX buffer so UART rx_work can process the next frame (ACK when reliable). */
	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		dect_rpc_report_cmd_decoding_error(DECT_RPC_CMD_IF_LINK_STATE);
		nrf_rpc_decoding_done(group, ctx->in_packet);
		return;
	}
	iface = net_if_get_first_by_type(&NET_L2_GET_NAME(DECT_RPC_L2));
	if (iface) {
		if (carrier_ok) {
			net_if_carrier_on(iface);
		} else {
			net_if_carrier_off(iface);
		}
		if (dormant) {
			net_if_dormant_on(iface);
		} else {
			net_if_dormant_off(iface);
		}
#if IS_ENABLED(CONFIG_MDNS_RESPONDER) || IS_ENABLED(CONFIG_MDNS_RESOLVER)
		dect_rpc_mdns_join_schedule_if_ready(carrier_ok, dormant);
#endif
		LOG_DBG("Link state from server: carrier=%d dormant=%d", carrier_ok, dormant);
	}
}

NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_cmd_if_link_state, DECT_RPC_CMD_IF_LINK_STATE,
			 dect_rpc_cmd_if_link_state, NULL);

/* Server notifies client that addresses/link changed; client re-syncs (event). */
static void dect_rpc_cmd_if_addrs_changed(const struct nrf_rpc_group *group,
					  struct nrf_rpc_cbor_ctx *ctx,
					  void *handler_data)
{
	struct net_if *iface;

	ARG_UNUSED(handler_data);
	(void)nrf_rpc_decode_skip(ctx);
	nrf_rpc_decoding_done(group, ctx->in_packet);

	iface = net_if_get_first_by_type(&NET_L2_GET_NAME(DECT_RPC_L2));
	if (iface && net_if_is_up(iface)) {
		if (sync_addrs_from_server(iface) == 0) {
			LOG_INF("DECT RPC: re-synced addresses (server notified change)");
		}
	}
}

NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_cmd_if_addrs_changed,
			 DECT_RPC_CMD_IF_ADDRS_CHANGED, dect_rpc_cmd_if_addrs_changed, NULL);

/* Server flushes DECT shell output per line (event, no response). */
static void dect_rpc_cmd_shell_line(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
				    void *handler_data)
{
	const uint8_t *p;
	size_t sz;
	const struct shell *shell;
	char line_buf[256];
	size_t copy_sz = 0;

	ARG_UNUSED(handler_data);
	if (!nrf_rpc_decode_valid(ctx)) {
		nrf_rpc_decoding_done(group, ctx->in_packet);
		return;
	}
	p = nrf_rpc_decode_buffer_ptr_and_size(ctx, &sz);
	if (p != NULL && sz > 0) {
		copy_sz = sz;
		if (copy_sz >= sizeof(line_buf)) {
			copy_sz = sizeof(line_buf) - 1;
		}
		memcpy(line_buf, p, copy_sz);
		line_buf[copy_sz] = '\0';
	}
	/* Release RX buffer so UART rx_work can process the next frame (ACK when reliable). */
	if (!nrf_rpc_decoding_done_and_check(group, ctx)) {
		nrf_rpc_decoding_done(group, ctx->in_packet);
		return;
	}
	if (p != NULL && sz > 0) {
		shell = shell_backend_uart_get_ptr();
		if (shell != NULL) {
			shell_fprintf(shell, SHELL_NORMAL, "%s", line_buf);
			if (copy_sz > 0 && line_buf[copy_sz - 1] != '\n') {
				shell_fprintf(shell, SHELL_NORMAL, "\n");
			}
		} else {
			printk("%s", line_buf);
			if (copy_sz > 0 && line_buf[copy_sz - 1] != '\n') {
				printk("\n");
			}
		}
	}
}

NRF_RPC_CBOR_EVT_DECODER(dect_rpc_group, dect_rpc_cmd_shell_line, DECT_RPC_CMD_SHELL_LINE,
			 dect_rpc_cmd_shell_line, NULL);

