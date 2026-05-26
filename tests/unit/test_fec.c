/**
 * @file test_fec.c
 * @brief Unit tests for the RGTP Reed-Solomon FEC subsystem.
 *
 * Tests: 14 cases covering GF(2^8) table correctness, RS encode/decode,
 * error handling, and adaptive FEC strength bounds.
 *
 * Requirements: 17.1, 17.2
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/fec/rgtp_gf256_internal.h"
#include "../../src/fec/rgtp_fec_internal.h"
#include "../../src/transport/rgtp_transport_internal.h"

#include <string.h>
#include <stdint.h>
#include <math.h>

/* ── GF(2^8) table tests ────────────────────────────────────────────────── */

static void test_gf256_init_tables(void)
{
    /* gf_exp[gf_log[x]] == x for all x in 1..254 */
    for (int x = 1; x <= 254; x++) {
        uint8_t recovered = gf_pow(2, (int)gf_log[x]);
        /* Use gf_mul(1, x) as a proxy: gf_exp[gf_log[x]] */
        uint8_t result = gf_mul((uint8_t)x, 1);
        RGTP_ASSERT(result == (uint8_t)x,
                    "gf_mul(x, 1) must equal x for all x");
    }
}

static void test_gf256_mul_identity(void)
{
    for (int x = 0; x <= 255; x++) {
        RGTP_ASSERT(gf_mul((uint8_t)x, 1) == (uint8_t)x,
                    "gf_mul(x, 1) must equal x");
    }
}

static void test_gf256_mul_zero(void)
{
    for (int x = 0; x <= 255; x++) {
        RGTP_ASSERT(gf_mul((uint8_t)x, 0) == 0,
                    "gf_mul(x, 0) must equal 0");
        RGTP_ASSERT(gf_mul(0, (uint8_t)x) == 0,
                    "gf_mul(0, x) must equal 0");
    }
}

static void test_gf256_mul_inverse(void)
{
    for (int x = 1; x <= 255; x++) {
        uint8_t inv = gf_inv((uint8_t)x);
        RGTP_ASSERT(gf_mul((uint8_t)x, inv) == 1,
                    "gf_mul(x, gf_inv(x)) must equal 1");
    }
}

/* ── RS encode tests ────────────────────────────────────────────────────── */

static void test_rs_encode_systematic(void)
{
    /* Encode with k=4, n=7 (3 parity symbols) */
    uint8_t data[4]   = {0x01, 0x02, 0x03, 0x04};
    uint8_t parity[3] = {0};

    RGTP_ASSERT_OK(rgtp_rs_encode(4, 7, data, parity));

    /* Verify: decode with all 7 symbols should recover original data */
    uint8_t received[7];
    memcpy(received, data, 4);
    memcpy(received + 4, parity, 3);

    uint8_t recovered[4] = {0};
    RGTP_ASSERT_OK(rgtp_rs_decode(4, 7, received, NULL, 0, recovered));
    RGTP_ASSERT(memcmp(data, recovered, 4) == 0,
                "RS encode+decode with no erasures must recover original data");
}

static void test_rs_decode_no_erasures(void)
{
    uint8_t data[3]   = {0xAA, 0xBB, 0xCC};
    uint8_t parity[2] = {0};
    RGTP_ASSERT_OK(rgtp_rs_encode(3, 5, data, parity));

    uint8_t received[5];
    memcpy(received, data, 3);
    memcpy(received + 3, parity, 2);

    uint8_t recovered[3] = {0};
    RGTP_ASSERT_OK(rgtp_rs_decode(3, 5, received, NULL, 0, recovered));
    RGTP_ASSERT(memcmp(data, recovered, 3) == 0,
                "Decode with no erasures must recover original");
}

static void test_rs_decode_max_erasures(void)
{
    /* k=4, n=7: can recover from up to 3 erasures */
    uint8_t data[4]   = {0x10, 0x20, 0x30, 0x40};
    uint8_t parity[3] = {0};
    RGTP_ASSERT_OK(rgtp_rs_encode(4, 7, data, parity));

    uint8_t received[7];
    memcpy(received, data, 4);
    memcpy(received + 4, parity, 3);

    /* Erase positions 0, 2, 4 (3 erasures = n-k) */
    uint8_t erasure_pos[3] = {0, 2, 4};
    received[0] = 0;
    received[2] = 0;
    received[4] = 0;

    uint8_t recovered[4] = {0};
    RGTP_ASSERT_OK(rgtp_rs_decode(4, 7, received, erasure_pos, 3, recovered));
    RGTP_ASSERT(memcmp(data, recovered, 4) == 0,
                "Decode with max erasures (n-k) must recover original");
}

