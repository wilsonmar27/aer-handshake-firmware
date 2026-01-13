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
    // Ensure ACK is low before starting a new receive.
    hal_gpio_ack_deassert();

    // 1) wait DATA != 0 (forever if timeout==0)
    uint64_t deadline = 0;
    if (rx->wait_valid_timeout_us != 0u) {
        deadline = hal_time_deadline_us(rx->wait_valid_timeout_us);
    }

    uint32_t raw = 0;
    for (;;) {
        raw = hal_gpio_read_data_raw();
        if (raw != 0u) break;

        if (rx->wait_valid_timeout_us != 0u && hal_time_expired(deadline)) {
            rx->stats.timeouts_valid++;
            return AER_RX_POLL_TIMEOUT_WAIT_VALID;
        }
        tight_loop_contents();
    }

    const aer_raw_word_t word = (aer_raw_word_t)raw;

    // 2) assert ACK immediately after latch
    hal_gpio_ack_assert();

    // 3) push OR drop (but always continue handshake)
    if (ringbuf_u32_is_full(rx->rb)) {
        rx->stats.dropped_full++;
    } else {
        (void)ringbuf_u32_push(rx->rb, (uint32_t)word);
    }

    // 4) wait for neutral (optional timeout; disabled if 0)
    if (rx->wait_neutral_timeout_us != 0u) {
        deadline = hal_time_deadline_us(rx->wait_neutral_timeout_us);
    }

    for (;;) {
        raw = hal_gpio_read_data_raw();
        if (raw == 0u) break;

        if (rx->wait_neutral_timeout_us != 0u && hal_time_expired(deadline)) {
            rx->stats.timeouts_neutral++;
            // recovery: drop ACK and return
            hal_gpio_ack_deassert();
            return AER_RX_POLL_TIMEOUT_WAIT_NEUTRAL;
        }
        tight_loop_contents();
    }

    // 5) deassert ACK
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

        // On timeouts, return control to main so you can log / attempt recovery.
        break;
    }

    return ok;
}
