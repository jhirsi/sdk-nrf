/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>

#include <zephyr/shell/shell.h>

#include "desh_print.h"

#include "auto_connect.h"

static int auto_connect_shell_enable(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	int err = auto_connect_sett_enable_disable(true);

	if (err) {
		desh_error("Failed to enable auto connect");
		return err;
	}

	desh_print("Auto connect enabled");
	return 0;
}
static int auto_connect_shell_disable(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	int err = auto_connect_sett_enable_disable(false);

	if (err) {
		desh_error("Failed to disable auto connect");
		return err;
	}

	desh_print("Auto connect disabled");
	return 0;
}

static int auto_connect_shell_delay_set(const struct shell *shell, size_t argc, char **argv)
{
	int delay_secs;

	if (argc < 2) {
		desh_error("Invalid number of arguments");
		return -EINVAL;
	}

	delay_secs = atoi(argv[1]);
	if (delay_secs < 0) {
		desh_error("Invalid delay_secs %d", delay_secs);
		return -EINVAL;
	}
	int err = auto_connect_sett_delay_set(delay_secs);

	if (err) {
		desh_error("Failed to set auto connect delay: %d", err);
		return err;
	}

	desh_print("Auto connect delay set to %d seconds", delay_secs);
	return 0;
}

static int auto_connect_shell_sett_read(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	bool enabled = auto_connect_sett_is_enabled();
	int delay_secs = auto_connect_sett_delay_get();

	desh_print("Auto connect settings:");
	desh_print("  Enabled: %s", enabled ? "true" : "false");
	desh_print("  Delay (secs): %d", delay_secs);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_auto_connect_shell,
	SHELL_CMD_ARG(
		enable, NULL,
		"desh specific settings to enable auto connect after modem activation.\n"
		"Default: off. If enabled:\n"
		"FT: same as nw_create, i.e. performs RSSI scan and creates cluster to free "
		"channel at set band.\n"
		"PT: same as nw_join, Performs network scan on a set band and associates with "
		"FT device as set in nw_join settings.\n",
		auto_connect_shell_enable, 1, 0),
	SHELL_CMD(disable, NULL, "Setting to disable auto connect.\n", auto_connect_shell_disable),
	SHELL_CMD_ARG(delay, NULL, "Auto connect delay (in secs) after activation\n",
		      auto_connect_shell_delay_set, 2, 0),
	SHELL_CMD(sett_read, NULL, "Read auto connect settings\n", auto_connect_shell_sett_read),

	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(auto_connect, &sub_auto_connect_shell,
		   "Enable and disable auto-connect feature for DECT NR+ Shell.",
		   desh_print_help_shell);
