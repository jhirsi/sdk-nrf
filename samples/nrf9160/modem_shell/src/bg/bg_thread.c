/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>

#include <stdio.h>

#include <assert.h>

#include <shell/shell.h>

#include "utils/str_utils.h"

#if defined(CONFIG_MOSH_IPERF3)
#include <sys/select.h>
#include <iperf_api.h>
#endif

#define BG_THREADS_RESPONSE_BUFFER_SIZE 10240

#define BG_THREAD_1_STACK_SIZE 9216
#define BG_THREAD_1_PRIORITY 5
K_THREAD_STACK_DEFINE(bg_stack_area_1, BG_THREAD_1_STACK_SIZE);

#define BG_THREAD_2_STACK_SIZE 9216
#define BG_THREAD_2_PRIORITY 5
K_THREAD_STACK_DEFINE(bg_stack_area_2, BG_THREAD_2_STACK_SIZE);

struct k_work_q background_work_q_1;
struct k_work_q background_work_q_2;

struct bg_data_1 {
	struct k_work work;
	char *results_str;
	size_t argc;
	char **argv;
	const struct shell *shell;
} bg_work_data_1;

struct bg_data_2 {
	struct k_work work;
	char *results_str;
	size_t argc;
	char **argv;
	const struct shell *shell;
} bg_work_data_2;

static char **bg_thread_util_duplicate_argv(int argc, char **argv)
{
	char **ptr_array;
	int i;

	ptr_array = malloc(sizeof(char *) * argc);
	if (ptr_array == NULL) {
		return NULL;
	}

	for (i = 1; i < argc; i++) {
		ptr_array[i] = mosh_strdup(argv[i]);
		if (ptr_array[i] == NULL) {
			free(ptr_array);
			ptr_array = NULL;
			break;
		}
	}

	return ptr_array;
}

static void bg_thread_util_duplicate_argv_free(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		free(argv[i]);
	}

	free(argv);
}

static void bg_work_handler_1(struct k_work *work_item)
{
	struct bg_data_1 *data =
		CONTAINER_OF(work_item, struct bg_data_1, work);
	int ret;

	shell_print(data->shell, "Starting a bg process #1");

	assert(data->argv != NULL && data->results_str != NULL);

	ret = iperf_main(data->argc, data->argv, data->results_str,
			 BG_THREADS_RESPONSE_BUFFER_SIZE);

	shell_print(data->shell,
		    "--------------------------------------------------");
	shell_print(data->shell, "iperf_main returned %d from a process #1",
		    ret);
	shell_print(data->shell,
		    "Use shell command to print results: \"bg results 1\"");
	shell_print(data->shell,
		    "--------------------------------------------------");

	/* Clean up for cmd argv: */
	bg_thread_util_duplicate_argv_free(data->argc, data->argv);
}

static void bg_work_handler_2(struct k_work *work_item)
{
	struct bg_data_2 *data =
		CONTAINER_OF(work_item, struct bg_data_2, work);
	int ret;

	shell_print(data->shell, "Starting a bg process #2");

	assert(data->argv != NULL && data->results_str != NULL);

	ret = iperf_main(data->argc, data->argv, data->results_str,
			 BG_THREADS_RESPONSE_BUFFER_SIZE);

	shell_print(data->shell,
		    "--------------------------------------------------");
	shell_print(data->shell, "iperf_main returned %d from process 2", ret);
	shell_print(data->shell,
		    "Use shell command to print results: \"bg results 2\"");
	shell_print(data->shell,
		    "--------------------------------------------------");

	/* Clean up for cmd argv: */
	bg_thread_util_duplicate_argv_free(data->argc, data->argv);
}

