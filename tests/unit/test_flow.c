/**
 * @file test_flow.c
 * @brief Unit tests for AIMD flow control and RTT estimation.
 *
 * Tests: 6 cases covering EWMA formula, convergence, window reduction,
 * window increase, minimum window, and pull pressure tracking.
 *
 * Requirements: 17.1, 17.2
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/transport/rgtp_transport_internal.h"

#include <math.h>

static void test_ewma_rtt_formula(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);
    f.rtt_us           = 1000;
    f.rtt_baseline_us  = 1000;
    f.rtt_sample_count = 1;   /* skip first-sample init path */

    rgtp_flow_update_rtt(&f, 2000);
    /* new = 0.875 * 1000 + 0.125 * 2000 = 875 + 250 = 1125 */
    RGTP_ASSERT(f.rtt_us == 1125u,
                "EWMA RTT: 0.875*1000 + 0.125*2000 must equal 1125");
}

static void test_ewma_rtt_convergence(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);

    /* Feed 100 identical samples of 5000µs */
    for (int i = 0; i < 100; i++) {
        rgtp_flow_update_rtt(&f, 5000);
    }
    /* After convergence, RTT should be close to 5000 */
    RGTP_ASSERT(f.rtt_us >= 4900u && f.rtt_us <= 5100u,
                "EWMA RTT must converge to sample value after 100 identical samples");
}

static void test_window_reduce_on_high_rtt(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);
    f.window_size      = 64;
    f.rtt_us           = 1000;
    f.rtt_baseline_us  = 1000;
    f.rtt_sample_count = 10;

    /* RTT > 2x baseline triggers congestion */
    rgtp_flow_update_rtt(&f, 3000);   /* 3000 > 2*1000 */
    RGTP_ASSERT(f.window_size == 32u,
                "Window must halve when RTT > 2x baseline");
}

static void test_window_increase_on_low_rtt(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);
    f.window_size      = 32;
    f.rtt_us           = 1000;
    f.rtt_baseline_us  = 1000;
    f.rtt_sample_count = 10;

    /* 5 consecutive RTTs within 1.1x baseline trigger window increase */
    for (int i = 0; i < 5; i++) {
        rgtp_flow_update_rtt(&f, 1050);   /* 1050 <= 1.1 * 1000 */
    }
    RGTP_ASSERT(f.window_size == 33u,
                "Window must increase by 1 after 5 consecutive low-RTT periods");
}

static void test_window_minimum_one(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);
    f.window_size      = 1;
    f.rtt_us           = 1000;
    f.rtt_baseline_us  = 1000;
    f.rtt_sample_count = 10;

    /* Congestion on window=1 must not go below 1 */
    rgtp_flow_on_congestion(&f);
    RGTP_ASSERT(f.window_size >= 1u,
                "Window must never drop below 1");
}

static void test_pull_pressure_tracking(void)
{
    rgtp_flow_t f;
    rgtp_flow_init(&f, 64);

    uint64_t t = 0;
    /* Record 50 pulls in the first 100ms window */
    for (int i = 0; i < 50; i++) {
        rgtp_flow_record_pull(&f, t);
        t += 1000;   /* 1ms apart */
    }
    /* Advance past 100ms to trigger window slide */
    rgtp_flow_record_pull(&f, 200000);   /* 200ms */

    RGTP_ASSERT(f.pull_pressure == 50u,
                "Pull pressure must reflect count in last 100ms window");
}

int main(void)
{
    RGTP_ASSERT_OK(rgtp_init());
    RGTP_RUN_TEST(test_ewma_rtt_formula);
    RGTP_RUN_TEST(test_ewma_rtt_convergence);
    RGTP_RUN_TEST(test_window_reduce_on_high_rtt);
    RGTP_RUN_TEST(test_window_increase_on_low_rtt);
    RGTP_RUN_TEST(test_window_minimum_one);
    RGTP_RUN_TEST(test_pull_pressure_tracking);
    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
