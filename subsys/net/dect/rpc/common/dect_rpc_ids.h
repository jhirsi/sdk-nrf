/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_RPC_IDS_H_
#define DECT_RPC_IDS_H_

/** Command IDs received by the DECT NR+ RPC client (sent by DECT NR+ RPC server). */
enum dect_rpc_cmd_client {
	DECT_RPC_CMD_IF_RECEIVE = 0,
	DECT_RPC_CMD_IF_LINK_STATE,
	/** DECT NR+ RPC server notifies DECT NR+ RPC client to re-sync addrs/prefixes
	 * (e.g. after link or addr change).
	 */
	DECT_RPC_CMD_IF_ADDRS_CHANGED,
	/** DECT NR+ RPC server flushes one line of DECT NR+ shell output
	 * (sync flush to DECT NR+ RPC right away).
	 */
	DECT_RPC_CMD_SHELL_LINE,
};

/** Command IDs received by the DECT NR+ RPC server (sent by DECT NR+ RPC client). */
enum dect_rpc_cmd_server {
	DECT_RPC_CMD_IF_ENABLE = 0,
	DECT_RPC_CMD_IF_SEND,
	DECT_RPC_CMD_IF_GET_ADDRS,
	DECT_RPC_CMD_IF_STATUS,
	/** Run DECT NR+ L2 shell command on DECT NR+ server.
	 * DECT NR+ server response is captured output or BUSY.
	 */
	DECT_RPC_CMD_SHELL,
	/** UART test: no request payload.
	 * DECT NR+ server responds with 0. Confirms DECT NR+ link both ways.
	 */
	DECT_RPC_CMD_PING,
};

#endif /* DECT_RPC_IDS_H_ */
