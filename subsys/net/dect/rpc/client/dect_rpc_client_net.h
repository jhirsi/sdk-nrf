/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Minimal header for DECT NR+ RPC client net_if. Declares only the L2 type
 * (DECT_RPC_L2) so the client can register its DECT NR+ RPC-backed interface without
 * including the full DECT NR+ stack header.
 */

#ifndef DECT_RPC_CLIENT_NET_H_
#define DECT_RPC_CLIENT_NET_H_

#include <zephyr/kernel.h>
#include <zephyr/net/net_l2.h>

#if defined(CONFIG_DECT_NR_RPC_CLIENT)
#include <nrf_rpc.h>

/** DECT NR+ RPC client net_if L2 type. Use with NET_L2_INIT, NET_DEVICE_INIT, NET_L2_GET_NAME. */
NET_L2_DECLARE_PUBLIC(DECT_RPC_L2);

/** DECT NR+ RPC group (for nrf_rpc_set_bound_handler etc.). */
const struct nrf_rpc_group *dect_rpc_client_get_group(void);

/** Call when DECT NR+ RPC has been initialized (e.g. after shell "dect rpc init"). */
void dect_rpc_client_notify_rpc_init_done(void);

/** Block until DECT NR+ RPC init is done (e.g. user ran "dect rpc init"). Use before net_if_up. */
int dect_rpc_client_wait_rpc_init(k_timeout_t timeout);

/** Send RPC ping to server; returns 0 if pong received within timeout, else -ETIMEDOUT. */
int dect_rpc_client_ping(k_timeout_t timeout);

#if defined(CONFIG_DECT_NR_RPC_NET_IF)
#include <zephyr/net/net_if.h>
/** Sync IPv6 addrs/prefixes from server to client iface (GET_ADDRS). Run after "net iface up". */
int sync_addrs_from_server(struct net_if *iface);
#endif
#endif

#endif /* DECT_RPC_CLIENT_NET_H_ */
