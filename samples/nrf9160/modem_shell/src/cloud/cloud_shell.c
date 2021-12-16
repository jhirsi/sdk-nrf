/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <zephyr.h>
#include <net/nrf_cloud.h>
#include <nrf_cloud_fsm.h>
#include <shell/shell.h>
#include <shell/shell_uart.h>
#include "mosh_print.h"

#if defined(CONFIG_NRF_CLOUD_AGPS)
#include <net/nrf_cloud_agps.h>
#endif
#if defined(CONFIG_NRF_CLOUD_PGPS)
#include <net/nrf_cloud_pgps.h>
#endif

#define CLOUD_CMD_MAX_LENGTH 150

BUILD_ASSERT(
	IS_ENABLED(CONFIG_NRF_CLOUD_MQTT) &&
	IS_ENABLED(CONFIG_NRF_CLOUD_CONNECTION_POLL_THREAD));

static struct k_work_delayable cloud_reconnect_work;
#if defined(CONFIG_NRF_CLOUD_PGPS)
static struct k_work notify_pgps_work;
#endif
static struct k_work cloud_cmd_work;

static char shell_cmd[CLOUD_CMD_MAX_LENGTH];

static int cloud_shell_print_usage(const struct shell *shell, size_t argc, char **argv)
{
	int ret = 1;

	if (argc > 1) {
		mosh_error("%s: subcommand not found", argv[1]);
		ret = -EINVAL;
	}

	shell_help(shell);

	return ret;
}

static void cloud_reconnect_work_fn(struct k_work *work)
{
	int err = nrf_cloud_connect(NULL);

	if (err == NRF_CLOUD_CONNECT_RES_SUCCESS) {
		mosh_print("Connection to nRF Cloud established");
	} else if (err == NRF_CLOUD_CONNECT_RES_ERR_ALREADY_CONNECTED) {
		mosh_print("nRF Cloud connection already established");
	} else {
		mosh_error("nrf_cloud_connect, error: %d", err);
	}
}

#if defined(CONFIG_NRF_CLOUD_PGPS)
static void notify_pgps(struct k_work *work)
{
	ARG_UNUSED(work);
	int err;

	err = nrf_cloud_pgps_notify_prediction();
	if (err) {
		mosh_error("Error requesting notification of prediction availability: %d", err);
	}
}
#endif /* defined(CONFIG_NRF_CLOUD_PGPS) */

static void cloud_cmd_execute(struct k_work *work)
{
	const struct shell *shell = shell_backend_uart_get_ptr();

	shell_execute_cmd(shell, shell_cmd);
	memset(shell_cmd, 0, CLOUD_CMD_MAX_LENGTH);
}

static bool cloud_shell_parse_mosh_cmd(const char *buf_in)
{
	char *cmd_start_ptr = NULL;
	char *cmd_end_ptr = NULL;

	/* MoSh commands are identified by setting appId to "MODEM_SHELL" */
	char *string_ptr = strstr(buf_in, "\"appId\"");

	if (string_ptr) {
		string_ptr = strstr(string_ptr, ":");
		if (string_ptr) {
			string_ptr = strstr(string_ptr, "\"MODEM_SHELL\"");
		}
	}

	if (!string_ptr) {
		return false; /* appId is not present or it is different from MODEM_SHELL */
	}

	/* TODO: - Maybe add similar block for checking the that messageType == "CMD".
	 *       - Handle quotation marks inside commands. They are not parsed properly.
	 */

	/* Actual command is contained in data field */
	string_ptr = strstr(buf_in, "\"data\"");
	if (string_ptr) {
		string_ptr = strstr(string_ptr, ":");
		if (string_ptr) {
			cmd_start_ptr = strstr(string_ptr, "\"");
			if (cmd_start_ptr) {
				cmd_end_ptr = strstr(cmd_start_ptr + 1, "\"");
			}
		}
	}

	if (!cmd_end_ptr) {
		return false; /* data field is not present or it's malformed */
	}

	int parsed_cmd_length = cmd_end_ptr - cmd_start_ptr - 1;

	if (parsed_cmd_length <= CLOUD_CMD_MAX_LENGTH) {
		memcpy(shell_cmd, cmd_start_ptr + 1, parsed_cmd_length);
	} else {
		mosh_error("Received cloud command exceeds the maximum permissible length %d",
			   CLOUD_CMD_MAX_LENGTH);
	}

	return true;
}

