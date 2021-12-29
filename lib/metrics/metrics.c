/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr.h>
#include <init.h>

#include <logging/log.h>

#include <modem/modem_info.h>
#include <modem/lte_lc.h>
#include <modem/location.h>
#include <nrf_modem_at.h>
#include <modem/at_monitor.h>

#include "integrations/integrations.h"

LOG_MODULE_REGISTER(metrics, CONFIG_METRICS_LOG_LEVEL);

BUILD_ASSERT(IS_ENABLED(CONFIG_METRICS_CLOUD_THINGSPEAK), "Only one metrics sink must be enabled");

/** Handler for getting an IMEI. */
static void metrics_fetch_imei_work_fn(struct k_work *work);

/** Work item for IMEI fetching. */
K_WORK_DELAYABLE_DEFINE(metrics_imei_work, metrics_fetch_imei_work_fn);

/** Work queue thread for a metrics library. */
static struct k_work_q metrics_work_q;

#define METRICS_STACK_SIZE 5120
#define METRICS_THREAD_PRIORITY 5

K_THREAD_STACK_DEFINE(metrics_stack, METRICS_STACK_SIZE);

/** Handler for metrics sending. */
static void metrics_send_work_fn(struct k_work *work);

/** Work item for sending metrics to cloud. */
K_WORK_DEFINE(metrics_send_work, metrics_send_work_fn);

/** Metrics data storages. */
static struct lte_lc_ncell neighbor_cells[10];
static struct lte_lc_cells_info cell_data = {
	.neighbor_cells = neighbor_cells,
};
struct location_metrics current_metrics;

static void metrics_cellular_lte_ind_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_CELL_UPDATE: {
		LOG_INF("LTE cell changed: Cell ID: %d, Tracking area: %d", evt->cell.id,
			evt->cell.tac);
		memset(&cell_data, 0, sizeof(struct lte_lc_cells_info));
		cell_data.current_cell.id = evt->cell.id;
	} break;
	case LTE_LC_EVT_NEIGHBOR_CELL_MEAS: {
		struct lte_lc_cells_info cells = evt->cells_info;
		struct lte_lc_cell cur_cell = cells.current_cell;

		LOG_DBG("Cell measurements results received");
		memset(&cell_data, 0, sizeof(struct lte_lc_cells_info));

		if (cur_cell.id != LTE_LC_CELL_EUTRAN_ID_INVALID) {
			/* Copy current and neighbor cell information */
			memcpy(&cell_data, &evt->cells_info, sizeof(struct lte_lc_cells_info));
			memcpy(neighbor_cells, evt->cells_info.neighbor_cells,
			       sizeof(struct lte_lc_ncell) * cell_data.ncells_count);
		} else {
			LOG_INF("No current cell information from modem.");
		}
		if (!evt->cells_info.ncells_count) {
			LOG_INF("No neighbor cell information from modem.");
		}
	} break;
	default:
		break;
	}
}

/* AT monitor for NCELLMEAS notifications */
AT_MONITOR(metrics, "NCELLMEAS", metrics_ncellmeas_at_notif_handler, PAUSED);

static void metrics_ncellmeas_at_notif_handler(const char *notif)
{
	LOG_INF("notif: %s", log_strdup(notif));

	if (current_metrics.ncell_meas_notif_str != NULL) {
		LOG_INF("cleaning previous NCELLMEAS notif: %s",
			log_strdup(current_metrics.ncell_meas_notif_str));
		k_free(current_metrics.ncell_meas_notif_str);
		current_metrics.ncell_meas_notif_str = NULL;
	}

	current_metrics.ncell_meas_notif_str = k_malloc(strlen(notif) + 1);
	if (current_metrics.ncell_meas_notif_str == NULL) {
		LOG_WRN("Cannot allocate memory for storing NCELLMEAS notification %s",
			log_strdup(notif));
		return;
	}
	strcpy(current_metrics.ncell_meas_notif_str, notif);
}

