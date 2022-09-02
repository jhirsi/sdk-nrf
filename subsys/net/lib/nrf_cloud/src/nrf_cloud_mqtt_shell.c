/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <net/nrf_cloud.h>
#include <nrf_cloud_fsm.h>
#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

#if defined(CONFIG_NRF_CLOUD_AGPS)
#include <net/nrf_cloud_agps.h>
#endif
#if defined(CONFIG_NRF_CLOUD_PGPS)
#include <net/nrf_cloud_pgps.h>
#endif

#define CLOUD_CMD_MAX_LENGTH 150

BUILD_ASSERT(IS_ENABLED(CONFIG_NRF_CLOUD_MQTT) &&
	     IS_ENABLED(CONFIG_NRF_CLOUD_CONNECTION_POLL_THREAD));

static struct k_work_delayable cloud_reconnect_work;
#if defined(CONFIG_NRF_CLOUD_PGPS)
static struct k_work notify_pgps_work;
#endif
static struct k_work cloud_cmd_work;
static struct k_work shadow_update_work;

static char shell_cmd[CLOUD_CMD_MAX_LENGTH + 1];

static int cloud_shell_print_usage(const struct shell *shell, size_t argc, char **argv)
{
	int ret = 1;

	if (argc > 1) {
		shell_error(shell, "%s: subcommand not found", argv[1]);
		ret = -EINVAL;
	}

	shell_help(shell);

	return ret;
}

static void cloud_reconnect_work_fn(struct k_work *work)
{
	int err = nrf_cloud_connect(NULL);
	const struct shell *shell = shell_backend_uart_get_ptr();

	if (err == NRF_CLOUD_CONNECT_RES_SUCCESS) {
		shell_print(shell, "Connecting to nRF Cloud...");
	} else if (err == NRF_CLOUD_CONNECT_RES_ERR_ALREADY_CONNECTED) {
		shell_print(shell, "nRF Cloud connection already established");
	} else {
		shell_error(shell, "nrf_cloud_connect, error: %d", err);
	}
}

