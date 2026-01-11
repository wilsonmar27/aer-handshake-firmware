// pico/hal/hal_stdio.h
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB-only stdio + structured streaming.
 *
 * IMPORTANT CMake knobs:
 *   pico_enable_stdio_uart(<target> 0)
 *   pico_enable_stdio_usb(<target> 1)
 */

typedef enum hal_log_level_e {
    HAL_LOG_ERROR = 0,
    HAL_LOG_WARN  = 1,
    HAL_LOG_INFO  = 2,
    HAL_LOG_DEBUG = 3,
    HAL_LOG_TRACE = 4,
} hal_log_level_t;

/** Stream packet types (framing for host parsing). */
typedef enum hal_stream_type_e {
    HAL_STREAM_LOG_TEXT   = 1,  // payload: UTF-8 text (no NUL)
    HAL_STREAM_EVENT_BIN  = 2,  // payload: binary event records (your choice)
    HAL_STREAM_RAW_BIN    = 3,  // payload: arbitrary binary
    HAL_STREAM_MARKER     = 4,  // payload: small markers (optional)
} hal_stream_type_t;

/** Basic init; if wait_for_usb is true, blocks up to timeout_ms for host connection. */
void hal_stdio_init(bool wait_for_usb, uint32_t timeout_ms);

/** True if USB CDC is connected (best-effort). */
bool hal_stdio_is_connected(void);

/** Wait for a connection up to timeout_ms; returns true if connected. */
bool hal_stdio_wait_connected(uint32_t timeout_ms);

/** Set minimum log level emitted (default: INFO). */
void hal_log_set_level(hal_log_level_t level);
hal_log_level_t hal_log_get_level(void);

/**
 * Enable "packetized mode" for logs/events.
 * - When enabled, logs are sent as framed packets (type=HAL_STREAM_LOG_TEXT)
 * - When disabled, logs use plain printf (human readable) but MUST NOT be used
 *   concurrently with binary event streaming.
 *
 * Default: packetized = true (safe for mixed logs + events).
 */
void hal_stdio_set_packetized(bool enabled);
bool hal_stdio_get_packetized(void);

/** Flush stdio output. */
void hal_stdio_flush(void);

/* ---------------- Logging ---------------- */

void hal_logf(hal_log_level_t level, const char *fmt, ...);
void hal_vlogf(hal_log_level_t level, const char *fmt, va_list ap);

/* Convenience macros */
#define LOGE(...) hal_logf(HAL_LOG_ERROR, __VA_ARGS__)
#define LOGW(...) hal_logf(HAL_LOG_WARN,  __VA_ARGS__)
#define LOGI(...) hal_logf(HAL_LOG_INFO,  __VA_ARGS__)
#define LOGD(...) hal_logf(HAL_LOG_DEBUG, __VA_ARGS__)
#define LOGT(...) hal_logf(HAL_LOG_TRACE, __VA_ARGS__)

/* ---------------- Framed streaming ---------------- */

/**
 * Write a framed packet.
 * Returns false if not connected or write failed.
 */
bool hal_stream_write(hal_stream_type_t type, const void *payload, uint16_t len);

/**
 * Helper to stream one decoded event as a simple fixed binary record.
 * Record format (little-endian) chosen for easy host parsing:
 *   uint16_t row;
 *   uint16_t col;
 *   uint32_t t_us;   // timestamp in microseconds (monotonic since boot)
 *   uint8_t  flags;  // user-defined (e.g., polarity=1, row/col type, etc.)
 *   uint8_t  rsvd[3];
 * Total: 12 bytes.
 */
bool hal_stream_write_event_u16(uint16_t row, uint16_t col, uint32_t t_us, uint8_t flags);

/** Write an arbitrary binary blob as EVENT_BIN (caller defines record structure). */
static inline bool hal_stream_write_events_blob(const void *data, uint16_t len) {
    return hal_stream_write(HAL_STREAM_EVENT_BIN, data, len);
}

/** Optional marker (small text) for host-side debugging. */
bool hal_stream_marker(const char *text);

#ifdef __cplusplus
} // extern "C"
#endif
