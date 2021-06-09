/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <string.h>

#include <zephyr.h>
#include <init.h>
#include <nrf_modem.h>

#include <sys/types.h>
#include <logging/log_ctrl.h>
#include <power/reboot.h>
#include <dfu/mcuboot.h>

#include <shell/shell.h>
#include <shell/shell_uart.h>

#include <modem/nrf_modem_lib.h>
#include <modem/at_cmd.h>
#include <modem/at_notif.h>
#include <modem/modem_info.h>
#include <modem/lte_lc.h>

#if defined(CONFIG_MOSH_PPP)
#include <shell/shell.h>
#include "ppp_ctrl.h"
#endif

#if defined(CONFIG_MOSH_LINK)
#include "link.h"
#endif

#if defined(CONFIG_MOSH_GNSS)
#include "gnss.h"
#endif

#if defined(CONFIG_MOSH_FOTA)
#include "fota.h"
#endif

/* global variables */
struct modem_param_info modem_param;
/**
 * @brief Global shell pointer that can be used for printing.
 *
 * @details This is obtained in the beginning of main().
 * However, it won't work instantly but only later in main().
 */
const struct shell *shell_global;

K_SEM_DEFINE(nrf_modem_lib_initialized, 0, 1);

static void mosh_print_version_info(void)
{
#if defined(APP_VERSION)
	printk("\nMOSH version:       %s", STRINGIFY(APP_VERSION));
#else
	printk("\nMOSH version:       unknown");
#endif

#if defined(BUILD_ID)
	printk("\nMOSH build id:      v%s", STRINGIFY(BUILD_ID));
#else
	printk("\nMOSH build id:      custom");
#endif

#if defined(BUILD_VARIANT)
	printk("\nMOSH build variant: %s\n\n", STRINGIFY(BUILD_VARIANT));
#else
	printk("\nMOSH build variant: dev\n\n");
#endif
}

/***** START OF INTERNAL CHANGE. DO NOT COMMIT TO NCS MASTER. *****/

#include <hal/nrf_gpio.h>
static void modem_trace_enable(void)
{
/* GPIO configurations for trace and debug */
#define CS_PIN_CFG_TRACE_CLK 21 //GPIO_OUT_PIN21_Pos
#define CS_PIN_CFG_TRACE_DATA0 22 //GPIO_OUT_PIN22_Pos
#define CS_PIN_CFG_TRACE_DATA1 23 //GPIO_OUT_PIN23_Pos
#define CS_PIN_CFG_TRACE_DATA2 24 //GPIO_OUT_PIN24_Pos
#define CS_PIN_CFG_TRACE_DATA3 25 //GPIO_OUT_PIN25_Pos

	// Configure outputs.
	// CS_PIN_CFG_TRACE_CLK
	NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_CLK] =
		(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
		(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

	// CS_PIN_CFG_TRACE_DATA0
	NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA0] =
		(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
		(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

	// CS_PIN_CFG_TRACE_DATA1
	NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA1] =
		(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
		(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

	// CS_PIN_CFG_TRACE_DATA2
	NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA2] =
		(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
		(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

	// CS_PIN_CFG_TRACE_DATA3
	NRF_P0_NS->PIN_CNF[CS_PIN_CFG_TRACE_DATA3] =
		(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos) |
		(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos);

	NRF_P0_NS->DIR = 0xFFFFFFFF;
}

/***** END OF INTERNAL CHANGE *****/

void main(void)
{
	int err;

	/***** START OF INTERNAL CHANGE */
	modem_trace_enable();
	/***** END OF INTERNAL CHANGE *****/

	shell_global = shell_backend_uart_get_ptr();

	mosh_print_version_info();

#if !defined(CONFIG_LWM2M_CARRIER)
	printk("Initializing modemlib...\n");
	err = nrf_modem_lib_init(NORMAL_MODE);
	switch (err) {
	case MODEM_DFU_RESULT_OK:
		printk("Modem firmware update successful!\n");
		printk("Modem will run the new firmware after reboot\n");
		sys_reboot(SYS_REBOOT_WARM);
		break;
	case MODEM_DFU_RESULT_UUID_ERROR:
	case MODEM_DFU_RESULT_AUTH_ERROR:
		printk("Modem firmware update failed!\n");
		printk("Modem will run non-updated firmware on reboot.\n");
		sys_reboot(SYS_REBOOT_WARM);
		break;
	case MODEM_DFU_RESULT_HARDWARE_ERROR:
	case MODEM_DFU_RESULT_INTERNAL_ERROR:
		printk("Modem firmware update failed!\n");
		printk("Fatal error.\n");
		sys_reboot(SYS_REBOOT_WARM);
		break;
	case -1:
		printk("Could not initialize modemlib.\n");
		printk("Fatal error.\n");
		return;
	default:
		break;
	}
	printk("Initialized modemlib\n");

	at_cmd_init();
#if !defined(CONFIG_AT_NOTIF_SYS_INIT)
	at_notif_init();
#endif
	lte_lc_init();
#else
	/* Wait until modemlib has been initialized. */
	k_sem_take(&nrf_modem_lib_initialized, K_FOREVER);

#endif
#if defined(CONFIG_MOSH_PPP)
	ppp_ctrl_init();
#endif

#if defined(CONFIG_MOSH_GNSS_ENABLE_LNA)
	gnss_set_lna_enabled(true);
#endif

#if defined(CONFIG_MOSH_FOTA)
	err = fota_init();
	if (err) {
		printk("Could not initialize FOTA: %d\n", err);
	}
#endif

#if defined(CONFIG_LTE_LINK_CONTROL) && defined(CONFIG_MOSH_LINK)
	link_init();
#endif

#if defined(CONFIG_MODEM_INFO)
	err = modem_info_init();
	if (err) {
		printk("Modem info could not be established: %d\n", err);
		return;
	}
	modem_info_params_init(&modem_param);
#endif

	/* Application started successfully, mark image as OK to prevent
	 * revert at next reboot.
	 */
#if defined(CONFIG_BOOTLOADER_MCUBOOT)
	boot_write_img_confirmed();
#endif
}
