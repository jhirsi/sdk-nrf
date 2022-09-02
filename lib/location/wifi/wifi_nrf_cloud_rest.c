/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <modem/lte_lc.h>
#include <cJSON.h>
#include <cJSON_os.h>

#include <zephyr/logging/log.h>

#include <cJSON.h>

#include <net/rest_client.h>
#include <net/nrf_cloud.h>

#include "../location_utils.h"
#include "wifi_nrf_cloud_codec.h"
#include "wifi_nrf_cloud_rest.h"

LOG_MODULE_DECLARE(location, CONFIG_LOCATION_LOG_LEVEL);

/******************************************************************************/

#if defined(CONFIG_NRF_CLOUD_MQTT)
static K_SEM_DEFINE(location_ready, 0, 1);
static struct location_data wifi_pos_result;
int wifi_nrf_cloud_mqtt_rx_data_handler(const char *const rx_buf)
{
	int ret = wifi_nrf_cloud_mqtt_pos_response_parse(rx_buf, &wifi_pos_result);

	if (ret) {
		LOG_ERR("nRF Cloud mqtt response (%s) parsing failed, err: %d",
			rx_buf, ret);
		ret = -EBADMSG;
		k_sem_reset(&location_ready);
		return -1;
	}
	k_sem_give(&location_ready);

	return 0;
}

/******************************************************************************/

int wifi_nrf_cloud_mqtt_pos_get(char *rcv_buf, size_t rcv_buf_len,
				const struct rest_wifi_pos_request *request,
				struct location_data *result)
{
	__ASSERT_NO_MSG(request != NULL);
	__ASSERT_NO_MSG(result != NULL);
	__ASSERT_NO_MSG(rcv_buf != NULL);
	__ASSERT_NO_MSG(rcv_buf_len > 0);

	char *body = NULL;
	int ret = 0;

	k_sem_reset(&location_ready);

	ret = nrf_cloud_app_id_specific_rx_data_handler_set(wifi_nrf_cloud_mqtt_rx_data_handler,
							    NRF_CLOUD_JSON_APPID_VAL_WIFI_POS);
	if (ret) {
		LOG_ERR("Failed to set mqtt cb for wifi pos rx data, err: %d", ret);
		goto clean_up;
	}

	/* Get the body/payload to request: */
	ret = wifi_nrf_cloud_mqtt_format_pos_req_body(request->scanning_results,
						      request->wifi_scanning_result_count, &body);
	if (ret) {
		LOG_ERR("Failed to generate nrf cloud positioning request, err: %d", ret);
		goto clean_up;
	}

	struct nrf_cloud_tx_data mqtt_msg = {
		.data.ptr = body,
		.data.len = strlen(body),
		.qos = MQTT_QOS_1_AT_LEAST_ONCE,
		.topic_type = NRF_CLOUD_TOPIC_MESSAGE,
	};

	LOG_DBG("Sending Wi-Fi locationing request to nRF Cloud via MQTT, body: %s", body);

	ret = nrf_cloud_send(&mqtt_msg);
	if (ret) {
		LOG_ERR("MQTT: location request sending failed, %d", ret);
		goto clean_up;
	}

	/* Wait for the MQTT response */
	if (k_sem_take(&location_ready, K_MSEC(request->timeout_ms)) == -EAGAIN) {
		LOG_ERR("Wi-Fi MQTT positioning data request timed out or "
			"cloud did not return a location");
		ret  = -ETIMEDOUT;
		goto clean_up;
	}
	*result = wifi_pos_result;

clean_up:
	nrf_cloud_app_id_specific_rx_data_handler_set(NULL, NRF_CLOUD_JSON_APPID_VAL_WIFI_POS);

	if (body) {
		cJSON_free(body);
	}
	k_sem_reset(&location_ready);
	return ret;
}

#elif defined(CONFIG_NRF_CLOUD_REST)

#define HOSTNAME CONFIG_LOCATION_METHOD_WIFI_SERVICE_NRF_CLOUD_HOSTNAME

BUILD_ASSERT(sizeof(HOSTNAME) > 1, "Hostname must be configured");

