/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <zephyr.h>
#include <shell/shell.h>

#include <settings/settings.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#include "mosh_print.h"

extern struct k_sem bt_initialized;

static void bt_ctrl_ble_ready(int err)
{
	mosh_print("Bluetooth ready");
	k_sem_give(&bt_initialized);
}

int bt_ctrl_init(void)
{
	int err;

	mosh_print("Enabling Bluetooth...");
	err = bt_enable(bt_ctrl_ble_ready);
	if (err) {
		mosh_warn("Bluetooth init failed (err %d)", err);
	}
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	return err;
}
