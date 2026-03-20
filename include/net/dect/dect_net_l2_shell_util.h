/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_NET_L2_SHELL_UTIL_H
#define DECT_NET_L2_SHELL_UTIL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward declarations */
struct shell;
enum dect_status_values;

/**
 * @brief Print function pointer type matching shell_print signature.
 *
 * @param shell Shell instance (can be NULL if not using shell).
 * @param fmt Format string.
 * @param ... Variable arguments.
 */
typedef void (*dect_net_l2_shell_print_fn_t)(const struct shell *shell, const char *fmt, ...);

/**
 * @brief Print function pointer type for variadic arguments.
 *
 * @param shell Shell instance (can be NULL if not using shell).
 * @param fmt Format string.
 * @param args Variable arguments as va_list.
 */
typedef void (*dect_net_l2_shell_vprint_fn_t)(const struct shell *shell, const char *fmt,
					      va_list args);

/**
 * @brief Structure for custom print functions.
 */
struct dect_net_l2_shell_print_fns {
	dect_net_l2_shell_print_fn_t print_fn;  /**< Print function (normal output) */
	dect_net_l2_shell_print_fn_t error_fn;  /**< Error print function */
	dect_net_l2_shell_print_fn_t warn_fn;   /**< Warning print function */
};

/**
 * @brief Initialize l2_shell library with custom print functions.
 *
 * This function allows the application to provide custom print functions
 * (for example, with timestamping) instead of using the default shell_print functions.
 * This is optional - if not called, the library will use default shell_print functions.
 *
 * @param print_fns Structure containing custom print function pointers.
 *                  If NULL, custom print functions are disabled and default
 *                  shell_print functions will be used.
 *
 * @return 0 on success, negative error code on failure
 */
int dect_net_l2_shell_init(const struct dect_net_l2_shell_print_fns *print_fns);

char *dect_net_l2_shell_util_mac_err_to_string(enum dect_status_values status, char *out_str_buff,
					       size_t out_str_buff_len);

#if defined(CONFIG_DECT_L2_SHELL_RPC)
/**
 * Execute a DECT L2 shell subcommand by name (e.g. "status", "nw_join").
 * Output is appended to @p out_buf. Used by DECT RPC server for remote shell.
 * Caller must hold the shell session lock when calling (e.g. after try_lock on server).
 *
 * @param subcmd Subcommand name (e.g. "status", "nw_join").
 * @param argc   Argument count (including subcmd as argv[0]).
 * @param argv   Argument vector (argv[0] = subcmd, argv[1] = first arg, ...).
 * @param out_buf Buffer for captured output (null-terminated).
 * @param out_len Size of @p out_buf.
 * @return 0 on success, -ENOENT if subcommand unknown, negative on other error.
 */
int dect_shell_exec_by_name(const char *subcmd, int argc, char **argv,
			    char *out_buf, size_t out_len);
#endif /* CONFIG_DECT_L2_SHELL_RPC */

#endif /* DECT_NET_L2_SHELL_UTIL_H */
