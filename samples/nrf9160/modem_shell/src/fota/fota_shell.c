/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <stdlib.h>

#include <zephyr.h>
#include <shell/shell.h>

#include "fota.h"

/* Sets the global shell variable, must be called before calling any FOTA
 * module functions.
 */
#define FOTA_SET_GLOBAL_SHELL() \
	fota_shell_global = shell;

static const char fota_server_eu[] = "nrf-test-eu.s3.amazonaws.com";
static const char fota_server_usa[] = "nrf-test-us.s3.amazonaws.com";
static const char fota_server_jpn[] = "nrf-test-jpn.s3.amazonaws.com";
static const char fota_server_au[] = "nrf-test-au.s3.amazonaws.com";

const struct shell *fota_shell_global;

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	int ret = 1;

	if (argc > 1) {
		shell_error(shell, "%s: subcommand not found", argv[1]);
		ret = -EINVAL;
	}

	shell_help(shell);

	return ret;
}

static int cmd_fota(const struct shell *shell, size_t argc, char **argv)
{
	FOTA_SET_GLOBAL_SHELL();

	return print_help(shell, argc, argv);
}

static int cmd_fota_download(const struct shell *shell, size_t argc, char **argv)
{
	FOTA_SET_GLOBAL_SHELL();

	int err;
	const char *fota_server;

	if (strcmp(argv[1], "eu") == 0) {
		fota_server = fota_server_eu;
	} else if (strcmp(argv[1], "us") == 0) {
		fota_server = fota_server_usa;
	} else if (strcmp(argv[1], "jpn") == 0) {
		fota_server = fota_server_jpn;
	} else if (strcmp(argv[1], "au") == 0) {
		fota_server = fota_server_au;
	} else {
		shell_error(shell, "FOTA: Unknown server: %s", argv[1]);
		return -EINVAL;
	}

	shell_print(shell, "FOTA: Starting download...");

	err = fota_start(fota_server, argv[2]);

	if (err) {
		shell_error(shell, "Failed to start FOTA download, error %d", err);
		return err;
	}

	shell_print(shell, "FOTA: Download started");

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_fota,
	SHELL_CMD_ARG(download, NULL, "<server> <filename>\nDownload and install a FOTA update. Available servers are \"eu\", \"us\", \"jpn\" and \"au\".", cmd_fota_download, 3, 0),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(fota, &sub_fota, "Commands for FOTA update.", cmd_fota);
