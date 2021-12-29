#
# Copyright (c) 2022 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

zephyr_library()
zephyr_library_sources(metrics.c)
zephyr_library_sources_ifdef(CONFIG_METRICS_CLOUD_THINGSPEAK integrations/thingspeak_rest.c)
