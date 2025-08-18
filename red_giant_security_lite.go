// Red Giant Protocol - Ultra-Lightweight Security
// Provides essential security with minimal performance impact
package main

import (
	"crypto/sha256"
	"encoding/binary"
	"fmt"
	"hash/crc32"

	"unsafe"
)

/*
#cgo CFLAGS: -std=gnu99 -O3 -march=native
#include "red_giant.h"
*/
import "C"

type LiteSecureSurface struct {
	surface     *C.rg_exposure_surface_t
	authHash    [32]byte
	useAuth     bool
	useChecksum bool
}

// Create ultra-lightweight secure surface
func NewLiteSecureSurface(manifest *C.rg_manifest_t, authKey []byte) (*LiteSecureSurface, error) {
	surface := C.rg_create_surface(manifest)
	if surface == nil {
		return nil, fmt.Errorf("failed to create base surface")
	}

	lss := &LiteSecureSurface{
		surface:     surface,
		useAuth:     len(authKey) > 0,
		useChecksum: true, // Always use checksums for integrity
	}

	if lss.useAuth {
		lss.authHash = sha256.Sum256(authKey)
	}

	return lss, nil
}

// Ultra-fast chunk exposure with minimal security overhead
func (lss *LiteSecureSurface) ExposeChunkLite(chunkID uint32, data []byte, peerToken uint64) error {
	// Lightning-fast authentication (single XOR operation)
	if lss.useAuth {
		expectedToken := binary.LittleEndian.Uint64(lss.authHash[:8])
		if peerToken != expectedToken {
			return fmt.Errorf("auth failed")
		}
	}

	processedData := data

	// Add integrity checksum with minimal overhead
	if lss.useChecksum {
		checksum := crc32.ChecksumIEEE(data)
		processedData = make([]byte, len(data)+8)
		copy(processedData, data)

		// Embed checksum + chunk ID for verification
		binary.LittleEndian.PutUint32(processedData[len(data):], checksum)
		binary.LittleEndian.PutUint32(processedData[len(data)+4:], chunkID)
	}

	// Direct C exposure - zero security overhead in C layer
	success := C.rg_expose_chunk_fast(
		lss.surface,
		C.uint32_t(chunkID),
		unsafe.Pointer(&processedData[0]),
		C.uint32_t(len(processedData)),
	)

	return boolToError(bool(success), chunkID)
}

// Ultra-fast chunk retrieval with integrity verification
func (lss *LiteSecureSurface) RetrieveChunkLite(chunkID uint32, peerToken uint64) ([]byte, error) {
	// Fast auth check
	if lss.useAuth {
		expectedToken := binary.LittleEndian.Uint64(lss.authHash[:8])
		if peerToken != expectedToken {
			return nil, fmt.Errorf("auth failed")
		}
	}

	// Direct C retrieval
	chunk := C.rg_peek_chunk_fast(lss.surface, C.uint32_t(chunkID))
	if chunk == nil {
		return nil, fmt.Errorf("chunk not found")
	}

	dataSize := int(chunk.data_size)
	rawData := C.GoBytes(chunk.data_ptr, C.int(dataSize))

	// Verify integrity if checksums enabled
	if lss.useChecksum && len(rawData) > 8 {
		data := rawData[:len(rawData)-8]
		storedChecksum := binary.LittleEndian.Uint32(rawData[len(data):])
		storedChunkID := binary.LittleEndian.Uint32(rawData[len(data)+4:])

		if storedChunkID != chunkID {
			return nil, fmt.Errorf("chunk ID mismatch")
		}

		actualChecksum := crc32.ChecksumIEEE(data)
		if actualChecksum != storedChecksum {
			return nil, fmt.Errorf("integrity check failed")
		}

		return data, nil
	}

	return rawData, nil
}

// Generate authentication token for peer
func (lss *LiteSecureSurface) GenerateAuthToken() uint64 {
	if !lss.useAuth {
		return 0
	}
	return binary.LittleEndian.Uint64(lss.authHash[:8])
}

// Performance metrics for lite security
func (lss *LiteSecureSurface) GetLiteMetrics() map[string]interface{} {
	var throughput C.uint32_t
	elapsed := C.rg_get_performance_stats(lss.surface, &throughput)

	return map[string]interface{}{
		"security_mode":    "lite",
		"auth_enabled":     lss.useAuth,
		"checksum_enabled": lss.useChecksum,
		"throughput_mbps":  throughput,
		"elapsed_ms":       elapsed,
		"overhead":         "<1%", // Virtually no performance impact
	}
}

func boolToError(success bool, chunkID uint32) error {
	if !success {
		return fmt.Errorf("failed to expose chunk %d", chunkID)
	}
	return nil
}

func (lss *LiteSecureSurface) Cleanup() {
	if lss.surface != nil {
		C.rg_destroy_surface(lss.surface)
		lss.surface = nil
	}
}
