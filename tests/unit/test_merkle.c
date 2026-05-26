/**
 * @file test_merkle.c
 * @brief Unit tests for the RGTP Merkle tree subsystem.
 *
 * Tests: 8 cases covering single/two/power-of-2/non-power-of-2 trees,
 * proof verification, tampered proofs, and domain separation.
 *
 * Requirements: 17.1, 17.2
 */

#include "rgtp/rgtp.h"
#include "../helpers/rgtp_test.h"
#include "../../src/merkle/rgtp_merkle_internal.h"

#include <string.h>
#include <stdint.h>

/* ── Helpers ────────────────────────────────────────────────────────────── */

static uint8_t chunk_a[4] = {0x01, 0x02, 0x03, 0x04};
static uint8_t chunk_b[4] = {0x05, 0x06, 0x07, 0x08};
static uint8_t chunk_c[4] = {0x09, 0x0A, 0x0B, 0x0C};
static uint8_t chunk_d[4] = {0x0D, 0x0E, 0x0F, 0x10};
static uint8_t chunk_e[4] = {0x11, 0x12, 0x13, 0x14};

/* ── Test: Single-chunk tree — root equals leaf hash ────────────────────── */
static void test_merkle_single_chunk(void)
{
    const uint8_t* chunks[1] = { chunk_a };
    size_t         sizes[1]  = { sizeof(chunk_a) };
    uint8_t        root[32];
    uint32_t       depth = 0;

    RGTP_ASSERT_OK(rgtp_merkle_build(chunks, sizes, 1, root, NULL, &depth));
    RGTP_ASSERT(depth == 0, "Single-chunk tree depth must be 0");

    /* Root must equal the leaf hash of chunk_a */
    uint8_t leaf[32];
    RGTP_ASSERT_OK(rgtp_leaf_hash(chunk_a, sizeof(chunk_a), leaf));
    RGTP_ASSERT(memcmp(root, leaf, 32) == 0,
                "Single-chunk root must equal leaf hash");
}

/* ── Test: Two-chunk tree — root = H(0x01 || leaf0 || leaf1) ────────────── */
static void test_merkle_two_chunks(void)
{
    const uint8_t* chunks[2] = { chunk_a, chunk_b };
    size_t         sizes[2]  = { sizeof(chunk_a), sizeof(chunk_b) };
    uint8_t        root[32];
    uint32_t       depth = 0;

    RGTP_ASSERT_OK(rgtp_merkle_build(chunks, sizes, 2, root, NULL, &depth));
    RGTP_ASSERT(depth == 1, "Two-chunk tree depth must be 1");

    uint8_t leaf0[32], leaf1[32], expected_root[32];
    RGTP_ASSERT_OK(rgtp_leaf_hash(chunk_a, sizeof(chunk_a), leaf0));
    RGTP_ASSERT_OK(rgtp_leaf_hash(chunk_b, sizeof(chunk_b), leaf1));
    RGTP_ASSERT_OK(rgtp_internal_hash(leaf0, leaf1, expected_root));
    RGTP_ASSERT(memcmp(root, expected_root, 32) == 0,
                "Two-chunk root must equal H(0x01||leaf0||leaf1)");
}

/* ── Test: Power-of-2 tree (8 chunks) — all proofs verify ──────────────── */
static void test_merkle_power_of_two(void)
{
    static uint8_t data[8][16];
    const uint8_t* chunks[8];
    size_t         sizes[8];

    for (int i = 0; i < 8; i++) {
        memset(data[i], (uint8_t)i, 16);
        chunks[i] = data[i];
        sizes[i]  = 16;
    }

    uint8_t   root[32];
    uint8_t** proofs = NULL;
    uint32_t  depth  = 0;

    RGTP_ASSERT_OK(rgtp_merkle_build(chunks, sizes, 8, root, &proofs, &depth));
    RGTP_ASSERT(depth == 3, "8-chunk tree depth must be 3");

    for (int i = 0; i < 8; i++) {
        rgtp_error_t err = rgtp_merkle_verify(data[i], 16,
                                               proofs[i], depth,
                                               (uint32_t)i, root);
        RGTP_ASSERT(err == RGTP_OK, "All proofs in 8-chunk tree must verify");
    }

    /* Cleanup */
    for (int i = 0; i < 8; i++) {
        if (proofs && proofs[i]) { /* freed by caller */ }
    }
}

/* ── Test: Non-power-of-2 tree (5 chunks, padded to 8) ─────────────────── */
static void test_merkle_non_power_of_two(void)
{
    static uint8_t data[5][8];
    const uint8_t* chunks[5];
    size_t         sizes[5];

    for (int i = 0; i < 5; i++) {
        memset(data[i], (uint8_t)(i + 1), 8);
        chunks[i] = data[i];
        sizes[i]  = 8;
    }

    uint8_t   root[32];
    uint8_t** proofs = NULL;
    uint32_t  depth  = 0;

    RGTP_ASSERT_OK(rgtp_merkle_build(chunks, sizes, 5, root, &proofs, &depth));
    RGTP_ASSERT(depth == 3, "5-chunk tree (padded to 8) depth must be 3");

    for (int i = 0; i < 5; i++) {
        rgtp_error_t err = rgtp_merkle_verify(data[i], 8,
                                               proofs[i], depth,
                                               (uint32_t)i, root);
        RGTP_ASSERT(err == RGTP_OK, "All proofs in 5-chunk tree must verify");
    }
}

