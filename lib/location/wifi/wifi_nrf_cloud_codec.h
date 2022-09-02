/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef WIFI_NRF_CLOUD_CODEC_H
#define WIFI_NRF_CLOUD_CODEC_H

#include "wifi_service.h"

#if defined(CONFIG_NRF_CLOUD_MQTT)
#define NRF_CLOUD_JSON_APPID_VAL_WIFI_POS "WIFI"

int wifi_nrf_cloud_mqtt_format_pos_req_body(
	const struct wifi_scanning_result_info scanning_results[],
	uint8_t wifi_scanning_result_count, char **json_str_out);
int wifi_nrf_cloud_mqtt_pos_response_parse(const char *const buf, struct location_data *result);
#elif defined(CONFIG_NRF_CLOUD_REST)
int wifi_nrf_cloud_rest_format_pos_req_body(
	const struct wifi_scanning_result_info scanning_results[],
	uint8_t wifi_scanning_result_count, char **json_str_out);
int wifi_nrf_cloud_rest_pos_response_parse(const char *const buf, struct location_data *result);
#endif

#endif /* WIFI_NRF_CLOUD_CODEC_H */
