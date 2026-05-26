/**
 * @file rgtp_wire_internal.h
 * @brief Internal declarations for the RGTP wire protocol layer.
 */

#ifndef RGTP_WIRE_INTERNAL_H
#define RGTP_WIRE_INTERNAL_H

#include "rgtp/rgtp.h"
#include "rgtp_packet_types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Serialize any valid rgtp_packet_t into a byte buffer.
 *
 * All multi-byte integers are written in big-endian (network) byte order.
 *
 * @param pkt       Packet to serialize.
 * @param buf       Output buffer.
 * @param buf_size  Size of @p buf in bytes.
 * @param out_len   Receives the number of bytes written.
 * @return RGTP_OK or RGTP_ERR_INVALID_ARG (buffer too small / unknown type).
 */
rgtp_error_t rgtp_serialize_packet(const rgtp_packet_t* pkt,
                                    uint8_t*             buf,
                                    size_t               buf_size,
                                    size_t*              out_len);

/**
 * @brief Parse a byte buffer into a typed rgtp_packet_t.
 *
 * Non-allocating: writes into the caller-supplied @p out on the stack.
 * Pointer fields (payload, proof, chunk_indices) point into @p buf — the
 * caller must keep @p buf alive as long as @p out is used.
 *
 * @param buf                 Received byte buffer.
 * @param len                 Number of bytes in @p buf.
 * @param manifest_chunk_count  Chunk count from the most recently received
 *                            Manifest (used to validate chunk indices).
 *                            Pass UINT32_MAX if no Manifest has been received.
 * @param out                 Caller-supplied packet struct to populate.
 * @return RGTP_OK on success, or one of:
 *   RGTP_ERR_TRUNCATED       — declared Length > len
 *   RGTP_ERR_INVALID_ARG     — unknown packet type or invalid field value
 *   RGTP_ERR_CHUNK_INDEX_OOB — chunk_index >= manifest_chunk_count
 *   RGTP_ERR_NOT_SUPPORTED   — unsupported protocol version
 */
rgtp_error_t rgtp_parse_packet(const uint8_t* buf,
                                size_t         len,
                                uint32_t       manifest_chunk_count,
                                rgtp_packet_t* out);

#ifdef __cplusplus
}
#endif

#endif /* RGTP_WIRE_INTERNAL_H */
