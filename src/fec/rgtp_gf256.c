/**
 * @file rgtp_gf256.c
 * @brief GF(2^8) arithmetic tables and operations.
 *
 * Critical fix from the original code:
 *  - The original rs_init used 0x1D as the reduction polynomial, which is
 *    NOT a primitive polynomial for GF(2^8).
 *  - The correct primitive polynomial is x^8 + x^4 + x^3 + x^2 + 1 = 0x11D.
 *
 * Table layout:
 *  gf_exp[512] — antilog table, doubled to avoid modular wrap-around in gf_mul
 *  gf_log[256] — log table (gf_log[0] = 0 by convention; log(0) is undefined)
 *
 * Requirements: 4.1
 */

#include "rgtp_gf256_internal.h"

/* ── Tables ─────────────────────────────────────────────────────────────── */

static uint8_t gf_exp[512];   /* antilog: gf_exp[i] = alpha^i mod p(x) */
static uint8_t gf_log[256];   /* log:     gf_log[x] = i  such that alpha^i = x */

/* ── Initialisation ─────────────────────────────────────────────────────── */

void gf256_init(void)
{
    /*
     * Generate the antilog table using the primitive polynomial
     * p(x) = x^8 + x^4 + x^3 + x^2 + 1  (0x11D).
     *
     * alpha = 2 (the generator element).
     * alpha^0 = 1, alpha^1 = 2, alpha^2 = 4, ...
     * When the value exceeds 8 bits, reduce modulo p(x) by XOR with 0x11D.
     */
    uint32_t x = 1;
    for (int i = 0; i < 255; i++) {
        gf_exp[i] = (uint8_t)x;
        gf_log[x] = (uint8_t)i;
        x <<= 1;
        if (x & 0x100u) {
            x ^= 0x11Du;   /* reduce mod x^8+x^4+x^3+x^2+1 */
        }
    }

    /* gf_exp[255] wraps back to gf_exp[0] = 1 (alpha^255 = 1 in GF(2^8)) */
    gf_exp[255] = gf_exp[0];

    /* Double the antilog table to avoid modular arithmetic in gf_mul */
    for (int i = 256; i < 512; i++) {
        gf_exp[i] = gf_exp[i - 255];
    }

    /* log(0) is mathematically undefined; set to 0 by convention */
    gf_log[0] = 0;
}

/* ── Arithmetic ─────────────────────────────────────────────────────────── */

uint8_t gf_mul(uint8_t a, uint8_t b)
{
    if (a == 0 || b == 0) return 0;
    /* gf_exp is doubled so gf_log[a] + gf_log[b] never exceeds 508 */
    return gf_exp[(int)gf_log[a] + (int)gf_log[b]];
}

uint8_t gf_div(uint8_t a, uint8_t b)
{
    /* Division by zero is undefined — caller must ensure b != 0 */
    if (a == 0) return 0;
    int exp = (int)gf_log[a] - (int)gf_log[b] + 255;
    return gf_exp[exp % 255];
}

uint8_t gf_inv(uint8_t a)
{
    /* Inverse of 0 is undefined — caller must ensure a != 0 */
    return gf_exp[255 - (int)gf_log[a]];
}

uint8_t gf_pow(uint8_t a, int p)
{
    if (p == 0) return 1;
    if (a == 0) return 0;
    int exp = ((int)gf_log[a] * p) % 255;
    if (exp < 0) exp += 255;
    return gf_exp[exp];
}
