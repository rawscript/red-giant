/**
 * @file rgtp_merkle_internal.h
 * @brief Internal declarations for the RGTP Merkle tree subsystem.
 */

#ifndef RGTP_MERKLE_INTERNAL_H
#define RGTP_MERKLE_INTERNAL_H

#include "rgtp/rgtp.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RGTP_MERKLE_HASH_BYTES 32u   /* BLAKE2b-256 or SHA-256 output size */

/* ── Hash primitives ────────────────────────────────────────────────────── */

/**
 * @brief Compute a Merkle leaf hash: H(0x00 || data).
 *
 * Domain prefix 0x00 prevents second-preimage attacks where an internal node
 * hash could be confused with a leaf hash.
 *
 * @param data    Chunk plaintext.
 * @param len     Length of @p data in bytes.
 * @param out     32-byte output buffer.
 * @return RGTP_OK or RGTP_ERR_CRYPTO_INIT.
 */
rgtp_error_t rgtp_leaf_hash(const uint8_t* data, size_t len,
                             uint8_t out[RGTP_MERKLE_HASH_BYTES]);

/**
 * @brief Compute a Merkle internal node hash: H(0x01 || left || right).
 *
 * @param left    32-byte left child hash.
 * @param right   32-byte right child hash.
 * @param out     32-byte output buffer.
 * @return RGTP_OK or RGTP_ERR_CRYPTO_INIT.
 */
rgtp_error_t rgtp_internal_hash(const uint8_t left[RGTP_MERKLE_HASH_BYTES],
                                 const uint8_t right[RGTP_MERKLE_HASH_BYTES],
                                 uint8_t       out[RGTP_MERKLE_HASH_BYTES]);

/* ── Tree operations ────────────────────────────────────────────────────── */

/**
 * @brief Build a Merkle tree over @p chunk_count plaintext chunks.
 *
 * Pads to the next power-of-2 with zero-hash leaves. Stores the root in
 * @p root_out and optionally pre-computes all proofs into @p proofs_out.
 *
 * @param chunks        Array of @p chunk_count plaintext chunk buffers.
 * @param chunk_sizes   Array of @p chunk_count chunk sizes in bytes.
 * @param chunk_count   Number of chunks.
 * @param root_out      32-byte buffer to receive the Merkle root.
 * @param proofs_out    If non-NULL, receives an array of chunk_count proof
 *                      buffers (each proof_depth * 32 bytes). The caller is
 *                      responsible for freeing each proof buffer.
 * @param proof_depth_out  If non-NULL, receives the proof depth (log2 of
 *                         padded chunk count).
 * @return RGTP_OK, RGTP_ERR_NOMEM, or RGTP_ERR_INVALID_ARG.
 */
rgtp_error_t rgtp_merkle_build(const uint8_t* const* chunks,
                                const size_t*         chunk_sizes,
                                uint32_t              chunk_count,
                                uint8_t               root_out[RGTP_MERKLE_HASH_BYTES],
                                uint8_t***            proofs_out,
                                uint32_t*             proof_depth_out);

/**
 * @brief Verify a Merkle proof for a single chunk.
 *
 * @param chunk_data   Plaintext chunk data.
 * @param chunk_len    Length of @p chunk_data in bytes.
 * @param proof        Array of @p proof_depth sibling hashes (32 bytes each).
 * @param proof_depth  Number of sibling hashes in the proof.
 * @param chunk_index  Index of the chunk within the Exposure.
 * @param root         32-byte Merkle root to verify against.
 * @return RGTP_OK if the proof is valid, RGTP_ERR_MERKLE_FAIL otherwise.
 */
rgtp_error_t rgtp_merkle_verify(const uint8_t* chunk_data,
                                 size_t         chunk_len,
                                 const uint8_t* proof,
                                 uint32_t       proof_depth,
                                 uint32_t       chunk_index,
                                 const uint8_t  root[RGTP_MERKLE_HASH_BYTES]);

#ifdef __cplusplus
}
#endif

#endif /* RGTP_MERKLE_INTERNAL_H */
