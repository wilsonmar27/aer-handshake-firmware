// main.c
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "tusb.h"  // TinyUSB CDC DTR

#include "hal/hal_gpio.h"
#include "hal/hal_time.h"
#include "hal/hal_stdio.h"

#include "aer_rx_poll.h"
#include "usb_stream.h"
#include "aer_event_sink.h"

#include "ringbuf.h"
#include "aer_codec.h"
#include "aer_burst.h"

// ---------------- Pin map ----------------
#define AER_DATA_BASE_GPIO   2u   // GP2..GP13 inclusive
#define AER_DATA_WIDTH_BITS  12u
#define AER_ACK_GPIO         14u  // ACK active-high
#define AER_RESET_GPIO       15u  // RESET active-high (held low unless commanded)

// ---------------- Ring buffer sizing ----------------
// NOTE: ringbuf stores up to (capacity - 1) elements.
#define RAW_RB_CAPACITY      2048u

static inline bool cdc_dtr_asserted(void)
{
    // "Connected" is not enough; you want terminal opened (DTR asserted).
    if (!tud_cdc_connected()) return false;
    return (tud_cdc_get_line_state() & 0x01u) != 0u; // bit0 = DTR
}

static void wait_for_usb_dtr_with_led(void)
{
#if defined(PICO_DEFAULT_LED_PIN)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);

    absolute_time_t next_toggle = make_timeout_time_ms(200);
    bool led_on = false;
#endif

    while (!cdc_dtr_asserted()) {
        tud_task();   // keep USB stack serviced
        sleep_ms(10);

#if defined(PICO_DEFAULT_LED_PIN)
        if (absolute_time_diff_us(get_absolute_time(), next_toggle) <= 0) {
            led_on = !led_on;
            gpio_put(PICO_DEFAULT_LED_PIN, led_on ? 1 : 0);
            next_toggle = make_timeout_time_ms(200);
        }
#endif
    }

#if defined(PICO_DEFAULT_LED_PIN)
    gpio_put(PICO_DEFAULT_LED_PIN, 1); // solid ON when terminal opened
#endif
}

int main(void)
{
    // Bring up USB stdio. We will gate acquisition on CDC DTR ourselves.
    hal_stdio_init(false, 0);
    hal_stdio_set_packetized(true); // safe for binary event stream

    // Wait until host opens CDC port (DTR asserted), blink LED while waiting.
    wait_for_usb_dtr_with_led();

    // Time / cycle counter (usb_stream uses hal_cycles_now() internally for ticks).
    hal_time_init();

    // RESET pin: active-high, keep deasserted (low) unless commanded.
    gpio_init(AER_RESET_GPIO);
    gpio_set_dir(AER_RESET_GPIO, GPIO_OUT);
    gpio_put(AER_RESET_GPIO, 0);

    // GPIO HAL init: DATA inputs with pulldowns, ACK output active-high.
    const hal_gpio_cfg_t gpio_cfg = {
        .data_base            = (uint8_t)AER_DATA_BASE_GPIO,
        .data_width           = (uint8_t)AER_DATA_WIDTH_BITS,
        .ack_pin              = (uint8_t)AER_ACK_GPIO,
        .ack_active_high      = true,
        .data_pull_down       = true,
        .data_pull_up         = false,
        .ack_deasserted_level = false, // active-high => deassert = low
    };
    hal_gpio_init(&gpio_cfg);

    // USB stream wrapper: ON events only, timestamps enabled (cycle ticks record).
    usb_stream_init(&(usb_stream_cfg_t){
        .timestamps_enabled = true,  // => USB_EVT_REC_V1_TICKS
        .data_width_bits    = (uint8_t)AER_DATA_WIDTH_BITS,
    });

    // Event sink (common parser callback -> usb_stream)
    aer_event_sink_t sink;
    aer_event_sink_init(&sink, &(aer_event_sink_cfg_t){ .enabled = true });

    // Ring buffer owned by main
    static uint32_t raw_storage[RAW_RB_CAPACITY];
    ringbuf_u32_t raw_rb;
    (void)ringbuf_u32_init(&raw_rb, raw_storage, RAW_RB_CAPACITY);
    ringbuf_u32_reset(&raw_rb);

    // Polling RX:
    // - wait_valid_timeout_us = 0 => idle is not an error (wait forever)
    // - wait_neutral_timeout_us = 0 => disabled (debug-only)
    aer_rx_poll_t rx;
    aer_rx_poll_init(&rx, &raw_rb, 0u, 0u);

    // Burst assembler (portable)
    aer_burst_t burst;
    aer_burst_init(&burst);

    while (true) {
        tud_task(); // keep USB alive even under load

        // Avoid blocking forever inside aer_rx_poll_step() during idle
        // (so we can keep servicing USB). Only handshake when DATA is nonzero.
        if (hal_gpio_read_data_raw() == 0u) {
            tight_loop_contents();
            continue;
        }

        // Complete exactly one handshake (rx MUST be drop-and-continue, not backpressure)
        (void)aer_rx_poll_step(&rx);

        // Drain raw words -> decode -> burst parser -> event sink
        uint32_t raw_u32 = 0;
        while (ringbuf_u32_pop(&raw_rb, &raw_u32)) {
            const aer_codec_result_t dec = aer_decode_word((aer_raw_word_t)raw_u32);
            (void)aer_burst_feed(&burst, dec, aer_event_sink_on_event, &sink);
        }
    }
}
