/**
 * @file rgtp_rs_decode.c
 * @brief Reed-Solomon erasure decoder using Gaussian elimination over GF(2^8).
 *
 * For erasure-only decoding (positions of lost symbols are known), we solve
 * the linear system:
 *
 *   V * c = r
 *
 * where V is the Vandermonde-like submatrix of the generator matrix formed
 * from the k received (non-erased) positions, c is the k data symbols, and
 * r is the k received symbol values.
 *
 * Algorithm:
 *  1. Select k non-erased positions from the n received symbols.
 *  2. Build the k×k evaluation matrix M where M[i][j] = alpha^(pos[i]*j).
 *  3. Solve M * data = received[pos] via Gaussian elimination over GF(2^8).
 *
 * Requirements: 4.4
 */

#include "rgtp_fec_internal.h"

#include <string.h>   /* memset, memcpy */

/* ── Gaussian elimination over GF(2^8) ─────────────────────────────────── */

/**
 * Solve the k×k linear system A * x = b over GF(2^8) in-place.
 * A is augmented with b as the last column: [A | b].
 * On success, b contains the solution x.
 * Returns RGTP_OK or RGTP_ERR_FEC_FAIL (singular matrix).
 */
static rgtp_error_t gauss_elim(uint8_t k, uint8_t mat[RGTP_RS_MAX_N][RGTP_RS_MAX_N + 1])
{
    for (uint8_t col = 0; col < k; col++) {
        /* Find pivot */
        uint8_t pivot_row = k;   /* sentinel: not found */
        for (uint8_t row = col; row < k; row++) {
            if (mat[row][col] != 0) { pivot_row = row; break; }
        }
        if (pivot_row == k) {
            return RGTP_ERR_FEC_FAIL;   /* singular — too many erasures */
        }

        /* Swap rows */
        if (pivot_row != col) {
            for (uint8_t j = 0; j <= k; j++) {
                uint8_t tmp = mat[col][j];
                mat[col][j] = mat[pivot_row][j];
                mat[pivot_row][j] = tmp;
            }
        }

        /* Scale pivot row so pivot element = 1 */
        uint8_t inv_pivot = gf_inv(mat[col][col]);
        for (uint8_t j = col; j <= k; j++) {
            mat[col][j] = gf_mul(mat[col][j], inv_pivot);
        }

        /* Eliminate column in all other rows */
        for (uint8_t row = 0; row < k; row++) {
            if (row == col || mat[row][col] == 0) continue;
            uint8_t factor = mat[row][col];
            for (uint8_t j = col; j <= k; j++) {
                mat[row][j] ^= gf_mul(factor, mat[col][j]);
            }
        }
    }
    return RGTP_OK;
}

/* ── Decoder ────────────────────────────────────────────────────────────── */

rgtp_error_t rgtp_rs_decode(uint8_t        k,
                             uint8_t        n,
                             const uint8_t* received,
                             const uint8_t* erasure_pos,
                             uint8_t        erasure_count,
                             uint8_t*       data_out)
{
    if (received == NULL || data_out == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    if (k == 0 || n == 0 || k >= n || n > RGTP_RS_MAX_N) {
        return RGTP_ERR_INVALID_ARG;
    }
    if (erasure_count > (uint8_t)(n - k)) {
        return RGTP_ERR_FEC_FAIL;
    }

    /*
     * Build a boolean mask of erased positions.
     */
    uint8_t erased[RGTP_RS_MAX_N];
    memset(erased, 0, n);
    if (erasure_pos != NULL) {
        for (uint8_t i = 0; i < erasure_count; i++) {
            if (erasure_pos[i] < n) {
                erased[erasure_pos[i]] = 1;
            }
        }
    }

    /*
     * Collect k non-erased positions.
     */
    uint8_t good_pos[RGTP_RS_MAX_N];
    uint8_t good_count = 0;
    for (uint8_t i = 0; i < n && good_count < k; i++) {
        if (!erased[i]) {
            good_pos[good_count++] = i;
        }
    }
    if (good_count < k) {
        return RGTP_ERR_FEC_FAIL;
    }

    /*
     * Build the k×k evaluation matrix M and augment with the received values.
     *
     * For a systematic (n,k) RS code, the codeword is:
     *   c[i] = data[i]  for i in [0, k)
     *   c[i] = parity   for i in [k, n)
     *
     * The evaluation matrix for position p is:
     *   M[row][col] = alpha^(good_pos[row] * col)  for col in [0, k)
     *
     * This is the standard Vandermonde formulation.
     */
    uint8_t mat[RGTP_RS_MAX_N][RGTP_RS_MAX_N + 1];
    memset(mat, 0, sizeof(mat));

    for (uint8_t row = 0; row < k; row++) {
        uint8_t pos = good_pos[row];
        for (uint8_t col = 0; col < k; col++) {
            mat[row][col] = gf_pow(2, (int)pos * (int)col);
        }
        mat[row][k] = received[pos];   /* augmented column */
    }

    /* Solve via Gaussian elimination */
    rgtp_error_t err = gauss_elim(k, mat);
    if (err != RGTP_OK) return err;

    /* Extract solution: data_out[col] = mat[col][k] */
    for (uint8_t col = 0; col < k; col++) {
        data_out[col] = mat[col][k];
    }
    return RGTP_OK;
}
