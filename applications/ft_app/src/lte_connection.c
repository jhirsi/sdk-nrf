/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <nrf9160.h>
#include <hal/nrf_gpio.h>

#include <shell/shell.h>
#include <shell/shell_uart.h>

#include <modem/modem_info.h>
#include <modem/lte_lc.h>

#include "lte_connection.h"

const struct shell *uart_shell;

#if defined(CONFIG_MODEM_INFO)
/* System work queue for getting the modem info that ain't in lte connection ind.
   TODO: things like these mighjt be good to be in lte connection ind, 
   i.e. merge certain stuff from modem info to there */

static struct k_work modem_info_work;

static void get_modem_info(struct k_work *unused)
{
	int ret;
	char info_str[MODEM_INFO_MAX_RESPONSE_SIZE];
	
	ARG_UNUSED(unused);

	ret = modem_info_string_get(MODEM_INFO_OPERATOR, info_str, sizeof(info_str));
	if (ret >= 0) {
		shell_print(uart_shell, "Operator: %s", info_str);
	} else {
		shell_error(uart_shell, "\nUnable to obtain modem operator parameters (%d)", ret);
		}
	ret = modem_info_string_get(MODEM_INFO_APN, info_str, sizeof(info_str));
	if (ret >= 0) {
		shell_print(uart_shell, "APN: %s", info_str);
	} else {
		shell_error(uart_shell, "\nUnable to obtain modem apn parameters (%d)", ret);
	}
	ret = modem_info_string_get(MODEM_INFO_IP_ADDRESS, info_str, sizeof(info_str));
	if (ret >= 0) {
		shell_print(uart_shell, "IP address: %s", info_str);
	} else {
		shell_error(uart_shell, "\nUnable to obtain modem ip parameters (%d)", ret);
	}
}
#endif

void lte_connection_init(void)
{
#if defined(CONFIG_MODEM_INFO)
	k_work_init(&modem_info_work, get_modem_info);
#endif
}

void lte_connection_ind_handler(const struct lte_lc_evt *const evt)
{
	uart_shell = shell_backend_uart_get_ptr();
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		switch (evt->nw_reg_status) {
		case LTE_LC_NW_REG_NOT_REGISTERED:
			shell_print(
				uart_shell,
				"Network registration status: not registered");
			break;
		case LTE_LC_NW_REG_SEARCHING:
			shell_print(uart_shell,
				   "Network registration status: searching");
			break;
		case LTE_LC_NW_REG_REGISTRATION_DENIED:
			shell_print(uart_shell,
				   "Network registration status: denied");
			break;
		case LTE_LC_NW_REG_UNKNOWN:
			shell_print(uart_shell,
				   "Network registration status: unknown");
			break;
		case LTE_LC_NW_REG_UICC_FAIL:
			shell_print(uart_shell,
				   "Network registration status: UICC fail");
			break;
		case LTE_LC_NW_REG_REGISTERED_HOME:
		case LTE_LC_NW_REG_REGISTERED_ROAMING:
			shell_print(
				uart_shell, "Network registration status: %s",
				evt->nw_reg_status ==
						LTE_LC_NW_REG_REGISTERED_HOME ?
					"Connected - home network" :
					"Connected - roaming");
#if defined(CONFIG_MODEM_INFO)
			k_work_submit(&modem_info_work);
#endif
		default:
			break;
		}
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		shell_print(uart_shell, "LTE cell changed: Cell ID: %d, Tracking area: %d",
		       evt->cell.id, evt->cell.tac);
		break;
	default:
		break;
	}
}
