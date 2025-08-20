/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef DECT_L2_SHELL_UTIL_H
#define DECT_L2_SHELL_UTIL_H

char *dect_shell_util_mac_error_to_string(enum dect_status_values status, char *out_str_buff,
					  size_t out_str_buff_len);

#endif /* DECT_L2_SHELL_UTIL_H */
