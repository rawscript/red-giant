/**
 * @file rgtp_error.c
 * @brief RGTP error code definitions and rgtp_strerror().
 *
 * Requirements: 21.1, 21.2, 21.3
 */

#include "rgtp/rgtp.h"

const char* rgtp_strerror(rgtp_error_t err)
{
    switch (err) {
    case RGTP_OK:                  return "Success";
    case RGTP_ERR_NOMEM:           return "Memory allocation failed";
    case RGTP_ERR_INVALID_ARG:     return "Invalid argument (NULL pointer or out-of-range value)";
    case RGTP_ERR_SOCKET:          return "Socket operation failed";
    case RGTP_ERR_CRYPTO_INIT:     return "Cryptographic library initialisation failed";
    case RGTP_ERR_ENCRYPT:         return "AEAD encryption failed";
    case RGTP_ERR_DECRYPT:         return "AEAD decryption failed";
    case RGTP_ERR_AUTH_FAIL:       return "AEAD authentication tag verification failed";
    case RGTP_ERR_MERKLE_FAIL:     return "Merkle proof verification failed";
    case RGTP_ERR_FEC_FAIL:        return "Reed-Solomon FEC decoding failed (too many erasures)";
    case RGTP_ERR_TRUNCATED:       return "Packet truncated: declared length exceeds received bytes";
    case RGTP_ERR_CHUNK_INDEX_OOB: return "Chunk index out of bounds (>= chunk_count)";
    case RGTP_ERR_TIMEOUT:         return "Operation timed out";
    case RGTP_ERR_RATE_LIMITED:    return "Pull request rate limit exceeded";
    case RGTP_ERR_NOT_SUPPORTED:   return "Feature not supported on this platform or build configuration";
    case RGTP_ERR_INTERNAL:        return "Internal invariant violation (this is a bug)";
    default:                       return "Unknown error code";
    }
}
