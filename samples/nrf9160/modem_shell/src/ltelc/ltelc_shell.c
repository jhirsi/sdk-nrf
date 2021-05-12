/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>

#include <shell/shell.h>
#include <shell/shell_uart.h>
#include <unistd.h>
#include <getopt.h>

#include <modem/at_cmd.h>

#include "ltelc.h"
#include "ltelc_api.h"
#include "ltelc_shell.h"
#include "ltelc_shell_print.h"
#include "ltelc_shell_pdn.h"
#include "ltelc_settings.h"

#define LTELC_SHELL_EDRX_VALUE_STR_LENGTH 4
#define LTELC_SHELL_EDRX_PTW_STR_LENGTH 4
#define LTELC_SHELL_PSM_PARAM_STR_LENGTH 8

typedef enum {
	LTELC_CMD_STATUS = 0,
	LTELC_CMD_SETTINGS,
	LTELC_CMD_CONEVAL,
	LTELC_CMD_DEFCONT,
	LTELC_CMD_DEFCONTAUTH,
	LTELC_CMD_RSRP,
	LTELC_CMD_NCELLMEAS,
	LTELC_CMD_MDMSLEEP,
	LTELC_CMD_TAU,
	LTELC_CMD_CONNECT,
	LTELC_CMD_DISCONNECT,
	LTELC_CMD_FUNMODE,
	LTELC_CMD_SYSMODE,
	LTELC_CMD_NORMAL_MODE_AT,
	LTELC_CMD_NORMAL_MODE_AUTO,
	LTELC_CMD_EDRX,
	LTELC_CMD_PSM,
	LTELC_CMD_HELP
} ltelc_shell_command;

typedef enum {
	LTELC_COMMON_NONE = 0,
	LTELC_COMMON_READ,
	LTELC_COMMON_ENABLE,
	LTELC_COMMON_DISABLE,
	LTELC_COMMON_SUBSCRIBE,
	LTELC_COMMON_UNSUBSCRIBE,
	LTELC_COMMON_START,
	LTELC_COMMON_STOP,
	LTELC_COMMON_RESET
} ltelc_shell_common_options;

typedef struct {
	ltelc_shell_command command;
	ltelc_shell_common_options common_option;
	enum lte_lc_func_mode funmode_option;
	enum lte_lc_system_mode sysmode_option;
	enum lte_lc_system_mode_preference sysmode_lte_pref_option;
	enum lte_lc_lte_mode lte_mode;
} ltelc_shell_cmd_args_t;

/******************************************************************************/
const char ltelc_usage_str[] =
	"Usage: ltelc <subcommand> [options]\n"
	"\n"
	"<subcommand> is one of the following:\n"
	"  <subcommand>:            Subcommand usage if options\n"
	"  help:                    Show this message (no options)\n"
	"  status:                  Show status of the current connection (no options)\n"
	"  settings:                Option to print or reset all persistent\n"
	"                           ltelc subcmd settings.\n"
	"  coneval:                 Get connection evaluation parameters (no options)\n"
	"  defcont:                 Set custom default PDP context config.\n"
	"                           Persistent between the sessions.\n"
	"                           Effective when going to normal mode.\n"
	"  defcontauth:             Set custom authentication parameters for\n"
	"                           the default PDP context. Persistent between the sessions.\n"
	"                           Effective when going to normal mode.\n"
	"  connect:                 Connect to given apn by creating and activating a new PDP context\n"
	"  disconnect:              Disconnect from given apn by deactivating and destroying a PDP context\n"
	"  rsrp:                    Subscribe/unsubscribe for RSRP signal info\n"
	"  ncellmeas:               Start/stop neighbor cell measurements\n"
	"  msleep:                  Subscribe/unsubscribe for modem sleep notifications\n"
	"  tau:                     Subscribe/unsubscribe for modem periodic TAU notifications\n"
	"  funmode:                 Set/read functional modes of the modem\n"
	"  sysmode:                 Set/read system modes of the modem\n"
	"                           Persistent between the sessions. Effective when\n"
	"                           going to normal mode.\n"
	"  nmodeat:                 Set custom AT commmands that are run when going to normal mode\n"
	"  nmodeauto:               Enabling/disabling of automatic connecting and going to\n"
	"                           normal mode after the bootup. Persistent between the sessions.\n"
	"                           Has impact after the bootup\n"
	"  edrx:                    Enable/disable eDRX with default or with custom parameters\n"
	"  psm:                     Enable/disable Power Saving Mode (PSM) with\n"
	"                           default or with custom parameters\n"
	"\n";

const char ltelc_settings_usage_str[] =
	"Usage: ltelc settings --read | --reset\n"
	"Options:\n"
	"  -r, --read,   Read and print current persistent settings\n"
	"      --reset,  Reset all persistent settings as their defaults\n"
	"\n";

const char ltelc_defcont_usage_str[] =
	"Usage: ltelc defcont --enable [options] | --disable | --read\n"
	"Options:\n"
	"  -r, --read,           Read and print current config\n"
	"  -d, --disable,        Disable custom config for default PDP context\n"
	"  -e, --enable,         Enable custom config for default PDP context\n"
	"  -a, --apn,     [str]  Set default Access Point Name\n"
	"  -f, --family,  [str]  Address family: 'ipv4v6' (default), 'ipv4', 'ipv6', 'non-ip'\n"
	"\n";

const char ltelc_defcontauth_usage_str[] =
	"Usage: ltelc defcontauth --enable [options] | --disable | --read\n"
	"Options:\n"
	"  -r, --read,           Read and print current config\n"
	"  -d, --disable,        Disable custom config for default PDP context\n"
	"  -e, --enable,         Enable custom config for default PDP context\n"
	"  -U, --uname,   [str]  Username\n"
	"  -P, --pword,   [str]  Password\n"
	"  -A, --prot,    [int]  Authentication protocol (Default: 0 (None), 1 (PAP), 2 (CHAP)\n"
	"\n";

const char ltelc_connect_usage_str[] =
	"Usage: ltelc connect --apn <apn str> [--family <pdn family str>]\n"
	"Options:\n"
	"  -a, --apn,        [str]  Access Point Name\n"
	"  -f, --family,     [str]  PDN family: 'ipv4v6', 'ipv4', 'ipv6', 'non-ip'\n"
	"\n"
	"Usage: ltelc disconnect -I <cid>\n"
	"Options:\n"
	"  -I, --cid,        [int]  Use this option to disconnect specific PDN CID\n"
	"\n";

