//! @file
//!
//! Copyright (c) Memfault, Inc.
//! See License.txt for details
//!
//! @brief
//! An example implementation of overriding the Memfault logging macros by
//! placing definitions in memfault_platform_log_config.h and adding
//! -DMEMFAULT_PLATFORM_HAS_LOG_CONFIG=1 to the compiler flags

#pragma once

#include "memfault/core/compiler.h"

#define MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES 256

#define _MEMFAULT_LOG_IMPL(_level, ...)  memfault_platform_log(_level, __VA_ARGS__)

#define MEMFAULT_LOG_DEBUG(...) _MEMFAULT_LOG_IMPL(kMemfaultPlatformLogLevel_Debug, __VA_ARGS__)

#define MEMFAULT_LOG_INFO(...) _MEMFAULT_LOG_IMPL(kMemfaultPlatformLogLevel_Info, __VA_ARGS__)

#define MEMFAULT_LOG_WARN(...) _MEMFAULT_LOG_IMPL(kMemfaultPlatformLogLevel_Warning, __VA_ARGS__)

#define MEMFAULT_LOG_ERROR(...) _MEMFAULT_LOG_IMPL(kMemfaultPlatformLogLevel_Error, __VA_ARGS__)

//! Only needs to be implemented when using demo component
#define MEMFAULT_LOG_RAW(...) memfault_platform_log_raw(__VA_ARGS__)
