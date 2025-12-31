#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "../common/include/aer_cfg.h"
#include "../common/include/aer_types.h"
#include "../common/include/aer_codec.h"

/* ---------------- tiny test helpers ---------------- */

static int g_failures = 0;

#define TASSERT(cond) do { \
    if (!(cond)) { \
        ++g_failures; \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define TASSERT_EQ_U32(a,b) do { \
    uint32_t _a = (uint32_t)(a); \
    uint32_t _b = (uint32_t)(b); \
    if (_a != _b) { \
        ++g_failures; \
        fprintf(stderr, "[FAIL] %s:%d: %s (%u) != %s (%u)\n", __FILE__, __LINE__, #a, _a, #b, _b); \
    } \
} while (0)

#define TASSERT_EQ_BOOL(a,b) do { \
    bool _a = (bool)(a); \
    bool _b = (bool)(b); \
    if (_a != _b) { \
        ++g_failures; \
        fprintf(stderr, "[FAIL] %s:%d: %s (%d) != %s (%d)\n", __FILE__, __LINE__, #a, (int)_a, #b, (int)_b); \
    } \
} while (0)

/* ---------------- vector file runner ----------------
 * Format (one per line, comments allowed with '#'):
 *   <name> <raw_hex> <expect_ok 0|1> <expect_payload_dec> <expect_tail 0|1> <expect_err_mask_hex>
 */
static void run_codec_vectors(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f) {
        ++g_failures;
        fprintf(stderr, "[FAIL] could not open vector file: %s\n", path);
        return;
    }

    char line[256];
    unsigned lineno = 0;

    while (fgets(line, sizeof(line), f)) {
        ++lineno;
        /* strip leading spaces */
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n' || *p == '#') continue;

        char name[64];
        unsigned raw_hex = 0;
        unsigned expect_ok = 0;
        unsigned expect_payload = 0;
        unsigned expect_tail = 0;
        unsigned expect_err = 0;

        int n = sscanf(p, "%63s %x %u %u %u %x",
                       name, &raw_hex, &expect_ok, &expect_payload, &expect_tail, &expect_err);
        if (n != 6) {
            ++g_failures;
            fprintf(stderr, "[FAIL] %s:%u: could not parse line: %s", path, lineno, p);
            continue;
        }

        const aer_codec_result_t r = aer_decode_word((aer_raw_word_t)raw_hex);

        if (r.ok != (expect_ok != 0)) {
            ++g_failures;
            fprintf(stderr, "[FAIL] %s:%u (%s): ok=%d expected=%u\n", path, lineno, name, (int)r.ok, expect_ok);
        }
        if (r.payload != (uint8_t)expect_payload) {
            ++g_failures;
            fprintf(stderr, "[FAIL] %s:%u (%s): payload=%u expected=%u\n", path, lineno, name, r.payload, expect_payload);
        }
        if (r.is_tail != (expect_tail != 0)) {
            ++g_failures;
            fprintf(stderr, "[FAIL] %s:%u (%s): is_tail=%d expected=%u\n", path, lineno, name, (int)r.is_tail, expect_tail);
        }
        /* Require at least the expected flags (vector mask is a subset). */
        if ((r.err_flags & (uint32_t)expect_err) != (uint32_t)expect_err) {
            ++g_failures;
            fprintf(stderr, "[FAIL] %s:%u (%s): err_flags=0x%08x missing expected mask=0x%08x\n",
                    path, lineno, name, (unsigned)r.err_flags, expect_err);
        }
    }

    fclose(f);
}

static void test_codec_core_cases(void)
{
    /* Neutral must not be treated as data. */
    {
        aer_codec_result_t r = aer_decode_word((aer_raw_word_t)0u);
        TASSERT_EQ_BOOL(r.ok, false);
        TASSERT((r.err_flags & AER_CODEC_ERR_NEUTRAL) != 0u);
        TASSERT_EQ_BOOL(r.is_tail, false);
    }

    /* Round-trip several payloads. */
    {
        const uint8_t payloads[] = {0u, 1u, 5u, 31u};
        for (size_t i = 0; i < sizeof(payloads)/sizeof(payloads[0]); ++i) {
            aer_raw_word_t raw = 0u;
            uint32_t enc_err = 0u;
            bool ok_enc = aer_encode_payload(payloads[i], &raw, &enc_err);
            TASSERT_EQ_BOOL(ok_enc, true);
            TASSERT_EQ_U32(enc_err, 0u);

            aer_codec_result_t r = aer_decode_word(raw);
            TASSERT_EQ_BOOL(r.ok, true);
            TASSERT_EQ_U32(r.payload, payloads[i]);
            TASSERT_EQ_BOOL(r.is_tail, false);
        }
    }

    /* Tail must be recognized. */
    {
        aer_raw_word_t raw = 0u;
        uint32_t enc_err = 0u;
        bool ok_enc = aer_encode_payload((uint8_t)AER_TAIL_PAYLOAD, &raw, &enc_err);
        TASSERT_EQ_BOOL(ok_enc, true);

        aer_codec_result_t r = aer_decode_word(raw);
        TASSERT_EQ_BOOL(r.ok, true);
        TASSERT_EQ_BOOL(r.is_tail, true);
        TASSERT_EQ_U32(r.payload, (uint32_t)AER_TAIL_PAYLOAD);
    }

    /* Invalid: multi-hot in a group. */
    {
        /* group0=0b0011 (two-hot), group1=0b0001, group2=0b0001 */
        aer_raw_word_t raw = (aer_raw_word_t)(0x3u | 0x10u | 0x100u);
        aer_codec_result_t r = aer_decode_word(raw);
        TASSERT_EQ_BOOL(r.ok, false);
        TASSERT((r.err_flags & AER_CODEC_ERR_MULTI_HOT) != 0u);
    }

    /* Invalid: zero-hot in a group (word is non-neutral overall). */
    {
        /* group0=0b0001, group1=0b0000, group2=0b0001 */
        aer_raw_word_t raw = (aer_raw_word_t)(0x1u | 0x000u | 0x100u);
        aer_codec_result_t r = aer_decode_word(raw);
        TASSERT_EQ_BOOL(r.ok, false);
        TASSERT((r.err_flags & AER_CODEC_ERR_ZERO_HOT) != 0u);
    }

    /* Out-of-range bits should set flag but still decode based on masked value. */
    {
        aer_raw_word_t raw = 0u;
        uint32_t enc_err = 0u;
        (void)aer_encode_payload(5u, &raw, &enc_err);
        raw |= (aer_raw_word_t)(1u << 20u); /* outside 12-bit DATA */
        aer_codec_result_t r = aer_decode_word(raw);
        TASSERT_EQ_BOOL(r.ok, true);
        TASSERT_EQ_U32(r.payload, 5u);
        TASSERT((r.err_flags & AER_CODEC_ERR_OUT_OF_RANGE) != 0u);
    }

    /* Pad bit warning: payload with pad bits set but not tail. */
    {
        const uint8_t p = (uint8_t)(1u << AER_INDEX_BITS); /* e.g., 32 when index bits=5 */
        if (p != (uint8_t)AER_TAIL_PAYLOAD) {
            aer_raw_word_t raw = 0u;
            uint32_t enc_err = 0u;
            bool ok_enc = aer_encode_payload(p, &raw, &enc_err);
            TASSERT_EQ_BOOL(ok_enc, true);

            aer_codec_result_t r = aer_decode_word(raw);
            TASSERT_EQ_BOOL(r.ok, true);
            TASSERT_EQ_U32(r.payload, p);
            TASSERT_EQ_BOOL(r.is_tail, false);
            TASSERT((r.err_flags & AER_CODEC_WARN_PAD_BIT_SET) != 0u);
        }
    }
}

int main(void)
{
    test_codec_core_cases();

    /* Golden vector files. (Run from repo root so paths resolve.) */
    run_codec_vectors("tests/vectors/codec_valid.txt");
    run_codec_vectors("tests/vectors/codec_invalid.txt");

    if (g_failures == 0) {
        printf("[PASS] test_codec (%u groups, %u data bits)\n",
               (unsigned)AER_NUM_GROUPS, (unsigned)AER_DATA_WIDTH);
        return 0;
    }

    fprintf(stderr, "[FAIL] test_codec: %d failures\n", g_failures);
    return 1;
}
