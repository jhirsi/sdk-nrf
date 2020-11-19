/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LTE_CONNECTION_H
#define LTE_CONNECTION_H
#include <modem/lte_lc.h>

#define LTELC_APN_STR_MAX_LENGTH 100
#define LTELC_MAX_PDN_SOCKETS 5 //TODO: what is the actual max in modem?

/* Note: if adding values here, remember to update mapping_table in 
   ltelc_shell_funmode_to_string() */
typedef enum {
	LTELC_FUNMODE_PWROFF = 0,
	LTELC_FUNMODE_NORMAL = 1,
	LTELC_FUNMODE_FLIGHTMODE = 4,
	LTELC_FUNMODE_READ = 66,
	LTELC_FUNMODE_NONE = 99
} ltelc_shell_funmode_options;

void ltelc_init(void);
void ltelc_ind_handler(const struct lte_lc_evt *const evt);
void ltelc_rsrp_subscribe(bool subscribe) ;
int ltelc_func_mode_set(int fun);
int ltelc_func_mode_get(void);
int ltelc_pdn_init_and_connect(const char *apn_name);
int ltelc_pdn_disconnect(const char* apn, int pdn_cid);

#endif
