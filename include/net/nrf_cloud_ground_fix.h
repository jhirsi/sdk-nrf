/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF_CLOUD_GROUND_FIX_H_
#define NRF_CLOUD_GROUND_FIX_H_

/** @file nrf_cloud_ground_fix.h
 * @brief Module to provide nRF Cloud ground fix support to nRF9160 SiP.
 */

#include <zephyr/kernel.h>
#include <zephyr/net/wifi.h>
#include <modem/lte_lc.h>
#include <net/nrf_cloud.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup nrf_cloud_ground_fix nRF Cloud ground fix
 * @{
 */

/* 2 chars per byte, colon separated */
#define WIFI_MAC_ADDR_STR_LEN ((WIFI_MAC_ADDR_LEN * 2) + 5)

/** @defgroup nrf_cloud_ground_fix_wifi_omit Omit item from WiFi ground fix request.
 * @{
 */
#define NRF_CLOUD_GROUND_FIX_WIFI_OMIT_RSSI	(INT8_MAX)
#define NRF_CLOUD_GROUND_FIX_WIFI_OMIT_CHAN	(0)
/** @} */

struct wifi_ap_info {
	char mac_addr_str[WIFI_MAC_ADDR_STR_LEN + 1];
	char ssid_str[WIFI_SSID_MAX_LEN + 1];
	uint8_t channel;
	int8_t rssi;
};

/* Minimum number of access points required by nRF Cloud */
#define NRF_CLOUD_GROUND_FIX_WIFI_AP_CNT_MIN 2
struct wifi_scan_result {
	struct wifi_ap_info *ap_info;
	uint8_t cnt;
};

/** @brief Ground fix request type */
enum nrf_cloud_ground_fix_type {
	GROUND_FIX_TYPE_SINGLE_CELL,
	GROUND_FIX_TYPE_MULTI_CELL,
	GROUND_FIX_TYPE_WIFI,

	GROUND_FIX_TYPE__INVALID
};

/** @brief Ground fix request result */
struct nrf_cloud_ground_fix_result {
	enum nrf_cloud_ground_fix_type type;
	double lat;
	double lon;
	uint32_t unc;
	/* Error value received from nRF Cloud */
	enum nrf_cloud_error err;
};

/** @defgroup nrf_cloud_ground_fix_cell_omit Omit item from ground fix request.
 * @{
 */
#define NRF_CLOUD_GROUND_FIX_CELL_OMIT_TIME_ADV	LTE_LC_CELL_TIMING_ADVANCE_INVALID
#define NRF_CLOUD_GROUND_FIX_CELL_OMIT_RSRQ	LTE_LC_CELL_RSRQ_INVALID
#define NRF_CLOUD_GROUND_FIX_CELL_OMIT_RSRP	LTE_LC_CELL_RSRP_INVALID
#define NRF_CLOUD_GROUND_FIX_CELL_OMIT_EARFCN	UINT32_MAX
/** @} */

#define NRF_CLOUD_GROUND_FIX_CELL_TIME_ADV_MAX	LTE_LC_CELL_TIMING_ADVANCE_MAX

#if defined(CONFIG_NRF_CLOUD_MQTT)
/**
 * @brief Cloud ground fix result handler function type.
 *
 * @param pos   Ground fix result.
 */
typedef void (*nrf_cloud_ground_fix_response_t)
	(const struct nrf_cloud_ground_fix_result *const pos);

/**@brief Perform an nRF Cloud ground fix request via MQTT.
 *
 * @param cells_inf Pointer to cell info. The required network parameters are
 *                  cell identifier, mcc, mnc and tac. The required neighboring
 *                  cell parameters are E-ARFCN and physical cell identity.
 *                  The parameters for time diff and measurement time are not used.
 *                  The remaining parameters are optional; including them may improve
 *                  location accuracy. To omit a request parameter, use the appropriate
 *                  define in @ref nrf_cloud_ground_fix_cell_omit.
 *                  Can be NULL if WiFi info is provided.
 * @param wifi_inf Pointer to WiFi info. The MAC address is the only required parameter
 *                 for each item.
 *                 To omit a request parameter, use the appropriate
 *                  define in @ref nrf_cloud_ground_fix_wifi_omit.
 *                 Can be NULL if cell info is provided.
 * @param request_loc If true, cloud will send location to the device.
 *                    If false, cloud will not send location to the device.
 * @param cb Callback function to receive parsed ground fix result. Only used when
 *           request_loc is true. If NULL, JSON result will be sent to the cloud event
 *           handler as an NRF_CLOUD_EVT_RX_DATA_GROUND_FIX event.
 * @retval 0       Request sent successfully.
 * @retval -EACCES Cloud connection is not established; wait for @ref NRF_CLOUD_EVT_READY.
 * @retval -EDOM The number of access points in the WiFi-only request was smaller than
 *               the minimum required value NRF_CLOUD_GROUND_FIX_WIFI_AP_CNT_MIN.
 * @return A negative value indicates an error.
 */
int nrf_cloud_ground_fix_request(const struct lte_lc_cells_info *const cells_inf,
				 const struct wifi_scan_result *const wifi_inf,
				 const bool request_loc, nrf_cloud_ground_fix_response_t cb);
#endif /* CONFIG_NRF_CLOUD_MQTT */

/**@brief Get the reference to a cJSON object containing a ground fix request.
 *
 * @param cells_inf Pointer to cell info; can be NULL if WiFi info is provided.
 * @param wifi_inf Pointer to wifi info; can be NULL if cell info is provided.
 * @param request_loc If true, cloud will send location to the device.
 *                    If false, cloud will not send location to the device.
 * @param req_obj_out Pointer used to get the reference to the generated cJSON object.
 *
 * @retval 0 If successful.
 * @retval -EDOM The number of access points in the WiFi-only request was smaller than
 *               the minimum required value NRF_CLOUD_GROUND_FIX_WIFI_AP_CNT_MIN.
 * @return A negative value indicates an error.
 */
int nrf_cloud_ground_fix_request_json_get(const struct lte_lc_cells_info *const cells_inf,
					  const struct wifi_scan_result *const wifi_inf,
					  const bool request_loc, cJSON **req_obj_out);

/**@brief Processes ground fix data received from nRF Cloud via MQTT or REST.
 *
 * @param buf Pointer to data received from nRF Cloud.
 * @param result Pointer to buffer for parsing result.
 *
 * @retval 0 If processed successfully and groud fix location found.
 * @retval 1 If processed successfully but no ground fix location data found. This
 *           indicates that the data is not a ground fix location response.
 * @retval -EFAULT An nRF Cloud error code was processed.
 * @return A negative value indicates an error.
 */
int nrf_cloud_ground_fix_process(const char *buf, struct nrf_cloud_ground_fix_result *result);

int nrf_cloud_ground_fix_scell_data_get(struct lte_lc_cell *const cell_inf);
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* NRF_CLOUD_GROUND_FIX_H_ */
