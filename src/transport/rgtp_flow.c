/**
 * @file rgtp_flow.c
 * @brief AIMD congestion control, RTT estimation, and adaptive FEC strength.
 *
 * RTT estimation:
 *   new_rtt = 0.875 * old_rtt + 0.125 * sample  (EWMA, alpha = 0.125)
 *
 * Window control:
 *   Congestion (RTT > 2x baseline): window *= 0.5  (minimum 1)
 *   Recovery   (RTT within 1.1x baseline for 5 consecutive periods):
 *              window += 1 per RTT period
 *
 * Adaptive FEC:
 *   loss > 5%:  fec_overhead = min(fec_overhead + 0.10, 0.50)
 *   loss < 1%:  fec_overhead = max(fec_overhead - 0.05, 0.00)
 *   otherwise:  no change
 *
 * Pull pressure:
 *   Count of pull requests received in the last 100ms sliding window.
 *
 * Requirements: 6.4, 6.5, 6.6, 6.7, 6.8, 4.9
 */

#include "rgtp_transport_internal.h"

#include <stdint.h>

#define EWMA_ALPHA        0.125f
#define EWMA_ALPHA_LOSS   0.125f
#define CONGESTION_RATIO  2.0f
#define RECOVERY_RATIO    1.1f
#define RECOVERY_PERIODS  5u
#define PULL_WINDOW_US    100000u   /* 100ms in microseconds */

void rgtp_flow_init(rgtp_flow_t* f, uint32_t initial_window)
{
    f->rtt_us              = 0;
    f->rtt_baseline_us     = 0;
    f->rtt_sample_count    = 0;
    f->window_size         = (initial_window > 0) ? initial_window : 64u;
    f->window_min          = 1u;
    f->window_max          = 65536u;
    f->consecutive_low_rtt = 0;
    f->loss_rate           = 0.0f;
    f->fec_overhead        = 0.0f;
    f->pull_pressure       = 0;
    f->pull_pressure_window_start_us = 0;
    f->pull_pressure_count = 0;
}

void rgtp_flow_update_rtt(rgtp_flow_t* f, uint32_t sample_us)
{
    if (sample_us == 0) return;

    if (f->rtt_sample_count == 0) {
        /* First sample — initialise both EWMA and baseline */
        f->rtt_us          = sample_us;
        f->rtt_baseline_us = sample_us;
    } else {
        /* EWMA update: new = (1 - alpha) * old + alpha * sample */
        f->rtt_us = (uint32_t)(
            (1.0f - EWMA_ALPHA) * (float)f->rtt_us +
            EWMA_ALPHA * (float)sample_us);

        /* Update baseline to the minimum observed RTT */
        if (f->rtt_us < f->rtt_baseline_us) {
            f->rtt_baseline_us = f->rtt_us;
        }
    }
    f->rtt_sample_count++;

    /* Check for congestion / recovery */
    if (f->rtt_baseline_us > 0) {
        float ratio = (float)f->rtt_us / (float)f->rtt_baseline_us;

        if (ratio > CONGESTION_RATIO) {
            rgtp_flow_on_congestion(f);
        } else if (ratio <= RECOVERY_RATIO) {
            f->consecutive_low_rtt++;
            if (f->consecutive_low_rtt >= RECOVERY_PERIODS) {
                rgtp_flow_on_ack(f);
                f->consecutive_low_rtt = 0;
            }
        } else {
            f->consecutive_low_rtt = 0;
        }
    }
}

void rgtp_flow_update_loss(rgtp_flow_t* f, float loss_sample)
{
    if (loss_sample < 0.0f) loss_sample = 0.0f;
    if (loss_sample > 1.0f) loss_sample = 1.0f;

    f->loss_rate = (1.0f - EWMA_ALPHA_LOSS) * f->loss_rate
                 + EWMA_ALPHA_LOSS * loss_sample;
}

void rgtp_flow_on_congestion(rgtp_flow_t* f)
{
    /* Multiplicative decrease: halve the window */
    uint32_t new_window = f->window_size / 2u;
    if (new_window < f->window_min) {
        new_window = f->window_min;
    }
    f->window_size         = new_window;
    f->consecutive_low_rtt = 0;
}

void rgtp_flow_on_ack(rgtp_flow_t* f)
{
    /* Additive increase: +1 chunk per RTT period */
    uint32_t new_window = f->window_size + 1u;
    if (new_window > f->window_max) {
        new_window = f->window_max;
    }
    f->window_size = new_window;
}

void rgtp_flow_update_fec(rgtp_flow_t* f)
{
    if (f->loss_rate > 0.05f) {
        f->fec_overhead += 0.10f;
        if (f->fec_overhead > 0.50f) f->fec_overhead = 0.50f;
    } else if (f->loss_rate < 0.01f) {
        f->fec_overhead -= 0.05f;
        if (f->fec_overhead < 0.00f) f->fec_overhead = 0.00f;
    }
    /* 1% <= loss <= 5%: no change */
}

void rgtp_flow_record_pull(rgtp_flow_t* f, uint64_t now_us)
{
    /* Slide the 100ms window */
    if (now_us - f->pull_pressure_window_start_us >= PULL_WINDOW_US) {
        f->pull_pressure                 = f->pull_pressure_count;
        f->pull_pressure_count           = 0;
        f->pull_pressure_window_start_us = now_us;
    }
    f->pull_pressure_count++;
}
