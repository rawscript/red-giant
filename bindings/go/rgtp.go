// Package rgtp provides Go bindings for the Red Giant Transport Protocol.
//
// All blocking operations accept a context.Context for cancellation.
// C memory is managed by the library; Go buffers are pinned for the
// duration of each call using runtime.Pinner.
//
// Requirements: 14.4, 14.5, 14.8, 23.5
package rgtp

/*
#cgo CFLAGS: -I../../include
#cgo LDFLAGS: -lrgtp -lsodium

#include "rgtp/rgtp.h"
#include <stdlib.h>
#include <string.h>
*/
import "C"

import (
	"context"
	"errors"
	"fmt"
	"net"
	"runtime"
	"unsafe"
)

// ── Error type ───────────────────────────────────────────────────────────

// Error wraps an RGTP error code.
type Error struct {
	Code    int
	Message string
}

func (e *Error) Error() string {
	return fmt.Sprintf("rgtp error %d: %s", e.Code, e.Message)
}

func rgtpErr(code C.rgtp_error_t) error {
	if code == C.RGTP_OK {
		return nil
	}
	msg := C.GoString(C.rgtp_strerror(code))
	return &Error{Code: int(code), Message: msg}
}

// ── Library lifecycle ────────────────────────────────────────────────────

// Init initialises the RGTP library. Must be called once before any other
// function. Safe to call multiple times (idempotent).
func Init() error {
	return rgtpErr(C.rgtp_init())
}

// Cleanup releases all global library resources.
func Cleanup() {
	C.rgtp_cleanup()
}

// Version returns the library version string (e.g. "1.0.0").
func Version() string {
	return C.GoString(C.rgtp_version())
}

// ── Socket ───────────────────────────────────────────────────────────────

// Socket wraps an rgtp_socket_t handle.
type Socket struct {
	ptr *C.rgtp_socket_t
}

// NewSocket creates and binds an RGTP UDP socket.
func NewSocket() (*Socket, error) {
	var ptr *C.rgtp_socket_t
	err := rgtpErr(C.rgtp_socket_create(nil, &ptr))
	if err != nil {
		return nil, err
	}
	s := &Socket{ptr: ptr}
	runtime.SetFinalizer(s, (*Socket).Close)
	return s, nil
}

// Close destroys the socket and releases all associated resources.
func (s *Socket) Close() {
	if s.ptr != nil {
		C.rgtp_socket_destroy(s.ptr)
		s.ptr = nil
	}
}

// ── Surface ──────────────────────────────────────────────────────────────

// Surface wraps an rgtp_surface_t handle (exposer or puller).
type Surface struct {
	ptr *C.rgtp_surface_t
}

// Close destroys the surface and securely zeroizes all key material.
func (s *Surface) Close() {
	if s.ptr != nil {
		C.rgtp_destroy_surface(s.ptr)
		s.ptr = nil
	}
}

// ExposureID returns the 16-byte Exposure_ID for this surface.
func (s *Surface) ExposureID() ([16]byte, error) {
	var id [16]byte
	err := rgtpErr(C.rgtp_get_exposure_id(s.ptr, (*C.uint8_t)(unsafe.Pointer(&id[0]))))
	return id, err
}

// Progress returns the transfer completion fraction [0.0, 1.0].
func (s *Surface) Progress() float32 {
	return float32(C.rgtp_progress(s.ptr))
}

// Stats returns transfer statistics for this surface.
func (s *Surface) Stats() (Stats, error) {
	var cs C.rgtp_stats_t
	err := rgtpErr(C.rgtp_get_stats(s.ptr, &cs))
	if err != nil {
		return Stats{}, err
	}
	return Stats{
		BytesSent:        uint64(cs.bytes_sent),
		BytesReceived:    uint64(cs.bytes_received),
		ChunksSent:       uint32(cs.chunks_sent),
		ChunksReceived:   uint32(cs.chunks_received),
		AuthFailures:     uint32(cs.auth_failures),
		MalformedPackets: uint32(cs.malformed_packets),
		PacketLossRate:   float32(cs.packet_loss_rate),
		RTTUs:            uint32(cs.rtt_us),
	}, nil
}

