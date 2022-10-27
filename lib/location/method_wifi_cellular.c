/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <modem/location.h>

#include <net/multicell_location.h> /* TODO: rename to contain also wifi */

#include "location_core.h"
#include "location_utils.h"
#include "wifi/wifi_service.h"

LOG_MODULE_DECLARE(location, CONFIG_LOCATION_LOG_LEVEL);

/* TODO: note combined method only with ground-fix and with nRF Cloud */
BUILD_ASSERT(
	IS_ENABLED(CONFIG_LOCATION_METHOD_WIFI_SERVICE_NRF_CLOUD) ||
	IS_ENABLED(CONFIG_LOCATION_METHOD_WIFI_SERVICE_HERE),
	"At least one Wi-Fi positioning service must be enabled");

/* Common for both */
struct method_wifi_cellular_start_work_args {
	struct k_work work_item;
	struct location_wifi_cellular_config wifi_cell_config;
	int64_t starting_uptime_ms;
};

static struct method_wifi_cellular_start_work_args method_wifi_cellular_start_work;
static bool running;

/* Wi-Fi */

static struct net_if *wifi_iface;
static uint32_t current_scan_result_count;
static uint32_t latest_scan_result_count;
static struct wifi_ap_info
	latest_scan_results[CONFIG_LOCATION_METHOD_WIFI_SCANNING_RESULTS_MAX_CNT];
static struct wifi_scan_info latest_wifi_info = {
	.ap_info = latest_scan_results,
};
static K_SEM_DEFINE(wifi_scan_ready, 0, 1);

/* Cellular */

static struct lte_lc_ncell neighbor_cells[CONFIG_LTE_NEIGHBOR_CELLS_MAX];
static struct lte_lc_cell gci_cells[15]; /* TODO: own config for max gci_count */
static struct lte_lc_cells_info cell_data = {
	.neighbor_cells = neighbor_cells,
	.gci_cells = gci_cells,
};

static K_SEM_DEFINE(ncellmeas_ready, 0, 1);

/******************************************************************************/

void method_wifi_cellular_lte_ind_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NEIGHBOR_CELL_MEAS: {
		LOG_DBG("Cell measurements results received");

		/* Copy current cell information. */
		memcpy(&cell_data.current_cell,
		       &evt->cells_info.current_cell,
		       sizeof(struct lte_lc_cell));

		/* Copy neighbor cell information if present. */
		if (evt->cells_info.ncells_count > 0 && evt->cells_info.neighbor_cells) {
			memcpy(cell_data.neighbor_cells,
			       evt->cells_info.neighbor_cells,
			       sizeof(struct lte_lc_ncell) * evt->cells_info.ncells_count);

			cell_data.ncells_count = evt->cells_info.ncells_count;
		} else {
			cell_data.ncells_count = 0;
			LOG_DBG("No neighbor cell information from modem.");
		}

		/* Copy GCI neighbor cell information if present. */
		if (evt->cells_info.gci_cells_count > 0 && evt->cells_info.gci_cells) {
			memcpy(cell_data.gci_cells,
			       evt->cells_info.gci_cells,
			       sizeof(struct lte_lc_cell) * evt->cells_info.gci_cells_count);

			cell_data.gci_cells_count = evt->cells_info.gci_cells_count;
		} else {
			cell_data.gci_cells_count = 0;
			LOG_DBG("No GCI neighbor cell information from modem.");
		}
		k_sem_give(&ncellmeas_ready);
	} break;
	default:
		break;
	}
}

static int method_cellular_ncellmeas_start(struct lte_lc_ncellmeas_params ncellmeas_params)
{
	struct location_utils_modem_params_info modem_params = { 0 };
	int err;

	LOG_DBG("Triggering cell measurements");

	err = lte_lc_neighbor_cell_measurement(ncellmeas_params);
	if (err) {
		LOG_WRN("Failed to initiate neighbor cell measurements: %d, "
			"next: fallback to get modem parameters",
			err);

		/* Doing fallback to get only the mandatory items manually:
		 * cell id, mcc, mnc and tac
		 */
		err = location_utils_modem_params_read(&modem_params);
		if (err < 0) {
			LOG_ERR("Could not obtain modem parameters");
			return err;
		}
		memset(&cell_data, 0, sizeof(struct lte_lc_cells_info));

		/* Filling only the mandatory parameters: */
		cell_data.current_cell.mcc = modem_params.mcc;
		cell_data.current_cell.mnc = modem_params.mnc;
		cell_data.current_cell.tac = modem_params.tac;
		cell_data.current_cell.id = modem_params.cell_id;
		cell_data.current_cell.phys_cell_id = modem_params.phys_cell_id;
		k_sem_give(&ncellmeas_ready);
	}
	return 0;
}

