/**
 * @file rgtp_zeroize.c
 * @brief Secure memory zeroization — wraps sodium_memzero / OPENSSL_cleanse.
 *
 * A compile-time #error fires if neither backend is available, ensuring key
 * material can always be securely erased before freeing.
 *
 * Requirements: 3.12, 3.14
 */

#include "rgtp_crypto_internal.h"

#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
#  include <sodium.h>
#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
#  include <openssl/crypto.h>
#else
#  error "rgtp_zeroize: no crypto backend available. " \
         "Build with RGTP_CRYPTO_BACKEND=libsodium or openssl."
#endif

void rgtp_zeroize(void* ptr, size_t len)
{
    if (ptr == NULL || len == 0) {
        return;
    }

#if defined(RGTP_CRYPTO_BACKEND_LIBSODIUM)
    sodium_memzero(ptr, len);
#elif defined(RGTP_CRYPTO_BACKEND_OPENSSL)
    OPENSSL_cleanse(ptr, len);
#endif
}
