/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief DECT NR+ RPC client — public bring-up hooks.
 *
 * Available when @kconfig{CONFIG_DECT_NR_RPC_CLIENT} is enabled.
 */

#ifndef DECT_RPC_H_
#define DECT_RPC_H_

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup dect_rpc DECT NR+ RPC
 *
 * @brief DECT NR+ RPC client bring-up API.
 *
 * @details Normal use is the client ``net_if``
 * (see :kconfig:option:`CONFIG_DECT_NR_RPC_CLIENT_IFACE_NAME`) and ``dect`` shell commands.
 * This header adds hooks when the application calls
 * @c nrf_rpc_init() itself instead of @kconfig{CONFIG_NRF_RPC_INIT}.
 *
 * @warning DECT NR+ RPC is experimental (@kconfig{CONFIG_DECT_NR_RPC} selects
 * @kconfig{CONFIG_EXPERIMENTAL}). See :ref:`dect_rpc` for maturity and protocol details.
 *
 * @{
 */

#if defined(CONFIG_DECT_NR_RPC_CLIENT)

#include <nrf_rpc.h>

/**
 * @brief Get the ``dect_rpc`` nRF RPC group.
 *
 * @return Pointer to the DECT NR+ RPC group (for @c NRF_RPC_GROUP_STATUS()).
 */
const struct nrf_rpc_group *dect_rpc_client_get_group(void);

/**
 * @brief Tell the client subsystem that RPC to the server is ready.
 *
 * Call after @c nrf_rpc_init() and when the group is bound. With
 * @kconfig{CONFIG_DECT_NR_RPC_AUTO_SYNC}, schedules the first address sync from the server.
 */
void dect_rpc_client_notify_rpc_init_done(void);


/**
 * @brief UART RPC loopback test to the server (same as ``dect rpc ping`` shell).
 *
 * @param timeout Maximum time to wait for the command response per attempt.
 *
 */

#endif /* CONFIG_DECT_NR_RPC_CLIENT */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* DECT_RPC_H_ */
