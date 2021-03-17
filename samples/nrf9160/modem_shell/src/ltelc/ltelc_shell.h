/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LTELC_SHELL_H
#define LTELC_SHELL_H
#include <shell/shell.h>

int ltelc_shell(const struct shell *shell, size_t argc, char **argv);
void ltelc_shell_print_current_system_modes(const struct shell *shell);

#endif /* LTELC_SHELL_H */