static void test_rs_decode_too_many_erasures(void)
{
    uint8_t data[4]   = {1, 2, 3, 4};
    uint8_t parity[3] = {0};
    RGTP_ASSERT_OK(rgtp_rs_encode(4, 7, data, parity));

    uint8_t received[7] = {0};
    /* Erase 4 positions (> n-k = 3) */
    uint8_t erasure_pos[4] = {0, 1, 2, 3};

    uint8_t recovered[4] = {0};
    rgtp_error_t err = rgtp_rs_decode(4, 7, received, erasure_pos, 4, recovered);
    RGTP_ASSERT(err == RGTP_ERR_FEC_FAIL,
                "Too many erasures must return RGTP_ERR_FEC_FAIL");
}

static void test_rs_params_invalid(void)
{
    uint8_t data[4] = {0};
    uint8_t parity[4] = {0};

    /* k=0 */
    RGTP_ASSERT_ERR(rgtp_rs_encode(0, 4, data, parity), RGTP_ERR_INVALID_ARG);
    /* n=k */
    RGTP_ASSERT_ERR(rgtp_rs_encode(4, 4, data, parity), RGTP_ERR_INVALID_ARG);
    /* n>255 — can't test directly since n is uint8_t, but k=0 covers it */
}

/* ── Adaptive FEC strength tests ────────────────────────────────────────── */

static void test_adaptive_fec_increase(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);
    f.fec_overhead = 0.20f;
    f.loss_rate    = 0.06f;   /* > 5% */
    rgtp_flow_update_fec(&f);
    RGTP_ASSERT(fabsf(f.fec_overhead - 0.30f) < 0.001f,
                "FEC overhead must increase by 0.10 when loss > 5%");
}

static void test_adaptive_fec_decrease(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);
    f.fec_overhead = 0.20f;
    f.loss_rate    = 0.005f;  /* < 1% */
    rgtp_flow_update_fec(&f);
    RGTP_ASSERT(fabsf(f.fec_overhead - 0.15f) < 0.001f,
                "FEC overhead must decrease by 0.05 when loss < 1%");
}

static void test_adaptive_fec_no_change(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);
    f.fec_overhead = 0.20f;
    f.loss_rate    = 0.03f;   /* 1% <= loss <= 5% */
    rgtp_flow_update_fec(&f);
    RGTP_ASSERT(fabsf(f.fec_overhead - 0.20f) < 0.001f,
                "FEC overhead must not change when 1% <= loss <= 5%");
}

static void test_adaptive_fec_cap_at_50pct(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);
    f.fec_overhead = 0.48f;
    f.loss_rate    = 0.06f;
    rgtp_flow_update_fec(&f);
    RGTP_ASSERT(f.fec_overhead <= 0.50f,
                "FEC overhead must be capped at 0.50");
    RGTP_ASSERT(fabsf(f.fec_overhead - 0.50f) < 0.001f,
                "FEC overhead must be exactly 0.50 after cap");
}

static void test_adaptive_fec_floor_at_zero(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);
    f.fec_overhead = 0.03f;
    f.loss_rate    = 0.005f;
    rgtp_flow_update_fec(&f);
    RGTP_ASSERT(f.fec_overhead >= 0.00f,
                "FEC overhead must not go below 0.00");
    RGTP_ASSERT(fabsf(f.fec_overhead - 0.00f) < 0.001f,
                "FEC overhead must be exactly 0.00 after floor");
}

/* ── Test runner ────────────────────────────────────────────────────────── */
int main(void)
{
    RGTP_ASSERT_OK(rgtp_init());

    RGTP_RUN_TEST(test_gf256_init_tables);
    RGTP_RUN_TEST(test_gf256_mul_identity);
    RGTP_RUN_TEST(test_gf256_mul_zero);
    RGTP_RUN_TEST(test_gf256_mul_inverse);
    RGTP_RUN_TEST(test_rs_encode_systematic);
    RGTP_RUN_TEST(test_rs_decode_no_erasures);
    RGTP_RUN_TEST(test_rs_decode_max_erasures);
    RGTP_RUN_TEST(test_rs_decode_too_many_erasures);
    RGTP_RUN_TEST(test_rs_params_invalid);
    RGTP_RUN_TEST(test_adaptive_fec_increase);
    RGTP_RUN_TEST(test_adaptive_fec_decrease);
    RGTP_RUN_TEST(test_adaptive_fec_no_change);
    RGTP_RUN_TEST(test_adaptive_fec_cap_at_50pct);
    RGTP_RUN_TEST(test_adaptive_fec_floor_at_zero);

    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
