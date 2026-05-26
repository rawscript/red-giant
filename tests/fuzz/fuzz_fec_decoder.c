/**
 * @file fuzz_fec_decoder.c
 * @brief libFuzzer target for the Reed-Solomon FEC decoder.
 *
 * Requirements: 17.5
 */

#include "rgtp/rgtp.h"
#include "../../src/fec/rgtp_fec_internal.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static int initialized = 0;
    if (!initialized) { rgtp_init(); initialized = 1; }

    if (size < 3) return 0;

    uint8_t k = data[0];
    uint8_t n = data[1];
    uint8_t erasure_count = data[2];

    /* Validate params — out-of-range must return RGTP_ERR_INVALID_ARG, not crash */
    if (k == 0 || n == 0 || k >= n) {
        uint8_t received[255] = {0};
        uint8_t data_out[255] = {0};
        rgtp_error_t err = rgtp_rs_decode(k, n, received, NULL, 0, data_out);
        (void)err;
        return 0;
    }

    size_t needed = (size_t)n + (size_t)erasure_count;
    if (3 + needed > size) return 0;

    const uint8_t* received_data = data + 3;
    const uint8_t* erasure_pos   = data + 3 + n;

    uint8_t received[255];
    uint8_t erasures[255];
    uint8_t data_out[255];

    memcpy(received, received_data, n);
    if (erasure_count > 0) {
        memcpy(erasures, erasure_pos, erasure_count);
        /* Clamp erasure positions to valid range */
        for (int i = 0; i < erasure_count; i++) {
            erasures[i] = erasures[i] % n;
        }
    }

    rgtp_error_t err = rgtp_rs_decode(k, n, received,
                                       erasure_count > 0 ? erasures : NULL,
                                       erasure_count, data_out);
    (void)err;
    return 0;
}