const char ltelc_sysmode_usage_str[] =
	"Usage: ltelc sysmode [options] | --read | --reset\n"
	"Options:\n"
	"  -r, --read,                 Read system modes set in modem and by 'ltelc sysmode'\n"
	"      --reset,                Reset the set sysmode as default\n"
	"  -m, --ltem,                 Set LTE-M (LTE Cat-M1) system mode\n"
	"  -n, --nbiot,                Set NB-IoT (LTE Cat-NB1) system mode\n"
	"      --ltem_nbiot,           Set LTE-M + NB-IoT system mode\n"
	"  -g, --gps,                  Set GPS system mode\n"
	"  -M, --ltem_gps,             Set LTE-M + GPS system mode\n"
	"  -N, --nbiot_gps,            Set NB-IoT + GPS system mode\n"
	"      --ltem_nbiot_gps,       Set LTE-M + NB-IoT + GPS system mode\n"
	"\n"
	"Additional LTE mode preference that can be optionally given\n"
	"and might make an impact with multimode system modes in modem,\n"
	" i.e. with --ltem_nbiot or --ltem_nbiot_gps\n"
	"      --pref_auto,            auto, selected by modem (set as default if not given)\n"
	"      --pref_ltem,            LTE-M is preferred over PLMN selection\n"
	"      --pref_nbiot,           NB-IoT is preferred over PLMN selection\n"
	"      --pref_ltem_plmn_prio,  LTE-M is preferred, but PLMN selection is more important\n"
	"      --pref_nbiot_plmn_prio, NB-IoT is preferred, but PLMN selection is more important\n"
	"\n";

const char ltelc_funmode_usage_str[] =
	"Usage: ltelc funmode [option] | --read\n"
	"Options:\n"
	"  -r, --read,              Read modem functional mode\n"
	"  -0, --pwroff,            Set modem power off\n"
	"  -1, --normal,            Set modem normal mode\n"
	"  -4, --flightmode,        Set modem offline.\n"
	"      --lteoff,            Deactivates LTE without shutting down GNSS services.\n"
	"      --lteon,             Activates LTE without changing GNSS.\n"
	"      --gnssoff,           Deactivates GNSS without shutting down LTE services.\n"
	"      --gnsson,            Activates GNSS without changing LTE.\n"
	"      --uiccoff,           Deactivates UICC.\n"
	"      --uiccon,            Activates UICC.\n"
	"      --flightmode_uiccon, Sets the device to flight mode without shutting down UICC.\n"
	"\n";

const char ltelc_normal_mode_at_usage_str[] =
	"Usage: ltelc nmodeat --read | --mem<1-3>\n"
	"Options:\n"
	"  -r, --read,            Read all set custom normal mode at commands\n"
	"      --mem[1-3],        Set at cmd to given memory slot,\n"
	"                         Example: \"ltelc nmodeat --mem1 \"at%xbandlock=2,\\\"100\\\"\"\"\n"
	"                         To clear the given memslot by given the empty string:\n"
	"                         \"ltelc nmodeat --mem2 \"\"\"\n"
	"\n";

const char ltelc_normal_mode_auto_usage_str[] =
	"Usage: ltelc nmodeauto --read | --enable | --disable\n"
	"Options:\n"
	"  -r, --read,            Read and print current setting\n"
	"  -e, --enable,          Enable autoconnect (default)\n"
	"  -d, --disable,         Disable autoconnect\n"
	"\n";

const char ltelc_edrx_usage_str[] =
	"Usage: ltelc edrx --enable --ltem|--nbiot [options] | --disable\n"
	"Options:\n"
	"  -d, --disable,           Disable eDRX\n"
	"  -e, --enable,            Enable eDRX\n"
	"  -m, --ltem,              Set for LTE-M (LTE Cat-M1) system mode\n"
	"  -n, --nbiot,             Set for NB-IoT (LTE Cat-NB1) system mode\n"
	"  -x, --edrx_value, [str]  Sets custom eDRX value to be requested when\n"
	"                           enabling eDRX with -e option.\n"
	"  -w, --ptw,        [str]  Sets custom Paging Time Window value to be\n"
	"                           requested when enabling eDRX -e option.\n"
	"\n";

const char ltelc_psm_usage_str[] =
	"Usage: ltelc psm --enable [options] | --disable | --read\n"
	"Options:\n"
	"  -r, --read,              Read PSM config\n"
	"  -d, --disable,           Disable PSM\n"
	"  -e, --enable,            Enable PSM\n"
	"  -p, --rptau,      [str]  Sets custom requested periodic TAU value to be requested\n"
	"                           when enabling PSM -e option.\n"
	"  -t, --rat,        [str]  Sets custom requested active time (RAT) value to be\n"
	"                           requested when enabling PSM -e option.\n"
	"\n";

const char ltelc_rsrp_usage_str[] =
	"Usage: ltelc rsrp --subscribe | --unsubscribe\n"
	"Options:\n"
	"  -s, --subscribe,         Subscribe for RSRP info\n"
	"  -u, --unsubscribe,       Unsubscribe for RSRP info\n"
	"\n";

const char ltelc_ncellmeas_usage_str[] =
	"Usage: ltelc ncellmeas [--cancel]\n"
	"Options:\n"
	"                   Start neighbor cell measurements and report result\n"
	"      --cancel,    Cancel/Stop started neighbor cell measurements if still on going\n"
	"\n";

const char ltelc_msleep_usage_str[] =
	"Usage: ltelc msleep --subscribe [options] | --unsubscribe\n"
	"Options:\n"
	"  -u, --unsubscribe,        Unsubscribe for modem sleep notifications\n"
	"  -s, --subscribe,          Subscribe for modem sleep notifications\n"
	"      --warn_time,   [int]  Advance warning time in milliseconds. \n"
	"                            Notification is sent as a pre-warning for modem wakeup.\n"
	"      --threshold,   [int]  Shortest sleep time indicated to application in milliseconds.\n"
	"\n";

const char ltelc_tau_usage_str[] =
	"Usage: ltelc tau --subscribe [options] | --unsubscribe\n"
	"Options:\n"
	"  -u, --unsubscribe,        nsubscribe for TAU notifications\n"
	"  -s, --subscribe,          Subscribe for TAU notifications\n"
	"      --warn_time,   [int]  Advance warning time in milliseconds. \n"
	"                            Notification is sent as a pre-warning for periodic TAU.\n"
	"      --threshold,   [int]  Shortest periodic TAU time indicated to application in milliseconds.\n"
	"\n";

/******************************************************************************/

/* Following are not having short options: */
enum {
	LTELC_SHELL_OPT_MEM_SLOT_1 = 1001,
	LTELC_SHELL_OPT_MEM_SLOT_2,
	LTELC_SHELL_OPT_MEM_SLOT_3,
	LTELC_SHELL_OPT_RESET,
	LTELC_SHELL_OPT_SYSMODE_LTEM_NBIOT,
	LTELC_SHELL_OPT_SYSMODE_LTEM_NBIOT_GPS,
	LTELC_SHELL_OPT_SYSMODE_PREF_AUTO,
	LTELC_SHELL_OPT_SYSMODE_PREF_LTEM,
	LTELC_SHELL_OPT_SYSMODE_PREF_NBIOT,
	LTELC_SHELL_OPT_SYSMODE_PREF_LTEM_PLMN_PRIO,
	LTELC_SHELL_OPT_SYSMODE_PREF_NBIOT_PLMN_PRIO,
	LTELC_SHELL_OPT_FUNMODE_LTEOFF,
	LTELC_SHELL_OPT_FUNMODE_LTEON,
	LTELC_SHELL_OPT_FUNMODE_GNSSOFF,
	LTELC_SHELL_OPT_FUNMODE_GNSSON,
	LTELC_SHELL_OPT_FUNMODE_UICCOFF,
	LTELC_SHELL_OPT_FUNMODE_UICCON,
	LTELC_SHELL_OPT_FUNMODE_FLIGHTMODE_UICCON,
	LTELC_SHELL_OPT_WARN_TIME,
	LTELC_SHELL_OPT_THRESHOLD_TIME,
	LTELC_SHELL_OPT_START,
	LTELC_SHELL_OPT_STOP,
};