static void nrf_cloud_event_handler(const struct nrf_cloud_evt *evt)
{
	int err = 0;
	const int reconnection_delay = 10;

	switch (evt->type) {
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTING:
		mosh_print("NRF_CLOUD_EVT_TRANSPORT_CONNECTING");
		break;
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTED:
		mosh_print("NRF_CLOUD_EVT_TRANSPORT_CONNECTED");
		break;
	case NRF_CLOUD_EVT_READY:
		mosh_print("NRF_CLOUD_EVT_READY");
		break;
	case NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED:
		mosh_print("NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED");
		if (!nfsm_get_disconnect_requested()) {
			mosh_print("Reconnecting in %d seconds...", reconnection_delay);
			k_work_reschedule(&cloud_reconnect_work, K_SECONDS(reconnection_delay));
		}
		break;
	case NRF_CLOUD_EVT_ERROR:
		mosh_print("NRF_CLOUD_EVT_ERROR");
		break;
	case NRF_CLOUD_EVT_SENSOR_DATA_ACK:
		mosh_print("NRF_CLOUD_EVT_SENSOR_DATA_ACK");
		break;
	case NRF_CLOUD_EVT_FOTA_DONE:
		mosh_print("NRF_CLOUD_EVT_FOTA_DONE");
		break;
	case NRF_CLOUD_EVT_RX_DATA:
		mosh_print("NRF_CLOUD_EVT_RX_DATA");
		if (((char *)evt->data.ptr)[0] == '{') {
			/* Not A-GPS data. Check if it's a MoSh command sent from the cloud */
			mosh_print("%s", evt->data.ptr);

			bool cmd_found = cloud_shell_parse_mosh_cmd(evt->data.ptr);

			if (cmd_found) {
				k_work_submit(&cloud_cmd_work);
			}
			break;
		}
#if defined(CONFIG_NRF_CLOUD_AGPS)
		err = nrf_cloud_agps_process((char *)evt->data.ptr, evt->data.len);
		if (!err) {
			mosh_print("A-GPS data processed");
#if defined(CONFIG_NRF_CLOUD_PGPS)
			/* call us back when prediction is ready */
			k_work_submit(&notify_pgps_work);
#endif
			/* data was valid; no need to pass to other handlers */
			break;
		}
#endif
#if defined(CONFIG_NRF_CLOUD_PGPS)
		err = nrf_cloud_pgps_process(evt->data.ptr, evt->data.len);
		if (err) {
			mosh_error("Error processing P-GPS packet: %d", err);
		}
#else
		if (err) {
			mosh_print("Unable to process A-GPS data, error: %d", err);
		}
#endif
		break;
	case NRF_CLOUD_EVT_USER_ASSOCIATION_REQUEST:
		mosh_print("NRF_CLOUD_EVT_USER_ASSOCIATION_REQUEST");
		mosh_warn("Add the device to nRF Cloud and reconnect");
		break;
	case NRF_CLOUD_EVT_USER_ASSOCIATED:
		mosh_print("NRF_CLOUD_EVT_USER_ASSOCIATED");
		break;
	default:
		mosh_error("Unknown nRF Cloud event type: %d", evt->type);
		break;
	}
}

static void cmd_cloud_connect(const struct shell *shell, size_t argc, char **argv)
{
	int err;
	static bool initialized;

	if (!initialized) {
		struct nrf_cloud_init_param config = {
			.event_handler = nrf_cloud_event_handler,
		};

		err = nrf_cloud_init(&config);
		if (err == -EACCES) {
			mosh_print("nRF Cloud module already initialized");
		} else if (err) {
			mosh_error("nrf_cloud_init, error: %d", err);
			return;
		}

		initialized = true;

		k_work_init(&cloud_cmd_work, cloud_cmd_execute);
#if defined(CONFIG_NRF_CLOUD_PGPS)
		k_work_init(&notify_pgps_work, notify_pgps);
#endif
		k_work_init_delayable(&cloud_reconnect_work, cloud_reconnect_work_fn);
	}

	k_work_reschedule(&cloud_reconnect_work, K_NO_WAIT);

	mosh_print("Endpoint: %s", CONFIG_NRF_CLOUD_HOST_NAME);
}

static void cmd_cloud_disconnect(const struct shell *shell, size_t argc, char **argv)
{
	int err = nrf_cloud_disconnect();

	if (err == -EACCES) {
		mosh_print("Not connected to nRF Cloud");
	} else if (err) {
		mosh_error("nrf_cloud_disconnect, error: %d", err);
		return;
	}
}

static int cmd_cloud(const struct shell *shell, size_t argc, char **argv)
{
	return cloud_shell_print_usage(shell, argc, argv);
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_cloud,
	SHELL_CMD_ARG(connect, NULL, "Establish MQTT connection to nRF Cloud.", cmd_cloud_connect,
		      1, 0),
	SHELL_CMD_ARG(disconnect, NULL, "Disconnect from nRF Cloud.", cmd_cloud_disconnect, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(cloud, &sub_cloud, "MQTT connection to nRF Cloud", cmd_cloud);
