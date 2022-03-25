/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdlib.h>
#include <zephyr.h>
#include <shell/shell.h>

#include "mosh_print.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

static bool bt_initialized;

static uint8_t mfg_data[] = { 0xff, 0xff, 0x00 };

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, 3),
};

static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	mosh_print("BT device found: %s (RSSI %d)\n", addr_str, rssi);
}

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
	int ret = 1;

	if (argc > 1) {
		mosh_error("%s: subcommand not found", argv[1]);
		ret = -EINVAL;
	}

	shell_help(shell);

	return ret;
}

static int cmd_bt(const struct shell *shell, size_t argc, char **argv)
{
	return print_help(shell, argc, argv);
}

static int cmd_bt_scan_start(const struct shell *shell, size_t argc, char **argv)
{
	int err;

	struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_PASSIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};

	if (!bt_initialized) {
		err = bt_enable(NULL);
		if (err) {
			mosh_warn("Bluetooth init failed (err %d)\n", err);
		}
		bt_initialized = true;
	}

	err = bt_le_scan_start(&scan_param, scan_cb);
	if (err) {
		mosh_error("Starting scanning failed (err %d)\n", err);
		return err;
	}

	return err;
}

static int cmd_bt_scan_stop(const struct shell *shell, size_t argc, char **argv)
{
	int err;

	err = bt_le_scan_stop();
	if (err) {
		mosh_error("Stopping scanning failed (err %d)\n", err);
		return err;
	}

	return err;
}

static int cmd_bt_adv_start(const struct shell *shell, size_t argc, char **argv)
{
	int err;

	if (!bt_initialized) {
		err = bt_enable(NULL);
		if (err) {
			mosh_warn("Bluetooth init failed (err %d)", err);
		}
		bt_initialized = true;
	}

	err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		mosh_error("Advertising failed to start (err %d)", err);
	}
	return err;
}

static int cmd_bt_adv_stop(const struct shell *shell, size_t argc, char **argv)
{
	int err;

	err = bt_le_adv_stop();
	if (err) {
		mosh_error("Advertising failed to stop (err %d)", err);
		return err;
	}

	return err;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_bt,
	SHELL_CMD_ARG(scan_start, NULL, "Start BT scanning.", cmd_bt_scan_start, 1, 0),
	SHELL_CMD_ARG(scan_stop, NULL, "Stop BT scanning.", cmd_bt_scan_stop, 1, 0),
	SHELL_CMD_ARG(adv_start, NULL, "Start advertising.", cmd_bt_adv_start, 1, 0),
	SHELL_CMD_ARG(adv_stop, NULL, "Stop advertising.", cmd_bt_adv_stop, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ble, &sub_bt, "Commands for controlling BT.", cmd_bt);
