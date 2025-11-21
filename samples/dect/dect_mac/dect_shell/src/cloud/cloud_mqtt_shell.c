/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <net/nrf_cloud.h>
#include <nrf_cloud_fsm.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/base64.h>
#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
#include <nrf_cloud_transport.h>
#endif

#if defined(CONFIG_DESH_NATIVE_TLS) && !defined(CONFIG_NRF_CLOUD_PROVISION_CERTIFICATES)
#include "desh_native_tls.h"
#endif
#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
#include <net/l2_dect_nrp/rpc/common/dect_rpc_mqtt_transport.h>
#include <net/l2_dect_nrp/rpc/server/dect_rpc_server.h>
#include <nrf_rpc.h>
#include <zephyr/net/mqtt.h>
#endif
#include "desh_print.h"

#define CLOUD_CMD_MAX_LENGTH 150

BUILD_ASSERT(IS_ENABLED(CONFIG_NRF_CLOUD_MQTT));
BUILD_ASSERT(!IS_ENABLED(CONFIG_NRF_CLOUD_COAP));

extern const struct shell *desh_shell;
extern struct k_work_q desh_common_work_q;

#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
static void dect_rpc_err_handler(const struct nrf_rpc_err_report *report)
{
	desh_error("nRF RPC error %d occurred", report->code);
}
#endif

static struct k_work_delayable cloud_reconnect_work;
static struct k_work cloud_cmd_execute_work;
static struct k_work shadow_update_work;
#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
static struct k_work dect_rpc_process_work;
static struct nrf_cloud_data dect_rpc_pending_data;
static bool dect_rpc_data_pending = false;
#endif

static char shell_cmd[CLOUD_CMD_MAX_LENGTH + 1];
static void cmd_cloud_disconnect(const struct shell *shell, size_t argc, char **argv);

static void cloud_reconnect_work_fn(struct k_work *work)
{
	int err = nrf_cloud_connect();

	if (err == NRF_CLOUD_CONNECT_RES_SUCCESS) {
		desh_print("Connecting to nRF Cloud...");
	} else if (err == NRF_CLOUD_CONNECT_RES_ERR_ALREADY_CONNECTED) {
		desh_print("nRF Cloud connection already established");
	} else {
		desh_error("nrf_cloud_connect, error: %d", err);
	}
}

static K_WORK_DELAYABLE_DEFINE(cloud_reconnect_work, cloud_reconnect_work_fn);

static void cloud_cmd_execute_work_fn(struct k_work *work)
{
	shell_execute_cmd(desh_shell, shell_cmd);
	memset(shell_cmd, 0, CLOUD_CMD_MAX_LENGTH);
}

static K_WORK_DEFINE(cloud_cmd_execute_work, cloud_cmd_execute_work_fn);

#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
/**
 * @brief Process DECT RPC data in work queue context
 */
static void dect_rpc_process_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!dect_rpc_data_pending) {
		return;
	}

	/* Process the RPC data */
	if (dect_rpc_mqtt_transport_handle_rx_data(&dect_rpc_pending_data)) {
		desh_print("DECT RPC packet received and handled");
	} else {
		desh_print("DECT RPC packet not handled");
	}

	/* Free the decoded buffer */
	if (dect_rpc_pending_data.ptr) {
		k_free((void *)dect_rpc_pending_data.ptr);
		dect_rpc_pending_data.ptr = NULL;
		dect_rpc_pending_data.len = 0;
	}

	dect_rpc_data_pending = false;
}

static K_WORK_DEFINE(dect_rpc_process_work, dect_rpc_process_work_fn);
/**
 * @brief Parse JSON with base64-encoded DECT RPC data
 *
 * Format: {"appId":"DECT_RPC", "data":"<base64_string>"}
 *
 * @param buf_in JSON string
 * @param out_data Output structure with decoded binary data
 * @return true if JSON contains DECT_RPC base64 data, false otherwise
 */
static bool cloud_shell_parse_dect_rpc_json(const char *buf_in,
					     struct nrf_cloud_data *out_data)
{
	const cJSON *app_id = NULL;
	const cJSON *data = NULL;
	bool ret = false;
	uint8_t *decoded_buf = NULL;
	size_t decoded_len = 0;
	size_t base64_len;
	cJSON *json = NULL;

