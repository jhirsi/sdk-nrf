/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>

#include <zephyr/shell/shell.h>
#include <modem/at_monitor.h>
#include <nrf_modem_at.h>

#include "desh_print.h"
#include "desh_defines.h"

#if defined(CONFIG_DESH_AT_CMD_MODE)
#include "at_cmd_mode.h"
#include "at_cmd_mode_sett.h"
#endif

extern char desh_at_resp_buf[DESH_AT_CMD_RESPONSE_MAX_LEN];
extern struct k_mutex desh_at_resp_buf_mutex;

AT_MONITOR(desh_at_handler, ANY, at_cmd_handler, PAUSED);

#if defined(CONFIG_DESH_AT_CMD_MODE)

static int at_shell_cmd_mode_start(const struct shell *shell, size_t argc, char **argv)
{
	at_cmd_mode_start(shell);
	return 0;
}

static int at_shell_cmd_mode_enable_autostart(const struct shell *shell, size_t argc, char **argv)
{
	at_cmd_mode_sett_autostart_enabled(true);
	return 0;
}
static int at_shell_cmd_mode_disable_autostart(const struct shell *shell, size_t argc, char **argv)
{
	at_cmd_mode_sett_autostart_enabled(false);
	return 0;
}
static int at_shell_cmd_mode_term_cr_lf(const struct shell *shell, size_t argc, char **argv)
{
	at_cmd_mode_line_termination_set(CR_LF_TERM);
	return 0;
}
static int at_shell_cmd_mode_term_lf(const struct shell *shell, size_t argc, char **argv)
{
	at_cmd_mode_line_termination_set(LF_TERM);
	return 0;
}
static int at_shell_cmd_mode_term_cr(const struct shell *shell, size_t argc, char **argv)
{
	at_cmd_mode_line_termination_set(CR_TERM);
	return 0;
}
#endif

static void at_cmd_handler(const char *response)
{
	desh_print("AT event handler: %s", response);
}

static int sub_at_cmd_events_enable(const struct shell *shell, size_t argc, char **argv)
{
	at_monitor_resume(&desh_at_handler);
	desh_print("AT command events enabled");

	return 0;
}

static int sub_at_cmd_events_disable(const struct shell *shell, size_t argc, char **argv)
{
	at_monitor_pause(&desh_at_handler);
	desh_print("AT command events disabled");

	return 0;
}

static int cmd_at(const struct shell *shell, size_t argc, char **argv)
{
	int err;

	if (argc < 2 || (strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)) {
		shell_help(shell);
	} else {
		k_mutex_lock(&desh_at_resp_buf_mutex, K_FOREVER);

		/* z_shell_make_argv() is not splitting to argv as we want because for example:
		 * desh:~$ at AT%CMNG=0,16842753,0,/"-----BEGIN CERTIFICATE-----/"
		 * is interpreted as argv[1]:
		 * desh:~$ at AT%CMNG=0,16842753,0,/"-----BEGIN
		 * Thus, we need to combine the arguments to a single string separated by space.
		 */
		strcpy(desh_at_resp_buf, argv[1]);

		if (argc > 2) {
			desh_at_resp_buf[0] = '\0';

			/* combine argvs */
			for (int i = 1; i < argc; i++) {
				strcat(desh_at_resp_buf, argv[i]);
				if (i < argc - 1) {
					strcat(desh_at_resp_buf, " ");
				}
			}
		}
		err = nrf_modem_at_cmd(desh_at_resp_buf, sizeof(desh_at_resp_buf), "%s",
				       desh_at_resp_buf);
		if (err == 0) {
			desh_print("%s", desh_at_resp_buf);
		} else if (err > 0) {
			desh_error("%s", desh_at_resp_buf);
			err = -EINVAL;
		} else {
			/* Negative values are error codes */
			desh_error("Failed to send AT command, err %d", err);
		}
		k_mutex_unlock(&desh_at_resp_buf_mutex);
	}

	return 0;
}

#if defined(CONFIG_DESH_AT_CMD_MODE)
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_cmd_at_cmd_mode,
	SHELL_CMD(start, NULL, "Start AT command mode.", at_shell_cmd_mode_start),
	SHELL_CMD(enable_autostart, NULL, "Enable AT command autostart on bootup.",
		  at_shell_cmd_mode_enable_autostart),
	SHELL_CMD(disable_autostart, NULL, "Disable AT command mode autostart on bootup.",
		  at_shell_cmd_mode_disable_autostart),
	SHELL_CMD(term_cr_lf, NULL, "Receive CR+LF as command line termination.",
		  at_shell_cmd_mode_term_cr_lf),
	SHELL_CMD(term_lf, NULL, "Receive LF as command line termination.",
		  at_shell_cmd_mode_term_lf),
	SHELL_CMD(term_cr, NULL, "Receive CR as command line termination.",
		  at_shell_cmd_mode_term_cr),
	SHELL_SUBCMD_SET_END);
#endif

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_at_shell,
	SHELL_CMD_ARG(events_enable, NULL, "Enable AT event handler which prints AT notifications.",
		      sub_at_cmd_events_enable, 1, 0),
	SHELL_CMD(events_disable, NULL, "Disable AT event handler.", sub_at_cmd_events_disable),
#if defined(CONFIG_DESH_AT_CMD_MODE)
	SHELL_CMD(at_cmd_mode, &sub_cmd_at_cmd_mode, "Enable/disable AT command mode.",
		  desh_print_help_shell),
#endif
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(at, &sub_at_shell,
		   "Execute an AT command. Any subcommand not listed below is interpreted "
		   "as AT command and sent to the modem.",
		   cmd_at);