/* ── Test: Modified root causes proof failure ───────────────────────────── */
static void test_merkle_proof_wrong_root(void)
{
    const uint8_t* chunks[2] = { chunk_a, chunk_b };
    size_t         sizes[2]  = { sizeof(chunk_a), sizeof(chunk_b) };
    uint8_t        root[32];
    uint8_t**      proofs = NULL;
    uint32_t       depth  = 0;

    RGTP_ASSERT_OK(rgtp_merkle_build(chunks, sizes, 2, root, &proofs, &depth));

    uint8_t bad_root[32];
    memcpy(bad_root, root, 32);
    bad_root[0] ^= 0xFF;

    rgtp_error_t err = rgtp_merkle_verify(chunk_a, sizeof(chunk_a),
                                           proofs[0], depth, 0, bad_root);
    RGTP_ASSERT(err == RGTP_ERR_MERKLE_FAIL,
                "Modified root must cause RGTP_ERR_MERKLE_FAIL");
}

/* ── Test: Modified sibling hash causes proof failure ───────────────────── */
static void test_merkle_proof_wrong_sibling(void)
{
    const uint8_t* chunks[2] = { chunk_a, chunk_b };
    size_t         sizes[2]  = { sizeof(chunk_a), sizeof(chunk_b) };
    uint8_t        root[32];
    uint8_t**      proofs = NULL;
    uint32_t       depth  = 0;

    RGTP_ASSERT_OK(rgtp_merkle_build(chunks, sizes, 2, root, &proofs, &depth));

    /* Tamper with the first sibling hash in proof[0] */
    uint8_t bad_proof[32];
    memcpy(bad_proof, proofs[0], 32);
    bad_proof[0] ^= 0xFF;

    rgtp_error_t err = rgtp_merkle_verify(chunk_a, sizeof(chunk_a),
                                           bad_proof, depth, 0, root);
    RGTP_ASSERT(err == RGTP_ERR_MERKLE_FAIL,
                "Modified sibling must cause RGTP_ERR_MERKLE_FAIL");
}

/* ── Test: Wrong chunk index causes proof failure ───────────────────────── */
static void test_merkle_proof_wrong_index(void)
{
    const uint8_t* chunks[2] = { chunk_a, chunk_b };
    size_t         sizes[2]  = { sizeof(chunk_a), sizeof(chunk_b) };
    uint8_t        root[32];
    uint8_t**      proofs = NULL;
    uint32_t       depth  = 0;

    RGTP_ASSERT_OK(rgtp_merkle_build(chunks, sizes, 2, root, &proofs, &depth));

    /* Use proof[0] but claim it's for index 1 */
    rgtp_error_t err = rgtp_merkle_verify(chunk_a, sizeof(chunk_a),
                                           proofs[0], depth, 1, root);
    RGTP_ASSERT(err == RGTP_ERR_MERKLE_FAIL,
                "Wrong chunk index must cause RGTP_ERR_MERKLE_FAIL");
}

/* ── Test: Domain separation — leaf hash != internal hash for same input ── */
static void test_merkle_domain_separation(void)
{
    uint8_t leaf_hash[32], internal_hash[32];
    uint8_t zero32[32] = {0};

    RGTP_ASSERT_OK(rgtp_leaf_hash(zero32, 32, leaf_hash));
    RGTP_ASSERT_OK(rgtp_internal_hash(zero32, zero32, internal_hash));

    RGTP_ASSERT(memcmp(leaf_hash, internal_hash, 32) != 0,
                "Leaf hash and internal hash must differ for same input");
}

/* ── Test runner ────────────────────────────────────────────────────────── */
int main(void)
{
    RGTP_ASSERT_OK(rgtp_init());

    RGTP_RUN_TEST(test_merkle_single_chunk);
    RGTP_RUN_TEST(test_merkle_two_chunks);
    RGTP_RUN_TEST(test_merkle_power_of_two);
    RGTP_RUN_TEST(test_merkle_non_power_of_two);
    RGTP_RUN_TEST(test_merkle_proof_wrong_root);
    RGTP_RUN_TEST(test_merkle_proof_wrong_sibling);
    RGTP_RUN_TEST(test_merkle_proof_wrong_index);
    RGTP_RUN_TEST(test_merkle_domain_separation);

    RGTP_PRINT_RESULTS();
    return rgtp_test_failures > 0 ? 1 : 0;
}
