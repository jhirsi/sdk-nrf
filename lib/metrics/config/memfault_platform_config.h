#pragma once

/*
 * Platform overrides for the default configuration settings in the
 * memfault-firmware-sdk. Default configuration settings can be found in
 * "<NCS folder>/modules/lib/memfault-firmware-sdk/components/include/memfault/default_config.h"
 */

/* Uncomment the definition below to override the default setting for
 * heartbeat interval. This will prepare the captured metric data for upload
 * to Memfault cloud at the specified interval.
 */
/*
 * #define MEMFAULT_METRICS_HEARTBEAT_INTERVAL_SECS 1800
 */

/* We want to have our own logs to memfault */
#ifdef MEMFAULT_SDK_LOG_SAVE_DISABLE
#undef MEMFAULT_SDK_LOG_SAVE_DISABLE
#define MEMFAULT_SDK_LOG_SAVE_DISABLE 0
#endif

/* We need to have our own because we don't want _MEMFAULT_LOG_IMPL to log to cloud */
#define MEMFAULT_PLATFORM_HAS_LOG_CONFIG 1
