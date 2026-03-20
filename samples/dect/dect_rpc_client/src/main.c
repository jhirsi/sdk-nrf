/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * DECT NR+ RPC client: exposes a DECT net_if that forwards IPv6 over UART to the dect RPC server.
 * Optional mdns.conf advertises hostname via mDNS.
 *
 */

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>

#include <zephyr/logging/log.h>
#include <nrf_rpc.h>

#if defined(CONFIG_DECT_NR_RPC_CLIENT)
#include "dect_rpc_client_net.h"
#endif

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#if defined(CONFIG_DECT_NR_RPC_CLIENT) && !defined(CONFIG_NRF_RPC_INIT)
static void rpc_err_handler(const struct nrf_rpc_err_report *report)
{
	LOG_ERR("RPC error: %d", report->code);
}

static K_SEM_DEFINE(rpc_bound_sem, 0, 1);

static void bound_handler(const struct nrf_rpc_group *group)
{
	if (group == dect_rpc_client_get_group()) {
		k_sem_give(&rpc_bound_sem);
	}
}
#endif

int main(void)
{
#if defined(CONFIG_DECT_NR_RPC_CLIENT) && !defined(CONFIG_NRF_RPC_INIT)
	nrf_rpc_set_bound_handler(bound_handler);
	LOG_INF("Initializing RPC...");
	int err;

	err = nrf_rpc_init(rpc_err_handler);
	if (err != 0) {
		LOG_ERR("nrf_rpc_init failed: %d", err);
		return err;
	}
	/* Wait until server sends group init (same idea as OpenThread: know when peer is ready). */
	LOG_INF("Waiting for server... (start server now)");
	if (k_sem_take(&rpc_bound_sem, K_MSEC(15000)) != 0) {
		LOG_ERR("Timeout waiting for RPC group (server not connected?)");
		return -ETIMEDOUT;
	}
	LOG_INF("RPC initialized (server ready)");
	dect_rpc_client_notify_rpc_init_done();
#endif

	if (net_if_get_by_name(CONFIG_DECT_NR_RPC_CLIENT_IFACE_NAME) < 0) {
		LOG_ERR("No DECT RPC interface named \"%s\" (CONFIG_NET_INTERFACE_NAME?)",
			CONFIG_DECT_NR_RPC_CLIENT_IFACE_NAME);
		return -ENOENT;
	}

	/* Do not call net_if_up() here as using DECT_RPC_L2 flag NET_IF_NO_AUTO_START */
	LOG_INF("DECT RPC client running. Interface is down; bring up with: net if up %s",
		CONFIG_DECT_NR_RPC_CLIENT_IFACE_NAME);
#if IS_ENABLED(CONFIG_NET_HOSTNAME_ENABLE)
	LOG_INF("mDNS name: %s.local (resolve from server or other hosts on link)",
		CONFIG_NET_HOSTNAME);
#endif

	return 0;
}