/******************************************************************************/

static int method_wifi_scanning_start(void)
{
	int ret;

	LOG_DBG("Triggering start of Wi-Fi scanning");

	latest_scan_result_count = 0;
	current_scan_result_count = 0;

	__ASSERT_NO_MSG(wifi_iface != NULL);
	ret = net_mgmt(NET_REQUEST_WIFI_SCAN, wifi_iface, NULL, 0);
	if (ret) {
		LOG_ERR("Failed to initiate Wi-Fi scanning: %d", ret);
		ret = -EFAULT;
		k_sem_give(&wifi_scan_ready);
	}
	return ret;
}

static void method_wifi_scan_result_handle(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *entry = (const struct wifi_scan_result *)cb->info;
	struct wifi_ap_info *current;

	current_scan_result_count++;

	if (current_scan_result_count <= CONFIG_LOCATION_METHOD_WIFI_SCANNING_RESULTS_MAX_CNT) {
		current = &latest_scan_results[current_scan_result_count - 1];
		sprintf(current->mac_addr_str,
			"%02x:%02x:%02x:%02x:%02x:%02x",
			entry->mac[0], entry->mac[1], entry->mac[2],
			entry->mac[3], entry->mac[4], entry->mac[5]);
		snprintf(current->ssid_str, entry->ssid_length + 1, "%s", entry->ssid);

		current->channel = entry->channel;
		current->rssi = entry->rssi;

		LOG_DBG("scan result #%d stored: ssid %s, mac address: %s, channel %d,",
			current_scan_result_count,
			current->ssid_str,
			current->mac_addr_str,
			current->channel);
	} else {
		LOG_WRN("Scanning result (mac %02x:%02x:%02x:%02x:%02x:%02x) "
			"did not fit to result buffer - dropping it",
				entry->mac[0], entry->mac[1], entry->mac[2],
				entry->mac[3], entry->mac[4], entry->mac[5]);
	}
}

static void method_wifi_scan_done_handle(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	if (status->status) {
		LOG_WRN("Wi-Fi scan request failed (%d).", status->status);
	} else {
		LOG_INF("Scan request done.");
	}

	latest_scan_result_count =
		(current_scan_result_count > CONFIG_LOCATION_METHOD_WIFI_SCANNING_RESULTS_MAX_CNT) ?
			CONFIG_LOCATION_METHOD_WIFI_SCANNING_RESULTS_MAX_CNT :
			current_scan_result_count;
	current_scan_result_count = 0;
	k_sem_give(&wifi_scan_ready);
}

static struct net_mgmt_event_callback method_wifi_net_mgmt_cb;

void method_wifi_cellular_net_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
					struct net_if *iface)
{
	ARG_UNUSED(iface);

	if (running) {
		switch (mgmt_event) {
		case NET_EVENT_WIFI_SCAN_RESULT:
			method_wifi_scan_result_handle(cb);
			break;
		case NET_EVENT_WIFI_SCAN_DONE:
			method_wifi_scan_done_handle(cb);
			break;
		default:
			break;
		}
	}
}

/******************************************************************************/

