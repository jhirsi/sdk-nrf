/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <time.h>

#include "time_utils.h"

time_t time_utils_time(time_t *t)
{
	/* at+cclk? TODO to get real time? or use date_time.h services?
	 * for now: our epoch is the seconds since bootup
	 */
	uint64_t elapsed_msecs;
	time_t elapsed_secs;

	elapsed_msecs = k_uptime_get();
	elapsed_secs = (time_t)(elapsed_msecs) / 1000;

	if (t != NULL) {
		*t = elapsed_secs;
	}

	return elapsed_secs;
}