/* Specifying the expected options (both long and short): */
static struct option long_options[] = {
	{ "apn", required_argument, 0, 'a' },
	{ "cid", required_argument, 0, 'I' },
	{ "family", required_argument, 0, 'f' },
	{ "subscribe", no_argument, 0, 's' },
	{ "unsubscribe", no_argument, 0, 'u' },
	{ "read", no_argument, 0, 'r' },
	{ "pwroff", no_argument, 0, '0' },
	{ "normal", no_argument, 0, '1' },
	{ "flightmode", no_argument, 0, '4' },
	{ "lteoff", no_argument, 0, LTELC_SHELL_OPT_FUNMODE_LTEOFF },
	{ "lteon", no_argument, 0, LTELC_SHELL_OPT_FUNMODE_LTEON },
	{ "gnssoff", no_argument, 0, LTELC_SHELL_OPT_FUNMODE_GNSSOFF },
	{ "gnsson", no_argument, 0, LTELC_SHELL_OPT_FUNMODE_GNSSON },
	{ "uiccoff", no_argument, 0, LTELC_SHELL_OPT_FUNMODE_UICCOFF },
	{ "uiccon", no_argument, 0, LTELC_SHELL_OPT_FUNMODE_UICCON },
	{ "flightmode_uiccon", no_argument, 0,
	  LTELC_SHELL_OPT_FUNMODE_FLIGHTMODE_UICCON },
	{ "ltem", no_argument, 0, 'm' },
	{ "nbiot", no_argument, 0, 'n' },
	{ "gps", no_argument, 0, 'g' },
	{ "ltem_gps", no_argument, 0, 'M' },
	{ "nbiot_gps", no_argument, 0, 'N' },
	{ "enable", no_argument, 0, 'e' },
	{ "disable", no_argument, 0, 'd' },
	{ "edrx_value", required_argument, 0, 'x' },
	{ "ptw", required_argument, 0, 'w' },
	{ "prot", required_argument, 0, 'A' },
	{ "pword", required_argument, 0, 'P' },
	{ "uname", required_argument, 0, 'U' },
	{ "rptau", required_argument, 0, 'p' },
	{ "rat", required_argument, 0, 't' },
	{ "mem1", required_argument, 0, LTELC_SHELL_OPT_MEM_SLOT_1 },
	{ "mem2", required_argument, 0, LTELC_SHELL_OPT_MEM_SLOT_2 },
	{ "mem3", required_argument, 0, LTELC_SHELL_OPT_MEM_SLOT_3 },
	{ "reset", no_argument, 0, LTELC_SHELL_OPT_RESET },
	{ "ltem_nbiot", no_argument, 0, LTELC_SHELL_OPT_SYSMODE_LTEM_NBIOT },
	{ "ltem_nbiot_gps", no_argument, 0,
	  LTELC_SHELL_OPT_SYSMODE_LTEM_NBIOT_GPS },
	{ "pref_auto", no_argument, 0, LTELC_SHELL_OPT_SYSMODE_PREF_AUTO },
	{ "pref_ltem", no_argument, 0, LTELC_SHELL_OPT_SYSMODE_PREF_LTEM },
	{ "pref_nbiot", no_argument, 0, LTELC_SHELL_OPT_SYSMODE_PREF_NBIOT },
	{ "pref_ltem_plmn_prio", no_argument, 0,
	  LTELC_SHELL_OPT_SYSMODE_PREF_LTEM_PLMN_PRIO },
	{ "pref_nbiot_plmn_prio", no_argument, 0,
	  LTELC_SHELL_OPT_SYSMODE_PREF_NBIOT_PLMN_PRIO },
	{ "start", no_argument, 0, LTELC_SHELL_OPT_START },
	{ "stop", no_argument, 0, LTELC_SHELL_OPT_STOP },
	{ "cancel", no_argument, 0, LTELC_SHELL_OPT_STOP },
	{ "warn_time", required_argument, 0, LTELC_SHELL_OPT_WARN_TIME },
	{ "threshold", required_argument, 0, LTELC_SHELL_OPT_THRESHOLD_TIME },
	{ 0, 0, 0, 0 }
};

/******************************************************************************/

static void ltelc_shell_print_usage(const struct shell *shell,
				    ltelc_shell_cmd_args_t *ltelc_cmd_args)
{
	switch (ltelc_cmd_args->command) {
	case LTELC_CMD_SETTINGS:
		shell_print(shell, "%s", ltelc_settings_usage_str);
		break;
	case LTELC_CMD_DEFCONT:
		shell_print(shell, "%s", ltelc_defcont_usage_str);
		break;
	case LTELC_CMD_DEFCONTAUTH:
		shell_print(shell, "%s", ltelc_defcontauth_usage_str);
		break;
	case LTELC_CMD_CONNECT:
	case LTELC_CMD_DISCONNECT:
		shell_print(shell, "%s", ltelc_connect_usage_str);
		break;
	case LTELC_CMD_SYSMODE:
		shell_print(shell, "%s", ltelc_sysmode_usage_str);
		break;
	case LTELC_CMD_FUNMODE:
		shell_print(shell, "%s", ltelc_funmode_usage_str);
		break;
	case LTELC_CMD_NORMAL_MODE_AT:
		shell_print(shell, "%s", ltelc_normal_mode_at_usage_str);
		break;
	case LTELC_CMD_NORMAL_MODE_AUTO:
		shell_print(shell, "%s", ltelc_normal_mode_auto_usage_str);
		break;
	case LTELC_CMD_EDRX:
		shell_print(shell, "%s", ltelc_edrx_usage_str);
		break;
	case LTELC_CMD_PSM:
		shell_print(shell, "%s", ltelc_psm_usage_str);
		break;
	case LTELC_CMD_RSRP:
		shell_print(shell, "%s", ltelc_rsrp_usage_str);
		break;
	case LTELC_CMD_NCELLMEAS:
		shell_print(shell, "%s", ltelc_ncellmeas_usage_str);
		break;
	case LTELC_CMD_MDMSLEEP:
		shell_print(shell, "%s", ltelc_msleep_usage_str);
		break;
	case LTELC_CMD_TAU:
		shell_print(shell, "%s", ltelc_tau_usage_str);
		break;
	default:
		shell_print(shell, "%s", ltelc_usage_str);
		break;
	}
}

