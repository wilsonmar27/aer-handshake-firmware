#ifndef LOG_H
#define LOG_H

/*
 * Portable logging hooks.
 *
 * This header provides lightweight log macros that can map to:
 * - printf() on host builds, or
 * - a platform-specific printf-like function (UART/USB) on embedded builds.
 *
 * Integration:
 * - Default: if AER_LOG_PRINTF is not defined, we include <stdio.h> and use printf.
 * - On Pico: define AER_LOG_PRINTF(...) to route to your preferred output
 *   (e.g., printf from pico_stdio, or a UART writer).
 *
 * Log levels:
 *   0 = silent
 *   1 = error
 *   2 = warn
 *   3 = info
 *   4 = debug
 */

#include <stdint.h>

/* Compile-time log level (override in build system). */
#define AER_LOG_LEVEL 3

/* tag/prefix for log lines. */
#define AER_LOG_TAG "AER"

/* Provide a printf-like sink if user didn't supply one. */
#ifndef AER_LOG_PRINTF
#include <stdio.h>
#define AER_LOG_PRINTF(...) printf(__VA_ARGS__)
#endif

#if AER_LOG_LEVEL >= 1
#define AER_LOGE(fmt, ...) AER_LOG_PRINTF("[E] %s: " fmt "\n", AER_LOG_TAG, ##__VA_ARGS__)
#else
#define AER_LOGE(fmt, ...) ((void)0)
#endif

#if AER_LOG_LEVEL >= 2
#define AER_LOGW(fmt, ...) AER_LOG_PRINTF("[W] %s: " fmt "\n", AER_LOG_TAG, ##__VA_ARGS__)
#else
#define AER_LOGW(fmt, ...) ((void)0)
#endif

#if AER_LOG_LEVEL >= 3
#define AER_LOGI(fmt, ...) AER_LOG_PRINTF("[I] %s: " fmt "\n", AER_LOG_TAG, ##__VA_ARGS__)
#else
#define AER_LOGI(fmt, ...) ((void)0)
#endif

#if AER_LOG_LEVEL >= 4
#define AER_LOGD(fmt, ...) AER_LOG_PRINTF("[D] %s: " fmt "\n", AER_LOG_TAG, ##__VA_ARGS__)
#else
#define AER_LOGD(fmt, ...) ((void)0)
#endif

#endif /* LOG_H */