#if defined(CONFIG_NRF_CLOUD_PGPS)
static void notify_pgps(struct k_work *work)
{
	ARG_UNUSED(work);
	int err;
	const struct shell *shell = shell_backend_uart_get_ptr();

	err = nrf_cloud_pgps_notify_prediction();
	if (err) {
		shell_error(shell, "Error requesting notification of prediction availability: %d",
			    err);
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
	const cJSON *app_id = NULL;
	const cJSON *mosh_cmd = NULL;
	bool ret = false;
	const struct shell *shell = shell_backend_uart_get_ptr();

	cJSON *cloud_cmd_json = cJSON_Parse(buf_in);

	/* A format example expected from nrf cloud:
	 * {"appId":"MODEM_SHELL", "data":"location get"}
	 */
	if (cloud_cmd_json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();

		if (error_ptr != NULL) {
			shell_error(shell, "JSON parsing error before: %s\n", error_ptr);
		}
		ret = false;
		goto end;
	}

	/* MoSh commands are identified by checking if appId equals "MODEM_SHELL" */
	app_id = cJSON_GetObjectItemCaseSensitive(cloud_cmd_json, "appId");
	if (cJSON_IsString(app_id) && (app_id->valuestring != NULL)) {
		if (strcmp(app_id->valuestring, "MODEM_SHELL") != 0) {
			ret = false;
			goto end;
		}
	}

	/* The value of attribute "data" contains the actual command */
	mosh_cmd = cJSON_GetObjectItemCaseSensitive(cloud_cmd_json, "data");
	if (cJSON_IsString(mosh_cmd) && (mosh_cmd->valuestring != NULL)) {
		shell_print(shell, "%s", mosh_cmd->valuestring);
		if (strlen(mosh_cmd->valuestring) <= CLOUD_CMD_MAX_LENGTH) {
			strcpy(shell_cmd, mosh_cmd->valuestring);
			ret = true;
		} else {
			shell_error(shell,
				    "Received cloud command exceeds maximum permissible length %d",
				    CLOUD_CMD_MAX_LENGTH);
		}
	}
end:
	cJSON_Delete(cloud_cmd_json);
	return ret;
}

/**
 * @brief Updates the nRF Cloud shadow with information about supported capabilities.
 */
static void nrf_cloud_update_shadow(struct k_work *work)
{
	int err;
	struct nrf_cloud_svc_info_ui ui_info = {
#if defined(CONFIG_LOCATION)
		.gps = true, /* Show map on nrf cloud */
#else
		.gps = false,
#endif
	};
	struct nrf_cloud_svc_info service_info = { .ui = &ui_info };
#if defined(CONFIG_MODEM_INFO)
	struct nrf_cloud_modem_info modem_info = {
		.device = NRF_CLOUD_INFO_SET,
		.network = NRF_CLOUD_INFO_SET,
		.sim = NRF_CLOUD_INFO_SET,
		.mpi = NULL /* Modem data will be fetched */
	};
#endif
	struct nrf_cloud_device_status device_status = {
#if defined(CONFIG_MODEM_INFO)
		.modem = &modem_info,
#endif
		.svc = &service_info
	};
	const struct shell *shell = shell_backend_uart_get_ptr();

	ARG_UNUSED(work);
	err = nrf_cloud_shadow_device_status_update(&device_status);
	if (err) {
		shell_error(shell, "Failed to update device shadow, error: %d", err);
	}
}

static void nrf_cloud_event_handler(const struct nrf_cloud_evt *evt)
{
	int err = 0;
	const int reconnection_delay = 10;
	const struct shell *shell = shell_backend_uart_get_ptr();

	switch (evt->type) {
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTING:
		shell_print(shell, "NRF_CLOUD_EVT_TRANSPORT_CONNECTING");
		break;
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTED:
		shell_print(shell, "NRF_CLOUD_EVT_TRANSPORT_CONNECTED");
		break;
	case NRF_CLOUD_EVT_READY:
		shell_print(shell, "NRF_CLOUD_EVT_READY: Connection to nRF Cloud established");
		k_work_submit(&shadow_update_work);
		break;
	case NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED:
		shell_print(shell, "NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED: Connection to nRF Cloud "
				   "disconnected");
		if (!nfsm_get_disconnect_requested()) {
			shell_print(shell, "Reconnecting in %d seconds...", reconnection_delay);
			k_work_reschedule(&cloud_reconnect_work, K_SECONDS(reconnection_delay));
		}
		break;
	case NRF_CLOUD_EVT_ERROR:
		shell_print(shell, "NRF_CLOUD_EVT_ERROR");
		break;
	case NRF_CLOUD_EVT_SENSOR_DATA_ACK:
		shell_print(shell, "NRF_CLOUD_EVT_SENSOR_DATA_ACK");
		break;
	case NRF_CLOUD_EVT_FOTA_START:
		shell_print(shell, "NRF_CLOUD_EVT_FOTA_START");
		break;
	case NRF_CLOUD_EVT_FOTA_DONE:
		shell_print(shell, "NRF_CLOUD_EVT_FOTA_DONE");
		break;
	case NRF_CLOUD_EVT_FOTA_ERROR:
		shell_print(shell, "NRF_CLOUD_EVT_FOTA_ERROR");
		break;
	case NRF_CLOUD_EVT_RX_DATA:
		shell_print(shell, "NRF_CLOUD_EVT_RX_DATA");
		shell_print(shell, "  Data received on topic len: %d: %s",
			evt->topic.len, (char *)evt->topic.ptr);
		shell_print(shell, "  Data len: %d: %s",
			evt->data.len, (char *)evt->data.ptr);

		if (((char *)evt->data.ptr)[0] == '{') {
			/* Check if it's a MoSh command sent from the cloud */
			bool cmd_found = cloud_shell_parse_mosh_cmd(evt->data.ptr);

			if (cmd_found) {
				k_work_submit(&cloud_cmd_work);
				break;
			}
		}
#if defined(CONFIG_NRF_CLOUD_AGPS)
		err = nrf_cloud_agps_process((char *)evt->data.ptr, evt->data.len);
		if (!err) {
			shell_print(shell, "A-GPS data processed");
#if defined(CONFIG_NRF_CLOUD_PGPS)
			/* call us back when prediction is ready */
			k_work_submit(&notify_pgps_work);
#endif
			/* data was valid; no need to pass to other handlers */
			break;
		} else if (err == -EFAULT) {
			/* data were an A-GPS error; no need to pass to other handlers */
			break;
		}
#endif
#if defined(CONFIG_NRF_CLOUD_PGPS)
		/* Check if we are waiting for P-GPS data in order to avoid parsing other data as
		 * P-GPS data and showing irrelevant error messages.
		 */
		if (nrf_cloud_pgps_loading()) {
			err = nrf_cloud_pgps_process(evt->data.ptr, evt->data.len);
			if (err == -EFAULT) {
				/* P-GPS error code from nRF Cloud was received */
			} else if (err) {
				shell_error(shell, "Error processing P-GPS packet: %d", err);
			}
			break;
		}
#endif
		/* P-GPS is not compiled in or we are not expecting P-GPS data,
		 * finish A-GPS error handling
		 */
		if (err == -ENOMSG) {
			/* JSON data received that were not an A-GPS error either.
			 * This message was likely just not AGPS-related so don't print an error.
			 */
		} else if (err) {
			shell_print(shell, "Unable to process A-GPS data, error: %d", err);
		}
		break;
	case NRF_CLOUD_EVT_USER_ASSOCIATION_REQUEST:
		shell_print(shell, "NRF_CLOUD_EVT_USER_ASSOCIATION_REQUEST");
		shell_warn(shell, "Add the device to nRF Cloud and reconnect");
		break;
	case NRF_CLOUD_EVT_USER_ASSOCIATED:
		shell_print(shell, "NRF_CLOUD_EVT_USER_ASSOCIATED");
		break;
	case NRF_CLOUD_EVT_PINGRESP:
		shell_print(shell, "NRF_CLOUD_EVT_PINGRESP");
		break;
	default:
		shell_print(shell, "Unknown nRF Cloud event type: %d", evt->type);
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
			shell_print(shell, "nRF Cloud module already initialized");
		} else if (err) {
			shell_error(shell, "nrf_cloud_init, error: %d", err);
			return;
		}

		initialized = true;

		k_work_init(&cloud_cmd_work, cloud_cmd_execute);
		k_work_init(&shadow_update_work, nrf_cloud_update_shadow);
#if defined(CONFIG_NRF_CLOUD_PGPS)
		k_work_init(&notify_pgps_work, notify_pgps);
#endif
		k_work_init_delayable(&cloud_reconnect_work, cloud_reconnect_work_fn);
	}

	k_work_reschedule(&cloud_reconnect_work, K_NO_WAIT);

	shell_print(shell, "Endpoint: %s", CONFIG_NRF_CLOUD_HOST_NAME);
}

static void cmd_cloud_disconnect(const struct shell *shell, size_t argc, char **argv)
{
	int err = nrf_cloud_disconnect();

	if (err == -EACCES) {
		shell_print(shell, "Not connected to nRF Cloud");
	} else if (err) {
		shell_error(shell, "nrf_cloud_disconnect, error: %d", err);
		return;
	}
}

static int cmd_cloud(const struct shell *shell, size_t argc, char **argv)
{
	return cloud_shell_print_usage(shell, argc, argv);
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_cloud,
			       SHELL_CMD_ARG(connect, NULL,
					     "Establish MQTT connection to nRF Cloud.",
					     cmd_cloud_connect, 1, 0),
			       SHELL_CMD_ARG(disconnect, NULL, "Disconnect from nRF Cloud.",
					     cmd_cloud_disconnect, 1, 0),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(cloud, &sub_cloud, "MQTT connection to nRF Cloud", cmd_cloud);
