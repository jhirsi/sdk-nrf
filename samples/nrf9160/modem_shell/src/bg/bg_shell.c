/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <stdlib.h>

#include <shell/shell.h>

#include "bg/bg_thread.h"

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	int ret = 1;

	shell_help(shell);

	return ret;
}
static int cmd_bg_start(const struct shell *shell, size_t argc, char **argv)
{
    
    bg_threads_submit(shell, argc, argv);
    return 0;
}

static int cmd_bg_get_results(const struct shell *shell, size_t argc, char **argv)
{
	int process_nbr;

	process_nbr = atoi(argv[1]);
	if (process_nbr <= 0 || process_nbr > BG_THREADS_MAX_NBR) {
		shell_error(
			shell,
			"invalid process value %d",
			process_nbr);
		return -EINVAL;
	}
   	bg_threads_result_print(shell, process_nbr);

    return 0;
}

static int cmd_bg(const struct shell *shell, size_t argc, char **argv)
{
	return print_help(shell, argc, argv);
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_bg,
			       SHELL_CMD(start, NULL, "iperf3 <params>\nStart a background process. Experimental feature.", cmd_bg_start),
			       SHELL_CMD_ARG(results, NULL, "<bg process nbr>\nGet results.", cmd_bg_get_results, 2, 0),
			       SHELL_SUBCMD_SET_END
			       );
                   /* TODO: status, delete data */
SHELL_CMD_REGISTER(bg, &sub_bg, "Commands for background processes. Experimental feature", cmd_bg);
