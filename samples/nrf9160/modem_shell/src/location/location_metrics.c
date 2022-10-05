/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <zephyr/shell/shell.h>
#include <date_time.h>
#include <cJSON.h>
#include <math.h>
#include <getopt.h>
#include <net/nrf_cloud.h>

#include <modem/location.h>

#include "mosh_print.h"
#include "location_cmd_utils.h"

/******************************************************************************/

static int json_add_num_cs(cJSON *parent, const char *str, double item)
{
	if (!parent || !str) {
		return -EINVAL;
	}

	return cJSON_AddNumberToObjectCS(parent, str, item) ? 0 : -ENOMEM;
}

/******************************************************************************/

#define GNSS_PVT_KEY_LAT "lat"
#define GNSS_PVT_KEY_LON "lng"
#define GNSS_PVT_KEY_ACC "acc"

#define GNSS_PVT_KEY_ALTITUDE "alt"
#define GNSS_PVT_KEY_ALTITUDE_ACC "altAcc"
#define GNSS_PVT_KEY_SPEED "spd"
#define GNSS_PVT_KEY_SPEED_ACC "spdAcc"
#define GNSS_PVT_KEY_VER_SPEED "verSpd"
#define GNSS_PVT_KEY_VER_SPEED_ACC "verSpdAcc"
#define GNSS_PVT_KEY_HEADING "hdg"
#define GNSS_PVT_KEY_HEADING_ACC "hdgAcc"

#define GNSS_PVT_KEY_PDOP "pdop"
#define GNSS_PVT_KEY_HDOP "hdop"
#define GNSS_PVT_KEY_VDOP "vdop"
#define GNSS_PVT_KEY_TDOP "tdop"
#define GNSS_PVT_KEY_FLAGS "flags"

static int location_data_pvt_encode(const struct location_data *location, cJSON *const pvt_data_obj)
{
	if (!location || !pvt_data_obj) {
		return -EINVAL;
	}

	if (json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_LON, location->longitude) ||
	    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_LAT, location->latitude) ||
	    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_ACC, location->accuracy)) {
		mosh_error("Failed to encode PVT data");
		return -ENOMEM;
	}

	return 0;
}

static int
location_data_modem_pvt_detailed_encode(const struct nrf_modem_gnss_pvt_data_frame *const mdm_pvt,
					cJSON *const pvt_data_obj)
{
	if (!mdm_pvt || !pvt_data_obj) {
		return -EINVAL;
	}

	/* Flags */
	if (json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_FLAGS, mdm_pvt->flags)) {
		return -ENOMEM;
	}

	/* Fix data */
	if (mdm_pvt->flags & NRF_MODEM_GNSS_PVT_FLAG_FIX_VALID) {
		if (json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_LON, mdm_pvt->longitude) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_LAT, mdm_pvt->latitude) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_ACC, mdm_pvt->accuracy) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_ALTITUDE, mdm_pvt->altitude) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_ALTITUDE_ACC,
				    mdm_pvt->altitude_accuracy) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_SPEED, mdm_pvt->speed) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_SPEED_ACC,
				    mdm_pvt->speed_accuracy) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_HEADING, mdm_pvt->heading) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_HEADING_ACC,
				    mdm_pvt->heading_accuracy) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_VER_SPEED,
				    mdm_pvt->vertical_speed) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_VER_SPEED_ACC,
				    mdm_pvt->vertical_speed_accuracy) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_PDOP, mdm_pvt->pdop) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_HDOP, mdm_pvt->hdop) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_VDOP, mdm_pvt->vdop) ||
		    json_add_num_cs(pvt_data_obj, GNSS_PVT_KEY_TDOP, mdm_pvt->tdop)) {
			mosh_error("Failed to encode detailed PVT data");
			return -ENOMEM;
		}
	}
	cJSON *sat_info_array = NULL;
	cJSON *sat_info_obj = NULL;

	/* SV data */
	sat_info_array = cJSON_AddArrayToObjectCS(pvt_data_obj, "sv_info");
	if (!sat_info_array) {
		mosh_error("Cannot create json array for satellites");
		return -ENOMEM;
	}
	for (uint32_t i = 0; i < NRF_MODEM_GNSS_MAX_SATELLITES; i++) {
		if (mdm_pvt->sv[i].sv == 0) {
			break;
		}
		sat_info_obj = cJSON_CreateObject();
		if (sat_info_obj == NULL) {
			mosh_error("No memory create json obj for satellite data");
			cJSON_Delete(sat_info_array);
			return -ENOMEM;
		}

		if (!cJSON_AddItemToArray(sat_info_array, sat_info_obj)) {
			cJSON_Delete(sat_info_obj);
			mosh_error("Cannot add sat info json object to sat array");
			return -ENOMEM;
		}
		if (json_add_num_cs(sat_info_obj, "sv", mdm_pvt->sv[i].sv) ||
		    json_add_num_cs(sat_info_obj, "c_n0", (mdm_pvt->sv[i].cn0 * 0.1)) ||
		    json_add_num_cs(sat_info_obj, "sig", mdm_pvt->sv[i].signal) ||
		    json_add_num_cs(sat_info_obj, "elev", mdm_pvt->sv[i].elevation) ||
		    json_add_num_cs(sat_info_obj, "az", mdm_pvt->sv[i].azimuth) ||
		    json_add_num_cs(
			    sat_info_obj, "in_fix",
			    (mdm_pvt->sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_USED_IN_FIX ? 1 : 0)) ||
		    json_add_num_cs(
			    sat_info_obj, "unhealthy",
			    (mdm_pvt->sv[i].flags & NRF_MODEM_GNSS_SV_FLAG_UNHEALTHY ? 1 : 0))) {
			mosh_error("Failed to encode detailed PVT data");
			cJSON_Delete(sat_info_obj);
			return -ENOMEM;
		}
	}

	return 0;
}