// Stats holds per-surface transfer statistics.
type Stats struct {
	BytesSent        uint64
	BytesReceived    uint64
	ChunksSent       uint32
	ChunksReceived   uint32
	AuthFailures     uint32
	MalformedPackets uint32
	PacketLossRate   float32
	RTTUs            uint32
}

// ── Exposer API ──────────────────────────────────────────────────────────

// Expose pre-encrypts data and creates an immutable Exposure.
// The returned Surface must be polled to serve pull requests.
func Expose(ctx context.Context, sock *Socket, data []byte) (*Surface, error) {
	if len(data) == 0 {
		return nil, errors.New("data must not be empty")
	}

	var pinner runtime.Pinner
	pinner.Pin(&data[0])
	defer pinner.Unpin()

	var ptr *C.rgtp_surface_t
	err := rgtpErr(C.rgtp_expose(
		sock.ptr,
		unsafe.Pointer(&data[0]),
		C.size_t(len(data)),
		nil,
		&ptr,
	))
	if err != nil {
		return nil, err
	}

	s := &Surface{ptr: ptr}
	runtime.SetFinalizer(s, (*Surface).Close)
	return s, nil
}

// Poll serves pending pull requests for an active Exposure.
// Returns when the context is cancelled or the timeout elapses.
func Poll(ctx context.Context, surface *Surface, timeoutMs int) error {
	select {
	case <-ctx.Done():
		return ctx.Err()
	default:
	}
	return rgtpErr(C.rgtp_poll(surface.ptr, C.int(timeoutMs)))
}

// ── Puller API ───────────────────────────────────────────────────────────

// PullStart begins pulling an Exposure from a remote Exposer.
func PullStart(ctx context.Context, sock *Socket, server net.Addr,
	exposureID [16]byte) (*Surface, error) {

	select {
	case <-ctx.Done():
		return nil, ctx.Err()
	default:
	}

	// Resolve server address to sockaddr_storage
	udpAddr, ok := server.(*net.UDPAddr)
	if !ok {
		return nil, errors.New("server must be a *net.UDPAddr")
	}

	var ss C.struct_sockaddr_storage
	if ip4 := udpAddr.IP.To4(); ip4 != nil {
		sa := (*C.struct_sockaddr_in)(unsafe.Pointer(&ss))
		sa.sin_family = C.AF_INET
		sa.sin_port = C.uint16_t(udpAddr.Port<<8 | udpAddr.Port>>8) // htons
		copy((*[4]byte)(unsafe.Pointer(&sa.sin_addr))[:], ip4)
	} else {
		return nil, errors.New("IPv6 not yet implemented in Go binding")
	}

	var ptr *C.rgtp_surface_t
	err := rgtpErr(C.rgtp_pull_start(
		sock.ptr,
		&ss,
		(*C.uint8_t)(unsafe.Pointer(&exposureID[0])),
		nil,
		&ptr,
	))
	if err != nil {
		return nil, err
	}

	s := &Surface{ptr: ptr}
	runtime.SetFinalizer(s, (*Surface).Close)
	return s, nil
}

// ChunkResult holds the result of a PullNext call.
type ChunkResult struct {
	Data       []byte
	ChunkIndex uint32
}

// PullNext receives the next available chunk.
// Returns context.Canceled if ctx is cancelled.
func PullNext(ctx context.Context, surface *Surface, bufSize int) (ChunkResult, error) {
	select {
	case <-ctx.Done():
		return ChunkResult{}, ctx.Err()
	default:
	}

	if bufSize <= 0 {
		bufSize = 65536
	}

	buf := make([]byte, bufSize)
	var received C.size_t
	var chunkIndex C.uint32_t

	var pinner runtime.Pinner
	pinner.Pin(&buf[0])
	defer pinner.Unpin()

	err := rgtpErr(C.rgtp_pull_next(
		surface.ptr,
		unsafe.Pointer(&buf[0]),
		C.size_t(bufSize),
		&received,
		&chunkIndex,
	))
	if err != nil {
		return ChunkResult{}, err
	}

	return ChunkResult{
		Data:       buf[:int(received)],
		ChunkIndex: uint32(chunkIndex),
	}, nil
}
