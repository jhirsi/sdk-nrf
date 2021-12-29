/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <modem/lte_lc.h>
#include <cJSON.h>
#include <cJSON_os.h>

#include <logging/log.h>

#include <net/rest_client.h>

#include "integrations.h"

LOG_MODULE_DECLARE(metrics, CONFIG_METRICS_LOG_LEVEL);

#define HOSTNAME CONFIG_METRICS_CLOUD_THINGSPEAK_HOSTNAME

BUILD_ASSERT(sizeof(HOSTNAME) > 1, "Hostname must be configured");

#define METRICS_WRITE_PATH "/update"
#define REQUEST_BASE_URL_POST METRICS_WRITE_PATH

#define HEADER_CONTENT_TYPE "Content-Type: application/json\r\n"
#define HEADER_CONNECTION_CLOSE "Connection: close\r\n"
#define HTTP_PORT 80

/******************************************************************************/

static int thingspeak_rest_format_post_write_req_body(const struct location_metrics *metrics_data,
						      char **json_str_out)
{
	int err = 0;
	cJSON *req_obj = cJSON_CreateObject();
	char tmp[64];

	if (!metrics_data || !json_str_out || !req_obj) {
		return -EINVAL;
	}

	/*
	 * Supported for channel settings for custom fields:
	 *
	 * "field1": "device_imei (string)",
	 * "field2": "operator_plmn (string)",
	 * "field3": "ncellmeas_at_notif (string)",
	 * "field4": "modem_info (string, format: <key1>=<value1>,<key2>=<value2>),
	 * "field5": "latitude (double number)",
	 * "field6": "longitude (double number)",
	 * "field7": "accuracy (double number)",
	 * "field8": "location_method (string)"
	 */

	if (!cJSON_AddStringToObjectCS(req_obj, "api_key",
				       CONFIG_METRICS_CLOUD_THINGSPEAK_WRITE_API_KEY)) {
		goto cleanup;
	}

	if (!cJSON_AddStringToObjectCS(req_obj, "field1", metrics_data->device_imei_str)) {
		goto cleanup;
	}

	if (metrics_data->cell_data.current_cell.id) {
		sprintf(tmp, "%d%d", metrics_data->cell_data.current_cell.mcc,
			metrics_data->cell_data.current_cell.mnc);

		if (!cJSON_AddStringToObjectCS(req_obj, "field2", tmp)) {
			goto cleanup;
		}

		if (!cJSON_AddStringToObjectCS(req_obj, "field3",
					       ((metrics_data->ncell_meas_notif_str != NULL) ?
							metrics_data->ncell_meas_notif_str :
							"Not available"))) {
			goto cleanup;
		}

		sprintf(tmp, "vbat=%d,temp=%f", metrics_data->bat_voltage,
			metrics_data->temperature);
		if (!cJSON_AddStringToObjectCS(req_obj, "field4", tmp)) {
			goto cleanup;
		}
	}

	if (!cJSON_AddNumberToObjectCS(req_obj, "field5",
				       metrics_data->location_data.location.latitude)) {
		goto cleanup;
	}

	if (!cJSON_AddNumberToObjectCS(req_obj, "field6",
				       metrics_data->location_data.location.longitude)) {
		goto cleanup;
	}

	if (!cJSON_AddNumberToObjectCS(req_obj, "field7",
				       metrics_data->location_data.location.accuracy)) {
		goto cleanup;
	}
	if (!cJSON_AddStringToObjectCS(
		    req_obj, "field8",
		    location_method_str(metrics_data->location_data.location.method))) {
		goto cleanup;
	}

	*json_str_out = cJSON_PrintUnformatted(req_obj);
	if (*json_str_out == NULL) {
		err = -ENOMEM;
	}
	return err;

cleanup:
	cJSON_Delete(req_obj);
	return -ENOMEM;
	;
}

int rest_integration_metrics_data_send(const struct location_metrics *metrics_data)
{
	__ASSERT_NO_MSG(metrics_data != NULL);

	struct rest_client_req_context req_ctx = { 0 };
	struct rest_client_resp_context resp_ctx = { 0 };
	int ret = 0;
	char *body = NULL;
	char recv_buf[1024];
	char *const headers[] = { HEADER_CONTENT_TYPE, HEADER_CONNECTION_CLOSE,
				  /* Note: Host and Content-length set by http_client */
				  NULL };

	ret = thingspeak_rest_format_post_write_req_body(metrics_data, &body);
	if (ret) {
		LOG_ERR("Failed to format Thingspeak write REST request body, err %d", ret);
		goto clean_up;
	}

	/* Set the defaults: */
	rest_client_request_defaults_set(&req_ctx);
	req_ctx.http_method = HTTP_POST;
	req_ctx.url = REQUEST_BASE_URL_POST;
	req_ctx.port = HTTP_PORT;
	req_ctx.host = HOSTNAME;
	req_ctx.header_fields = (const char **)headers;
	req_ctx.resp_buff = recv_buf;
	req_ctx.resp_buff_len = sizeof(recv_buf);
	req_ctx.body = body;

	ret = rest_client_request(&req_ctx, &resp_ctx);
	if (ret) {
		LOG_ERR("Error from rest_client lib, err: %d", ret);
		goto clean_up;
	}

	if (resp_ctx.http_status_code != REST_CLIENT_HTTP_STATUS_OK) {
		LOG_ERR("HTTP status: %d", resp_ctx.http_status_code);
	}

	LOG_DBG("Received response body, in this case indicating an entry_id of stored metrics: %s",
		log_strdup(resp_ctx.response));

clean_up:
	if (body) {
		cJSON_free(body);
	}

	return ret;
}
