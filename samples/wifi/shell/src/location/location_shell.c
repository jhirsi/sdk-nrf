/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <stdlib.h>

#include <zephyr/kernel.h>

#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_uart.h>

#include <modem/location.h>

#include <zephyr/posix/time.h>
#include <zephyr/sys/timeutil.h>

#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>

static const struct shell *used_shell;
static bool only_scan;

/**************************************************************************************************/

/** Buffer used for printing the timestamp. */
static char datetime_str[128];

static void date_get_str_create(void)
{
	struct timespec tp;
	struct tm tm;

	clock_gettime(CLOCK_REALTIME, &tp);

	gmtime_r(&tp.tv_sec, &tm);

	sprintf(datetime_str,
		    "[%d-%02u-%02u "
		    "%02u:%02u:%02u UTC]",
		    tm.tm_year + 1900,
		    tm.tm_mon + 1,
		    tm.tm_mday,
		    tm.tm_hour,
		    tm.tm_min,
		    tm.tm_sec);
}

static char *date_time_str_get(void)
{
	date_get_str_create();
	return datetime_str;
}

/**************************************************************************************************/

#define WIFI_SHELL_LOCATION_MGMT_EVENTS (NET_EVENT_WIFI_SCAN_DONE)

static struct net_mgmt_event_callback cmd_loc_wifi_net_mgmt_cb;

void cmd_loc_wifi_net_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
					struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_DONE:
		shell_print(used_shell, "%s: NET_EVENT_WIFI_SCAN_DONE event.", date_time_str_get());
		break;
	default:
		break;
	}
}

static void location_lib_event_handler(const struct location_event_data *event_data)
{
	shell_print(used_shell, "%s: Location event received.", date_time_str_get());

	switch (event_data->id) {
	case LOCATION_EVT_LOCATION:
		if (only_scan) {
			shell_print(used_shell, "%s: Location req scanning only DONE.",
				date_time_str_get());
			return;
		}
		shell_print(used_shell, "Location:");
		shell_print(used_shell,
			"  used method: %s (%d)",
			location_method_str(event_data->location.method),
			event_data->location.method);
		shell_print(used_shell, "  latitude: %.06f", event_data->location.latitude);
		shell_print(used_shell, "  longitude: %.06f", event_data->location.longitude);
		shell_print(used_shell, "  accuracy: %.01f m", event_data->location.accuracy);
		if (event_data->location.datetime.valid) {
			shell_print(used_shell,
				"  date: %04d-%02d-%02d",
				event_data->location.datetime.year,
				event_data->location.datetime.month,
				event_data->location.datetime.day);
			shell_print(used_shell,
				"  time: %02d:%02d:%02d.%03d UTC",
				event_data->location.datetime.hour,
				event_data->location.datetime.minute,
				event_data->location.datetime.second,
				event_data->location.datetime.ms);
		}
		shell_print(used_shell,
			"  Google maps URL: https://maps.google.com/?q=%f,%f",
			event_data->location.latitude, event_data->location.longitude);
		break;

	case LOCATION_EVT_TIMEOUT:
		shell_error(used_shell, "Location request timed out");
		break;

	case LOCATION_EVT_ERROR:
		shell_error(used_shell, "Location request failed");
		break;
	default:
		shell_warn(used_shell, "Unknown event from location library, id %d",
			event_data->id);
		break;
	}
}

/**************************************************************************************************/

static int cmd_loc_get(const struct shell *shell, size_t argc, char **argv)
{
	struct location_config config = { 0 };
	enum location_method methods[] = {LOCATION_METHOD_WIFI};
	int err;
	int interval;

	used_shell = shell;

	only_scan = false;
	err = location_init(location_lib_event_handler);
	if (err) {
		shell_error(shell, "Initializing the Location library failed, err: %d\n", err);
	}

	location_config_defaults_set(&config, 1, methods);

	if (argc > 1) {
		interval = atoi(argv[1]);
		if (interval < 0) {
			shell_error(shell, "location get: invalid interval value %d", interval);
			return -EINVAL;
		}
		config.interval = interval;
	}

	shell_print(shell, "%s: Location request starting", date_time_str_get());

	err = location_request(&config);
	if (err) {
		shell_error(shell, "Requesting location failed, error: %d\n", err);
		return err;
	}
	return 0;
}

static int cmd_loc_scan_only(const struct shell *shell, size_t argc, char **argv)
{
	struct location_config config = { 0 };
	enum location_method methods[] = {LOCATION_METHOD_WIFI};
	int err;
	int interval;

	used_shell = shell;

	err = location_init(location_lib_event_handler);
	if (err) {
		shell_error(shell, "Initializing the Location library failed, err: %d\n", err);
	}

	location_config_defaults_set(&config, 1, methods);
	config.methods[0].wifi.only_scan = true;
	only_scan = true;

	if (argc > 1) {
		interval = atoi(argv[1]);
		if (interval < 0) {
			shell_error(shell, "location get: invalid interval value %d", interval);
			return -EINVAL;
		}
		config.interval = interval;
	}

	shell_print(shell, "%s: Location request (only Wi-Fi scanning) starting.",
		date_time_str_get());

	err = location_request(&config);
	if (err) {
		shell_error(shell, "Requesting location failed, error: %d\n", err);
		return err;
	}
	return 0;
}

static int cmd_loc_cancel(const struct shell *shell, size_t argc, char **argv)
{
	int err = location_request_cancel();

	only_scan = false;
	if (err) {
		shell_error(shell, "Canceling location request failed, err: %d", err);
		return -1;
	}
	shell_print(shell, "%s: Location request cancelled.", date_time_str_get());
	return 0;
}

/**************************************************************************************************/

void cmd_loc_init(void)
{
	used_shell = shell_backend_uart_get_ptr();

	net_mgmt_init_event_callback(&cmd_loc_wifi_net_mgmt_cb,
				     cmd_loc_wifi_net_mgmt_event_handler,
				     (WIFI_SHELL_LOCATION_MGMT_EVENTS));
	net_mgmt_add_event_callback(&cmd_loc_wifi_net_mgmt_cb);
}

/**************************************************************************************************/

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_loc,
	SHELL_CMD(
		get, NULL,
		"Requests the current position. Usage:\n"
		"  location get [interval_in_secs]\n",
		cmd_loc_get),
	SHELL_CMD(
		cancel, NULL,
		"Cancel/stop on going request.",
		cmd_loc_cancel),
	SHELL_CMD(
		scan_only, NULL,
		"Perform only Wi-Fi scanning(s). Usage:\n"
		"  location scan_only [interval_in_secs]\n",
		cmd_loc_scan_only),

	SHELL_SUBCMD_SET_END);

static const char location_usage_str[] =
	"Usage: location <subcommand>\n";

SHELL_CMD_REGISTER(location, &sub_loc, location_usage_str, NULL);
