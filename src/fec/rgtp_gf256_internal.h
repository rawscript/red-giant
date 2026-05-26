/**
 * @file rgtp_gf256_internal.h
 * @brief Internal GF(2^8) arithmetic — used by the RS encoder and decoder.
 *
 * Primitive polynomial: x^8 + x^4 + x^3 + x^2 + 1 = 0x11D
 */

#ifndef RGTP_GF256_INTERNAL_H
#define RGTP_GF256_INTERNAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the GF(2^8) log/antilog tables.
 *
 * Must be called once before any gf_* function is used.
 * Called automatically by rgtp_init() when FEC is enabled.
 */
void gf256_init(void);

/* ── Arithmetic operations ──────────────────────────────────────────────── */

/** Multiply two GF(2^8) elements. */
uint8_t gf_mul(uint8_t a, uint8_t b);

/** Divide a by b in GF(2^8). Undefined for b == 0. */
uint8_t gf_div(uint8_t a, uint8_t b);

/** Multiplicative inverse of a in GF(2^8). Undefined for a == 0. */
uint8_t gf_inv(uint8_t a);

/** Raise a to the power p in GF(2^8). */
uint8_t gf_pow(uint8_t a, int p);

/** Add two GF(2^8) elements (XOR). */
static inline uint8_t gf_add(uint8_t a, uint8_t b) { return a ^ b; }

/** Subtract two GF(2^8) elements (same as add in characteristic 2). */
static inline uint8_t gf_sub(uint8_t a, uint8_t b) { return a ^ b; }

#ifdef __cplusplus
}
#endif

#endif /* RGTP_GF256_INTERNAL_H */
