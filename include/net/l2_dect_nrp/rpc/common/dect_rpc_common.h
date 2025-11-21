/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_RPC_COMMON_H_
#define DECT_RPC_COMMON_H_

#include <stdint.h>
#include <zephyr/net/net_if.h>
#include <nrf_rpc_cbor.h>
#include "dect_rpc_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Get network interface by index
 *
 * @param iface_index Network interface index
 * @return Pointer to net_if, or NULL if not found
 */
struct net_if *dect_rpc_get_iface_by_index(int iface_index);

/** @brief Get network interface index
 *
 * @param iface Network interface pointer
 * @return Interface index, or -1 if invalid
 */
int dect_rpc_get_iface_index(struct net_if *iface);

/** @brief Report RPC decoding error
 *
 * @param cmd_id Command ID that failed
 */
void dect_rpc_report_cmd_decoding_error(enum dect_rpc_cmd_server cmd_id);

/** @brief Report RPC response decoding error
 *
 * @param cmd_id Command ID that failed
 */
void dect_rpc_report_rsp_decoding_error(enum dect_rpc_cmd_server cmd_id);

#ifdef __cplusplus
}
#endif

#endif /* DECT_RPC_COMMON_H_ */

