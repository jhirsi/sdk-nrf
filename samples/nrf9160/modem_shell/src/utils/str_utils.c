/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "str_utils.h"

char *get_command_str_from_argv(size_t argc, char **argv, char *out_buf, uint16_t out_buf_len)
{
	int i, total_len = 0, arg_len = 0;

	for (i = 0; i < argc; i++) {
		arg_len = strlen(argv[i]);
		if (total_len + arg_len > out_buf_len) {
			break;
		}
		if (i == (argc - 1)) {
			/* Do not put space to last one */
			sprintf(out_buf + total_len, "%s", argv[i]);

		} else {
			sprintf(out_buf + total_len, "%s ", argv[i]);
		}
		total_len = strlen(out_buf);
	}

	return out_buf;
}

int str_hex_to_bytes(char *str, uint32_t str_length, uint8_t *buf, uint16_t buf_length)
{
	/* Remove any spaces from the input string */
	uint32_t str_length_no_space = 0;

	for (int i = 0; i < str_length; i++) {
		if (str[i] != ' ') {
			str[str_length_no_space] = str[i];
			str_length_no_space++;
		}
	}

	buf_length = hex2bin(str, str_length_no_space, buf, buf_length);

	return buf_length;
}

/* strdup() C-library function implemented here because wasn't find from used
 * MOSH libs (e.g. newlibc).
 */
char *mosh_strdup(const char *str)
{
	size_t len;
	char *newstr;

	if (!str) {
		return (char *)NULL;
	}

	len = strlen(str) + 1;
	newstr = malloc(len);
	if (!newstr) {
		return (char *)NULL;
	}

	memcpy(newstr, str, len);
	return newstr;
}
