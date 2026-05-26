/**
 * @file rgtp_fec_internal.h
 * @brief Internal declarations for the RGTP Reed-Solomon FEC subsystem.
 */

#ifndef RGTP_FEC_INTERNAL_H
#define RGTP_FEC_INTERNAL_H

#include "rgtp/rgtp.h"
#include "rgtp_gf256_internal.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RGTP_RS_MAX_N 255u   /* Maximum codeword length */
#define RGTP_RS_MAX_T 127u   /* Maximum number of parity symbols */

/**
 * @brief Encode k data symbols into n codeword symbols (systematic RS).
 *
 * The first k output symbols equal the input data (systematic form).
 * The last n-k output symbols are the Reed-Solomon parity.
 *
 * @param k           Number of data symbols (1 <= k < n <= 255).
 * @param n           Total codeword length.
 * @param data        Input: k data symbols (each symbol is one byte).
 * @param parity_out  Output: n-k parity symbols.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_rs_encode(uint8_t k, uint8_t n,
                             const uint8_t* data,
                             uint8_t*       parity_out);

/**
 * @brief Decode k data symbols from any k of n received symbols.
 *
 * Uses Gaussian elimination over GF(2^8) for erasure-only decoding.
 *
 * @param k                Number of data symbols.
 * @param n                Total codeword length.
 * @param received         Array of n received symbols (0 for erased positions).
 * @param erasure_pos      Array of erasure_count erased positions (0-indexed).
 * @param erasure_count    Number of erased positions (must be <= n-k).
 * @param data_out         Output: k recovered data symbols.
 * @return RGTP_OK, RGTP_ERR_FEC_FAIL (too many erasures), or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_rs_decode(uint8_t        k,
                             uint8_t        n,
                             const uint8_t* received,
                             const uint8_t* erasure_pos,
                             uint8_t        erasure_count,
                             uint8_t*       data_out);

#ifdef __cplusplus
}
#endif

#endif /* RGTP_FEC_INTERNAL_H */
