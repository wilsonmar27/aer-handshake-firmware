// aer_rx_poll.c
#include "aer_rx_poll.h"

#include "hal_gpio.h"
#include "hal_time.h"

#include "pico.h" // tight_loop_contents()

static inline bool data_is_valid(uint32_t raw)  { return raw != 0u; }
static inline bool data_is_neutral(uint32_t raw){ return raw == 0u; }

void aer_rx_poll_init(aer_rx_poll_t *rx,
                      ringbuf_u32_t *rb,
                      uint32_t wait_valid_timeout_us,
                      uint32_t wait_neutral_timeout_us)
{
    if (!rx || !rb) {
        while (1) { tight_loop_contents(); }
    }

    rx->rb = rb;
    rx->wait_valid_timeout_us   = wait_valid_timeout_us;
    rx->wait_neutral_timeout_us = wait_neutral_timeout_us;
    rx->stats = (aer_rx_poll_stats_t){0};

    // Safe start state.
    hal_gpio_ack_deassert();
}

void aer_rx_poll_reset(aer_rx_poll_t *rx)
{
    if (!rx) return;
    rx->stats = (aer_rx_poll_stats_t){0};
    hal_gpio_ack_deassert();
}

/**
 * One handshake.
 *
 * Important ordering choice:
 * - We ACK immediately after latching the word (per DI timing).
 * - We push the word to the ring buffer immediately after ACK (still fast),
 *   so even if waiting for neutral times out, you still captured something.
 */
aer_rx_poll_status_t aer_rx_poll_step(aer_rx_poll_t *rx)
{
    if (!rx || !rx->rb) {
        while (1) { tight_loop_contents(); }
    }

    // Backpressure: if no space, do not ACK; transmitter will hold DATA valid.
    if (ringbuf_u32_is_full(rx->rb)) {
        rx->stats.no_space++;
        return AER_RX_POLL_NO_SPACE;
    }

    // Ensure ACK is low before starting a new receive.
    hal_gpio_ack_deassert();

    // 1) Wait for DATA != 0
    uint64_t deadline = hal_time_deadline_us(rx->wait_valid_timeout_us);
    uint32_t raw = 0;

    for (;;) {
        raw = hal_gpio_read_data_raw();
        if (data_is_valid(raw)) break;
        if (hal_time_expired(deadline)) {
            rx->stats.timeouts_valid++;
            return AER_RX_POLL_TIMEOUT_WAIT_VALID;
        }
        tight_loop_contents();
    }

    const aer_raw_word_t word = (aer_raw_word_t)raw;

    // 2) Assert ACK as soon as we've latched the raw word.
    hal_gpio_ack_assert();

    // 3) Push into ring buffer (should succeed; we checked fullness above).
    //    If it fails anyway, we still keep protocol moving.
    (void)ringbuf_u32_push(rx->rb, (uint32_t)word);

    // 4) Wait for DATA == 0 (neutral)
    deadline = hal_time_deadline_us(rx->wait_neutral_timeout_us);
    for (;;) {
        raw = hal_gpio_read_data_raw();
        if (data_is_neutral(raw)) break;
        if (hal_time_expired(deadline)) {
            // Try to recover: drop ACK so link can return to idle.
            hal_gpio_ack_deassert();
            rx->stats.timeouts_neutral++;
            return AER_RX_POLL_TIMEOUT_WAIT_NEUTRAL;
        }
        tight_loop_contents();
    }

    // 5) Deassert ACK
    hal_gpio_ack_deassert();

    rx->stats.words_ok++;
    return AER_RX_POLL_OK;
}

uint32_t aer_rx_poll_service(aer_rx_poll_t *rx, uint32_t max_words, uint32_t time_budget_us)
{
    if (!rx) return 0;

    uint32_t ok = 0;
    const uint64_t budget_deadline = (time_budget_us == 0u)
        ? 0u
        : hal_time_deadline_us(time_budget_us);

    for (uint32_t i = 0; i < max_words; ++i) {
        if (time_budget_us != 0u && hal_time_expired(budget_deadline)) {
            break;
        }

        const aer_rx_poll_status_t st = aer_rx_poll_step(rx);
        if (st == AER_RX_POLL_OK) {
            ok++;
            continue;
        }

        // If we're out of space, don't spin here; let consumer drain.
        if (st == AER_RX_POLL_NO_SPACE) {
            break;
        }

        // On timeouts, return control to main so you can log / attempt recovery.
        break;
    }

    return ok;
}