static void method_wifi_cellular_positioning_work_fn(struct k_work *work)
{
	struct multicell_location_params params = { 0 };
	struct multicell_location location;
	struct location_data location_result = { 0 };
	struct method_wifi_cellular_start_work_args *work_data =
		CONTAINER_OF(work, struct method_wifi_cellular_start_work_args, work_item);
	const struct location_wifi_cellular_config wifi_cell_config = work_data->wifi_cell_config;
	int64_t scan_start_time;
	int64_t scan_time;
	int32_t used_timeout_ms;
	int err;

	used_timeout_ms =
		MIN(wifi_cell_config.cell_conf.timeout, wifi_cell_config.wifi_conf.timeout);
	
	location_core_timer_start(used_timeout_ms);
	scan_start_time = k_uptime_get();

	LOG_INF("Triggering WiFi Scanning");
	err = method_wifi_scanning_start();
	if (err) {
		LOG_WRN("Cannot start Wi-Fi scanning, err %d", err);
	}

	LOG_INF("Triggering neighbor cell measurements");
	err = method_cellular_ncellmeas_start(wifi_cell_config.cell_conf.ncellmeas_params);
	if (err) {
		LOG_WRN("Cannot start neighbor cell measurements");
	}

	k_sem_take(&wifi_scan_ready, K_FOREVER);
	k_sem_take(&ncellmeas_ready, K_FOREVER);
	if (!running) {
		goto end;
	}

	location_core_timer_stop();

	/* Scannings done at this point of time. Store current time to response. */
	location_utils_systime_to_location_datetime(&location_result.datetime);

	/* Check if timeout is given */
	params.timeout = used_timeout_ms;
	if (used_timeout_ms != SYS_FOREVER_MS) {
		/* +1 to round the time up */
		scan_time = (k_uptime_get() - scan_start_time) + 1;

		/* Check if timeout has already elapsed */
		if (scan_time >= used_timeout_ms) {
			LOG_WRN("Timeout occurred during scannings");
			err = -ETIMEDOUT;
			goto end;
		}
		/* Take time used for neighbour cell measurements into account */
		params.timeout = used_timeout_ms - scan_time;
	}

	if (!location_utils_is_default_pdn_active()) {
		/* Not worth to start trying to fetch the location over LTE.
		 * Thus, fail faster in this case and save the trying "costs".
		 */
		LOG_WRN("Default PDN context is NOT active, cannot retrieve a location");
		err = -EFAULT;
		goto end;
	}

	params.cell_data = &cell_data;
	if (latest_scan_result_count > 1) {
		/* Fill Wi-Fi scanning results: */
		latest_wifi_info.cnt = latest_scan_result_count;
		params.wifi_data = &latest_wifi_info;
	} else {
		params.wifi_data = NULL;		
		if (latest_scan_result_count == 1) {
			/* Following statement seems to be true at least with HERE
			 * (400: bad request).
			 */
			LOG_WRN("Retrieving a location based on a single Wi-Fi "
				"access point is not possible, using only cellular data");
		} else {
			LOG_WRN("No Wi-Fi scanning results, using only cellular data");
		}
	}

	err = multicell_location_get(&params, &location);
	if (err) {
		LOG_ERR("Failed to acquire location from multicell_location lib, error: %d", err);
	} else {
		location_result.method = LOCATION_METHOD_WIFI_CELLULAR;
		location_result.latitude = location.latitude;
		location_result.longitude = location.longitude;
		location_result.accuracy = location.accuracy;
		if (running) {
			running = false;
			location_core_event_cb(&location_result);
		}
	}

end:
	if (err == -ETIMEDOUT) {
		location_core_event_cb_timeout();
		running = false;
	} else if (err) {
		location_core_event_cb_error();
		running = false;
	}
}

/******************************************************************************/

int method_wifi_cellular_cancel(void)
{
	running = false;


	if (running) {
		(void)lte_lc_neighbor_cell_measurement_cancel();
		(void)k_work_cancel(&method_wifi_cellular_start_work.work_item);
		k_sem_reset(&wifi_scan_ready);
		k_sem_reset(&ncellmeas_ready);
		running = false;
	} else {
		return -EPERM;
	}

	return 0;
}

int method_wifi_cellular_location_get(const struct location_method_config *config)
{
	k_work_init(&method_wifi_cellular_start_work.work_item, method_wifi_cellular_positioning_work_fn);
	method_wifi_cellular_start_work.wifi_cell_config = config->wifi_cellular;
	method_wifi_cellular_start_work.starting_uptime_ms = k_uptime_get();
	k_work_submit_to_queue(location_core_work_queue_get(), &method_wifi_cellular_start_work.work_item);

	running = true;

	return 0;
}

int method_wifi_cellular_init(void)
{
	running = false;
	current_scan_result_count = 0;
	latest_scan_result_count = 0;
	const struct device *wifi_dev;

	wifi_iface = NULL;
#if defined(CONFIG_WIFI_NRF700X)
	wifi_dev = device_get_binding("wlan0");
#else
	wifi_dev = DEVICE_DT_GET(DT_CHOSEN(ncs_location_wifi));
#endif
	if (!device_is_ready(wifi_dev)) {
		LOG_ERR("Wi-Fi device %s not ready", wifi_dev->name);
		return -ENODEV;
	}

	wifi_iface = net_if_lookup_by_dev(wifi_dev);
	if (wifi_iface == NULL) {
		LOG_ERR("Could not get the Wi-Fi net interface");
		return -EFAULT;
	}

	net_mgmt_init_event_callback(&method_wifi_net_mgmt_cb, method_wifi_cellular_net_mgmt_event_handler,
				     (NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE));
	net_mgmt_add_event_callback(&method_wifi_net_mgmt_cb);

	lte_lc_register_handler(method_wifi_cellular_lte_ind_handler);

	return 0;
}
