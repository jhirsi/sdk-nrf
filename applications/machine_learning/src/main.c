/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <event_manager.h>

#define MODULE main
#include "module_state_event.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE);


void main(void)
{
	if (event_manager_init()) {
		LOG_ERR("Event Manager initialization failed");
	} else {
		module_set_state(MODULE_STATE_READY);
	}
}
