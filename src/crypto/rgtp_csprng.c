/**
 * @file rgtp_csprng.c
 * @brief CSPRNG wrapper — rgtp_csprng_bytes() and rgtp_generate_exposure_id().
 *
 * Replaces the current LCG-based exposure ID generator
 * (seed * 1103515245 + 12345) with a cryptographically secure PRNG.
 *
 * Requirements: 3.1
 */

#include "rgtp_crypto_internal.h"

#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
#  include <sodium.h>
#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
#  include <openssl/rand.h>
#else
#  error "rgtp_csprng: no crypto backend. Build with RGTP_CRYPTO_BACKEND=libsodium or openssl."
#endif

rgtp_error_t rgtp_csprng_bytes(void* buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return RGTP_ERR_INVALID_ARG;
    }

#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
    randombytes_buf(buf, len);
    return RGTP_OK;
#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
    if (RAND_bytes((unsigned char*)buf, (int)len) != 1) {
        return RGTP_ERR_CRYPTO_INIT;
    }
    return RGTP_OK;
#endif
}

rgtp_error_t rgtp_generate_exposure_id(uint8_t out_id[16])
{
    if (out_id == NULL) {
        return RGTP_ERR_INVALID_ARG;
    }
    return rgtp_csprng_bytes(out_id, 16);
}
