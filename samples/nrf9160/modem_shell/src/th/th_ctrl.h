/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef TH_CTRL_H
#define TH_CTRL_H
#include <shell/shell.h>

#define BG_THREADS_MAX_NBR 2

void th_ctrl_init(void);
void th_ctrl_result_print(const struct shell *shell, int nbr);
void th_ctrl_status_print(const struct shell *shell);
void th_ctrl_start(const struct shell *shell, size_t argc, char **argv,
		   bool bg_thread);
void th_ctrl_kill(const struct shell *shell, int nbr);

#endif /* TH_CTRL_H */
