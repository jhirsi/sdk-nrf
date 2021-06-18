/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MOSH_LINK_H
#define MOSH_LINK_H
#include <modem/lte_lc.h>

#if defined(CONFIG_MULTICELL_LOCATION)
#include <net/multicell_location.h>
#endif

enum link_ncellmeas_modes {
	LINK_NCELLMEAS_MODE_NONE = 0,
	LINK_NCELLMEAS_MODE_SINGLE,
	LINK_NCELLMEAS_MODE_CONTINUOUS,
};

#define LINK_APN_STR_MAX_LENGTH     100
#define LINK_API_KEY_STR_MAX_LENGTH 128

#define LINK_FUNMODE_NONE 99

void link_init(void);
void link_ind_handler(const struct lte_lc_evt *const evt);
void link_rsrp_subscribe(bool subscribe);
void link_ncellmeas_start(bool start, enum link_ncellmeas_modes mode,
	enum multicell_location_service_id service, char *api_key);
void link_modem_sleep_notifications_subscribe(uint32_t warn_time_ms, uint32_t threshold_ms);
void link_modem_sleep_notifications_unsubscribe(void);
void link_modem_tau_notifications_subscribe(uint32_t warn_time_ms, uint32_t threshold_ms);
void link_modem_tau_notifications_unsubscribe(void);
int link_func_mode_set(enum lte_lc_func_mode fun);
int link_func_mode_get(void);

#endif /* MOSH_LINK_H */
