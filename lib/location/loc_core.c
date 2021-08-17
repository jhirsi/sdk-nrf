/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr.h>
#include <assert.h>
#include <logging/log.h>
#include <modem/location.h>

#include "loc_core.h"
#if defined(CONFIG_LOCATION_METHOD_GNSS)
#include "method_gnss.h"
#endif
#if defined(CONFIG_LOCATION_METHOD_CELLULAR)
#include "method_cellular.h"
#endif

LOG_MODULE_DECLARE(location, CONFIG_LOCATION_LOG_LEVEL);

struct loc_event_data current_event_data;
static location_event_handler_t event_handler;
static int current_location_method_index;
static struct loc_config current_loc_config;

static void loc_core_periodic_work_fn(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(loc_periodic_work, loc_core_periodic_work_fn);

#if defined(CONFIG_LOCATION_METHOD_GNSS)
static const struct loc_method_api method_gnss_api = {
	.method           = LOC_METHOD_GNSS,
	.method_string    = "GNSS",
	.init             = method_gnss_init,
	.validate_params  = NULL,
	.location_get     = method_gnss_location_get,
	.cancel   = method_gnss_cancel,
};
#endif
#if defined(CONFIG_LOCATION_METHOD_CELLULAR)
static const struct loc_method_api method_cellular_api = {
	.method           = LOC_METHOD_CELLULAR,
	.method_string    = "Cellular",
	.init             = method_cellular_init,
	.validate_params  = NULL,
	.location_get     = method_cellular_location_get,
	.cancel   = method_cellular_cancel,
};
#endif

static const struct loc_method_api *methods_supported[] = {
#if defined(CONFIG_LOCATION_METHOD_GNSS)
	&method_gnss_api,
#else
	NULL,
#endif
#if defined(CONFIG_LOCATION_METHOD_CELLULAR)
	&method_cellular_api,
#else
	NULL,
#endif
};

static void loc_core_current_event_data_init(enum loc_method method)
{
	memset(&current_event_data, 0, sizeof(current_event_data));

	current_event_data.method = method;
}


/** @brief Returns API for given location method.
 *
 * @param method Method.
 *
 * @return API of the location method. NULL if given method is not supported.
 */
static const struct loc_method_api *loc_method_api_validity_get(enum loc_method method)
{
	const struct loc_method_api *method_api = NULL;

	for (int i = 0; i < LOC_MAX_METHODS; i++) {
		if (method == methods_supported[i]->method) {
			method_api = methods_supported[i];
			break;
		}
	}
	return method_api;
}

/** @brief Returns API for given location method.
 *
 * @param method Method. Must be valid location method or otherwise asserts.
 *
 * @return API of the location method.
 */
static const struct loc_method_api *loc_method_api_get(enum loc_method method)
{
	const struct loc_method_api *method_api = NULL;

	for (int i = 0; i < LOC_MAX_METHODS; i++) {
		if (method == methods_supported[i]->method) {
			method_api = methods_supported[i];
			break;
		}
	}
	/* This function is not supposed to be called when API is not found so
	 * to find issues elsewhere we'll assert here
	 */
	/* TODO: MOSH will hit this if some methods are disabled from compilation
	 *       and two methods are given
	 */
	assert(method_api != NULL);
	return method_api;
}

int loc_core_init(location_event_handler_t handler)
{
	int err;

	if (handler == NULL) {
		LOG_ERR("No event handler given");
		return -EINVAL;
	}

	event_handler = handler;

	for (int i = 0; i < LOC_MAX_METHODS; i++) {
		if (methods_supported[i]->method != 0) {
			err = methods_supported[i]->init();
			if (err) {
				LOG_ERR("Failed to initialize '%s' method",
					methods_supported[i]->method_string);
				return err;
			}
			LOG_DBG("Initialized '%s' method successfully",
				methods_supported[i]->method_string);
		}
	}

	return 0;
}

int loc_core_validate_params(const struct loc_config *config)
{
	const struct loc_method_api *method_api;
	int err;

	if ((config->interval > 0) && (config->interval < 10)) {
		LOG_ERR("Interval for periodic location updates must be 10...65535 seconds.");
		return -EINVAL;
	}

	for (int i = 0; i < LOC_MAX_METHODS; i++) {
		if (config->methods[i].method != 0) {
			method_api = loc_method_api_validity_get(config->methods[i].method);
			if (method_api == NULL) {
				LOG_ERR("Location method (%d) not supported",
					config->methods[i].method);
				return -EINVAL;
			}
			if (methods_supported[i]->validate_params) {
				err = methods_supported[i]->validate_params(&config->methods[i]);
				if (err) {
					return err;
				}
			}
		}
	}
	return 0;
}

int loc_core_location_get(const struct loc_config *config)
{
	int err;
	enum loc_method requested_location_method;

	/* Location request starts from the first method */
	current_location_method_index = 0;
	current_loc_config = *config;

	requested_location_method = config->methods[current_location_method_index].method;
	LOG_DBG("Requesting location with '%s' method",
		(char *)loc_method_api_get(requested_location_method)->method_string);
	loc_core_current_event_data_init(requested_location_method);
	err = loc_method_api_get(requested_location_method)->location_get(
		&config->methods[current_location_method_index]);

	return err;
}

void loc_core_event_cb_error(void)
{
	current_event_data.id = LOC_EVT_ERROR;

	loc_core_event_cb(NULL);
}

void loc_core_event_cb_timeout(void)
{
	current_event_data.id = LOC_EVT_TIMEOUT;

	loc_core_event_cb(NULL);
}

void loc_core_event_cb(const struct loc_location *location)
{
	char temp_str[16];
	enum loc_method requested_location_method;
	enum loc_method previous_location_method;
	int err;

	if (location != NULL) {
		/* Location was acquired properly, finish location request */
		current_event_data.id = LOC_EVT_LOCATION;
		current_event_data.location = *location;

		LOG_DBG("Location acquired successfully:");
		LOG_DBG("  method: %s (%d)",
			(char *)loc_method_api_get(current_event_data.method)->method_string,
			current_event_data.method);
		/* Logging v1 doesn't support double and float logging. Logging v2 would support
		 * but that's up to application to configure.
		 */
		sprintf(temp_str, "%.06f", current_event_data.location.latitude);
		LOG_DBG("  latitude: %s", log_strdup(temp_str));
		sprintf(temp_str, "%.06f", current_event_data.location.longitude);
		LOG_DBG("  longitude: %s", log_strdup(temp_str));
		sprintf(temp_str, "%.01f", current_event_data.location.accuracy);
		LOG_DBG("  accuracy: %s m", log_strdup(temp_str));
		if (current_event_data.location.datetime.valid) {
			LOG_DBG("  date: %04d-%02d-%02d",
				current_event_data.location.datetime.year,
				current_event_data.location.datetime.month,
				current_event_data.location.datetime.day);
			LOG_DBG("  time: %02d:%02d:%02d.%03d UTC",
				current_event_data.location.datetime.hour,
				current_event_data.location.datetime.minute,
				current_event_data.location.datetime.second,
				current_event_data.location.datetime.ms);
		}

		event_handler(&current_event_data);

	} else {
		/* Do fallback to next preferred method */
		previous_location_method = current_event_data.method;
		current_location_method_index++;
		if (current_location_method_index < LOC_MAX_METHODS) {
			requested_location_method =
				current_loc_config.methods[current_location_method_index].method;

			/* TODO: Should we have NONE method in the API? */
			if (requested_location_method != 0) {
				LOG_WRN("Failed to acquire location using '%s', "
					"trying with '%s' next",
					(char *)loc_method_api_get(previous_location_method)
						->method_string,
					(char *)loc_method_api_get(requested_location_method)
						->method_string);

				loc_core_current_event_data_init(requested_location_method);
				err = loc_method_api_get(requested_location_method)->
					location_get(
					&current_loc_config.methods[current_location_method_index]);
				return;
			}
			LOG_ERR("Location acquisition failed and fallbacks are also done");
		} else {
			LOG_ERR("Location acquisition failed and fallbacks are also done");
		}
	}

	if (current_loc_config.interval > 0) {
		/* TODO: Use own work queue k_work_schedule_for_queue */
		k_work_schedule(&loc_periodic_work, K_SECONDS(current_loc_config.interval));
	}
}

static void loc_core_periodic_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);
	loc_core_location_get(&current_loc_config);
}

int loc_core_cancel(void)
{
	/* TODO: Error code has to be checked as we get error when cancelling periodic now.
	 * We should probably run method cancel only if periodic work is not busy as otherwise
	 * we are just waiting for something to get running.
	 */
	int err = -EPERM;

	enum loc_method current_location_method =
	current_loc_config.methods[current_location_method_index].method;

	/* Check if location has been requested using one of the methods */
	if (current_location_method != 0) {
		err = loc_method_api_get(current_location_method)->cancel();
	}

	if (k_work_delayable_busy_get(&loc_periodic_work) > 0) {
		k_work_cancel_delayable(&loc_periodic_work);
	}
	return err;
}
