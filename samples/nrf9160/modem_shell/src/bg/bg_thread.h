/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BG_THREAD_H
#define BG_THREAD_H
#include <shell/shell.h>

#define BG_THREADS_MAX_NBR 2

void bg_init();
void bg_threads_submit(const struct shell *shell, size_t argc, char **argv);
void bg_threads_result_print(const struct shell *shell, int nbr);

#endif /* BG_THREAD_H */