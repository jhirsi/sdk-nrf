/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * Client shell: "dect <subcmd> [args...]" runs the DECT NR+ L2 shell on the server
 * over DECT NR+ RPC (DECT_RPC_CMD_SHELL). If the server's local shell is in use, the
 * server returns BUSY. "dect status" also available via dedicated DECT NR+ STATUS RPC.
 * "dect sync" is available via dedicated DECT NR+ SYNC RPC.
 */

#include "dect_rpc_ids.h"
#include "dect_rpc_common.h"
#include "dect_rpc_client_net.h"
#include <nrf_rpc_cbor.h>
#include <nrf_rpc/nrf_rpc_serialize.h>
#include <nrf_rpc.h>

#include <zephyr/kernel.h>
#include <errno.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_backend.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_if.h>

#include <stdio.h>
#include <string.h>

#define DECT_RPC_MAX_STATUS_CHILDREN 50

static void in6_addr_to_str(const uint8_t *addr, char *buf, size_t buf_len)
{
	snprintf(buf, buf_len,
		 "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
		 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],
		 addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]);
}

/* Run DECT NR+ L2 shell command on server: dect run <subcmd> [args...]. Sync pattern (cmd_rsp). */
static int cmd_dect_shell(const struct shell *shell, size_t argc, char **argv)
{
	struct nrf_rpc_cbor_ctx ctx;
	size_t i;
	size_t n_args;
	size_t req_size = 8;
	int err;
	uint32_t status;

	/* Subcommand handler receives argv[0]=run, argv[1]=subcmd, argv[2..]=args */
	if (argc < 2) {
		shell_error(shell, "Usage: dect run <subcmd> [args...] "
			    "(e.g. dect run status, dect run nw_join)");
		return -EINVAL;
	}
	n_args = argc - 1; /* subcmd + optional args to send to server */
	if (n_args > DECT_RPC_SHELL_MAX_ARGC) {
		shell_error(shell, "Too many args (max %d)", DECT_RPC_SHELL_MAX_ARGC);
		return -EINVAL;
	}
	for (i = 1; i < argc; i++) {
		req_size += 4 + strlen(argv[i]) + 1;
	}

	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, req_size);
	nrf_rpc_encode_uint(&ctx, (uint32_t)n_args);
	for (i = 1; i < argc; i++) {
		nrf_rpc_encode_buffer(&ctx, (const uint8_t *)argv[i], strlen(argv[i]) + 1);
	}

	err = nrf_rpc_cbor_cmd_rsp(&dect_rpc_group, DECT_RPC_CMD_SHELL, &ctx);
	if (err < 0) {
		shell_error(shell, "RPC send failed");
		return -EIO;
	}

	if (!nrf_rpc_decode_valid(&ctx)) {
		shell_error(shell, "Invalid shell response");
		return -EIO;
	}
	status = nrf_rpc_decode_uint(&ctx);
	/* Output is flushed per line via DECT NR+ RPC_CMD_SHELL_LINE; response is status only. */
	if (status != 0) {
		shell_warn(shell, "DECT NR+ Server returned status %u", (unsigned int)status);
	}

	if (!nrf_rpc_decoding_done_and_check(&dect_rpc_group, &ctx)) {
		shell_error(shell, "DECT NR+ Shell response decode error");
		return -EIO;
	}
	return 0;
}

