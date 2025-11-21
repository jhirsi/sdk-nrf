/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_RPC_SERVER_H_
#define DECT_RPC_SERVER_H_

#include <stdint.h>
#include <zephyr/net/net_if.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Initialize DECT RPC server
 *
 * This function registers all RPC command handlers and sets up
 * event forwarding from net_mgmt to RPC events.
 *
 * @return 0 on success, negative error code on failure
 */
int dect_rpc_server_init(void);

#ifdef __cplusplus
}
#endif

#endif /* DECT_RPC_SERVER_H_ */

