/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MOSH_LINK_SHELL_H
#define MOSH_LINK_SHELL_H

#include <zephyr/shell/shell.h>
#include <modem/lte_lc.h>

#define MOSH_NCELLMEAS_SEARCH_TYPE_NONE 0xFF

int link_shell_get_and_print_current_system_modes(
	enum lte_lc_system_mode *sys_mode_current,
	enum lte_lc_system_mode_preference *sys_mode_preferred,
	enum lte_lc_lte_mode *currently_active_mode);

enum lte_lc_neighbor_search_type
	link_shell_string_to_ncellmeas_search_type(const char *search_type_str);

#endif /* MOSH_LINK_SHELL_H */
