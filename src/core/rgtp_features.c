// rgtp_features.c - Additional RGTP features implementation
// Implements pre-encryption, exposure IDs, and Merkle proofs

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

// Simple random generator
static uint32_t rgtp_random() {
    static uint32_t seed = 0;
    if (seed == 0) {
#ifdef _WIN32
        seed = (uint32_t)GetTickCount();
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        seed = (uint32_t)(tv.tv_sec * 1000000 + tv.tv_usec);
#endif
    }
    seed = seed * 1103515245 + 12345;
    return seed;
}

// Generate 128-bit exposure ID
void rgtp_generate_exposure_id(uint64_t id[2]) {
    id[0] = ((uint64_t)rgtp_random() << 32) | rgtp_random();
    id[1] = ((uint64_t)rgtp_random() << 32) | rgtp_random();
    id[1] ^= (uint64_t)time(NULL); // Mix timestamp for uniqueness
}

// Simple XOR encryption for demonstration
void rgtp_xor_encrypt(const uint8_t* input, size_t len, 
                     uint8_t* output, uint64_t counter,
                     const uint8_t key[32]) {
    for (size_t i = 0; i < len; i++) {
        output[i] = input[i] ^ key[i % 32] ^ (uint8_t)(counter >> (i % 8));
    }
}

// Simple XOR decryption (same as encryption)
void rgtp_xor_decrypt(const uint8_t* input, size_t len,
                     uint8_t* output, uint64_t counter,
                     const uint8_t key[32]) {
    rgtp_xor_encrypt(input, len, output, counter, key);
}

// Generate simple key from password/material
void rgtp_derive_key(const char* material, size_t mat_len, uint8_t key[32]) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < mat_len; i++) {
        hash = ((hash << 5) + hash) + material[i];
    }
    
    // Expand hash to 32 bytes
    for (int i = 0; i < 32; i++) {
        key[i] = (hash >> (i % 4 * 8)) & 0xFF;
        if (i >= 4) key[i] ^= (uint8_t)rgtp_random();
    }
}

// Simple hash for Merkle tree construction
uint32_t rgtp_hash_chunk(const uint8_t* data, size_t len) {
    uint32_t hash = 0x811C9DC5; // FNV-1a offset basis
    for (size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 0x01000193; // FNV prime
    }
    return hash;
}

// Build simple Merkle tree (binary tree)
typedef struct {
    uint32_t* nodes;
    size_t node_count;
    size_t leaf_count;
} merkle_tree_t;

merkle_tree_t* rgtp_build_merkle_tree(const uint8_t** chunks, 
                                     const size_t* chunk_sizes,
                                     size_t chunk_count) {
    if (chunk_count == 0) return NULL;
    
    // Calculate tree size (full binary tree)
    size_t leaf_nodes = chunk_count;
    size_t internal_nodes = leaf_nodes - 1;
    size_t total_nodes = leaf_nodes + internal_nodes;
    
    merkle_tree_t* tree = malloc(sizeof(merkle_tree_t));
    if (!tree) return NULL;
    
    tree->nodes = malloc(total_nodes * sizeof(uint32_t));
    if (!tree->nodes) {
        free(tree);
        return NULL;
    }
    
    tree->leaf_count = leaf_nodes;
    tree->node_count = total_nodes;
    
    // Hash leaf nodes
    for (size_t i = 0; i < leaf_nodes; i++) {
        tree->nodes[internal_nodes + i] = rgtp_hash_chunk(chunks[i], chunk_sizes[i]);
    }
    
    // Build internal nodes bottom-up
    for (size_t i = internal_nodes; i > 0; i--) {
        size_t left_child = 2 * i;
        size_t right_child = 2 * i + 1;
        
        uint32_t left_hash = (left_child < total_nodes) ? tree->nodes[left_child] : 0;
        uint32_t right_hash = (right_child < total_nodes) ? tree->nodes[right_child] : 0;
        
        // Simple combination hash
        tree->nodes[i] = left_hash ^ right_hash ^ (uint32_t)i;
    }
    
    // Root is at index 1 (index 0 unused for easier math)
    tree->nodes[0] = tree->nodes[1];
    
    return tree;
}

// Get Merkle proof for a chunk
uint32_t* rgtp_get_merkle_proof(const merkle_tree_t* tree, size_t chunk_index, 
                               size_t* proof_length) {
    if (!tree || chunk_index >= tree->leaf_count) {
        *proof_length = 0;
        return NULL;
    }
    
    // Maximum proof length is tree height
    size_t max_proof = 0;
    size_t temp = tree->leaf_count;
    while (temp > 1) {
        max_proof++;
        temp = (temp + 1) / 2;
    }
    
    uint32_t* proof = malloc(max_proof * sizeof(uint32_t));
    if (!proof) {
        *proof_length = 0;
        return NULL;
    }
    
    size_t proof_idx = 0;
    size_t node_idx = tree->leaf_count - 1 + chunk_index; // Leaf node index
    
    // Walk up the tree collecting sibling hashes
    while (node_idx > 1) {
        size_t sibling_idx = (node_idx % 2 == 0) ? node_idx + 1 : node_idx - 1;
        if (sibling_idx < tree->node_count) {
            proof[proof_idx++] = tree->nodes[sibling_idx];
        }
        node_idx /= 2;
    }
    
    *proof_length = proof_idx;
    return proof;
}

// Verify Merkle proof
int rgtp_verify_merkle_proof(uint32_t chunk_hash, const uint32_t* proof,
                            size_t proof_length, uint32_t root_hash) {
    uint32_t current_hash = chunk_hash;
    
    for (size_t i = 0; i < proof_length; i++) {
        // Combine with proof element (order matters for security)
        current_hash = current_hash ^ proof[i] ^ (uint32_t)i;
    }
    
    return current_hash == root_hash;
}

// Cleanup functions
void rgtp_free_merkle_tree(merkle_tree_t* tree) {
    if (tree) {
        free(tree->nodes);
        free(tree);
    }
}

void rgtp_free_merkle_proof(uint32_t* proof) {
    free(proof);
}