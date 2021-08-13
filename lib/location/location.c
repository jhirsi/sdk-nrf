/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr.h>

#include <assert.h>

#include <nrf_modem_gnss.h>
#include <logging/log.h>

#include <net/multicell_location.h>
#include <modem/location.h>

#include "method_gnss.h"


#if defined(CONFIG_METHOD_CELLULAR)
#include "method_cellular.h"
#endif

#include "location.h"

LOG_MODULE_REGISTER(location, CONFIG_LOCATION_LOG_LEVEL);

struct loc_event_data current_event_data;
static location_event_handler_t event_handler;
static int current_location_method_index = -1;
static struct loc_config current_loc_config;

const static struct location_method_api method_gnss_api = {
	.method_string    = "GNSS",
	.init             = method_gnss_init,
	.location_request = method_gnss_configure_and_start,
	.cancel_request   = method_gnss_cancel,
};

const static struct location_method_api method_cellular_api = {
	.method_string    = "Cellular",
	.init             = method_cellular_init,
	.location_request = method_cellular_configure_and_start,
	.cancel_request   = method_cellular_cancel,
};

static struct location_method_supported methods_supported[LOC_MAX_METHODS] = {
	{LOC_METHOD_GNSS, &method_gnss_api},
	{LOC_METHOD_CELL_ID, &method_cellular_api},
};

void event_data_init(enum loc_event_id event_id, enum loc_method method)
{
	memset(&current_event_data, 0, sizeof(current_event_data));

	current_event_data.id = event_id;
	current_event_data.method = method;
}

const struct location_method_api *location_method_api_get(enum loc_method method)
{
	const struct location_method_api *method_api = NULL;

	for (int i = 0; i < LOC_MAX_METHODS; i++) {
		if (method == methods_supported[i].method) {
			method_api = methods_supported[i].api;
			break;
		}
	}
	/* This function is not supposed to be called when API is not found so
	 * to find issues elsewhere we'll assert here
	 */
	assert(method_api != NULL);
	return method_api;
}

int location_init(location_event_handler_t handler)
{
	int err;
	static bool initialized;

	if (initialized) {
		/* Already initialized */
		return -EPERM;
	}

	if (handler == NULL) {
		LOG_ERR("No event handler given");
		return -EINVAL;
	}

	event_handler = handler;

	/* TODO: Run inits in the loop but first need to compose LOC_MAX_METHODS
	 * based on configured methods
	 */

#if defined(CONFIG_METHOD_GNSS)
	/* GNSS init */
	err = method_gnss_init();
	if (err) {
		LOG_ERR("Failed to initialize GNSS method");
		return err;
	}
#endif

#if defined(CONFIG_METHOD_CELLULAR)
	/* Cellular positioning init */
	method_cellular_init();
#endif

	initialized = true;

	LOG_DBG("Library initialized");

	return 0;
}

int location_request(const struct loc_config *config)
{
	int err;
	enum loc_method requested_location_method;

	/* Location request starts from the first method */
	current_location_method_index = 0;
	current_loc_config = *config;

	/* TODO: Add protection so that only one request is handled at a time */

	/* TODO: Validate location method */
	/* TODO: Configuration validation into own function and own method specific validations */
	if ((config->interval > 0) && (config->interval < 10)) {
		LOG_ERR("Interval for periodic location updates must be 10...65535 seconds.");
		return -EINVAL;
	}

	requested_location_method = config->methods[current_location_method_index].method;
	memset(&current_event_data, 0, sizeof(current_event_data));
	current_event_data.method = requested_location_method;
	err = location_method_api_get(requested_location_method)->location_request(
		&config->methods[current_location_method_index], config->interval);

	return err;
}

void event_location_callback_error()
{
	current_event_data.id = LOC_EVT_ERROR;

	event_location_callback(&current_event_data);
}

void event_location_callback_timeout()
{
	current_event_data.id = LOC_EVT_TIMEOUT;

	event_location_callback(&current_event_data);
}

void event_location_callback(const struct loc_event_data *event_data_param)
{
	enum loc_method requested_location_method;
	enum loc_method previous_location_method;
	int err;

	event_handler(event_data_param);

	if (event_data_param->id == LOC_EVT_LOCATION) {
		/* Location was acquired properly, finish location request */
		return;
	}

	/* Do fallback to next preferred method */
	previous_location_method = event_data_param->method;
	current_location_method_index++;
	if (current_location_method_index < LOC_MAX_METHODS) {
		requested_location_method =
			current_loc_config.methods[current_location_method_index].method;
		LOG_INF("Failed to acquire location using '%s', trying with '%s' next",
			(char *)location_method_api_get(previous_location_method)->method_string,
			(char *)location_method_api_get(requested_location_method)->method_string);

		memset(&current_event_data, 0, sizeof(current_event_data));
		current_event_data.method = requested_location_method;

		err = location_method_api_get(requested_location_method)->location_request(
			&current_loc_config.methods[current_location_method_index],
			current_loc_config.interval);
	} else {
		LOG_ERR("Location acquisition failed and no further trials will be made");
	}
}

int location_request_cancel(void)
{
	int err = -EPERM;

	/* Check if location has been requested using one of the methods */
	if (current_location_method_index >= 0) {
		enum loc_method current_location_method =
		current_loc_config.methods[current_location_method_index].method;

		err = location_method_api_get(current_location_method)->cancel_request();
	}
	
	return err;
}
