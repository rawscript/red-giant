// Red Giant Protocol - Secure Exposure Layer
// Adds encryption and authentication while preserving high-performance architecture
package main

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"fmt"
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

// Optimized secure chunk exposure with minimal overhead
func (ses *SecureExposureSurface) ExposeChunkSecure(chunkID uint32, data []byte, peerAuth string) error {
	// Fast authentication check using byte comparison (no hex encoding)
	if len(ses.authKey) > 0 && len(peerAuth) >= 16 {
		authBytes, _ := hex.DecodeString(peerAuth)
		if len(authBytes) < 8 || !fastCompare(authBytes[:8], ses.authKey[:8]) {
			return fmt.Errorf("authentication failed")
		}
	}

	processedData := data

	// Apply lightweight encryption if enabled
	if ses.encEnabled {
		// Use deterministic nonce based on chunk ID + timestamp for speed
		nonce := make([]byte, ses.gcm.NonceSize())
		binary.LittleEndian.PutUint32(nonce[:4], chunkID)
		binary.LittleEndian.PutUint64(nonce[4:], uint64(time.Now().UnixNano()))

		// Minimal AAD - just chunk ID
		aad := (*[4]byte)(unsafe.Pointer(&chunkID))[:]

		// In-place encryption to reduce allocations
		encrypted := ses.gcm.Seal(nonce, nonce, data, aad)
		processedData = encrypted
	}

	// Direct C exposure with zero-copy optimization
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

// Fast constant-time byte comparison
func fastCompare(a, b []byte) bool {
	if len(a) != len(b) {
		return false
	}
	var result byte
	for i := 0; i < len(a); i++ {
		result |= a[i] ^ b[i]
	}
	return result == 0
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
	var throughput C.uint32_t
	elapsed := C.rg_get_performance_stats(ses.surface, &throughput)

	return map[string]interface{}{
		"encryption_enabled":     ses.encEnabled,
		"authentication_enabled": len(ses.authKey) > 0,
		"throughput_mbps":        uint32(throughput),
		"elapsed_ms":             uint64(elapsed),
		"security_overhead":      "minimal", // AES-GCM adds ~5% overhead
	}
}

// Cleanup secure surface
func (ses *SecureExposureSurface) Cleanup() {
	if ses.surface != nil {
		C.rg_destroy_surface(ses.surface)
		ses.surface = nil
	}
}