#define REQUEST_URL "/v1/location/wifi"

/******************************************************************************/
#define AUTH_HDR_BEARER_TEMPLATE "Authorization: Bearer %s\r\n"

static int wifi_nrf_cloud_rest_generate_auth_header(const char *const tok, char **auth_hdr_out)
{
	if (!tok || !auth_hdr_out) {
		return -EINVAL;
	}

	int ret;
	size_t buff_size = sizeof(AUTH_HDR_BEARER_TEMPLATE) + strlen(tok);

	*auth_hdr_out = k_calloc(buff_size, 1);
	if (!*auth_hdr_out) {
		return -ENOMEM;
	}
	ret = snprintk(*auth_hdr_out, buff_size, AUTH_HDR_BEARER_TEMPLATE, tok);
	if (ret < 0 || ret >= buff_size) {
		k_free(*auth_hdr_out);
		*auth_hdr_out = NULL;
		return -ETXTBSY;
	}

	return 0;
}

/******************************************************************************/
#define HEADER_CONTENT_TYPE "Content-Type: application/json\r\n"
#define HTTPS_PORT 443

int wifi_nrf_cloud_rest_pos_get(char *rcv_buf, size_t rcv_buf_len,
				const struct rest_wifi_pos_request *request,
				struct location_data *result)
{
	__ASSERT_NO_MSG(request != NULL);
	__ASSERT_NO_MSG(result != NULL);
	__ASSERT_NO_MSG(rcv_buf != NULL);
	__ASSERT_NO_MSG(rcv_buf_len > 0);

	struct rest_client_req_context req_ctx = { 0 };
	struct rest_client_resp_context resp_ctx = { 0 };
	char *body = NULL;
	int ret = 0;
	char *auth_hdr = NULL;
	const char *jwt_str = NULL;

	jwt_str = location_utils_nrf_cloud_jwt_generate();
	if (jwt_str == NULL) {
		return -EFAULT;
	}

	/* Format auth header */
	ret = wifi_nrf_cloud_rest_generate_auth_header(jwt_str, &auth_hdr);
	if (ret) {
		LOG_ERR("Could not format HTTP auth header, err: %d", ret);
		goto clean_up;
	}
	char *const headers[] = { HEADER_CONTENT_TYPE,
				  (char *const)auth_hdr,
				  /* Note: Host and Content-length set by http_client */
				  NULL };

	/* Set the defaults: */
	rest_client_request_defaults_set(&req_ctx);
	req_ctx.http_method = HTTP_POST;
	req_ctx.url = REQUEST_URL;
	req_ctx.sec_tag = CONFIG_NRF_CLOUD_SEC_TAG;
	req_ctx.port = HTTPS_PORT;
	req_ctx.host = HOSTNAME;
	req_ctx.header_fields = (const char **)headers;
	req_ctx.resp_buff = rcv_buf;
	req_ctx.resp_buff_len = rcv_buf_len;
	req_ctx.timeout_ms = request->timeout_ms;

	/* Get the body/payload to request: */
	ret = wifi_nrf_cloud_rest_format_pos_req_body(request->scanning_results,
						      request->wifi_scanning_result_count, &body);
	if (ret) {
		LOG_ERR("Failed to generate nrf cloud positioning request, err: %d", ret);
		goto clean_up;
	}
	req_ctx.body = body;

	ret = rest_client_request(&req_ctx, &resp_ctx);
	if (ret) {
		LOG_ERR("Error from rest_client lib, err: %d", ret);
		goto clean_up;
	}

	if (resp_ctx.http_status_code != REST_CLIENT_HTTP_STATUS_OK) {
		LOG_ERR("HTTP status: %d", resp_ctx.http_status_code);
		/* Let it fail in parsing */
	}

	ret = wifi_nrf_cloud_rest_pos_response_parse(resp_ctx.response, result);
	if (ret) {
		LOG_ERR("nRF Cloud rest response parsing failed, err: %d", ret);
		ret = -EBADMSG;
	}

clean_up:
	if (body) {
		cJSON_free(body);
	}
	if (auth_hdr) {
		k_free(auth_hdr);
	}
	return ret;
}
#endif
