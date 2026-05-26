/**
 * @file fuzz_merkle_verifier.c
 * @brief libFuzzer target for the Merkle proof verifier.
 *
 * Requirements: 17.5
 */

#include "rgtp/rgtp.h"
#include "../../src/merkle/rgtp_merkle_internal.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static int initialized = 0;
    if (!initialized) { rgtp_init(); initialized = 1; }

    if (size < 1 + 32 + 32) return 0;   /* need at least chunk_data + proof + root */

    /* Split input: first byte = proof_depth, next 32 = root,
     * next proof_depth*32 = proof, rest = chunk_data */
    uint8_t proof_depth = data[0] & 0x0F;   /* cap at 15 levels */
    const uint8_t* root = data + 1;

    size_t proof_bytes = (size_t)proof_depth * 32u;
    if (1 + 32 + proof_bytes > size) return 0;

    const uint8_t* proof      = data + 1 + 32;
    const uint8_t* chunk_data = data + 1 + 32 + proof_bytes;
    size_t         chunk_len  = size - 1 - 32 - proof_bytes;

    uint32_t chunk_index = 0;
    if (chunk_len >= 4) {
        chunk_index = ((uint32_t)chunk_data[0] << 24) | ((uint32_t)chunk_data[1] << 16)
                    | ((uint32_t)chunk_data[2] <<  8) |  (uint32_t)chunk_data[3];
        chunk_data += 4;
        chunk_len  -= 4;
    }

    rgtp_error_t err = rgtp_merkle_verify(chunk_data, chunk_len,
                                           proof, proof_depth,
                                           chunk_index, root);
    (void)err;   /* RGTP_OK or RGTP_ERR_MERKLE_FAIL are both valid */
    return 0;
}
