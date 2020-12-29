/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>

#include <zephyr.h>
#include <shell/shell.h>
#include <getopt.h>

#include "gnss.h"

/* Sets the global shell variable, must be called before calling any GNSS
 * module functions.
 */
#define GNSS_SET_GLOBAL_SHELL() \
    gnss_shell_global = shell;

/* Exits the function and returns an appropriate error code if GNSS is
 * running.
 */
#define GNSS_CMD_FAIL_IF_RUNNING() \
    if (gnss_running) { \
        shell_error(shell, "stop GNSS to configure"); \
        return -ENOEXEC; \
    }

const struct shell *gnss_shell_global;

static bool gnss_running = false;

static int print_help(const struct shell *shell, size_t argc, char **argv)
{
    int ret = 1;

    if (argc > 1) {
        shell_error(shell, "%s: subcommand not found", argv[1]);
        ret = -EINVAL;
    }

    shell_help(shell);

    return ret;
}

int cmd_gnss(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();

    return print_help(shell, argc, argv);
}

int cmd_gnss_start(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();

    int err;

    if (gnss_running) {
        shell_error(shell, "start: GNSS already running");
        return -ENOEXEC;
    }

    err = gnss_start();
    if (!err) {
        gnss_running = true;
    } else {
        shell_error(shell, "start: starting GNSS failed, err %d", err);
    }

    return err;
}

int cmd_gnss_stop(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();

    int err;

    if (!gnss_running) {
        shell_error(shell, "stop: GNSS not running");
        return -ENOEXEC;
    }

    err = gnss_stop();
    if (!err) {
        gnss_running = false;
    } else {
        shell_error(shell, "stop: stopping GNSS failed, err %d", err);
    }

    return err;
}

int cmd_gnss_mode(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();

    return print_help(shell, argc, argv);
}

int cmd_gnss_mode_cont(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    int err;

    err = gnss_set_continuous_mode();

    return err;
}

int cmd_gnss_mode_single(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    int err;
    int timeout;

    timeout = atoi(argv[1]);
    if (timeout < 0 || timeout > UINT16_MAX) {
        shell_error(
            shell,
            "single: invalid timeout value %d",
            timeout);
        return -EINVAL;
    }

    err = gnss_set_single_fix_mode(timeout);

    return err;
}

int cmd_gnss_mode_periodic(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    int err;
    int interval;
    int timeout;

    interval = atoi(argv[1]);
    if (interval < 10 || interval > 1800) {
        shell_error(
            shell,
            "periodic: invalid interval value %d, the value must be 10...1800",
            interval);
        return -EINVAL;
    }

    timeout = atoi(argv[2]);
    if (timeout < 0 || timeout > UINT16_MAX) {
        shell_error(
            shell,
            "periodic: invalid timeout value %d",
            timeout);
        return -EINVAL;
    }

    err = gnss_set_periodic_fix_mode(interval, timeout);

    return err;
}

int cmd_gnss_config(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();

    return print_help(shell, argc, argv);
}

int cmd_gnss_config_startmode(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();

    return print_help(shell, argc, argv);
}

int cmd_gnss_config_startmode_normal(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    gnss_set_delete_stored_data(false);

    return 0;
}

int cmd_gnss_config_startmode_cold(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    gnss_set_delete_stored_data(true);

    return 0;
}

int cmd_gnss_config_elevation(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    int err;
    int elevation;

    if (argc != 2) {
        shell_error(shell, "elevation: wrong parameter count");
        shell_print(shell, "elevation: <angle>");
        shell_print(shell, "angle:\tElevation threshold angle (in degrees). Satellites with elevation angle less than the threshold are excluded.");
        return -EINVAL;
    }

    elevation = atoi(argv[1]);

    if (elevation < 0 || elevation > 90) {
        shell_error(shell, "elevation: invalid elevation value %d", elevation);
        return -EINVAL;
    }

    err = gnss_set_elevation_threshold(elevation);
    if (err == -EOPNOTSUPP) {
        shell_error(shell, "elevation: operation not supported by selected API");
    }

    return err;
}

int cmd_gnss_config_system(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    uint8_t value;
    uint8_t system_mask;
    uint8_t system_mask_bit;

    system_mask = 0;
    system_mask_bit = 1;
    for (int i = 0; i < 3; i++) {
        value = atoi(argv[i+1]);
        if (value == 1) {
            system_mask |= system_mask_bit;
        }
        system_mask_bit = system_mask_bit << 1;
    }

    return gnss_set_system_mask(system_mask);
}

int cmd_gnss_config_nmea(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    uint8_t value;
    uint16_t nmea_mask;
    uint16_t nmea_mask_bit;

    nmea_mask = 0;
    nmea_mask_bit = 1;
    for (int i = 0; i < 5; i++) {
        value = atoi(argv[i+1]);
        if (value == 1) {
            nmea_mask |= nmea_mask_bit;
        }
        nmea_mask_bit = nmea_mask_bit << 1;
    }

    return gnss_set_nmea_mask(nmea_mask);
}

