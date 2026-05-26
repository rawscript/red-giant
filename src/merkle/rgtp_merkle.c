/**
 * @file rgtp_merkle.c
 * @brief Merkle tree build, proof generation, and proof verification.
 *
 * Tree construction:
 *  1. Compute leaf hashes for all chunk_count chunks.
 *  2. Pad to the next power-of-2 with zero-hash leaves.
 *  3. Build the tree bottom-up, computing internal node hashes.
 *  4. Store the root in surface->merkle_root[32].
 *
 * Proof generation:
 *  - A proof for chunk i consists of log2(padded_count) sibling hashes.
 *  - Proofs are pre-computed at expose time.
 *
 * Proof verification:
 *  - Recompute the path from leaf to root using the proof siblings.
 *  - Compare the computed root against the stored root.
 *
 * Requirements: 3.6, 3.7, 3.8, 3.9
 */

#include "rgtp_merkle_internal.h"
#include "../core/rgtp_alloc_internal.h"

#include <string.h>   /* memset, memcpy */
#include <stdint.h>

/* ── Helpers ────────────────────────────────────────────────────────────── */

/** Next power of 2 >= n (n must be > 0). */
static uint32_t next_pow2(uint32_t n)
{
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1;
}

/** Integer log2 for powers of 2. */
static uint32_t log2_u32(uint32_t n)
{
    uint32_t r = 0;
    while (n > 1) { n >>= 1; r++; }
    return r;
}

/* ── Tree build ─────────────────────────────────────────────────────────── */

rgtp_error_t rgtp_merkle_build(const uint8_t* const* chunks,
                                const size_t*         chunk_sizes,
                                uint32_t              chunk_count,
                                uint8_t               root_out[RGTP_MERKLE_HASH_BYTES],
                                uint8_t***            proofs_out,
                                uint32_t*             proof_depth_out)
{
    if (chunks == NULL || chunk_sizes == NULL || chunk_count == 0 || root_out == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }

    const uint32_t padded = next_pow2(chunk_count);
    const uint32_t depth  = log2_u32(padded);
    const uint32_t tree_size = 2u * padded;   /* 1-indexed binary tree */

    /* Allocate flat tree: tree[1] = root, tree[2*i] = left child of tree[i] */
    uint8_t* tree = (uint8_t*)rgtp_calloc(tree_size, RGTP_MERKLE_HASH_BYTES);
    if (tree == NULL) {
        return RGTP_ERR_NOMEM;
    }

    rgtp_error_t err = RGTP_OK;

    /* ── Step 1: Compute leaf hashes ──────────────────────────────────── */
    for (uint32_t i = 0; i < padded; i++) {
        uint8_t* leaf = tree + (padded + i) * RGTP_MERKLE_HASH_BYTES;
        if (i < chunk_count) {
            err = rgtp_leaf_hash(chunks[i], chunk_sizes[i], leaf);
            if (err != RGTP_OK) goto cleanup;
        } else {
            /* Padding: zero-hash leaf */
            memset(leaf, 0, RGTP_MERKLE_HASH_BYTES);
        }
    }

    /* ── Step 2: Build internal nodes bottom-up ───────────────────────── */
    for (uint32_t i = padded - 1; i >= 1; i--) {
        const uint8_t* left  = tree + (2u * i)      * RGTP_MERKLE_HASH_BYTES;
        const uint8_t* right = tree + (2u * i + 1u) * RGTP_MERKLE_HASH_BYTES;
        uint8_t*       node  = tree + i              * RGTP_MERKLE_HASH_BYTES;
        err = rgtp_internal_hash(left, right, node);
        if (err != RGTP_OK) goto cleanup;
    }

    /* ── Step 3: Copy root ────────────────────────────────────────────── */
    memcpy(root_out, tree + RGTP_MERKLE_HASH_BYTES, RGTP_MERKLE_HASH_BYTES);

    if (proof_depth_out != NULL) {
        *proof_depth_out = depth;
    }

    /* ── Step 4: Pre-compute proofs (optional) ────────────────────────── */
    if (proofs_out != NULL) {
        uint8_t** proofs = (uint8_t**)rgtp_calloc(chunk_count, sizeof(uint8_t*));
        if (proofs == NULL) { err = RGTP_ERR_NOMEM; goto cleanup; }

        for (uint32_t i = 0; i < chunk_count; i++) {
            proofs[i] = (uint8_t*)rgtp_malloc((size_t)depth * RGTP_MERKLE_HASH_BYTES);
            if (proofs[i] == NULL) {
                /* Free already-allocated proofs */
                for (uint32_t j = 0; j < i; j++) rgtp_free(proofs[j]);
                rgtp_free(proofs);
                err = RGTP_ERR_NOMEM;
                goto cleanup;
            }

            /* Walk from leaf to root, collecting sibling hashes */
            uint32_t node_idx = padded + i;
            for (uint32_t d = 0; d < depth; d++) {
                uint32_t sibling_idx = (node_idx % 2 == 0) ? node_idx + 1 : node_idx - 1;
                memcpy(proofs[i] + (size_t)d * RGTP_MERKLE_HASH_BYTES,
                       tree + sibling_idx * RGTP_MERKLE_HASH_BYTES,
                       RGTP_MERKLE_HASH_BYTES);
                node_idx /= 2;
            }
        }
        *proofs_out = proofs;
    }

cleanup:
    rgtp_free(tree);
    return err;
}

/* ── Proof verification ─────────────────────────────────────────────────── */

rgtp_error_t rgtp_merkle_verify(const uint8_t* chunk_data,
                                 size_t         chunk_len,
                                 const uint8_t* proof,
                                 uint32_t       proof_depth,
                                 uint32_t       chunk_index,
                                 const uint8_t  root[RGTP_MERKLE_HASH_BYTES])
{
    if (chunk_data == NULL || proof == NULL || root == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }

    uint8_t h[RGTP_MERKLE_HASH_BYTES];
    rgtp_error_t err = rgtp_leaf_hash(chunk_data, chunk_len, h);
    if (err != RGTP_OK) return err;

    uint32_t idx = chunk_index;
    for (uint32_t d = 0; d < proof_depth; d++) {
        const uint8_t* sibling = proof + (size_t)d * RGTP_MERKLE_HASH_BYTES;
        uint8_t combined[RGTP_MERKLE_HASH_BYTES];

        if (idx % 2 == 0) {
            /* Current node is left child */
            err = rgtp_internal_hash(h, sibling, combined);
        } else {
            /* Current node is right child */
            err = rgtp_internal_hash(sibling, h, combined);
        }
        if (err != RGTP_OK) return err;

        memcpy(h, combined, RGTP_MERKLE_HASH_BYTES);
        idx /= 2;
    }

    /* Constant-time comparison to prevent timing side-channels */
    uint8_t diff = 0;
    for (size_t i = 0; i < RGTP_MERKLE_HASH_BYTES; i++) {
        diff |= h[i] ^ root[i];
    }
    return (diff == 0) ? RGTP_OK : RGTP_ERR_MERKLE_FAIL;
}