	/* cJSON_Parse requires null-terminated string, but buf_in might not be null-terminated
	 * if it comes from MQTT payload. We need to check if it's already null-terminated
	 * or create a null-terminated copy.
	 * For now, assume buf_in is null-terminated (nRF Cloud library should ensure this)
	 */
	json = cJSON_Parse(buf_in);
	if (json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL) {
			desh_error("DECT RPC JSON parsing error: %s", error_ptr);
		} else {
			desh_error("DECT RPC JSON parsing failed (null pointer)");
		}
		return false;
	}

	/* Check if appId is "DECT_RPC" */
	app_id = cJSON_GetObjectItemCaseSensitive(json, NRF_CLOUD_JSON_APPID_KEY);
	if (!cJSON_IsString(app_id) || app_id->valuestring == NULL) {
		goto cleanup;
	}

	if (strcmp(app_id->valuestring, "DECT_RPC") != 0) {
		goto cleanup;
	}

	/* Get base64 data field */
	data = cJSON_GetObjectItemCaseSensitive(json, NRF_CLOUD_JSON_DATA_KEY);
	if (!cJSON_IsString(data) || data->valuestring == NULL) {
		goto cleanup;
	}

	base64_len = strlen(data->valuestring);
	if (base64_len == 0) {
		goto cleanup;
	}

	/* Calculate decoded size (base64: 4 chars = 3 bytes) */
	decoded_len = (base64_len * 3) / 4;
	if (decoded_len == 0) {
		goto cleanup;
	}

	/* Allocate buffer for decoded data */
	decoded_buf = k_malloc(decoded_len);
	if (!decoded_buf) {
		desh_error("Failed to allocate buffer for base64 decode");
		goto cleanup;
	}

	/* Decode base64 to binary */
	int err = base64_decode(decoded_buf, decoded_len, &decoded_len,
				(const uint8_t *)data->valuestring, base64_len);
	if (err < 0) {
		desh_error("Base64 decode failed: %d", err);
		k_free(decoded_buf);
		decoded_buf = NULL;
		goto cleanup;
	}

	/* Set output data */
	out_data->ptr = decoded_buf;
	out_data->len = decoded_len;
	ret = true;

cleanup:
	cJSON_Delete(json);
	return ret;
}
#endif /* CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT */

