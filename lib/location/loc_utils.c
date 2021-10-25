/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

#include <nrf_modem_at.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(location, CONFIG_LOCATION_LOG_LEVEL);

#define AT_CMD_PDP_ACT_READ "AT+CGACT?"

bool loc_utils_is_default_pdn_active(void)
{
	char at_response_str[128];
	const char *p;
	int err;
	bool is_active = false;

	err = nrf_modem_at_cmd(at_response_str, sizeof(at_response_str), "%s", AT_CMD_PDP_ACT_READ);
	if (err) {
		LOG_ERR("Cannot get PDP contexts activation states, err: %d", err);
		return false;
	}

	/* Search for a string +CGACT: <cid>,<state> */
	p = strstr(at_response_str, "+CGACT: 0,1");
	if (p) {
		is_active = true;
	}
	return is_active;
}
