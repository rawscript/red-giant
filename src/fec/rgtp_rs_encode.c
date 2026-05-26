/**
 * @file rgtp_rs_encode.c
 * @brief Systematic Reed-Solomon encoder over GF(2^8).
 *
 * Algorithm:
 *  1. Build the generator polynomial g(x) = prod_{i=0}^{n-k-1} (x - alpha^i).
 *  2. Compute parity = (x^(n-k) * data(x)) mod g(x) via polynomial long division.
 *  3. The systematic codeword is: [data[0..k-1] | parity[0..n-k-1]].
 *
 * Requirements: 4.2, 4.3
 */

#include "rgtp_fec_internal.h"

#include <string.h>   /* memset, memcpy */

/* ── Generator polynomial ───────────────────────────────────────────────── */

/**
 * Build g(x) = prod_{i=0}^{t-1} (x - alpha^i) where t = n - k.
 * Coefficients stored in g[0..t] with g[t] = 1 (monic).
 */
static void build_generator(uint8_t t, uint8_t g[RGTP_RS_MAX_T + 1])
{
    memset(g, 0, (size_t)(t + 1));
    g[0] = 1;   /* start with g(x) = 1 */

    for (uint8_t i = 0; i < t; i++) {
        /* Multiply g(x) by (x - alpha^i) = (x + alpha^i) in GF(2^8) */
        uint8_t root = gf_pow(2, i);   /* alpha^i */
        for (int j = (int)i; j >= 0; j--) {
            g[j + 1] ^= g[j];                    /* coefficient of x^(j+1) */
            g[j]      = gf_mul(g[j], root);       /* coefficient of x^j */
        }
    }
}

/* ── Encoder ────────────────────────────────────────────────────────────── */

rgtp_error_t rgtp_rs_encode(uint8_t k, uint8_t n,
                             const uint8_t* data,
                             uint8_t*       parity_out)
{
    if (data == NULL || parity_out == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    if (k == 0 || n == 0 || k >= n || n > RGTP_RS_MAX_N) {
        return RGTP_ERR_INVALID_ARG;
    }

    uint8_t t = n - k;   /* number of parity symbols */

    /* Build generator polynomial */
    uint8_t g[RGTP_RS_MAX_T + 1];
    build_generator(t, g);

    /*
     * Compute parity = (x^t * data(x)) mod g(x).
     *
     * We perform polynomial long division using a shift register of length t.
     * The register is initialised to zero and updated for each data symbol.
     */
    uint8_t reg[RGTP_RS_MAX_T];
    memset(reg, 0, t);

    for (int i = (int)k - 1; i >= 0; i--) {
        uint8_t feedback = data[i] ^ reg[t - 1];
        /* Shift register right and apply feedback */
        for (int j = (int)t - 1; j > 0; j--) {
            reg[j] = reg[j - 1] ^ gf_mul(feedback, g[j]);
        }
        reg[0] = gf_mul(feedback, g[0]);
    }

    /* The parity symbols are the register contents */
    memcpy(parity_out, reg, t);
    return RGTP_OK;
}
