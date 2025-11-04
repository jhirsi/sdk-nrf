/**
 * @file thread_safe_log.h
 * @brief Thread-safe logging utilities for DECT integration tests
 */

#ifndef THREAD_SAFE_LOG_H
#define THREAD_SAFE_LOG_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(test_dect_integration);

/* Mutex for thread-safe logging */
extern struct k_mutex log_mutex;

/* Thread-safe logging macros using LOG macros */
#define SAFE_LOG_INF(fmt, ...)                                                                     \
	do {                                                                                       \
		k_mutex_lock(&log_mutex, K_FOREVER);                                               \
		LOG_INF(fmt, ##__VA_ARGS__);                                                       \
		k_msleep(1); /* Give log thread time to process */                                 \
		k_mutex_unlock(&log_mutex);                                                        \
	} while (0)

#define SAFE_LOG_ERR(fmt, ...)                                                                     \
	do {                                                                                       \
		k_mutex_lock(&log_mutex, K_FOREVER);                                               \
		LOG_ERR(fmt, ##__VA_ARGS__);                                                       \
		k_msleep(1); /* Give log thread time to process */                                 \
		k_mutex_unlock(&log_mutex);                                                        \
	} while (0)

#define SAFE_LOG_DBG(fmt, ...)                                                                     \
	do {                                                                                       \
		k_mutex_lock(&log_mutex, K_FOREVER);                                               \
		LOG_DBG(fmt, ##__VA_ARGS__);                                                       \
		k_msleep(1); /* Give log thread time to process */                                 \
		k_mutex_unlock(&log_mutex);                                                        \
	} while (0)

/* Initialize thread-safe logging */
void init_thread_safe_logging(void);

#endif /* THREAD_SAFE_LOG_H */
