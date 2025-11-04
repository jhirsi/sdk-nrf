/**
 * @file thread_safe_log.c
 * @brief Thread-safe logging implementation
 */

#include "thread_safe_log.h"

/* Mutex for thread-safe logging */
K_MUTEX_DEFINE(log_mutex);

void init_thread_safe_logging(void)
{
	/* Mutex is already initialized by K_MUTEX_DEFINE */
	LOG_INF("Thread-safe logging initialized");
}