static void metrics_send_work_fn(struct k_work *work)
{
	int ret;
	int16_t bat_voltage = 0;
	int16_t temperature = 0;

	/* Request battery voltage data from the modem. */
	ret = modem_info_short_get(MODEM_INFO_BATTERY, &bat_voltage);

	if (ret != sizeof(bat_voltage)) {
		LOG_WRN("modem_info_short_get for MODEM_INFO_BATTERY, error: %d\n", ret);
	} else {
		current_metrics.bat_voltage = bat_voltage;
	}

	/* Request temperature data from the modem. */
	ret = modem_info_short_get(MODEM_INFO_TEMP, &temperature);
	if (ret != sizeof(temperature)) {
		LOG_WRN("modem_info_short_get for MODEM_INFO_TEMP, error: %d\n", ret);
	} else {
		current_metrics.temperature = temperature;
	}

	/* Finally, send gathered metrics to the cloud. */
	ret = rest_integration_metrics_data_send(&current_metrics);
	if (ret) {
		LOG_ERR("Metrics sending failed, err %d", ret);
	}
	k_free(current_metrics.ncell_meas_notif_str);
	current_metrics.ncell_meas_notif_str = NULL;
}

/* Trigger sending of metrics based on location */
static void metrics_location_event_handler(const struct location_event_data *event_data)
{
	switch (event_data->id) {
	case LOCATION_EVT_LOCATION:
		LOG_INF("LOCATION_EVT_LOCATION");

		memcpy(&current_metrics.location_data, event_data,
		       sizeof(struct location_event_data));
		memcpy(&current_metrics.cell_data, &cell_data, sizeof(struct lte_lc_cells_info));

		k_work_submit_to_queue(&metrics_work_q, &metrics_send_work);
		break;
	case LOCATION_EVT_TIMEOUT:
		LOG_INF("LOCATION_EVT_TIMEOUT");
		break;
	case LOCATION_EVT_ERROR:
		LOG_ERR("LOCATION_EVT_ERROR");
		break;
	default:
		break;
	}
}

#define AT_CMD_IMEI "AT+CGSN"
#define CGSN_RESPONSE_LENGTH (IMEI_LEN + 6 + 1) /* Add 6 for \r\nOK\r\n and 1 for \0 */

static void metrics_fetch_imei_work_fn(struct k_work *work)
{
	char imei_buf[CGSN_RESPONSE_LENGTH];
	int err = nrf_modem_at_cmd(imei_buf, CGSN_RESPONSE_LENGTH, AT_CMD_IMEI);

	if (err) {
		LOG_ERR("nrf_modem_at_cmd failed, error: %d", err);
		strcat(current_metrics.device_imei_str, "unknown");
	} else {
		imei_buf[IMEI_LEN] = '\0';
		snprintk(current_metrics.device_imei_str, sizeof(current_metrics.device_imei_str),
			 "%s", imei_buf);
	}
}

static int metrics_init(const struct device *unused)
{
	struct k_work_queue_config cfg = {
		.name = "metrics_workq",
	};
	int err;

	k_work_queue_start(&metrics_work_q, metrics_stack, K_THREAD_STACK_SIZEOF(metrics_stack),
			   METRICS_THREAD_PRIORITY, &cfg);

	memset(&current_metrics, 0, sizeof(current_metrics));
	strcat(current_metrics.device_imei_str, "unknown");

	lte_lc_register_handler(metrics_cellular_lte_ind_handler);
	location_register_handler(metrics_location_event_handler);

	err = modem_info_init();
	if (err) {
		LOG_ERR("Failed initializing modem info module, error: %d\n", err);
	}

	at_monitor_resume(metrics);

	/* Request an IMEI from modem as delayed to get modem up and running: */
	k_work_reschedule_for_queue(&metrics_work_q, &metrics_imei_work, K_SECONDS(3));

	return 0;
}

SYS_INIT(metrics_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
