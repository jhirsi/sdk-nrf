/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <modem/lte_lc.h>

#include <zephyr/logging/log.h>

#include <cJSON.h>

#include <net/rest_client.h>
#include <net/nrf_cloud.h>

#include "../location_utils.h"
#include "wifi_nrf_cloud_codec.h"

LOG_MODULE_DECLARE(location, CONFIG_LOCATION_LOG_LEVEL);

#define NRF_CLOUD_WIFI_POS_JSON_KEY_AP "accessPoints"

/******************************************************************************/

static int
wifi_nrf_cloud_wifi_pos_req_json_format(const struct wifi_scanning_result_info scanning_results[],
					uint8_t wifi_scanning_result_count,
					cJSON * const req_obj_out)
{
	cJSON *wifi_info_array = NULL;
	cJSON *wifi_info_obj = NULL;

	if (!scanning_results || !wifi_scanning_result_count || !req_obj_out) {
		return -EINVAL;
	}
	/* Request payload format example (ignore line breaks):
	 * {"accessPoints":[{"macAddress": "24-5a-4c-6b-9e-11" },
	 *                  {"macAddress": "6c:55:e8:9b:84:6d"}]}
	 */

	wifi_info_array = cJSON_AddArrayToObjectCS(req_obj_out, NRF_CLOUD_WIFI_POS_JSON_KEY_AP);
	if (!wifi_info_array) {
		goto cleanup;
	}

	for (size_t i = 0; i < wifi_scanning_result_count; ++i) {
		const struct wifi_scanning_result_info scan_result = scanning_results[i];

		wifi_info_obj = cJSON_CreateObject();

		if (!cJSON_AddItemToArray(wifi_info_array, wifi_info_obj)) {
			cJSON_Delete(wifi_info_obj);
			goto cleanup;
		}

		if (!cJSON_AddStringToObjectCS(wifi_info_obj, "macAddress",
					       scan_result.mac_addr_str)) {
			goto cleanup;
		}

		if (!cJSON_AddStringToObjectCS(wifi_info_obj, "ssid", scan_result.ssid_str)) {
			goto cleanup;
		}

		if (!cJSON_AddNumberToObjectCS(wifi_info_obj, "signalStrength", scan_result.rssi)) {
			goto cleanup;
		}

		if (!cJSON_AddNumberToObjectCS(wifi_info_obj, "channel", scan_result.channel)) {
			goto cleanup;
		}
	}
	return 0;

cleanup:
	/* Only need to delete the wifi_info_array since all items (if any) were added to it */
	cJSON_DeleteItemFromObject(req_obj_out, NRF_CLOUD_WIFI_POS_JSON_KEY_AP);
	LOG_ERR("Failed to format nRF Cloud Wi-Fi location request, out of memory");
	return -ENOMEM;
}

#if defined(CONFIG_NRF_CLOUD_MQTT)

#define NRF_CLOUD_JSON_DATA_KEY "data"
#define NRF_CLOUD_JSON_APPID_KEY "appId"
#define NRF_CLOUD_JSON_MSG_TYPE_KEY "messageType"
#define NRF_CLOUD_JSON_MSG_TYPE_VAL_DATA "DATA"

int wifi_nrf_cloud_mqtt_format_pos_req_body(
	const struct wifi_scanning_result_info scanning_results[],
	uint8_t wifi_scanning_result_count, char **json_str_out)
{
	if (!scanning_results || !wifi_scanning_result_count || !json_str_out) {
		return -EINVAL;
	}

	int err = 0;
	cJSON *root_obj = cJSON_CreateObject();

	if (!root_obj) {
		err = -ENOMEM;
		goto cleanup;
	}

	if (!cJSON_AddStringToObjectCS(root_obj, NRF_CLOUD_JSON_APPID_KEY,
				       NRF_CLOUD_JSON_APPID_VAL_WIFI_POS) ||
	    !cJSON_AddStringToObjectCS(root_obj, NRF_CLOUD_JSON_MSG_TYPE_KEY,
				       NRF_CLOUD_JSON_MSG_TYPE_VAL_DATA)) {
		err = -ENOMEM;
		goto cleanup;
	}

	cJSON *data_obj = cJSON_AddObjectToObject(root_obj, NRF_CLOUD_JSON_DATA_KEY);

	if (!data_obj) {
		err = -ENOMEM;
		goto cleanup;
	}

	err = wifi_nrf_cloud_wifi_pos_req_json_format(scanning_results, wifi_scanning_result_count,
						      data_obj);
	if (err) {
		goto cleanup;
	}

	*json_str_out = cJSON_PrintUnformatted(root_obj);
	if (*json_str_out == NULL) {
		err = -ENOMEM;
	}

cleanup:
	if (root_obj) {
		cJSON_Delete(root_obj);
	}

	return err;
}
#endif

#if defined(CONFIG_NRF_CLOUD_REST)
int wifi_nrf_cloud_rest_format_pos_req_body(
	const struct wifi_scanning_result_info scanning_results[],
	uint8_t wifi_scanning_result_count, char **json_str_out)
{
	if (!scanning_results || !wifi_scanning_result_count || !json_str_out) {
		return -EINVAL;
	}

	int err = 0;
	cJSON *req_obj = cJSON_CreateObject();

	err = wifi_nrf_cloud_wifi_pos_req_json_format(scanning_results, wifi_scanning_result_count,
						      req_obj);
	if (err) {
		goto cleanup;
	}

	*json_str_out = cJSON_PrintUnformatted(req_obj);
	if (*json_str_out == NULL) {
		err = -ENOMEM;
	}

cleanup:
	cJSON_Delete(req_obj);

	return err;
}
#endif