/******************************************************************************/

static int location_metrics_utils_payload_json_encode(
	int64_t ts_ms, const struct location_event_data *loc_evt_data, cJSON *const root_obj)
{
	if (!loc_evt_data || !root_obj) {
		return -EINVAL;
	}

	cJSON *loc_metrics_data_obj = NULL;
	cJSON *mdm_metrics_data_obj = NULL;
	cJSON *loc_data_obj = NULL;
	int err = 0;

	loc_data_obj = cJSON_CreateObject();
	loc_metrics_data_obj = cJSON_CreateObject();
	mdm_metrics_data_obj = cJSON_CreateObject();
	if (loc_data_obj == NULL || loc_metrics_data_obj == NULL || mdm_metrics_data_obj == NULL) {
		mosh_error("No memory create json obj for metrics data");
		err = -ENOMEM;
		goto cleanup;
	}

	if (!cJSON_AddItemToObject(root_obj, "location_data", loc_data_obj)) {
		mosh_error("No memory create json obj for location data");
		err = -ENOMEM;
		goto cleanup;
	}

	/* Fill location_data */
	if (!cJSON_AddStringToObjectCS(loc_data_obj, "loc_method",
				  location_method_str(loc_evt_data->metrics.used_method))) {
		mosh_error("No memory create json obj loc_method");
		err = -ENOMEM;
		goto cleanup;
	}

	if (loc_evt_data->id == LOCATION_EVT_LOCATION) {
		if (loc_evt_data->location.method != LOCATION_METHOD_GNSS) {
			err = location_data_pvt_encode(&loc_evt_data->location, loc_data_obj);
			if (err) {
				mosh_error("Failed to encode PVT data for nrf cloud to json");
				goto cleanup;
			}
		}

	} else {
		if (!cJSON_AddStringToObjectCS(root_obj, "err_cause_str",
					loc_evt_data->metrics.error_cause_str)) {
			mosh_error("No memory create json obj err_cause_str");
			err = -ENOMEM;
			goto cleanup;
		}
	}

	if (loc_evt_data->location.method == LOCATION_METHOD_GNSS) {
		if (json_add_num_cs(loc_data_obj, "tracked_satellites",
				loc_evt_data->metrics.gnss.tracked_satellites)) {
			mosh_error("No memory create json obj tracked_satellites");
			err = -ENOMEM;
			goto cleanup;
		}

		err = location_data_modem_pvt_detailed_encode(&loc_evt_data->metrics.gnss.pvt_data,
							      loc_data_obj);
		if (err) {
			mosh_error("Failed to encode PVT data for nrf cloud to json");
			goto cleanup;
		}
	}

	/* FIll location_req_metrics */
	if (!cJSON_AddItemToObject(root_obj, "location_req_metrics", loc_metrics_data_obj) ||
	    !cJSON_AddNumberToObjectCS(loc_metrics_data_obj, "used_time_ms",
				       loc_evt_data->metrics.used_time_ms)) {
		mosh_error("No memory for adding location metrics json");
		cJSON_Delete(loc_metrics_data_obj);
		err = -ENOMEM;
		goto cleanup;
	}

	/* FIll common modem metrics */
	if (!cJSON_AddItemToObject(root_obj, "modem_metrics", mdm_metrics_data_obj)) {
		cJSON_Delete(mdm_metrics_data_obj);
		err = -ENOMEM;
		goto cleanup;
	} else {
		/* TODO: get some data of link status and encode them */
		struct nrf_cloud_modem_info modem_info = {
			.device = NRF_CLOUD_INFO_SET,
			.network = NRF_CLOUD_INFO_SET,
			.mpi = NULL /* Modem data will be fetched */
		};

		err = nrf_cloud_modem_info_json_encode(&modem_info, mdm_metrics_data_obj);
		if (err) {
			mosh_warn("Failed to encode modem_info data to json, err %d", err);
			/* We didn't get modem_info, but we don't care. Let it be empty. */
		}
	}
	return 0;

cleanup:
	cJSON_Delete(loc_data_obj);
	return err;
}

/******************************************************************************/
#define MSG_TYPE "messageType"
#define MSG_APP_ID "appId"
#define MSG_DATA "data"
#define MSG_TIMESTAMP "ts"

