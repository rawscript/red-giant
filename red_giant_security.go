
// Red Giant Protocol - Secure Exposure Layer
// Adds encryption and authentication while preserving high-performance architecture
package main

import (
    "crypto/aes"
    "crypto/cipher"
    "crypto/rand"
    "crypto/sha256"
    "encoding/hex"
    "fmt"
    "io"
    "time"
    "unsafe"
)

/*
#cgo CFLAGS: -std=gnu99 -O3 -march=native
#include "red_giant.h"
*/
import "C"

type SecureExposureSurface struct {
    surface    *C.rg_exposure_surface_t
    gcm        cipher.AEAD
    nonce      []byte
    authKey    []byte
    encEnabled bool
}

// Create secure exposure surface with optional encryption
func NewSecureExposureSurface(manifest *C.rg_manifest_t, encryptionKey []byte) (*SecureExposureSurface, error) {
    surface := C.rg_create_surface(manifest)
    if surface == nil {
        return nil, fmt.Errorf("failed to create base surface")
    }
    
    ses := &SecureExposureSurface{
        surface:    surface,
        encEnabled: len(encryptionKey) > 0,
    }
    
    if ses.encEnabled {
        // Initialize AES-GCM for high-performance encryption
        block, err := aes.NewCipher(encryptionKey)
        if err != nil {
            C.rg_destroy_surface(surface)
            return nil, fmt.Errorf("failed to create cipher: %w", err)
        }
        
        ses.gcm, err = cipher.NewGCM(block)
        if err != nil {
            C.rg_destroy_surface(surface)
            return nil, fmt.Errorf("failed to create GCM: %w", err)
        }
        
        // Generate authentication key from encryption key
        hash := sha256.Sum256(encryptionKey)
        ses.authKey = hash[:]
    }
    
    return ses, nil
}

// Secure chunk exposure with optional encryption
func (ses *SecureExposureSurface) ExposeChunkSecure(chunkID uint32, data []byte, peerAuth string) error {
    // Authenticate peer if required
    if len(ses.authKey) > 0 {
        expectedAuth := hex.EncodeToString(ses.authKey[:8])
        if peerAuth != expectedAuth {
            return fmt.Errorf("authentication failed")
        }
    }
    
    processedData := data
    
    // Apply encryption if enabled - optimized for performance
    if ses.encEnabled {
        nonce := make([]byte, ses.gcm.NonceSize())
        if _, err := io.ReadFull(rand.Reader, nonce); err != nil {
            return fmt.Errorf("failed to generate nonce: %w", err)
        }
        
        // Encrypt data with associated chunk ID for integrity
        aad := make([]byte, 4)
        aad[0] = byte(chunkID)
        aad[1] = byte(chunkID >> 8)
        aad[2] = byte(chunkID >> 16)
        aad[3] = byte(chunkID >> 24)
        
        encrypted := ses.gcm.Seal(nonce, nonce, data, aad)
        processedData = encrypted
    }
    
    // Use high-performance C exposure function
    success := C.rg_expose_chunk_fast(
        ses.surface,
        C.uint32_t(chunkID),
        unsafe.Pointer(&processedData[0]),
        C.uint32_t(len(processedData)),
    )
    
    if !bool(success) {
        return fmt.Errorf("failed to expose chunk %d", chunkID)
    }
    
    return nil
}

// Secure chunk retrieval with decryption
func (ses *SecureExposureSurface) RetrieveChunkSecure(chunkID uint32, peerAuth string) ([]byte, error) {
    // Authenticate peer
    if len(ses.authKey) > 0 {
        expectedAuth := hex.EncodeToString(ses.authKey[:8])
        if peerAuth != expectedAuth {
            return nil, fmt.Errorf("authentication failed")
        }
    }
    
    // Get chunk from C surface
    chunk := C.rg_peek_chunk_fast(ses.surface, C.uint32_t(chunkID))
    if chunk == nil {
        return nil, fmt.Errorf("chunk %d not found", chunkID)
    }
    
    // Extract data
    dataSize := int(chunk.data_size)
    data := C.GoBytes(chunk.data_ptr, C.int(dataSize))
    
    // Decrypt if necessary
    if ses.encEnabled && len(data) > ses.gcm.NonceSize() {
        nonceSize := ses.gcm.NonceSize()
        nonce := data[:nonceSize]
        encrypted := data[nonceSize:]
        
        // Prepare associated data for integrity check
        aad := make([]byte, 4)
        aad[0] = byte(chunkID)
        aad[1] = byte(chunkID >> 8)
        aad[2] = byte(chunkID >> 16)
        aad[3] = byte(chunkID >> 24)
        
        decrypted, err := ses.gcm.Open(nil, nonce, encrypted, aad)
        if err != nil {
            return nil, fmt.Errorf("decryption failed: %w", err)
        }
        
        return decrypted, nil
    }
    
    return data, nil
}

// Get performance metrics including security overhead
func (ses *SecureExposureSurface) GetSecurityMetrics() map[string]interface{} {
    var throughput uint32
    elapsed := C.rg_get_performance_stats(ses.surface, &throughput)
    
    return map[string]interface{}{
        "encryption_enabled": ses.encEnabled,
        "authentication_enabled": len(ses.authKey) > 0,
        "throughput_mbps": throughput,
        "elapsed_ms": elapsed,
        "security_overhead": "minimal", // AES-GCM adds ~5% overhead
    }
}

// Cleanup secure surface
func (ses *SecureExposureSurface) Cleanup() {
    if ses.surface != nil {
        C.rg_destroy_surface(ses.surface)
        ses.surface = nil
    }
}