/* Decode and print status response (shared by sync path). */
static void decode_and_print_status(const struct shell *shell, struct nrf_rpc_cbor_ctx *ctx)
{
	uint32_t parent_count, child_count;
	uint32_t long_rd_id;
	const uint8_t *p;
	size_t sz;
	char addr_str[40];
	int i;

	shell_fprintf(shell, SHELL_NORMAL, "DECT NR+ status (from DECT NR+ RPC server):\n");

	shell_fprintf(shell, SHELL_NORMAL, "  Modem activated:              %s\n",
		      nrf_rpc_decode_bool(ctx) ? "yes" : "no");
	shell_fprintf(shell, SHELL_NORMAL, "  Cluster running:              %s\n",
		      nrf_rpc_decode_bool(ctx) ? "yes" : "no");
	shell_fprintf(shell, SHELL_NORMAL, "  Cluster channel:              %u\n",
		      nrf_rpc_decode_uint(ctx));
	shell_fprintf(shell, SHELL_NORMAL, "  Network beacon running:       %s\n",
		      nrf_rpc_decode_bool(ctx) ? "yes" : "no");

	parent_count = nrf_rpc_decode_uint(ctx);

	if (parent_count > 0) {
		shell_fprintf(shell, SHELL_NORMAL, "  Associations:\n");
		for (i = 0; i < (int)parent_count && i < 1; i++) {
			long_rd_id = nrf_rpc_decode_uint(ctx);
			shell_fprintf(shell, SHELL_NORMAL,
				     "    Parent long RD ID:              %u (0x%08x)\n",
				     (unsigned int)long_rd_id, (unsigned int)long_rd_id);
			p = nrf_rpc_decode_buffer_ptr_and_size(ctx, &sz);
			if (p && sz >= 16) {
				in6_addr_to_str(p, addr_str, sizeof(addr_str));
				shell_fprintf(shell, SHELL_NORMAL,
					     "      Local IPv6 address:           %s\n", addr_str);
			}
			if (nrf_rpc_decode_bool(ctx)) {
				p = nrf_rpc_decode_buffer_ptr_and_size(ctx, &sz);
				if (p && sz >= 16) {
					in6_addr_to_str(p, addr_str, sizeof(addr_str));
					shell_fprintf(shell, SHELL_NORMAL,
						     "      Global IPv6 address:          %s\n",
						     addr_str);
				}
			} else {
				(void)nrf_rpc_decode_skip(ctx);
			}
		}
	}

	child_count = nrf_rpc_decode_uint(ctx);
	if (child_count > 0 && parent_count == 0) {
		shell_fprintf(shell, SHELL_NORMAL, "  Associations:\n");
	}
	for (i = 0; i < (int)child_count && i < DECT_RPC_MAX_STATUS_CHILDREN; i++) {
		long_rd_id = nrf_rpc_decode_uint(ctx);
		shell_fprintf(shell, SHELL_NORMAL, "    Child #%d long RD ID:       %u (0x%08x)\n",
			      i + 1, (unsigned int)long_rd_id, (unsigned int)long_rd_id);
		p = nrf_rpc_decode_buffer_ptr_and_size(ctx, &sz);
		if (p && sz >= 16) {
			in6_addr_to_str(p, addr_str, sizeof(addr_str));
			shell_fprintf(shell, SHELL_NORMAL,
				     "      Local IPv6 address:       %s\n", addr_str);
		}
		if (nrf_rpc_decode_bool(ctx)) {
			p = nrf_rpc_decode_buffer_ptr_and_size(ctx, &sz);
			if (p && sz >= 16) {
				in6_addr_to_str(p, addr_str, sizeof(addr_str));
				shell_fprintf(shell, SHELL_NORMAL,
					     "      Global IPv6 address:      %s\n", addr_str);
			}
		} else {
			(void)nrf_rpc_decode_skip(ctx);
		}
	}

	if (nrf_rpc_decode_bool(ctx)) {
		p = nrf_rpc_decode_buffer_ptr_and_size(ctx, &sz);
		if (p && sz >= 16) {
			uint32_t prefix_len = nrf_rpc_decode_uint(ctx);

			in6_addr_to_str(p, addr_str, sizeof(addr_str));
			shell_fprintf(shell, SHELL_NORMAL,
				     "  Border router global IPv6 prefix/%u: %s\n",
				     (unsigned int)(prefix_len * 8), addr_str);
			shell_fprintf(shell, SHELL_NORMAL,
				     "    Connection to Internet should be available.\n");
		}
	} else {
		(void)nrf_rpc_decode_skip(ctx);
		(void)nrf_rpc_decode_skip(ctx);
		shell_fprintf(shell, SHELL_NORMAL,
			     "  Border router global IPv6 address: not set\n");
	}

	p = nrf_rpc_decode_buffer_ptr_and_size(ctx, &sz);
	if (p && sz > 0) {
		char fw[101];

		if (sz >= sizeof(fw)) {
			sz = sizeof(fw) - 1;
		}
		memcpy(fw, p, sz);
		fw[sz] = '\0';
		shell_fprintf(shell, SHELL_NORMAL, "  Modem FW version:             %s\n", fw);
	}
}

/* Request sequence so each DECT NR+ status has unique payload => unique CRC. */
static uint32_t dect_rpc_req_seq;

/* Dedicated status RPC: ref-style synchronous send+response (nrf_rpc_cbor_cmd_rsp). */
static int cmd_dect_status(const struct shell *shell, size_t argc, char **argv)
{
	struct nrf_rpc_cbor_ctx ctx;
	int err;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	NRF_RPC_CBOR_ALLOC(&dect_rpc_group, ctx, 8);
	nrf_rpc_encode_uint(&ctx, dect_rpc_req_seq++);

	err = nrf_rpc_cbor_cmd_rsp(&dect_rpc_group, DECT_RPC_CMD_IF_STATUS, &ctx);
	if (err < 0) {
		shell_error(shell, "RPC send failed");
		return -EIO;
	}

	if (!nrf_rpc_decode_valid(&ctx)) {
		shell_error(shell, "Invalid status response");
		return -EIO;
	}

	decode_and_print_status(shell, &ctx);

	if (!nrf_rpc_decoding_done_and_check(&dect_rpc_group, &ctx)) {
		shell_error(shell, "Status response decode error");
		return -EIO;
	}

	return 0;
}