static bool cloud_shell_parse_desh_cmd(const char *buf_in)
{
	const cJSON *app_id = NULL;
	const cJSON *desh_cmd = NULL;
	bool ret = false;

	cJSON *cloud_cmd_json = cJSON_Parse(buf_in);

	/* A format example expected from nrf cloud:
	 * {"appId":"DECT_SHELL", "data":"version"}
	 */
	if (cloud_cmd_json == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();

		if (error_ptr != NULL) {
			desh_error("JSON parsing error before: %s\n", error_ptr);
		}
		ret = false;
		goto end;
	}

	/* DeSh commands are identified by checking if appId equals "DECT_SHELL" */
	app_id = cJSON_GetObjectItemCaseSensitive(cloud_cmd_json, NRF_CLOUD_JSON_APPID_KEY);
	if (cJSON_IsString(app_id) && (app_id->valuestring != NULL)) {
		if (strcmp(app_id->valuestring, "DECT_SHELL") != 0) {
			ret = false;
			goto end;
		}
	}

	/* The value of attribute "data" contains the actual command */
	desh_cmd = cJSON_GetObjectItemCaseSensitive(cloud_cmd_json, NRF_CLOUD_JSON_DATA_KEY);
	if (cJSON_IsString(desh_cmd) && (desh_cmd->valuestring != NULL)) {
		desh_print("%s", desh_cmd->valuestring);
		if (strlen(desh_cmd->valuestring) <= CLOUD_CMD_MAX_LENGTH) {
			strcpy(shell_cmd, desh_cmd->valuestring);
			ret = true;
		} else {
			desh_error("Received cloud command exceeds maximum permissible length %d",
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
static void nrf_cloud_update_shadow_work_fn(struct k_work *work)
{
	int err;
	struct nrf_cloud_svc_info_ui ui_info = {
#if defined(CONFIG_LOCATION)
		.gnss = true, /* Show map on nrf cloud */
#else
		.gnss = false,
#endif
	};
	struct nrf_cloud_svc_info service_info = {.ui = &ui_info};
#if defined(CONFIG_MODEM_INFO)
	struct nrf_cloud_modem_info modem_info = {
		.device = NRF_CLOUD_INFO_SET, .mpi = NULL /* Modem data will be fetched */
	};
#endif
	struct nrf_cloud_device_status device_status = {
#if defined(CONFIG_MODEM_INFO)
		.modem = &modem_info,
#endif
		.svc = &service_info
	};

	ARG_UNUSED(work);
	err = nrf_cloud_shadow_device_status_update(&device_status);
	if (err) {
		shell_error(desh_shell, "Failed to update device shadow, error: %d", err);
	} else {
		shell_print(desh_shell, "Device shadow updated");
	}
}

static K_WORK_DEFINE(shadow_update_work, nrf_cloud_update_shadow_work_fn);

static void nrf_cloud_event_handler(const struct nrf_cloud_evt *evt)
{
	switch (evt->type) {
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTED:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_TRANSPORT_CONNECTED");
		break;
	case NRF_CLOUD_EVT_TRANSPORT_CONNECTING:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_TRANSPORT_CONNECTING");
		break;
	case NRF_CLOUD_EVT_USER_ASSOCIATION_REQUEST:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_USER_ASSOCIATION_REQUEST");
		desh_warn("Add the device to nRF Cloud and reconnect");
		break;
	case NRF_CLOUD_EVT_USER_ASSOCIATED:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_USER_ASSOCIATED");
		break;
	case NRF_CLOUD_EVT_READY:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_READY");
		desh_print("Connection to nRF Cloud established");
		k_work_submit_to_queue(&desh_common_work_q, &shadow_update_work);
#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
			/* Check DC_RX topic and log it for debugging */
			{
				struct nct_dc_endpoints eps;
				nct_dc_endpoint_get(&eps);
				
				if (eps.e[DC_RX].utf8 != NULL) {
					desh_print("DECT RPC: Subscribed to topic: %.*s",
							   eps.e[DC_RX].size,
							   (const char *)eps.e[DC_RX].utf8);
				}
			}
#endif
		break;
	case NRF_CLOUD_EVT_RX_DATA_GENERAL:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_RX_DATA_GENERAL");
#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
		/* First, check if it's raw binary RPC packet */
		/* This handles /c2d topic (device subscribes to pattern /+/r which matches /c2d/r) */
		if (dect_rpc_mqtt_transport_handle_rx_data(&evt->data)) {
			/* Data was handled by DECT RPC transport */
			desh_print("DECT RPC packet received and handled");
			break;
		}

		/* If it's JSON, check if it contains base64-encoded RPC data */
		/* REST API might wrap binary data in JSON */
		if (((char *)evt->data.ptr)[0] == '{') {
			struct nrf_cloud_data decoded_data;
			char *json_str = NULL;

			/* Log received JSON data for debugging */
			desh_print("Received JSON data on /c2d topic, len: %zu", evt->data.len);
			if (evt->data.len < 100) {
				desh_print("JSON data: %.*s", evt->data.len, (const char *)evt->data.ptr);
			}

			/* cJSON_Parse requires null-terminated string.
			 * Allocate a buffer and copy the data with null terminator.
			 */
			json_str = k_malloc(evt->data.len + 1);
			if (!json_str) {
				desh_error("Failed to allocate JSON buffer");
				break;
			}
			memcpy(json_str, evt->data.ptr, evt->data.len);
			json_str[evt->data.len] = '\0';

			if (cloud_shell_parse_dect_rpc_json(json_str,
							    &decoded_data)) {
				/* Free JSON buffer after parsing */
				k_free(json_str);
				json_str = NULL;

				/* Decoded base64 RPC data, schedule processing in work queue */
				desh_print("DECT RPC base64 data decoded from JSON");
				
				/* Check if previous data is still pending */
				if (dect_rpc_data_pending) {
					desh_error("Previous DECT RPC data still pending, dropping");
					k_free((void *)dect_rpc_pending_data.ptr);
				}
				
				/* Store decoded data for processing in work queue */
				dect_rpc_pending_data = decoded_data;
				dect_rpc_data_pending = true;
				
				/* Schedule processing in work queue (safe context) */
				k_work_submit_to_queue(&desh_common_work_q,
						       &dect_rpc_process_work);
				break;
			} else {
				/* JSON parsing failed, free buffer */
				if (json_str) {
					k_free(json_str);
				}
			}
		}
#endif
		if (((char *)evt->data.ptr)[0] == '{') {
			/* Check if it's a DeSh command sent from the cloud */
			if (cloud_shell_parse_desh_cmd(evt->data.ptr)) {
				k_work_submit_to_queue(&desh_common_work_q,
						       &cloud_cmd_execute_work);
			}
		}
		break;
	case NRF_CLOUD_EVT_RX_DATA_LOCATION:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_RX_DATA_LOCATION");
		break;
	case NRF_CLOUD_EVT_RX_DATA_SHADOW:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_RX_DATA_SHADOW");
		break;
	case NRF_CLOUD_EVT_PINGRESP:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_PINGRESP");
		break;
	case NRF_CLOUD_EVT_SENSOR_DATA_ACK:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_SENSOR_DATA_ACK");
		break;
	case NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED: {
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_TRANSPORT_DISCONNECTED, status: %d",
			   evt->status);
		desh_print("Connection to nRF Cloud disconnected");
		/* Stop possibly pending reconnection attempt. */
		k_work_cancel_delayable(&cloud_reconnect_work);

		int err = nrf_cloud_disconnect();

		if (err == -EACCES) {
			desh_print("Not connected to nRF Cloud");
		} else if (err) {
			desh_error("nrf_cloud_disconnect, error: %d", err);
		}
		break;
	}
	case NRF_CLOUD_EVT_FOTA_START:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_FOTA_START");
		break;
	case NRF_CLOUD_EVT_FOTA_DONE:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_FOTA_DONE");
		break;
	case NRF_CLOUD_EVT_FOTA_ERROR:
		desh_error("nRF Cloud event: NRF_CLOUD_EVT_FOTA_ERROR");
		break;
	case NRF_CLOUD_EVT_TRANSPORT_CONNECT_ERROR:
		desh_error("nRF Cloud event: NRF_CLOUD_EVT_TRANSPORT_CONNECT_ERROR, status: %d",
			   evt->status);
		desh_error("Connecting to nRF Cloud failed");
		break;
	case NRF_CLOUD_EVT_ERROR:
		desh_print("nRF Cloud event: NRF_CLOUD_EVT_ERROR, status: %d", evt->status);
		break;
	default:
		desh_error("Unknown nRF Cloud event type: %d", evt->type);
		break;
	}
}

static void cmd_cloud_connect(const struct shell *shell, size_t argc, char **argv)
{
	int err;
	static bool initialized;

	if (!initialized) {
		struct nrf_cloud_os_mem_hooks hooks = {
			.malloc_fn = k_malloc,
			.calloc_fn = k_calloc,
			.free_fn = k_free,
		};

		struct nrf_cloud_init_param config = {
			.event_handler = nrf_cloud_event_handler,
			.hooks = &hooks,
			.application_version = desh_print_version_str_get(),
		};

#if defined(CONFIG_DESH_NATIVE_TLS) && !defined(CONFIG_NRF_CLOUD_PROVISION_CERTIFICATES)
		err = desh_native_tls_load_credentials(CONFIG_NRF_CLOUD_SEC_TAG);
		if (err) {
			desh_error("%s: failed to load credentials, error: %d", (__func__), err);
		}
#endif
		err = nrf_cloud_init(&config);
		if (err == -EACCES) {
			desh_print("nRF Cloud module already initialized");
		} else if (err) {
			desh_error("nrf_cloud_init, error: %d", err);
			return;
		}

#if defined(CONFIG_DECT_NRP_RPC_MQTT_TRANSPORT)
		/* Initialize nRF RPC and DECT RPC server after nRF Cloud is initialized */
		err = nrf_rpc_init(dect_rpc_err_handler);
		if (err != 0) {
			desh_error("nRF RPC init failed: %d", err);
		} else {
			err = dect_rpc_server_init();
			if (err != 0) {
				desh_error("DECT RPC server init failed: %d", err);
			} else {
				desh_print("DECT RPC server initialized");
			}
		}
		desh_print("nRF RPC and DECT RPC server initialized");
#endif

		initialized = true;
	}

	k_work_reschedule(&cloud_reconnect_work, K_NO_WAIT);

	desh_print("Endpoint: %s", CONFIG_NRF_CLOUD_HOST_NAME);
}

static void cmd_cloud_disconnect(const struct shell *shell, size_t argc, char **argv)
{
	int err;

	/* Stop possibly pending reconnection attempt. */
	k_work_cancel_delayable(&cloud_reconnect_work);

	err = nrf_cloud_disconnect();
	if (err == -EACCES) {
		desh_print("Not connected to nRF Cloud");
	} else if (err) {
		desh_error("nrf_cloud_disconnect, error: %d", err);
	}
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_cloud,
			       SHELL_CMD_ARG(connect, NULL,
					     "Establish MQTT connection to nRF Cloud.",
					     cmd_cloud_connect, 1, 0),
			       SHELL_CMD_ARG(disconnect, NULL, "Disconnect from nRF Cloud.",
					     cmd_cloud_disconnect, 1, 0),
			       SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(cloud, &sub_cloud, "MQTT connection to nRF Cloud", desh_print_help_shell);
