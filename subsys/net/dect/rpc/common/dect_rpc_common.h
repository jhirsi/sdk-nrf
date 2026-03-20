/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_RPC_COMMON_H_
#define DECT_RPC_COMMON_H_

#include <nrf_rpc_cbor.h>

/** Shell-over-DECT NR+ RPC: match L2 DECT shell capacity .
 *  L2 "sett" subcommand uses 1..19 args; others use up to 8 (scan, tx, rx, etc.).
 */
#define DECT_RPC_SHELL_MAX_ARGC      20   /* subcmd + 19 args (sett) */
#define DECT_RPC_SHELL_ARG_LEN       96   /* max length per DECT NR+ argv string */
#define DECT_RPC_SHELL_OUTPUT_LEN    1024 /* max captured output from DECT NR+ server */

NRF_RPC_GROUP_DECLARE(dect_rpc_group);

void dect_rpc_decode_void(const struct nrf_rpc_group *group, struct nrf_rpc_cbor_ctx *ctx,
			  void *handler_data);
void dect_rpc_report_cmd_decoding_error(uint8_t cmd_evt_id);

#endif /* DECT_RPC_COMMON_H_ */