/*
 * Following added to given as an output to json_str_out as an example:
 *{
 *	"data": "12639",
 *	"appId": "LOC_GNSS_TTF",
 *	"messageType": "DATA",
 *	"ts": 1661233540054,
 *	"location_data": {
 *		"lng": xx.xxxxxxxxxxxxxxx,
 *		"lat": xx.xxxxxxxxxxxxx,
 *		"acc": 5.6439676284790039
 *	},
 *	"location_metrics": {
 *		"used_time_ms": 12639
 *	},
 *	"modem_metrics": {
 *		"deviceInfo": {
 *			"modemFirmware": "mfw_nrf9160_1.3.2",
 *			"batteryVoltage": 5066,
 *			"imei": "351358811331757",
 *			"board": "nrf9160dk_nrf9160",
 *			"appVersion": "v2.0.0-610-g727877b5376b",
 *			"appName": "MOSH"
 *		},
 *		"networkInfo": {
 *			"currentBand": 20,
 *			"supportedBands": "(1,2,3,4,5,8,12,13,18,19,20,25,26,28,66)",
 *			"areaCode": 23040,
 *			"mccmnc": "24405",
 *			"ipAddress": "10.52.78.98",
 *			"ueMode": 2,
 *			"cellID": 145670,
 *			"networkMode": "LTE-M GPS"
 *		}
 *	}
 *}
 */

int location_metrics_utils_json_payload_encode(const struct location_event_data *loc_evt_data,
					       int64_t timestamp_to_json, char **json_str_out)
{
	__ASSERT_NO_MSG(loc_evt_data != NULL);
	__ASSERT_NO_MSG(json_str_out != NULL);

	int err = 0;
	cJSON *root_obj = NULL;
	char used_time_ms_str[32];
	char app_id_str[32];

	if (loc_evt_data->id == LOCATION_EVT_LOCATION) {
		/* Used time (= TTF in this case) as a root "key"
		 * and custom appId based on used  method
		 */
		if (loc_evt_data->location.method == LOCATION_METHOD_GNSS) {
			strcpy(app_id_str, "LOC_GNSS_TTF");
		} else if (loc_evt_data->location.method == LOCATION_METHOD_CELLULAR) {
			strcpy(app_id_str, "LOC_CELL_TTF");
		} else {
			__ASSERT_NO_MSG(loc_evt_data->location.method == LOCATION_METHOD_WIFI);
			strcpy(app_id_str, "LOC_WIFI_TTF");
		}
	} else if (loc_evt_data->id == LOCATION_EVT_TIMEOUT) {
		if (loc_evt_data->metrics.used_method == LOCATION_METHOD_GNSS) {
			strcpy(app_id_str, "LOC_TIMEOUT_GNSS");
		} else if (loc_evt_data->location.method == LOCATION_METHOD_CELLULAR) {
			strcpy(app_id_str, "LOC_TIMEOUT_CELL");
		} else {
			__ASSERT_NO_MSG(loc_evt_data->location.method == LOCATION_METHOD_WIFI);
			strcpy(app_id_str, "LOC_TIMEOUT_WIFI");
		}
	} else if (loc_evt_data->id == LOCATION_EVT_ERROR) {
		if (loc_evt_data->metrics.used_method == LOCATION_METHOD_GNSS) {
			strcpy(app_id_str, "LOC_ERROR_GNSS");
		} else if (loc_evt_data->location.method == LOCATION_METHOD_CELLULAR) {
			strcpy(app_id_str, "LOC_ERROR_CELL");
		} else {
			__ASSERT_NO_MSG(loc_evt_data->location.method == LOCATION_METHOD_WIFI);
			strcpy(app_id_str, "LOC_ERROR_WIFI");
		}
	} else {
		goto cleanup;
	}

	/* This structure corresponds to the General Message Schema described in the
	 * application-protocols repo:
	 * https://github.com/nRFCloud/application-protocols
	 */

	root_obj = cJSON_CreateObject();
	if (root_obj == NULL) {
		mosh_error("Failed to create root json obj");
		err = -ENOMEM;
		goto cleanup;
	}

	sprintf(used_time_ms_str, "%d", loc_evt_data->metrics.used_time_ms);

	if (!cJSON_AddStringToObjectCS(root_obj, MSG_DATA, used_time_ms_str) ||
	    !cJSON_AddStringToObjectCS(root_obj, MSG_APP_ID, app_id_str) ||
	    !cJSON_AddStringToObjectCS(root_obj, MSG_TYPE, "DATA") ||
	    !cJSON_AddNumberToObjectCS(root_obj, MSG_TIMESTAMP, timestamp_to_json)) {
		mosh_error("Cannot add metadata json objects");
		err = -ENOMEM;
		goto cleanup;
	}

	err = location_metrics_utils_payload_json_encode(timestamp_to_json, loc_evt_data, root_obj);
	if (err) {
		mosh_error("Failed to encode metrics data to json");
		goto cleanup;
	}
	*json_str_out = cJSON_PrintUnformatted(root_obj);
	if (*json_str_out == NULL) {
		mosh_error("Failed to print json objects to string");
		err = -ENOMEM;
	}

cleanup:
	if (root_obj) {
		cJSON_Delete(root_obj);
	}
	return err;
}
