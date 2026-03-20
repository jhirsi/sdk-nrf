/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 * DECT NR+ RPC server: runs full DECT stack and modem; receives/sends
 * IPv6 from the RPC client over UART RPC.
 *
 * Init in main(): modem init, then nrf_rpc_init() so the server sends the group init.
 * Start the client first, then the server.
 */

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/logging/log.h>

#include <nrf_modem.h>
#include <modem/nrf_modem_lib.h>
#include <nrf_rpc.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

static void rpc_err_handler(const struct nrf_rpc_err_report *report)
{
	LOG_ERR("RPC error: %d", report->code);
}

static const char *modem_fault_reason_str(uint32_t reason)
{
	switch (reason) {
	case NRF_MODEM_FAULT_UNDEFINED:
		return "Undefined fault";
	case NRF_MODEM_FAULT_HW_WD_RESET:
		return "HW WD reset";
	case NRF_MODEM_FAULT_HARDFAULT:
		return "Hard fault";
	case NRF_MODEM_FAULT_MEM_MANAGE:
		return "Memory management fault";
	case NRF_MODEM_FAULT_BUS:
		return "Bus fault";
	case NRF_MODEM_FAULT_USAGE:
		return "Usage fault";
	case NRF_MODEM_FAULT_SECURE_RESET:
		return "Secure control reset";
	case NRF_MODEM_FAULT_PANIC_DOUBLE:
		return "Error handler crash";
	case NRF_MODEM_FAULT_PANIC_RESET_LOOP:
		return "Reset loop";
	case NRF_MODEM_FAULT_ASSERT:
		return "Assert";
	case NRF_MODEM_FAULT_PANIC:
		return "Unconditional SW reset";
	case NRF_MODEM_FAULT_FLASH_ERASE:
		return "Flash erase fault";
	case NRF_MODEM_FAULT_FLASH_WRITE:
		return "Flash write fault";
	case NRF_MODEM_FAULT_POFWARN:
		return "Undervoltage fault";
	case NRF_MODEM_FAULT_THWARN:
		return "Overtemperature fault";
	default:
		return "Unknown reason";
	}
}

void nrf_modem_fault_handler(struct nrf_modem_fault_info *fault_info)
{
	LOG_ERR("Modem fault: reason=0x%x (%s), PC=0x%x",
		fault_info->reason,
		modem_fault_reason_str(fault_info->reason),
		fault_info->program_counter);

	__ASSERT(false, "Modem crash detected, halting application");
}

int main(void)
{
	int err;

	LOG_INF("DECT RPC server running. DECT interface receives/sends via RPC.");

	/* Initialize modem library; this runs NRF_MODEM_LIB_ON_INIT callbacks (DECT stack init). */
	err = nrf_modem_lib_init();
	if (err) {
		LOG_ERR("nrf_modem_lib_init failed: %d", err);
		return err;
	}
	LOG_INF("nRF Modem library initialized");

	/* Initialize RPC (server is initiator: sends group init to client). */
	LOG_INF("Initializing RPC...");
	err = nrf_rpc_init(rpc_err_handler);
	if (err != 0) {
		LOG_ERR("nrf_rpc_init failed: %d", err);
		return err;
	}
	LOG_INF("RPC initialized");

	return 0;
}
