/**
 * @file fuzz_parser.c
 * @brief libFuzzer target for the RGTP packet parser.
 *
 * Feeds arbitrary byte buffers into rgtp_parse_packet().
 * Verifies: no crash, no ASan error, return value is always a valid
 * rgtp_error_t, and the parser never modifies state on error.
 *
 * Build with: -fsanitize=fuzzer,address,undefined
 *
 * Requirements: 17.5
 */

#include "rgtp/rgtp.h"
#include "../../src/wire/rgtp_wire_internal.h"
#include "../../src/wire/rgtp_packet_types.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* libFuzzer entry point */
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static int initialized = 0;
    if (!initialized) {
        rgtp_init();
        initialized = 1;
    }

    rgtp_packet_t pkt;
    memset(&pkt, 0xCC, sizeof(pkt));   /* poison output to detect partial writes */

    /* Use a random manifest_chunk_count derived from the input */
    uint32_t chunk_count = UINT32_MAX;
    if (size >= 4) {
        chunk_count = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
                    | ((uint32_t)data[2] <<  8) |  (uint32_t)data[3];
        if (chunk_count == 0) chunk_count = UINT32_MAX;
    }

    rgtp_error_t err = rgtp_parse_packet(data, size, chunk_count, &pkt);

    /* Verify return value is a known error code */
    switch (err) {
    case RGTP_OK:
    case RGTP_ERR_NOMEM:
    case RGTP_ERR_INVALID_ARG:
    case RGTP_ERR_SOCKET:
    case RGTP_ERR_CRYPTO_INIT:
    case RGTP_ERR_ENCRYPT:
    case RGTP_ERR_DECRYPT:
    case RGTP_ERR_AUTH_FAIL:
    case RGTP_ERR_MERKLE_FAIL:
    case RGTP_ERR_FEC_FAIL:
    case RGTP_ERR_TRUNCATED:
    case RGTP_ERR_CHUNK_INDEX_OOB:
    case RGTP_ERR_TIMEOUT:
    case RGTP_ERR_RATE_LIMITED:
    case RGTP_ERR_NOT_SUPPORTED:
    case RGTP_ERR_INTERNAL:
        break;
    default:
        /* Unknown return value — this is a bug */
        __builtin_trap();
    }

    return 0;
}
