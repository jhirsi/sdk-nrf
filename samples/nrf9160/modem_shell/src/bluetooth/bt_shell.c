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

#if defined(CONFIG_BT_SCAN)
#include <bluetooth/scan.h>
#endif

#include "bt_ctrl.h"

static uint8_t mfg_data[] = { 0xff, 0xff, 0x00 };

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, 3),
};

#if defined(CONFIG_BT_SCAN)
void scan_filter_match(struct bt_scan_device_info *device_info,
		       struct bt_scan_filter_match *filter_match,
		       bool connectable)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

	mosh_print("Device found: %s", log_strdup(addr));
}
void scan_connecting_error(struct bt_scan_device_info *device_info)
{
	mosh_print("Connection to peer failed!");
}
void scan_filter_no_match(struct bt_scan_device_info *device_info,
			  bool connectable)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

	mosh_print("Filter not match. Address: %s connectable: %d\n",
				addr, connectable);
}

#else
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char dev[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, dev, sizeof(dev));
	mosh_print("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i\n",
	       dev, type, ad->len, rssi);
}
#endif

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

#if defined(CONFIG_BT_SCAN)
BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);
#endif

static int cmd_bt_init(const struct shell *shell, size_t argc, char **argv)
{
	bt_ctrl_init();
	return 0;
}

static int cmd_bt_scan_start(const struct shell *shell, size_t argc, char **argv)
{
	int err;
#if defined(CONFIG_BT_SCAN)
	struct bt_le_scan_param scan_param = {
		.type       = BT_LE_SCAN_TYPE_ACTIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = BT_GAP_SCAN_FAST_INTERVAL,
		.window     = BT_GAP_SCAN_FAST_WINDOW,
	};
	struct bt_scan_init_param scan_init = {
		.scan_param = &scan_param
	};
#else
	struct bt_le_scan_param scan_param = {
			.type       = BT_LE_SCAN_TYPE_ACTIVE,
			.options    = BT_LE_SCAN_OPT_NONE,
			.interval   = BT_GAP_SCAN_FAST_INTERVAL,
			.window     = BT_GAP_SCAN_FAST_WINDOW, };
#endif

#if defined(CONFIG_BT_SCAN)
	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		mosh_error("Starting scanning failed (err %d)\n", err);
		return err;
	}
#else
	err = bt_le_scan_start(&scan_param, scan_cb);
	if (err) {
		mosh_error("Starting scanning failed (err %d)\n", err);
		return err;
	}
#endif
	mosh_print("Scanning...");

	return err;
}

static int cmd_bt_scan_stop(const struct shell *shell, size_t argc, char **argv)
{
	int err;

#if defined(CONFIG_BT_SCAN)
	err = bt_scan_stop();

	if (err == -EALREADY) {
		mosh_warn("Active scan already disabled");
	} else if (err) {
		mosh_error("Stop LE scan failed (err %d)", err);
	} else {
		mosh_print("Scan stopped");
	}
#else
	err = bt_le_scan_stop();
	if (err) {
		mosh_error("Stopping scanning failed (err %d)\n", err);
	}
#endif
	return err;
}

static int cmd_bt_adv_start(const struct shell *shell, size_t argc, char **argv)
{
	int err;

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
	SHELL_CMD_ARG(init, NULL, "Init BT.", cmd_bt_init, 1, 0),
	SHELL_CMD_ARG(scan_start, NULL, "Start BT scanning.", cmd_bt_scan_start, 1, 0),
	SHELL_CMD_ARG(scan_stop, NULL, "Stop BT scanning.", cmd_bt_scan_stop, 1, 0),
	SHELL_CMD_ARG(adv_start, NULL, "Start advertising.", cmd_bt_adv_start, 1, 0),
	SHELL_CMD_ARG(adv_stop, NULL, "Stop advertising.", cmd_bt_adv_stop, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ble, &sub_bt, "Commands for controlling BT.", cmd_bt);