void bg_threads_result_print(const struct shell *shell, int nbr)
{
	if (nbr == 1) {
		if (bg_work_data_1.results_str == NULL ||
		    !strlen(bg_work_data_1.results_str)) {
			shell_print(shell, "No results for process #1\n");
		} else {
			shell_print(shell, "background process #1 results: \n");
			shell_print(shell,
				    "-------------------------------------");
			shell_print(shell, "%s", bg_work_data_1.results_str);
			shell_print(shell,
				    "-------------------------------------");

			/* Delete data if the work is done: */
			if (!k_work_is_pending(&(bg_work_data_1.work))) {
				free(bg_work_data_1.results_str);
				bg_work_data_1.results_str = NULL;
				shell_print(
					shell,
					"Note: bg results #1 were deleted.");
			}
		}
	} else if (nbr == 2) {
		if (bg_work_data_2.results_str == NULL ||
		    !strlen(bg_work_data_2.results_str)) {
			shell_print(shell, "No results for process #2\n");
		} else {
			shell_print(shell, "background process #2 results: \n");
			shell_print(shell,
				    "-------------------------------------");

			shell_print(shell, "%s", bg_work_data_2.results_str);
			shell_print(shell,
				    "-------------------------------------");

			/* Delete data if the work is done: */
			if (!k_work_is_pending(&(bg_work_data_2.work))) {
				free(bg_work_data_2.results_str);
				bg_work_data_2.results_str = NULL;
				shell_print(
					shell,
					"Note: bg results #2 were deleted.");
			}
		}
	}
}

void bg_threads_submit(const struct shell *shell, size_t argc, char **argv)
{
	/* Only iperf3 currently supported: */
	if (strcmp(argv[1], "iperf3") != 0) {
		shell_error(shell, "Only iperf3 is supported currently.");
		return;
	}

	shell_print(shell, "Starting ..");

	if (!k_work_is_pending(&(bg_work_data_1.work))) {
		if (bg_work_data_1.results_str == NULL) {
			bg_work_data_1.results_str = (char *)calloc(
				BG_THREADS_RESPONSE_BUFFER_SIZE, sizeof(char));
			if (bg_work_data_1.results_str == NULL) {
				shell_error(
					shell,
					"Cannot start background process: no memory to store a response");
				return;
			}
		}

		memset(bg_work_data_1.results_str, 0,
		       BG_THREADS_RESPONSE_BUFFER_SIZE);

		bg_work_data_1.argv = bg_thread_util_duplicate_argv(argc, argv);
		if (bg_work_data_1.argv == NULL) {
				shell_error(
					shell,
					"Cannot start background process: no memory for duplicated cmd args");
				return;
		}

		bg_work_data_1.argc = argc;
		bg_work_data_1.shell = shell;

		k_work_submit_to_queue(&background_work_q_1,
				       &bg_work_data_1.work);
	} else if (!k_work_is_pending(&(bg_work_data_2.work))) {
		if (bg_work_data_2.results_str == NULL) {
			bg_work_data_2.results_str = (char *)calloc(
				BG_THREADS_RESPONSE_BUFFER_SIZE, sizeof(char));
			if (bg_work_data_2.results_str == NULL) {
				shell_error(
					shell,
					"Cannot start background process: no memory to store a response");
				return;
			}
		}

		memset(bg_work_data_2.results_str, 0,
		       BG_THREADS_RESPONSE_BUFFER_SIZE);

		bg_work_data_2.argv = bg_thread_util_duplicate_argv(argc, argv);
		if (bg_work_data_2.argv == NULL) {
				shell_error(
					shell,
					"Cannot start background process: no memory for duplicated cmd args");
				return;
		}

		bg_work_data_2.argc = argc;
		bg_work_data_2.shell = shell;

		k_work_submit_to_queue(&background_work_q_2,
				       &bg_work_data_2.work);
	} else {
		shell_error(
			shell,
			"Background threads are all busy. Try again later.");
	}
}

void bg_init()
{
	k_work_queue_start(&background_work_q_1, bg_stack_area_1,
			   K_THREAD_STACK_SIZEOF(bg_stack_area_1),
			   BG_THREAD_1_PRIORITY, NULL);
	k_thread_name_set(&(background_work_q_1.thread), "mosh_bg_1");
	k_work_init(&bg_work_data_1.work, bg_work_handler_1);

	k_work_queue_start(&background_work_q_2, bg_stack_area_2,
			   K_THREAD_STACK_SIZEOF(bg_stack_area_2),
			   BG_THREAD_1_PRIORITY, NULL);
	k_thread_name_set(&(background_work_q_2.thread), "mosh_bg_2");
	k_work_init(&bg_work_data_2.work, bg_work_handler_2);
}