#if !defined(CONFIG_NRF_RPC_INIT)
static void dect_rpc_err_handler(const struct nrf_rpc_err_report *report)
{
	ARG_UNUSED(report);
	/* Logging is done by nRF RPC; avoid k_oops so shell init can return error. */
}

/*
 * Initialize nRF RPC when server is connected over UART.
 * You must run this on the DECT NR+ CLIENT first; it will block. Then run "dect_rpc_init"
 * on the server. If you run the server first, the server will time out; run this
 * on the client first and try again.
 */
static int cmd_dect_rpc_init(const struct shell *shell, size_t argc, char **argv)
{
	int err;
	static bool inited;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (inited) {
		shell_info(shell, "RPC already initialized");
		dect_rpc_client_notify_rpc_init_done();
		return 0;
	}

	shell_info(shell, "Waiting for DECT NR+ server... Run 'dect_rpc_init' on the server now.");
	err = nrf_rpc_init(dect_rpc_err_handler);
	if (err != 0) {
		shell_error(shell, "nrf_rpc_init failed: %d (is DECT NR+ server connected? Run "
			    "dect_rpc_init on server first, then retry; if retry, reboot client)",
			    err);
		return err;
	}

	/* After a previous failed init the library can return 0 without the group being bound. */
	if (!NRF_RPC_GROUP_STATUS(dect_rpc_group)) {
		shell_error(shell, "DECT NR+ RPC group not bound (DECT NR+ server not ready). "
			"Reboot DECT NR+ client, "
			"start DECT NR+ server and run dect_rpc_init on DECT NR+ server, "
			"then run this again.");
		return -EAGAIN;
	}

	inited = true;
	dect_rpc_client_notify_rpc_init_done();
	shell_info(shell, "DECT NR+ RPC initialized; interface will be brought up");
	return 0;
}

static int cmd_dect_rpc_ping(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	if (dect_rpc_client_ping(K_MSEC(2000)) != 0) {
		shell_error(shell, "DECT NR+ RPC UART test: failed (timeout)");
		return -ETIMEDOUT;
	}
	shell_info(shell, "DECT NR+ RPC UART test: OK");
	return 0;
}

#if defined(CONFIG_DECT_NR_RPC_NET_IF)
static int cmd_dect_sync(const struct shell *shell, size_t argc, char **argv)
{
	struct net_if *iface;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	iface = net_if_get_first_by_type(&NET_L2_GET_NAME(DECT_RPC_L2));
	if (!iface) {
		shell_error(shell, "No DECT NR+ RPC interface");
		return -ENODEV;
	}
	if (sync_addrs_from_server(iface) != 0) {
		shell_error(shell, "Sync addresses from DECT NR+ server failed "
				   "(timeout or DECT NR+ RPC error)");
		return -EIO;
	}
	shell_info(shell, "Synced addresses/prefixes from server");
	return 0;
}
#endif

SHELL_STATIC_SUBCMD_SET_CREATE(sub_dect_rpc,
	SHELL_CMD(init, NULL, "Initialize DECT NR+ RPC when DECT NR+ server is connected",
		cmd_dect_rpc_init),
	SHELL_CMD(ping, NULL, "Test DECT NR+ RPC UART both ways (ping DECT NR+ server)",
		cmd_dect_rpc_ping),
	SHELL_SUBCMD_SET_END
);
#endif

SHELL_STATIC_SUBCMD_SET_CREATE(sub_dect,
#if !defined(CONFIG_NRF_RPC_INIT)
	SHELL_CMD(rpc, &sub_dect_rpc, "DECT NR+ RPC commands (init when DECT NR+ server connected)",
		NULL),
#endif
	SHELL_CMD(status, NULL, "DECT status from DECT NR+ server (dedicated DECT NR+ RPC)",
		cmd_dect_status),
#if defined(CONFIG_DECT_NR_RPC_NET_IF)
	SHELL_CMD(sync, NULL, "Sync IPv6 addresses/prefixes from DECT NR+ server.",
		  cmd_dect_sync),
#endif
	SHELL_CMD_ARG(run, NULL, "Run DECT L2 shell on server\n"
		     "dect run <subcmd> [args...]  e.g. dect run status, dect run nw_join",
		     cmd_dect_shell, 2, DECT_RPC_SHELL_MAX_ARGC + 1),
	SHELL_SUBCMD_SET_END
);

#if defined(CONFIG_NRF_RPC_INIT)
#define DECT_SHELL_HELP "DECT NR+ RPC client (status | run <subcmd> [args...] | rpc ping)"
#else
#define DECT_SHELL_HELP "DECT NR+ RPC client (rpc init | rpc ping | status | sync | run <subcmd> [args...])"
#endif
SHELL_CMD_REGISTER(dect, &sub_dect, DECT_SHELL_HELP, NULL);