static void ltelc_shell_cmd_defaults_set(ltelc_shell_cmd_args_t *ltelc_cmd_args)
{
	memset(ltelc_cmd_args, 0, sizeof(ltelc_shell_cmd_args_t));
	ltelc_cmd_args->funmode_option = LTELC_FUNMODE_NONE;
	ltelc_cmd_args->sysmode_option = LTE_LC_SYSTEM_MODE_NONE;
	ltelc_cmd_args->sysmode_lte_pref_option =
		LTE_LC_SYSTEM_MODE_PREFER_AUTO;
	ltelc_cmd_args->lte_mode = LTE_LC_LTE_MODE_NONE;
}

/******************************************************************************/

/* From lte_lc.c, and TODO: to be updated if something is added */
#define SYS_MODE_PREFERRED					   \
	(IS_ENABLED(CONFIG_LTE_NETWORK_MODE_LTE_M)              ?  \
	 LTE_LC_SYSTEM_MODE_LTEM                         :	   \
	 IS_ENABLED(CONFIG_LTE_NETWORK_MODE_NBIOT)               ? \
	 LTE_LC_SYSTEM_MODE_NBIOT                        :	   \
	 IS_ENABLED(CONFIG_LTE_NETWORK_MODE_LTE_M_GPS)           ? \
	 LTE_LC_SYSTEM_MODE_LTEM_GPS                     :	   \
	 IS_ENABLED(CONFIG_LTE_NETWORK_MODE_NBIOT_GPS)           ? \
	 LTE_LC_SYSTEM_MODE_NBIOT_GPS                    :	   \
	 IS_ENABLED(CONFIG_LTE_NETWORK_MODE_LTE_M_NBIOT)         ? \
	 LTE_LC_SYSTEM_MODE_LTEM_NBIOT                   :	   \
	 IS_ENABLED(CONFIG_LTE_NETWORK_MODE_LTE_M_NBIOT_GPS)     ? \
	 LTE_LC_SYSTEM_MODE_LTEM_NBIOT_GPS               :	   \
	 LTE_LC_SYSTEM_MODE_NONE)

static void ltelc_shell_sysmode_set(const struct shell *shell, int sysmode,
				    int lte_pref)
{
	enum lte_lc_func_mode functional_mode;
	char snum[64];
	int ret = lte_lc_system_mode_set(sysmode, lte_pref);

	if (ret < 0) {
		shell_error(shell, "Cannot set system mode to modem: %d", ret);
		ret = lte_lc_func_mode_get(&functional_mode);
		if (functional_mode != LTE_LC_FUNC_MODE_OFFLINE ||
		    functional_mode != LTE_LC_FUNC_MODE_POWER_OFF) {
			shell_warn(
				shell,
				"Requested mode couldn't set to modem. Not in flighmode nor in pwroff?");
		}
	} else {
		shell_print(shell, "System mode set successfully to modem: %s",
			    ltelc_shell_sysmode_to_string(sysmode, snum));
	}
}

/******************************************************************************/

int ltelc_shell_get_and_print_current_system_modes(
	const struct shell *shell, enum lte_lc_system_mode *sys_mode_current,
	enum lte_lc_system_mode_preference *sys_mode_preferred,
	enum lte_lc_lte_mode *currently_active_mode)
{
	int ret = -1;

	char snum[64];

	ret = lte_lc_system_mode_get(sys_mode_current, sys_mode_preferred);
	if (ret >= 0) {
		shell_print(shell, "Modem config for system mode: %s",
			    ltelc_shell_sysmode_to_string(*sys_mode_current,
							  snum));
		shell_print(shell, "Modem config for LTE preference: %s",
			    ltelc_shell_sysmode_preferred_to_string(
				    *sys_mode_preferred, snum));
	} else {
		return ret;
	}

	ret = lte_lc_lte_mode_get(currently_active_mode);
	if (ret >= 0) {
		shell_print(shell, "Currently active system mode: %s",
			    ltelc_shell_sysmode_currently_active_to_string(
				    *currently_active_mode, snum));
	}
	return ret;
}

