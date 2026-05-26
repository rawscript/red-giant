/**
 * @file rgtp_dds.c
 * @brief DDS/RTPS adapter — maps DDS participants to RGTP Exposures.
 *
 * Enables DDS participants to discover and communicate via RGTP by mapping
 * RTPS (Real-Time Publish-Subscribe) wire protocol messages to RGTP Exposures.
 *
 * Architecture:
 *  - Each DDS DataWriter maps to an RGTP Exposer.
 *  - Each DDS DataReader maps to an RGTP Puller.
 *  - RTPS messages are encapsulated as RGTP chunk payloads.
 *  - DDS discovery (SPDP/SEDP) is handled via a dedicated discovery Exposure.
 *
 * Built as a separate shared library (librgtp_dds.so).
 *
 * Requirements: 15.2, 15.4
 */

#include "rgtp/rgtp.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ── RTPS encapsulation ─────────────────────────────────────────────────── */

/* RTPS protocol header: "RTPS" magic + version + vendor ID + GUID prefix */
#define RTPS_MAGIC      "RTPS"
#define RTPS_MAGIC_LEN  4u
#define RTPS_HDR_LEN    20u   /* magic(4) + version(2) + vendor(2) + guid_prefix(12) */

static bool is_rtps_packet(const uint8_t* data, size_t len)
{
    if (len < RTPS_MAGIC_LEN) return false;
    return memcmp(data, RTPS_MAGIC, RTPS_MAGIC_LEN) == 0;
}

/* ── DDS DataWriter (maps to RGTP Exposer) ──────────────────────────────── */

typedef struct {
    rgtp_socket_t*  sock;
    rgtp_config_t   config;
    uint8_t         guid_prefix[12];   /**< RTPS GUID prefix */
    uint32_t        entity_id;         /**< RTPS entity ID */
} rgtp_dds_writer_t;

rgtp_error_t rgtp_dds_writer_create(rgtp_socket_t*       sock,
                                     const uint8_t        guid_prefix[12],
                                     uint32_t             entity_id,
                                     const rgtp_config_t* cfg,
                                     rgtp_dds_writer_t**  out)
{
    if (!sock || !out) return RGTP_ERR_INVALID_ARG;

    rgtp_dds_writer_t* w = (rgtp_dds_writer_t*)calloc(1, sizeof(*w));
    if (!w) return RGTP_ERR_NOMEM;

    w->sock      = sock;
    w->entity_id = entity_id;
    if (cfg)         w->config = *cfg;
    if (guid_prefix) memcpy(w->guid_prefix, guid_prefix, 12);

    *out = w;
    return RGTP_OK;
}

/**
 * @brief Write a DDS sample over RGTP.
 *
 * Encapsulates the RTPS DATA submessage as an RGTP Exposure.
 *
 * @param writer   DDS writer handle.
 * @param data     Serialized DDS sample (RTPS DATA submessage payload).
 * @param len      Length of @p data.
 * @param surface  Receives the created Exposure surface.
 */
rgtp_error_t rgtp_dds_write(rgtp_dds_writer_t* writer,
                              const void*        data,
                              size_t             len,
                              rgtp_surface_t**   surface)
{
    if (!writer || !data || !surface) return RGTP_ERR_INVALID_ARG;
    return rgtp_expose(writer->sock, data, len, &writer->config, surface);
}

void rgtp_dds_writer_destroy(rgtp_dds_writer_t* w) { free(w); }

/* ── DDS DataReader (maps to RGTP Puller) ───────────────────────────────── */

typedef struct {
    rgtp_socket_t*  sock;
    rgtp_config_t   config;
    uint8_t         guid_prefix[12];
    uint32_t        entity_id;
} rgtp_dds_reader_t;

rgtp_error_t rgtp_dds_reader_create(rgtp_socket_t*       sock,
                                     const uint8_t        guid_prefix[12],
                                     uint32_t             entity_id,
                                     const rgtp_config_t* cfg,
                                     rgtp_dds_reader_t**  out)
{
    if (!sock || !out) return RGTP_ERR_INVALID_ARG;

    rgtp_dds_reader_t* r = (rgtp_dds_reader_t*)calloc(1, sizeof(*r));
    if (!r) return RGTP_ERR_NOMEM;

    r->sock      = sock;
    r->entity_id = entity_id;
    if (cfg)         r->config = *cfg;
    if (guid_prefix) memcpy(r->guid_prefix, guid_prefix, 12);

    *out = r;
    return RGTP_OK;
}

/**
 * @brief Read the next DDS sample from an RGTP Exposure.
 *
 * @param reader       DDS reader handle.
 * @param server       Exposer address.
 * @param exposure_id  16-byte Exposure_ID.
 * @param buf          Output buffer.
 * @param buf_size     Size of @p buf.
 * @param out_len      Receives the sample length.
 */
rgtp_error_t rgtp_dds_read(rgtp_dds_reader_t*             reader,
                             const struct sockaddr_storage* server,
                             const uint8_t                  exposure_id[16],
                             void*                          buf,
                             size_t                         buf_size,
                             size_t*                        out_len)
{
    if (!reader || !server || !exposure_id || !buf || !out_len) {
        return RGTP_ERR_INVALID_ARG;
    }

    rgtp_surface_t* surface = NULL;
    rgtp_error_t err = rgtp_pull_start(reader->sock, server, exposure_id,
                                        &reader->config, &surface);
    if (err != RGTP_OK) return err;

    uint8_t  chunk_buf[65536];
    size_t   chunk_len = 0;
    uint32_t chunk_idx = 0;
    uint8_t* out_ptr   = (uint8_t*)buf;
    size_t   total     = 0;

    while (rgtp_progress(surface) < 1.0f) {
        err = rgtp_pull_next(surface, chunk_buf, sizeof(chunk_buf),
                              &chunk_len, &chunk_idx);
        if (err == RGTP_ERR_TIMEOUT) break;
        if (err != RGTP_OK) { rgtp_destroy_surface(surface); return err; }
        if (total + chunk_len > buf_size) {
            rgtp_destroy_surface(surface);
            return RGTP_ERR_INVALID_ARG;
        }
        memcpy(out_ptr + total, chunk_buf, chunk_len);
        total += chunk_len;
    }

    rgtp_destroy_surface(surface);
    *out_len = total;
    return RGTP_OK;
}

void rgtp_dds_reader_destroy(rgtp_dds_reader_t* r) { free(r); }
