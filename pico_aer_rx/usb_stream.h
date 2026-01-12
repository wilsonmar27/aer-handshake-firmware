// usb_stream.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * USB event/log stream wrapper.
 *
 * Uses hal_stdio's framed transport (AERS + type + len) and defines a *versioned*
 * payload format for decoded events and stream metadata.
 *
 * Design goals:
 *  - ON events only (row/col), timestamp taken at *event emission* time.
 *  - Timestamps enabled now, but easy to disable later without breaking host parsing.
 *  - Send a HELLO descriptor so the host learns the active event record type.
 */

/* --- Stream payload versions / record types (inside HAL_STREAM_EVENT_BIN) --- */
typedef enum usb_stream_event_rec_type_e {
    USB_EVT_REC_V1_NOTS = 1,  // row/col + flags (no timestamp)
    USB_EVT_REC_V1_TS   = 2,  // row/col + flags + t_us
} usb_stream_event_rec_type_t;

/* --- Flags inside event payload (yours to extend) --- */
enum {
    USB_EVT_FLAG_ON = 0x01u,   // pixel ON event (as requested)
};

/* --- Optional stats for diagnostics --- */
typedef struct usb_stream_stats_s {
    uint32_t events_sent;
    uint32_t events_dropped_not_connected;
    uint32_t hello_sent;
} usb_stream_stats_t;

typedef struct usb_stream_cfg_s {
    bool     timestamps_enabled;   // if true, emit USB_EVT_REC_V1_TS records
    bool     send_hello_on_init;   // if true, emits hello marker/descriptor from init
    uint8_t  data_width_bits;      // for hello (12 in your case)
} usb_stream_cfg_t;

/** Initialize the stream wrapper (does not init USB itself; call hal_stdio_init() first). */
void usb_stream_init(const usb_stream_cfg_t *cfg);

/** Enable/disable timestamps going forward. Optionally emits a new hello descriptor. */
void usb_stream_set_timestamps_enabled(bool enabled, bool send_hello);

/** Get the active record type used for events. */
usb_stream_event_rec_type_t usb_stream_event_record_type(void);

/** Send a HELLO descriptor packet (binary) + a short human marker (text). */
bool usb_stream_send_hello(void);

/**
 * Send one ON event (row,col). Flags will include USB_EVT_FLAG_ON.
 * Timestamp is included only if timestamps are enabled.
 */
bool usb_stream_send_on_event(uint16_t row, uint16_t col);

/**
 * If we ever want to send custom flags in the future, use this.
 * (Still treated as an "event" record.)
 */
bool usb_stream_send_event(uint16_t row, uint16_t col, uint8_t flags);

/** Get internal counters. */
const usb_stream_stats_t *usb_stream_stats(void);

#ifdef __cplusplus
} // extern "C"
#endif