int ltelc_shell(const struct shell *shell, size_t argc, char **argv)
{
	ltelc_shell_cmd_args_t ltelc_cmd_args;
	int ret = 0;
	bool require_apn = false;
	bool require_pdn_cid = false;
	bool require_subscribe = false;
	bool require_option = false;
	char *apn = NULL;
	char *family = NULL;
	int protocol = 0;
	bool protocol_given = false;
	char *username = NULL;
	char *password = NULL;
	int pdn_cid = 0;
	int warn_time = 0;
	int threshold_time = 0;

	ltelc_shell_cmd_defaults_set(&ltelc_cmd_args);

	if (argc < 2) {
		goto show_usage;
	}

	/* command = argv[0] = "ltelc" */
	/* sub-command = argv[1]       */
	if (strcmp(argv[1], "status") == 0) {
		ltelc_cmd_args.command = LTELC_CMD_STATUS;
	} else if (strcmp(argv[1], "settings") == 0) {
		ltelc_cmd_args.command = LTELC_CMD_SETTINGS;
	} else if (strcmp(argv[1], "coneval") == 0) {
		ltelc_cmd_args.command = LTELC_CMD_CONEVAL;
	} else if (strcmp(argv[1], "rsrp") == 0) {
		require_subscribe = true;
		ltelc_cmd_args.command = LTELC_CMD_RSRP;
	} else if (strcmp(argv[1], "ncellmeas") == 0) {
		ltelc_cmd_args.command = LTELC_CMD_NCELLMEAS;
	} else if (strcmp(argv[1], "msleep") == 0) {
		require_option = true;
		ltelc_cmd_args.command = LTELC_CMD_MDMSLEEP;
	} else if (strcmp(argv[1], "tau") == 0) {
		require_subscribe = true;
		ltelc_cmd_args.command = LTELC_CMD_TAU;
	} else if (strcmp(argv[1], "connect") == 0) {
		require_apn = true;
		ltelc_cmd_args.command = LTELC_CMD_CONNECT;
	} else if (strcmp(argv[1], "disconnect") == 0) {
		require_pdn_cid = true;
		ltelc_cmd_args.command = LTELC_CMD_DISCONNECT;
	} else if (strcmp(argv[1], "defcont") == 0) {
		ltelc_cmd_args.command = LTELC_CMD_DEFCONT;
	} else if (strcmp(argv[1], "defcontauth") == 0) {
		ltelc_cmd_args.command = LTELC_CMD_DEFCONTAUTH;
	} else if (strcmp(argv[1], "funmode") == 0) {
		require_option = true;
		ltelc_cmd_args.command = LTELC_CMD_FUNMODE;
	} else if (strcmp(argv[1], "sysmode") == 0) {
		require_option = true;
		ltelc_cmd_args.command = LTELC_CMD_SYSMODE;
	} else if (strcmp(argv[1], "nmodeat") == 0) {
		ltelc_cmd_args.command = LTELC_CMD_NORMAL_MODE_AT;
	} else if (strcmp(argv[1], "nmodeauto") == 0) {
		ltelc_cmd_args.command = LTELC_CMD_NORMAL_MODE_AUTO;
	} else if (strcmp(argv[1], "edrx") == 0) {
		require_option = true;
		ltelc_cmd_args.command = LTELC_CMD_EDRX;
	} else if (strcmp(argv[1], "psm") == 0) {
		require_option = true;
		ltelc_cmd_args.command = LTELC_CMD_PSM;
	} else if (strcmp(argv[1], "help") == 0) {
		ltelc_cmd_args.command = LTELC_CMD_HELP;
		goto show_usage;
	} else {
		shell_error(shell, "Unsupported command=%s\n", argv[1]);
		ret = -EINVAL;
		goto show_usage;
	}

	/* We start from subcmd arguments */
	optind = 2;

	int long_index = 0;
	int opt;
	int apn_len = 0;

	char edrx_value_str[LTELC_SHELL_EDRX_VALUE_STR_LENGTH + 1];
	bool edrx_value_set = false;
	char edrx_ptw_bit_str[LTELC_SHELL_EDRX_PTW_STR_LENGTH + 1];
	bool edrx_ptw_set = false;

	char psm_rptau_bit_str[LTELC_SHELL_PSM_PARAM_STR_LENGTH + 1];
	bool psm_rptau_set = false;
	char psm_rat_bit_str[LTELC_SHELL_PSM_PARAM_STR_LENGTH + 1];
	bool psm_rat_set = false;

	char *normal_mode_at_str = NULL;
	uint8_t normal_mode_at_mem_slot = 0;

	while ((opt = getopt_long(argc, argv,
				  "a:I:f:x:w:p:t:A:P:U:su014rmngMNed",
				  long_options, &long_index)) != -1) {
		switch (opt) {
		/* RSRP: */
		case 's':
			ltelc_cmd_args.common_option = LTELC_COMMON_SUBSCRIBE;
			break;
		case 'u':
			ltelc_cmd_args.common_option = LTELC_COMMON_UNSUBSCRIBE;
			break;

		/* Modem functional modes: */
		case '0':
			ltelc_cmd_args.funmode_option =
				LTE_LC_FUNC_MODE_POWER_OFF;
			break;
		case '1':
			ltelc_cmd_args.funmode_option = LTE_LC_FUNC_MODE_NORMAL;
			break;
		case '4':
			ltelc_cmd_args.funmode_option =
				LTE_LC_FUNC_MODE_OFFLINE;
			break;
		case LTELC_SHELL_OPT_FUNMODE_LTEOFF:
			ltelc_cmd_args.funmode_option =
				LTE_LC_FUNC_MODE_DEACTIVATE_LTE;
			break;
		case LTELC_SHELL_OPT_FUNMODE_LTEON:
			ltelc_cmd_args.funmode_option =
				LTE_LC_FUNC_MODE_ACTIVATE_LTE;
			break;
		case LTELC_SHELL_OPT_FUNMODE_GNSSOFF:
			ltelc_cmd_args.funmode_option =
				LTE_LC_FUNC_MODE_DEACTIVATE_GNSS;
			break;
		case LTELC_SHELL_OPT_FUNMODE_GNSSON:
			ltelc_cmd_args.funmode_option =
				LTE_LC_FUNC_MODE_ACTIVATE_GNSS;
			break;
		case LTELC_SHELL_OPT_FUNMODE_UICCOFF:
			ltelc_cmd_args.funmode_option =
				LTE_LC_FUNC_MODE_DEACTIVATE_UICC;
			break;
		case LTELC_SHELL_OPT_FUNMODE_UICCON:
			ltelc_cmd_args.funmode_option =
				LTE_LC_FUNC_MODE_ACTIVATE_UICC;
			break;
		case LTELC_SHELL_OPT_FUNMODE_FLIGHTMODE_UICCON:
			ltelc_cmd_args.funmode_option =
				LTE_LC_FUNC_MODE_OFFLINE_UICC_ON;
			break;

		/* eDRX specifics: */
		case 'x': /* drx_value */
			if (strlen(optarg) ==
			    LTELC_SHELL_EDRX_VALUE_STR_LENGTH) {
				strcpy(edrx_value_str, optarg);
				edrx_value_set = true;
			} else {
				shell_error(
					shell,
					"eDRX value string length must be %d.",
					LTELC_SHELL_EDRX_VALUE_STR_LENGTH);
				return -EINVAL;
			}
			break;
		case 'w': /* Paging Time Window */
			if (strlen(optarg) == LTELC_SHELL_EDRX_PTW_STR_LENGTH) {
				strcpy(edrx_ptw_bit_str, optarg);
				edrx_ptw_set = true;
			} else {
				shell_error(shell,
					    "PTW string length must be %d.",
					    LTELC_SHELL_EDRX_PTW_STR_LENGTH);
				return -EINVAL;
			}
			break;

		/* PSM specifics: */
		case 'p': /* rptau */
			if (strlen(optarg) ==
			    LTELC_SHELL_PSM_PARAM_STR_LENGTH) {
				strcpy(psm_rptau_bit_str, optarg);
				psm_rptau_set = true;
			} else {
				shell_error(
					shell,
					"RPTAU bit string length must be %d.",
					LTELC_SHELL_PSM_PARAM_STR_LENGTH);
				return -EINVAL;
			}
			break;
		case 't': /* rat */
			if (strlen(optarg) ==
			    LTELC_SHELL_PSM_PARAM_STR_LENGTH) {
				strcpy(psm_rat_bit_str, optarg);
				psm_rat_set = true;
			} else {
				shell_error(shell,
					    "RAT bit string length must be %d.",
					    LTELC_SHELL_PSM_PARAM_STR_LENGTH);
				return -EINVAL;
			}
			break;

		/* Modem system modes: */
		case 'm':
			ltelc_cmd_args.sysmode_option = LTE_LC_SYSTEM_MODE_LTEM;
			ltelc_cmd_args.lte_mode = LTE_LC_LTE_MODE_LTEM;
			break;
		case 'n':
			ltelc_cmd_args.sysmode_option =
				LTE_LC_SYSTEM_MODE_NBIOT;
			ltelc_cmd_args.lte_mode = LTE_LC_LTE_MODE_NBIOT;
			break;
		case 'g':
			ltelc_cmd_args.sysmode_option = LTE_LC_SYSTEM_MODE_GPS;
			break;
		case 'M':
			ltelc_cmd_args.sysmode_option =
				LTE_LC_SYSTEM_MODE_LTEM_GPS;
			break;
		case 'N':
			ltelc_cmd_args.sysmode_option =
				LTE_LC_SYSTEM_MODE_NBIOT_GPS;
			break;

		/* Common options: */
		case 'e':
			ltelc_cmd_args.common_option = LTELC_COMMON_ENABLE;
			break;
		case 'd':
			ltelc_cmd_args.common_option = LTELC_COMMON_DISABLE;
			break;
		case 'r':
			ltelc_cmd_args.common_option = LTELC_COMMON_READ;
			break;
		case 'I': /* PDN CID */
			pdn_cid = atoi(optarg);
			if (pdn_cid == 0) {
				shell_error(
					shell,
					"PDN CID (%d) must be positive integer. "
					"Default PDN context (CID=0) cannot be given.",
					pdn_cid);
				return -EINVAL;
			}
			break;
		case 'a': /* APN */
			apn_len = strlen(optarg);
			if (apn_len > LTELC_APN_STR_MAX_LENGTH) {
				shell_error(
					shell,
					"APN string length %d exceeded. Maximum is %d.",
					apn_len, LTELC_APN_STR_MAX_LENGTH);
				ret = -EINVAL;
				goto show_usage;
			}
			apn = optarg;
			break;
		case 'f': /* Address family */
			family = optarg;
			break;
		case 'A': /* defcont auth protocol */
			protocol = atoi(optarg);
			protocol_given = true;
			break;
		case 'U': /* defcont auth username */
			username = optarg;
			break;
		case 'P': /* defcont auth password */
			password = optarg;
			break;
		/* Options without short option: */
		case LTELC_SHELL_OPT_RESET:
			ltelc_cmd_args.common_option = LTELC_COMMON_RESET;
			break;
		case LTELC_SHELL_OPT_START:
			ltelc_cmd_args.common_option = LTELC_COMMON_START;
			break;
		case LTELC_SHELL_OPT_STOP:
			ltelc_cmd_args.common_option = LTELC_COMMON_STOP;
			break;

		case LTELC_SHELL_OPT_MEM_SLOT_1:
			normal_mode_at_str = optarg;
			normal_mode_at_mem_slot = 1;
			break;
		case LTELC_SHELL_OPT_MEM_SLOT_2:
			normal_mode_at_str = optarg;
			normal_mode_at_mem_slot = 2;
			break;
		case LTELC_SHELL_OPT_MEM_SLOT_3:
			normal_mode_at_str = optarg;
			normal_mode_at_mem_slot = 3;
			break;

		case LTELC_SHELL_OPT_SYSMODE_LTEM_NBIOT:
			ltelc_cmd_args.sysmode_option =
				LTE_LC_SYSTEM_MODE_LTEM_NBIOT;
			break;
		case LTELC_SHELL_OPT_SYSMODE_LTEM_NBIOT_GPS:
			ltelc_cmd_args.sysmode_option =
				LTE_LC_SYSTEM_MODE_LTEM_NBIOT_GPS;
			break;

		case LTELC_SHELL_OPT_SYSMODE_PREF_AUTO:
			ltelc_cmd_args.sysmode_lte_pref_option =
				LTE_LC_SYSTEM_MODE_PREFER_AUTO;
			break;
		case LTELC_SHELL_OPT_SYSMODE_PREF_LTEM:
			ltelc_cmd_args.sysmode_lte_pref_option =
				LTE_LC_SYSTEM_MODE_PREFER_LTEM;
			break;
		case LTELC_SHELL_OPT_SYSMODE_PREF_NBIOT:
			ltelc_cmd_args.sysmode_lte_pref_option =
				LTE_LC_SYSTEM_MODE_PREFER_NBIOT;
			break;
		case LTELC_SHELL_OPT_SYSMODE_PREF_LTEM_PLMN_PRIO:
			ltelc_cmd_args.sysmode_lte_pref_option =
				LTE_LC_SYSTEM_MODE_PREFER_LTEM_PLMN_PRIO;
			break;
		case LTELC_SHELL_OPT_SYSMODE_PREF_NBIOT_PLMN_PRIO:
			ltelc_cmd_args.sysmode_lte_pref_option =
				LTE_LC_SYSTEM_MODE_PREFER_NBIOT_PLMN_PRIO;
			break;
		case LTELC_SHELL_OPT_WARN_TIME:
			warn_time = atoi(optarg);
			if (warn_time == 0) {
				shell_error(
					shell,
					"Not a valid number for --warn_time (milliseconds).");
				return -EINVAL;
			}
			break;
		case LTELC_SHELL_OPT_THRESHOLD_TIME:
			threshold_time = atoi(optarg);
			if (threshold_time == 0) {
				shell_error(
					shell,
					"Not a valid number for --threshold_time (milliseconds).");
				return -EINVAL;
			}
			break;
		case '?':
			goto show_usage;
			break;
		default:
			shell_error(shell, "Unknown option. See usage:");
			goto show_usage;
			break;
		}
	}

	/* Check that all mandatory args were given: */
	if (require_apn && apn == NULL) {
		shell_error(shell,
			    "Option -a | -apn MUST be given. See usage:");
		goto show_usage;
	} else if (require_pdn_cid && pdn_cid == 0) {
		shell_error(shell, "-I / --cid MUST be given. See usage:");
		goto show_usage;
	} else if (require_subscribe &&
		   ltelc_cmd_args.common_option == LTELC_COMMON_NONE) {
		shell_error(shell, "Either -s or -u MUST be given. See usage:");
		goto show_usage;
	} else if (require_option &&
		   ltelc_cmd_args.funmode_option == LTELC_FUNMODE_NONE &&
		   ltelc_cmd_args.sysmode_option == LTE_LC_SYSTEM_MODE_NONE &&
		   ltelc_cmd_args.common_option == LTELC_COMMON_NONE) {
		shell_error(shell,
			    "Command needs option to be given. See usage:");
		goto show_usage;
	}

	char snum[64];

	switch (ltelc_cmd_args.command) {
	case LTELC_CMD_DEFCONT:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_READ) {
			ltelc_sett_defcont_conf_shell_print(shell);
		} else if (ltelc_cmd_args.common_option ==
			   LTELC_COMMON_ENABLE) {
			ltelc_sett_save_defcont_enabled(true);
		} else if (ltelc_cmd_args.common_option ==
			   LTELC_COMMON_DISABLE) {
			static const char cgdcont[] = "AT+CGDCONT=0";

			if (at_cmd_write(cgdcont, NULL, 0, NULL) != 0) {
				shell_warn(
					shell,
					"ERROR from modem. Getting the initial PDP context back wasn't successful.");
				shell_warn(
					shell,
					"Please note: you might need to visit the pwroff state to make an impact to modem.");
			}
			ltelc_sett_save_defcont_enabled(false);
			shell_print(shell,
				    "Custom default context config disabled.");
		} else if (ltelc_cmd_args.common_option == LTELC_COMMON_NONE &&
			   apn == NULL && family == NULL) {
			goto show_usage;
		}
		if (apn != NULL) {
			(void)ltelc_sett_save_defcont_apn(apn);
		}
		if (family != NULL) {
			enum pdn_fam pdn_lib_fam;

			ret = ltelc_family_str_to_pdn_lib_family(&pdn_lib_fam,
								 family);
			if (ret) {
				shell_error(shell, "Unknown PDN family %s",
					    family);
				goto show_usage;
			} else {
				(void)ltelc_sett_save_defcont_pdn_family(
					pdn_lib_fam);
			}
		}
		break;
	case LTELC_CMD_DEFCONTAUTH:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_READ) {
			ltelc_sett_defcontauth_conf_shell_print(shell);
		} else if (ltelc_cmd_args.common_option ==
			   LTELC_COMMON_ENABLE) {
			if (ltelc_sett_save_defcontauth_enabled(true) < 0) {
				shell_warn(shell,
					   "Cannot enable authentication.");
			}
		} else if (ltelc_cmd_args.common_option ==
			   LTELC_COMMON_DISABLE) {
			static const char cgauth[] = "AT+CGAUTH=0,0";

			if (at_cmd_write(cgauth, NULL, 0, NULL) != 0) {
				shell_warn(
					shell,
					"Disabling of auth cannot be done to modem.");
			}
			ltelc_sett_save_defcontauth_enabled(false);
		} else if (ltelc_cmd_args.common_option == LTELC_COMMON_NONE &&
			   !protocol_given && username == NULL &&
			   password == NULL) {
			goto show_usage;
		}

		if (protocol_given) {
			(void)ltelc_sett_save_defcontauth_prot(protocol);
		}
		if (username != NULL) {
			(void)ltelc_sett_save_defcontauth_username(username);
		}
		if (password != NULL) {
			(void)ltelc_sett_save_defcontauth_password(password);
		}
		break;

	case LTELC_CMD_STATUS: {
		enum lte_lc_nw_reg_status current_reg_status;
		enum lte_lc_func_mode functional_mode;
		bool connected = false;

		ret = lte_lc_func_mode_get(&functional_mode);
		if (ret) {
			shell_warn(shell,
				   "Cannot get functional mode from modem: %d",
				   ret);
		} else {
			shell_print(shell, "Modem functional mode: %s",
				    ltelc_shell_funmode_to_string(
					    functional_mode, snum));
		}
		ret = lte_lc_nw_reg_status_get(&current_reg_status);
		if (ret >= 0) {
			ltelc_shell_print_reg_status(shell, current_reg_status);
		} else {
			shell_error(
				shell,
				"Cannot get current registration status (%d)",
				ret);
		}
		if (current_reg_status == LTE_LC_NW_REG_REGISTERED_EMERGENCY ||
		    current_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ||
		    current_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING) {
			connected = true;
		}

		ltelc_api_modem_info_get_for_shell(shell, connected);
		break;
	}
	case LTELC_CMD_SETTINGS:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_READ) {
			ltelc_sett_all_print(shell);
		} else if (ltelc_cmd_args.common_option == LTELC_COMMON_RESET) {
			ltelc_sett_defaults_set(shell);
			ltelc_shell_sysmode_set(shell, SYS_MODE_PREFERRED,
						CONFIG_LTE_MODE_PREFERENCE);
		} else {
			goto show_usage;
		}
		break;
	case LTELC_CMD_CONEVAL:
		ltelc_api_coneval_read_for_shell(shell);
		break;

	case LTELC_CMD_SYSMODE:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_READ) {
			enum lte_lc_system_mode sys_mode_current;
			enum lte_lc_system_mode_preference sys_mode_pref_current;
			enum lte_lc_lte_mode currently_active_mode;

			ret = ltelc_shell_get_and_print_current_system_modes(
				shell, &sys_mode_current,
				&sys_mode_pref_current, &currently_active_mode);
			if (ret < 0) {
				shell_error(
					shell,
					"Cannot read system mode of the modem: %d",
					ret);
			} else {
				enum lte_lc_system_mode sett_sys_mode;
				enum lte_lc_system_mode_preference sett_lte_pref;

				/* Print also settings stored in mosh side: */
				ltelc_sett_sysmode_print(shell);
				sett_sys_mode = ltelc_sett_sysmode_get();
				sett_lte_pref =
					ltelc_sett_sysmode_lte_preference_get();
				if (sett_sys_mode != LTE_LC_SYSTEM_MODE_NONE &&
				    sett_sys_mode != sys_mode_current &&
				    sett_lte_pref != sys_mode_pref_current) {
					shell_warn(
						shell,
						"note: seems that set ltelc sysmode and a counterparts in modem are not in synch");
					shell_warn(
						shell,
						"but no worries; requested system mode retried next time when going to normal mode");
				}
			}
		} else if (ltelc_cmd_args.sysmode_option !=
			   LTE_LC_SYSTEM_MODE_NONE) {
			ltelc_shell_sysmode_set(
				shell, ltelc_cmd_args.sysmode_option,
				ltelc_cmd_args.sysmode_lte_pref_option);

			/* Save system modem to ltelc settings: */
			(void)ltelc_sett_sysmode_save(
				ltelc_cmd_args.sysmode_option,
				ltelc_cmd_args.sysmode_lte_pref_option);

		} else if (ltelc_cmd_args.common_option == LTELC_COMMON_RESET) {
			ltelc_shell_sysmode_set(shell, SYS_MODE_PREFERRED,
						CONFIG_LTE_MODE_PREFERENCE);

			(void)ltelc_sett_sysmode_default_set();
		} else {
			goto show_usage;
		}
		break;
	case LTELC_CMD_FUNMODE:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_READ) {
			enum lte_lc_func_mode functional_mode;

			ret = lte_lc_func_mode_get(&functional_mode);
			if (ret) {
				shell_error(shell,
					    "Cannot get functional mode: %d",
					    ret);
			} else {
				shell_print(
					shell,
					"Functional mode read successfully: %s",
					ltelc_shell_funmode_to_string(
						functional_mode, snum));
			}
		} else if (ltelc_cmd_args.funmode_option !=
			   LTELC_FUNMODE_NONE) {
			ret = ltelc_func_mode_set(
				ltelc_cmd_args.funmode_option);
			if (ret < 0) {
				shell_error(shell,
					    "Cannot set functional mode: %d",
					    ret);
			} else {
				shell_print(
					shell,
					"Functional mode set successfully: %s",
					ltelc_shell_funmode_to_string(
						ltelc_cmd_args.funmode_option,
						snum));
			}
		} else {
			goto show_usage;
		}
		break;
	case LTELC_CMD_NORMAL_MODE_AT:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_READ) {
			ltelc_sett_normal_mode_at_cmds_shell_print(shell);
		} else if (normal_mode_at_str != NULL) {
			ret = ltelc_sett_save_normal_mode_at_cmd_str(
				normal_mode_at_str, normal_mode_at_mem_slot);
			if (ret < 0) {
				shell_error(
					shell,
					"Cannot set normal mode AT-command: \"%s\"",
					normal_mode_at_str);
			} else {
				shell_print(
					shell,
					"Normal mode AT-command \"%s\" set successfully to memory slot %d.",
					((strlen(normal_mode_at_str)) ?
					 normal_mode_at_str :
					 "<empty>"),
					normal_mode_at_mem_slot);
			}
		} else {
			goto show_usage;
		}
		break;
	case LTELC_CMD_NORMAL_MODE_AUTO:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_READ) {
			ltelc_sett_normal_mode_autoconn_shell_print(shell);
		} else if (ltelc_cmd_args.common_option ==
			   LTELC_COMMON_ENABLE) {
			ltelc_sett_save_normal_mode_autoconn_enabled(true);
		} else if (ltelc_cmd_args.common_option ==
			   LTELC_COMMON_DISABLE) {
			ltelc_sett_save_normal_mode_autoconn_enabled(false);
		} else {
			goto show_usage;
		}
		break;

	case LTELC_CMD_EDRX:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_ENABLE) {
			char *value =
				NULL; /* Set with the defaults if not given */

			if (ltelc_cmd_args.lte_mode == LTE_LC_LTE_MODE_NONE) {
				shell_error(
					shell,
					"LTE mode is mandatory to be given. See usage:");
				goto show_usage;
			}

			if (edrx_value_set) {
				value = edrx_value_str;
			}

			ret = lte_lc_edrx_param_set(ltelc_cmd_args.lte_mode,
						    value);
			if (ret < 0) {
				shell_error(
					shell,
					"Cannot set eDRX value %s, error: %d",
					((value == NULL) ? "NULL" : value),
					ret);
				return -EINVAL;
			}
			value = NULL; /* Set with the defaults if not given */
			if (edrx_ptw_set) {
				value = edrx_ptw_bit_str;
			}

			ret = lte_lc_ptw_set(ltelc_cmd_args.lte_mode, value);
			if (ret < 0) {
				shell_error(
					shell,
					"Cannot set PTW value %s, error: %d",
					((value == NULL) ? "NULL" : value),
					ret);
				return -EINVAL;
			}

			ret = lte_lc_edrx_req(true);
			if (ret < 0) {
				shell_error(shell, "Cannot enable eDRX: %d",
					    ret);
			} else {
				shell_print(shell, "eDRX enabled");
			}
		} else if (ltelc_cmd_args.common_option ==
			   LTELC_COMMON_DISABLE) {
			ret = lte_lc_edrx_req(false);
			if (ret < 0) {
				shell_error(shell, "Cannot disable eDRX: %d",
					    ret);
			} else {
				shell_print(shell, "eDRX disabled");
			}
		} else {
			shell_error(
				shell,
				"Unknown option for edrx command. See usage:");
			goto show_usage;
		}
		break;
	case LTELC_CMD_PSM:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_ENABLE) {
			/* Set with the defaults if not given */
			char *rptau_bit_value = NULL;
			char *rat_bit_value = NULL;

			if (psm_rptau_set) {
				rptau_bit_value = psm_rptau_bit_str;
			}

			if (psm_rat_set) {
				rat_bit_value = psm_rat_bit_str;
			}

			ret = lte_lc_psm_param_set(rptau_bit_value,
						   rat_bit_value);
			if (ret < 0) {
				shell_error(
					shell,
					"Cannot set PSM parameters: error %d",
					ret);
				shell_error(shell, "  rptau %s, rat %s",
					    ((rptau_bit_value == NULL) ?
					     "NULL" :
					     rptau_bit_value),
					    ((rat_bit_value == NULL) ?
					     "NULL" :
					     rat_bit_value));
				return -EINVAL;
			}

			ret = lte_lc_psm_req(true);
			if (ret < 0) {
				shell_error(shell, "Cannot enable PSM: %d",
					    ret);
			} else {
				shell_print(shell, "PSM enabled");
			}
		} else if (ltelc_cmd_args.common_option ==
			   LTELC_COMMON_DISABLE) {
			ret = lte_lc_psm_req(false);
			if (ret < 0) {
				shell_error(shell, "Cannot disable PSM: %d",
					    ret);
			} else {
				shell_print(shell, "PSM disabled");
			}
		} else if (ltelc_cmd_args.common_option == LTELC_COMMON_READ) {
			int tau, active_time;

			ret = lte_lc_psm_get(&tau, &active_time);
			if (ret < 0) {
				shell_error(shell, "Cannot get PSM configs: %d",
					    ret);
			} else {
				shell_print(
					shell,
					"PSM config: TAU %d %s, active time %d %s",
					tau,
					(tau == -1) ? "(timer deactivated)" :
					"seconds",
					active_time,
					(active_time == -1) ?
					"(timer deactivated)" :
					"seconds");
			}
		} else {
			shell_error(
				shell,
				"Unknown option for psm command. See usage:");
			goto show_usage;
		}
		break;

	case LTELC_CMD_RSRP:
		(ltelc_cmd_args.common_option == LTELC_COMMON_SUBSCRIBE) ?
		ltelc_rsrp_subscribe(true) :
		ltelc_rsrp_subscribe(false);
		break;
	case LTELC_CMD_NCELLMEAS:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_STOP) {
			ltelc_ncellmeas_start(false);

		} else {
			ltelc_ncellmeas_start(true);
		}
		break;
	case LTELC_CMD_MDMSLEEP:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_SUBSCRIBE) {
			ltelc_modem_sleep_notifications_subscribe(
				((warn_time) ?
				 warn_time :
				 CONFIG_LTE_LC_MODEM_SLEEP_PRE_WARNING_TIME_MS),
				((threshold_time) ?
				 threshold_time :
				 CONFIG_LTE_LC_MODEM_SLEEP_NOTIFICATIONS_THRESHOLD_MS));
		} else {
			ltelc_modem_sleep_notifications_unsubscribe();
		}
		break;
	case LTELC_CMD_TAU:
		if (ltelc_cmd_args.common_option == LTELC_COMMON_SUBSCRIBE) {
			ltelc_modem_tau_notifications_subscribe(
				((warn_time) ?
				 warn_time :
				 CONFIG_LTE_LC_TAU_PRE_WARNING_TIME_MS),
				((threshold_time) ?
				 threshold_time :
				 CONFIG_LTE_LC_TAU_PRE_WARNING_THRESHOLD_MS));
		} else {
			ltelc_modem_tau_notifications_unsubscribe();
		}
		break;
	case LTELC_CMD_CONNECT:
		ret = ltelc_shell_pdn_connect(shell, apn, family);
		break;
	case LTELC_CMD_DISCONNECT:
		ret = ltelc_shell_pdn_disconnect(shell, pdn_cid);
		break;
	default:
		shell_error(shell, "Internal error. Unknown ltelc command=%d",
			    ltelc_cmd_args.command);
		ret = -EINVAL;
		break;
	}
	return ret;

show_usage:
	ltelc_shell_print_usage(shell, &ltelc_cmd_args);
	return ret;
}