int cmd_gnss_config_powersave(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();

    return print_help(shell, argc, argv);
}

int cmd_gnss_config_powersave_off(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    int err;

    err = gnss_set_duty_cycling_policy(GNSS_DUTY_CYCLING_DISABLED);

    return err;
}

int cmd_gnss_config_powersave_perf(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    int err;

    err = gnss_set_duty_cycling_policy(GNSS_DUTY_CYCLING_PERFORMANCE);

    return err;
}

int cmd_gnss_config_powersave_power(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();
    GNSS_CMD_FAIL_IF_RUNNING();

    int err;

    err = gnss_set_duty_cycling_policy(GNSS_DUTY_CYCLING_POWER);

    return err;
}

int cmd_gnss_output(const struct shell *shell, size_t argc, char **argv)
{
    GNSS_SET_GLOBAL_SHELL();

    int err;
    int pvt_level;
    int nmea_level;
    int event_level;

    if (argc != 4) {
        shell_error(shell, "output: wrong parameter count");
        shell_print(shell, "output: <pvt level> <nmea level> <event level>");
        shell_print(shell, "pvt level:\n  0 = no PVT output\n  1 = PVT output\n  2 = PVT output with SV information");
        shell_print(shell, "nmea level:\n  0 = no NMEA output\n  1 = NMEA output");
        shell_print(shell, "event level:\n  0 = no event output\n  1 = event output");
        return -EINVAL;
    }

    pvt_level = atoi(argv[1]);
    nmea_level = atoi(argv[2]);
    event_level = atoi(argv[3]);

    err = gnss_set_pvt_output_level(pvt_level);
    if (err) {
        shell_error(gnss_shell_global, "output: invalid PVT output level");
    }
    err = gnss_set_nmea_output_level(nmea_level);
    if (err) {
        shell_error(gnss_shell_global, "output: invalid NMEA output level");
    }
    err = gnss_set_event_output_level(event_level);
    if (err) {
        shell_error(gnss_shell_global, "output: invalid event output level");
    }

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gnss_mode,
    SHELL_CMD_ARG(cont, NULL, "Continuous tracking mode.", cmd_gnss_mode_cont, 1, 0),
    SHELL_CMD_ARG(single, NULL, "<timeout>\nSingle fix mode.", cmd_gnss_mode_single, 2, 0),
    SHELL_CMD_ARG(periodic, NULL, "<interval> <timeout>\nPeriodic fix mode.", cmd_gnss_mode_periodic, 3, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gnss_config_startmode,
    SHELL_CMD_ARG(normal, NULL, "Normal start.", cmd_gnss_config_startmode_normal, 1, 0),
    SHELL_CMD_ARG(cold, NULL, "Cold start (all stored GNSS data erased on each start command).", cmd_gnss_config_startmode_cold, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gnss_config_powersave,
    SHELL_CMD_ARG(off, NULL, "Power saving off.", cmd_gnss_config_powersave_off, 1, 0),
    SHELL_CMD_ARG(perf, NULL, "Power saving without significant performance degradation.", cmd_gnss_config_powersave_perf, 1, 0),
    SHELL_CMD_ARG(power, NULL, "Power saving with acceptable performance degradation.", cmd_gnss_config_powersave_power, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gnss_config,
    SHELL_CMD(startmode, &sub_gnss_config_startmode, "Start mode.", cmd_gnss_config_startmode),
    SHELL_CMD(elevation, NULL, "<angle>\nElevation threshold angle.", cmd_gnss_config_elevation),
    SHELL_CMD_ARG(system, NULL, "<GPS enabled> <SBAS enabled> <QZSS enabled>\nSystem mask. 0 = disabled, 1 = enabled.", cmd_gnss_config_system, 4, 0),
    SHELL_CMD_ARG(nmea, NULL, "<GGA enabled> <GLL enabled> <GSA enabled> <GSV enabled> <RMC enabled>\nNMEA mask. 0 = disabled, 1 = enabled.", cmd_gnss_config_nmea, 6, 0),
    SHELL_CMD(powersave, &sub_gnss_config_powersave, "Continuous tracking power saving mode.", cmd_gnss_config_powersave),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gnss,
    SHELL_CMD_ARG(start, NULL, "Start GNSS.", cmd_gnss_start, 1, 0),
    SHELL_CMD_ARG(stop, NULL, "Stop GNSS.", cmd_gnss_stop, 1, 0),
    SHELL_CMD(mode, &sub_gnss_mode, "Set tracking mode.", cmd_gnss_mode),
    SHELL_CMD(config, &sub_gnss_config, "Set GNSS configuration.", cmd_gnss_config),
    SHELL_CMD(output, NULL, "<pvt level> <nmea level> <event level>\nSet output levels.", cmd_gnss_output),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(gnss, &sub_gnss, "Commands for controlling GNSS.", cmd_gnss);
