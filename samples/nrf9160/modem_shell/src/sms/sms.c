/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <shell/shell.h>
#include <assert.h>
#include <strings.h>
#include <stdio.h>

#include <modem/at_cmd.h>
#include <modem/at_cmd_parser.h>
#include <modem/at_params.h>
#include <modem/sms.h>

#include "sms.h"
#include "fta_defines.h"

#define PAYLOAD_BUF_SIZE 160
#define SMS_HANDLE_NONE -1

extern const struct shell* shell_global;
static int sms_handle = SMS_HANDLE_NONE;
static int sms_recv_counter = 0;


static void sms_callback(struct sms_data *const data, void *context)
{
	if (data == NULL) {
		printk("sms_callback with NULL data\n");
	}

	if (data->type == SMS_TYPE_SUBMIT_REPORT) {
		/* TODO: Check whether we should parse SMS-SUBMIT-REPORT more carefully */
		shell_print(shell_global, "SMS submit report received");
		return;
	} else if (data->type == SMS_TYPE_DELIVER) {
		struct sms_deliver_header *header = &data->header.deliver;
		shell_print(shell_global, "Time:   %02d-%02d-%02d %02d:%02d:%02d",
			header->time.year,
			header->time.month,
			header->time.day,
			header->time.hour,
			header->time.minute,
			header->time.second);

		shell_print(shell_global, "Text:   '%s'", data->data);
		shell_print(shell_global, "Length: %d", data->data_len);

		if (header->app_port.present) {
			shell_print(shell_global,
				"Application port addressing scheme: dest_port=%d, src_port=%d",
				header->app_port.dest_port,
				header->app_port.src_port);
		}
		if (header->concatenated.present) {
			shell_print(shell_global,
				"Concatenated short messages: ref_number=%d, msg %d/%d",
				header->concatenated.ref_number,
				header->concatenated.seq_number,
				header->concatenated.total_msgs);
		}

		sms_recv_counter++;
	} else {
		shell_print(shell_global, "SMS protocol message with unknown type received");
	}

}

int sms_register()
{
	int ret;

	if (sms_handle != SMS_HANDLE_NONE) {
		return 0;
	}

	ret = sms_init();
	if (ret) {
		printf("sms_init returned err: %d\n", ret);
		return ret;
	}
	int handle = sms_register_listener(sms_callback, NULL);
	if (handle) {
		printf("sms_register_listener returned err: %d\n", handle);
		return handle;
	}

	sms_handle = handle;
	return 0;
}

int sms_unregister()
{
	sms_unregister_listener(sms_handle);
	sms_handle = SMS_HANDLE_NONE;
	sms_uninit();

	return 0;
}

/* Function name is not sms_send() because it's reserved by SMS library. */
int sms_send_msg(char* number, char* text)
{
	int ret;

	if (number == NULL || strlen(number) == 0) {
		shell_error(shell_global, "Number not given");
		return -EINVAL;
	}
	if (text == NULL || strlen(text) == 0) {
		shell_error(shell_global, "Text not given");
		return -EINVAL;
	}

	shell_print(shell_global, "Sending SMS to number=%s, text='%s'", number, text);

	ret = sms_register();
	if (ret != 0) {
		return ret;
	}

	ret = sms_send(number, text);

	return ret;
}

int sms_recv(bool arg_receive_start)
{
	if (arg_receive_start) {
		sms_recv_counter = 0;
		shell_print(shell_global, "SMS receive counter set to zero");
	} else {
		shell_print(shell_global, "SMS receive counter = %d",
			sms_recv_counter);
	}

	return 0;
}
