/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef LOCATION_H
#define LOCATION_H

struct location_method_api {
	int (*init)(void);
	int (*location_request)(const struct loc_method_config *config, uint16_t interval);
};

struct location_method_supported {
	enum loc_method method;
	const struct location_method_api *api;
};

void event_data_init(enum loc_event_id event_id, enum loc_method method);
void event_location_callback(const struct loc_event_data *event_data);

#endif /* LOCATION_H */
