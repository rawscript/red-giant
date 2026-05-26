/**
 * @file test_replay.c
 * @brief Unit tests for the anti-replay window.
 *
 * Tests: 6 cases covering accept, duplicate, below-base, window advance,
 * boundary, and just-outside-window behavior.
 *
 * Requirements: 17.1, 17.2
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/transport/rgtp_transport_internal.h"

static void test_replay_accept_new(void)
{
    rgtp_replay_window_t w;
    rgtp_replay_window_init(&w);
    RGTP_ASSERT(rgtp_replay_check_and_set(&w, 0) == RGTP_REPLAY_ACCEPT,
                "Fresh seq 0 must be accepted");
    RGTP_ASSERT(rgtp_replay_check_and_set(&w, 5) == RGTP_REPLAY_ACCEPT,
                "Fresh seq 5 must be accepted");
}

static void test_replay_reject_duplicate(void)
{
    rgtp_replay_window_t w;
    rgtp_replay_window_init(&w);
    rgtp_replay_check_and_set(&w, 10);
    RGTP_ASSERT(rgtp_replay_check_and_set(&w, 10) == RGTP_REPLAY_DUPLICATE,
                "Duplicate seq 10 must be rejected");
}

static void test_replay_reject_below_base(void)
{
    rgtp_replay_window_t w;
    rgtp_replay_window_init(&w);
    /* Advance window past 0 */
    for (uint32_t i = 0; i < 300; i++) {
        rgtp_replay_check_and_set(&w, i);
    }
    RGTP_ASSERT(rgtp_replay_check_and_set(&w, 0) == RGTP_REPLAY_TOO_OLD,
                "Seq below window base must be rejected as TOO_OLD");
}

static void test_replay_window_advance(void)
{
    rgtp_replay_window_t w;
    rgtp_replay_window_init(&w);
    /* Accept seq 0..254 */
    for (uint32_t i = 0; i < 255; i++) {
        rgtp_replay_check_and_set(&w, i);
    }
    /* Seq 256 is 256 ahead of base=0, should advance window */
    RGTP_ASSERT(rgtp_replay_check_and_set(&w, 256) == RGTP_REPLAY_ACCEPT,
                "Seq 256 ahead of base must advance window and be accepted");
    /* After advance, base should have moved; seq 0 is now too old */
    RGTP_ASSERT(rgtp_replay_check_and_set(&w, 0) == RGTP_REPLAY_TOO_OLD,
                "After window advance, old seq must be TOO_OLD");
}

static void test_replay_window_boundary(void)
{
    rgtp_replay_window_t w;
    rgtp_replay_window_init(&w);
    /* Seq 255 is the last slot in the initial window (base=0, size=256) */
    RGTP_ASSERT(rgtp_replay_check_and_set(&w, 255) == RGTP_REPLAY_ACCEPT,
                "Seq at window boundary (base+255) must be accepted");
}

static void test_replay_window_just_outside(void)
{
    rgtp_replay_window_t w;
    rgtp_replay_window_init(&w);
    /* Seq 256 is just outside the initial window — should advance */
    RGTP_ASSERT(rgtp_replay_check_and_set(&w, 256) == RGTP_REPLAY_ACCEPT,
                "Seq just outside window (base+256) must advance and be accepted");
    RGTP_ASSERT(w.base == 1, "Window base must advance to 1 after accepting seq 256");
}

int main(void)
{
    RGTP_ASSERT_OK(rgtp_init());
    RGTP_RUN_TEST(test_replay_accept_new);
    RGTP_RUN_TEST(test_replay_reject_duplicate);
    RGTP_RUN_TEST(test_replay_reject_below_base);
    RGTP_RUN_TEST(test_replay_window_advance);
    RGTP_RUN_TEST(test_replay_window_boundary);
    RGTP_RUN_TEST(test_replay_window_just_outside);
    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
