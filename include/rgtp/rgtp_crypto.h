#ifndef RGTP_CRYPTO_H
#define RGTP_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

// Simple XOR-based "encryption" for demonstration
// In production, replace with proper libsodium/chacha20-poly1305

typedef struct {
    uint8_t key[32];
    uint64_t counter;
} rgtp_crypto_ctx_t;

// Initialize crypto context
void rgtp_crypto_init(rgtp_crypto_ctx_t* ctx);

// Simple encryption (XOR cipher for demo)
void rgtp_simple_encrypt(const uint8_t* plaintext, size_t pt_len,
                        uint8_t* ciphertext, uint64_t counter,
                        const uint8_t* key);

// Simple decryption (XOR cipher for demo)  
void rgtp_simple_decrypt(const uint8_t* ciphertext, size_t ct_len,
                        uint8_t* plaintext, uint64_t counter,
                        const uint8_t* key);

// Generate 128-bit exposure ID
void rgtp_generate_exposure_id(uint64_t exposure_id[2]);

// Hash function for Merkle tree (simplified)
uint32_t rgtp_simple_hash(const uint8_t* data, size_t len);

#endif // RGTP_CRYPTO_H