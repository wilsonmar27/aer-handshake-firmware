// pico/hal/hal_stdio.c
#include "hal_stdio.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/stdio_usb.h"
#include "hardware/sync.h"

/* -------- Config / state -------- */

static volatile hal_log_level_t g_log_level = HAL_LOG_INFO;
/* Default ON so logs don't corrupt binary streams. */
static volatile bool g_packetized = true;

/* Packet framing:
 *   magic[4] = 'A' 'E' 'R' 'S'
 *   ver      = 1
 *   type     = hal_stream_type_t
 *   len_le   = uint16 payload length
 *   payload  = len bytes
 *
 * No CRC (USB CDC is reliable enough; host can resync using magic).
 */
#define HAL_STREAM_MAGIC_0 'A'
#define HAL_STREAM_MAGIC_1 'E'
#define HAL_STREAM_MAGIC_2 'R'
#define HAL_STREAM_MAGIC_3 'S'
#define HAL_STREAM_VER     1u

typedef struct __attribute__((packed)) hal_stream_hdr_s {
    uint8_t  magic[4];
    uint8_t  ver;
    uint8_t  type;
    uint16_t len_le;
} hal_stream_hdr_t;

static inline void lock_irq(uint32_t *saved) { *saved = save_and_disable_interrupts(); }
static inline void unlock_irq(uint32_t saved) { restore_interrupts(saved); }

void hal_stdio_init(bool wait_for_usb, uint32_t timeout_ms) {
    /* USB-only: do NOT call stdio_init_all() (it may bring up UART stdio). */
    stdio_usb_init();

    if (wait_for_usb) {
        (void)hal_stdio_wait_connected(timeout_ms);
    }
}

bool hal_stdio_is_connected(void) {
    /* Best-effort: returns true once host opens the CDC port. */
    return stdio_usb_connected();
}

bool hal_stdio_wait_connected(uint32_t timeout_ms) {
    const absolute_time_t until = make_timeout_time_ms(timeout_ms);
    while (!hal_stdio_is_connected()) {
        if (timeout_ms != 0u && absolute_time_diff_us(get_absolute_time(), until) <= 0) {
            return false;
        }
        tight_loop_contents();
    }
    return true;
}

void hal_log_set_level(hal_log_level_t level) { g_log_level = level; }
hal_log_level_t hal_log_get_level(void) { return g_log_level; }

void hal_stdio_set_packetized(bool enabled) { g_packetized = enabled; }
bool hal_stdio_get_packetized(void) { return g_packetized; }

void hal_stdio_flush(void) {
    stdio_flush();
}

/* -------- Framed streaming -------- */

static bool write_bytes_locked(const void *buf, size_t len) {
    /* fwrite to stdout is routed to USB CDC when pico_enable_stdio_usb is on. */
    if (!hal_stdio_is_connected()) return false;
    size_t n = fwrite(buf, 1, len, stdout);
    return (n == len);
}

bool hal_stream_write(hal_stream_type_t type, const void *payload, uint16_t len) {
    if (!hal_stdio_is_connected()) return false;

    hal_stream_hdr_t hdr;
    hdr.magic[0] = (uint8_t)HAL_STREAM_MAGIC_0;
    hdr.magic[1] = (uint8_t)HAL_STREAM_MAGIC_1;
    hdr.magic[2] = (uint8_t)HAL_STREAM_MAGIC_2;
    hdr.magic[3] = (uint8_t)HAL_STREAM_MAGIC_3;
    hdr.ver      = (uint8_t)HAL_STREAM_VER;
    hdr.type     = (uint8_t)type;
    hdr.len_le   = (uint16_t)len;

    /* Prevent interleaving headers/payloads between IRQ contexts. */
    uint32_t irq;
    lock_irq(&irq);

    bool ok = write_bytes_locked(&hdr, sizeof(hdr));
    if (ok && len && payload) {
        ok = write_bytes_locked(payload, len);
    }

    unlock_irq(irq);
    return ok;
}

bool hal_stream_write_event_u16(uint16_t row, uint16_t col, uint32_t t_us, uint8_t flags) {
    struct __attribute__((packed)) evt_s {
        uint16_t row;
        uint16_t col;
        uint32_t t_us;
        uint8_t  flags;
        uint8_t  rsvd[3];
    } e;

    e.row = row;
    e.col = col;
    e.t_us = t_us;
    e.flags = flags;
    e.rsvd[0] = 0;
    e.rsvd[1] = 0;
    e.rsvd[2] = 0;

    return hal_stream_write(HAL_STREAM_EVENT_BIN, &e, (uint16_t)sizeof(e));
}

bool hal_stream_marker(const char *text) {
    if (!text) return false;
    const size_t n = strnlen(text, 240);
    return hal_stream_write(HAL_STREAM_MARKER, text, (uint16_t)n);
}

/* -------- Logging -------- */

static const char *lvl_tag(hal_log_level_t lvl) {
    switch (lvl) {
        case HAL_LOG_ERROR: return "E";
        case HAL_LOG_WARN:  return "W";
        case HAL_LOG_INFO:  return "I";
        case HAL_LOG_DEBUG: return "D";
        case HAL_LOG_TRACE: return "T";
        default:            return "?";
    }
}

void hal_vlogf(hal_log_level_t level, const char *fmt, va_list ap) {
    if (level > g_log_level) return;

    /* Format into a small stack buffer (keeps packet payload single-shot). */
    char msg[256];
    int n = vsnprintf(msg, sizeof(msg), fmt, ap);
    if (n < 0) return;

    /* Clamp */
    if ((size_t)n >= sizeof(msg)) {
        msg[sizeof(msg) - 2] = '.';
        msg[sizeof(msg) - 1] = '\0';
        n = (int)strlen(msg);
    }

    const uint32_t t_ms = (uint32_t)(time_us_64() / 1000ULL);

    if (g_packetized) {
        /* Packetized log line: "[1234][I] message" */
        char line[320];
        int m = snprintf(line, sizeof(line), "[%lu][%s] %s",
                         (unsigned long)t_ms, lvl_tag(level), msg);
        if (m < 0) return;
        if ((size_t)m >= sizeof(line)) {
            line[sizeof(line) - 1] = '\0';
            m = (int)strlen(line);
        }
        (void)hal_stream_write(HAL_STREAM_LOG_TEXT, line, (uint16_t)m);
    } else {
        /* Plain printf (human-friendly). WARNING: don't use with binary streaming. */
        uint32_t irq;
        lock_irq(&irq);
        if (hal_stdio_is_connected()) {
            printf("[%lu][%s] %s\n", (unsigned long)t_ms, lvl_tag(level), msg);
            stdio_flush();
        }
        unlock_irq(irq);
    }
}

void hal_logf(hal_log_level_t level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    hal_vlogf(level, fmt, ap);
    va_end(ap);
}