static int wifi_nrf_cloud_pos_json_object_parse(const cJSON *const wifi_pos_obj,
						struct location_data *result)
{
	int ret = 0;
	struct cJSON *lat_obj, *lon_obj, *uncertainty_obj;

	/* Response payload format examples:
	 * {"lat":45.52364882,"lon":-122.68331772,"uncertainty":196}
	 */

	if (!wifi_pos_obj) {
		LOG_ERR("No JSON found for nRF Cloud Wi-Fi positioning response");
		ret = -ENOMSG;
		goto cleanup;
	}

	lat_obj = cJSON_GetObjectItemCaseSensitive(wifi_pos_obj, "lat");
	if (lat_obj == NULL) {
		LOG_ERR("No 'lat' object found");
		ret = -ENOMSG;
		goto cleanup;
	}

	lon_obj = cJSON_GetObjectItemCaseSensitive(wifi_pos_obj, "lon");
	if (lon_obj == NULL) {
		LOG_ERR("No 'lon' object found");
		ret = -ENOMSG;
		goto cleanup;
	}

	uncertainty_obj = cJSON_GetObjectItemCaseSensitive(wifi_pos_obj, "uncertainty");
	if (uncertainty_obj == NULL) {
		LOG_ERR("No 'uncertainty' object found");

		ret = -ENOMSG;
		goto cleanup;
	}
	if (!cJSON_IsNumber(lat_obj) || !cJSON_IsNumber(lon_obj) ||
	    !cJSON_IsNumber(uncertainty_obj)) {
		ret = -EBADMSG;
		goto cleanup;
	}

	result->latitude = lat_obj->valuedouble;
	result->longitude = lon_obj->valuedouble;
	result->accuracy = uncertainty_obj->valuedouble;

cleanup:
	if (ret) {
		LOG_DBG("Wi-Fi location object parsing failed");
	}
	return ret;
}

#if defined(CONFIG_NRF_CLOUD_REST)
int wifi_nrf_cloud_rest_pos_response_parse(const char *const buf, struct location_data *result)
{
	int ret = 0;
	struct cJSON *root_obj;

	if (buf == NULL) {
		return -EINVAL;
	}

	/* Response payload format example:
	 * REST:
	 * {"lat":45.52364882,"lon":-122.68331772,"uncertainty":196}
	 */

	root_obj = cJSON_Parse(buf);
	if (!root_obj) {
		LOG_ERR("No JSON found for nRF Cloud Wi-Fi positioning response");
		ret = -ENOMSG;
		goto cleanup;
	}

	ret = wifi_nrf_cloud_pos_json_object_parse(root_obj, result);
	if (ret) {
		LOG_ERR("Failed to parse wifi positioning data");
		ret = -ENOMSG;
	}
cleanup:
	if (ret) {
		LOG_DBG("Unparsed response:\n%s", buf);
	}
	cJSON_Delete(root_obj);
	return ret;
}
#endif

#if defined(CONFIG_NRF_CLOUD_MQTT)
static bool json_item_string_exists(const cJSON *const obj, const char *const key,
				    const char *const val)
{
	__ASSERT_NO_MSG(obj != NULL);
	__ASSERT_NO_MSG(key != NULL);

	char *str_val;
	cJSON *item = cJSON_GetObjectItem(obj, key);

	if (!item) {
		return false;
	}

	if (!val) {
		return cJSON_IsNull(item);
	}

	str_val = cJSON_GetStringValue(item);
	if (!str_val) {
		return false;
	}

	return (strcmp(str_val, val) == 0);
}

int wifi_nrf_cloud_mqtt_pos_response_parse(const char *const buf, struct location_data *result)
{
	int ret = 0;
	struct cJSON *root_obj, *data_obj;

	if (buf == NULL) {
		return -EINVAL;
	}

	/* Response payload format example:
	 * MQTT:
	 * {"appId":"WIFI","messageType":"DATA","data":
	 *   {"lat":61.49350559,"lon":23.77540562,"uncertainty":19}
	 * }
	 */

	root_obj = cJSON_Parse(buf);
	if (!root_obj) {
		LOG_ERR("No JSON found for nRF Cloud Wi-Fi positioning response");
		ret = -ENOMSG;
		goto cleanup;
	}

	/* Check for nRF Cloud MQTT message; valid appId and msgType */
	if (!json_item_string_exists(root_obj, NRF_CLOUD_JSON_MSG_TYPE_KEY,
				     NRF_CLOUD_JSON_MSG_TYPE_VAL_DATA) ||
	    !json_item_string_exists(root_obj, NRF_CLOUD_JSON_APPID_KEY,
				     NRF_CLOUD_JSON_APPID_VAL_WIFI_POS)) {
		/* Not a wifi positioning data message */
		goto cleanup;
	}
	/* MQTT payload format found, parse the data */
	data_obj = cJSON_GetObjectItem(root_obj, NRF_CLOUD_JSON_DATA_KEY);
	if (data_obj) {
		ret = wifi_nrf_cloud_pos_json_object_parse(data_obj, result);
		if (ret) {
			LOG_ERR("Failed to parse wifi positioning data");
			ret = -ENOMSG;
		}
		goto cleanup;
	} else {
		/* No data object */
		LOG_ERR("Failed to parse wifi positioning data: no data object");
		ret = -ENOMSG;
		goto cleanup;
	}

cleanup:
	if (ret) {
		LOG_DBG("Unparsed response:\n%s", buf);
	}
	cJSON_Delete(root_obj);
	return ret;
}
#endif
