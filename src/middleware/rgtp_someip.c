/**
 * @file rgtp_someip.c
 * @brief SOME/IP service discovery adapter.
 *
 * Maps SOME/IP service instances to RGTP Exposures, enabling SOME/IP clients
 * to pull service data via RGTP. Built as a separate shared library
 * (librgtp_someip.so).
 *
 * SOME/IP message header (16 bytes):
 *   Service_ID (2) | Method_ID (2) | Length (4) | Client_ID (2) |
 *   Session_ID (2) | Protocol_Version (1) | Interface_Version (1) |
 *   Message_Type (1) | Return_Code (1)
 *
 * Requirements: 15.3, 15.4
 */

#include "rgtp/rgtp.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* ── SOME/IP header constants ───────────────────────────────────────────── */

#define SOMEIP_HDR_LEN          16u
#define SOMEIP_PROTOCOL_VERSION 0x01u
#define SOMEIP_MSG_REQUEST      0x00u
#define SOMEIP_MSG_RESPONSE     0x80u
#define SOMEIP_MSG_NOTIFICATION 0x02u
#define SOMEIP_RETURN_OK        0x00u

/* ── SOME/IP header serialization ───────────────────────────────────────── */

static void someip_write_header(uint8_t*  hdr,
                                 uint16_t  service_id,
                                 uint16_t  method_id,
                                 uint32_t  payload_len,
                                 uint16_t  client_id,
                                 uint16_t  session_id,
                                 uint8_t   iface_version,
                                 uint8_t   msg_type,
                                 uint8_t   return_code)
{
    /* Big-endian encoding */
    hdr[0]  = (uint8_t)(service_id >> 8);
    hdr[1]  = (uint8_t)(service_id & 0xFF);
    hdr[2]  = (uint8_t)(method_id >> 8);
    hdr[3]  = (uint8_t)(method_id & 0xFF);
    uint32_t length = payload_len + 8u;   /* length field includes bytes 8-15 */
    hdr[4]  = (uint8_t)(length >> 24);
    hdr[5]  = (uint8_t)(length >> 16);
    hdr[6]  = (uint8_t)(length >>  8);
    hdr[7]  = (uint8_t)(length & 0xFF);
    hdr[8]  = (uint8_t)(client_id >> 8);
    hdr[9]  = (uint8_t)(client_id & 0xFF);
    hdr[10] = (uint8_t)(session_id >> 8);
    hdr[11] = (uint8_t)(session_id & 0xFF);
    hdr[12] = SOMEIP_PROTOCOL_VERSION;
    hdr[13] = iface_version;
    hdr[14] = msg_type;
    hdr[15] = return_code;
}

/* ── SOME/IP service instance ───────────────────────────────────────────── */

typedef struct {
    rgtp_socket_t*  sock;
    rgtp_config_t   config;
    uint16_t        service_id;
    uint16_t        instance_id;
    uint8_t         iface_version;
    uint16_t        session_counter;
} rgtp_someip_service_t;

/**
 * @brief Register a SOME/IP service instance backed by RGTP.
 */
rgtp_error_t rgtp_someip_service_create(rgtp_socket_t*          sock,
                                         uint16_t                service_id,
                                         uint16_t                instance_id,
                                         uint8_t                 iface_version,
                                         const rgtp_config_t*    cfg,
                                         rgtp_someip_service_t** out)
{
    if (!sock || !out) return RGTP_ERR_INVALID_ARG;

    rgtp_someip_service_t* svc = (rgtp_someip_service_t*)calloc(1, sizeof(*svc));
    if (!svc) return RGTP_ERR_NOMEM;

    svc->sock            = sock;
    svc->service_id      = service_id;
    svc->instance_id     = instance_id;
    svc->iface_version   = iface_version;
    svc->session_counter = 1;
    if (cfg) svc->config = *cfg;

    *out = svc;
    return RGTP_OK;
}

/**
 * @brief Send a SOME/IP notification (event) over RGTP.
 *
 * Prepends the SOME/IP header to the payload and exposes it as an RGTP
 * Exposure. Subscribers pull the Exposure to receive the notification.
 *
 * @param svc       Service instance handle.
 * @param method_id SOME/IP method/event ID.
 * @param payload   Event payload bytes.
 * @param len       Payload length.
 * @param surface   Receives the created Exposure surface.
 */
rgtp_error_t rgtp_someip_notify(rgtp_someip_service_t* svc,
                                 uint16_t               method_id,
                                 const uint8_t*         payload,
                                 size_t                 len,
                                 rgtp_surface_t**       surface)
{
    if (!svc || !payload || !surface) return RGTP_ERR_INVALID_ARG;

    size_t   total = SOMEIP_HDR_LEN + len;
    uint8_t* buf   = (uint8_t*)malloc(total);
    if (!buf) return RGTP_ERR_NOMEM;

    someip_write_header(buf,
                        svc->service_id, method_id,
                        (uint32_t)len,
                        0x0000,                  /* client_id = 0 for notifications */
                        svc->session_counter++,
                        svc->iface_version,
                        SOMEIP_MSG_NOTIFICATION,
                        SOMEIP_RETURN_OK);
    memcpy(buf + SOMEIP_HDR_LEN, payload, len);

    rgtp_error_t err = rgtp_expose(svc->sock, buf, total, &svc->config, surface);
    free(buf);
    return err;
}

/**
 * @brief Receive a SOME/IP notification from an RGTP Exposure.
 *
 * Pulls the Exposure and strips the SOME/IP header.
 *
 * @param sock         RGTP socket.
 * @param server       Exposer address.
 * @param exposure_id  16-byte Exposure_ID.
 * @param cfg          RGTP configuration.
 * @param payload_out  Output buffer for the SOME/IP payload.
 * @param buf_size     Size of @p payload_out.
 * @param out_len      Receives the payload length.
 * @param service_id   Receives the SOME/IP service ID.
 * @param method_id    Receives the SOME/IP method/event ID.
 */
rgtp_error_t rgtp_someip_receive(rgtp_socket_t*                 sock,
                                  const struct sockaddr_storage* server,
                                  const uint8_t                  exposure_id[16],
                                  const rgtp_config_t*           cfg,
                                  void*                          payload_out,
                                  size_t                         buf_size,
                                  size_t*                        out_len,
                                  uint16_t*                      service_id,
                                  uint16_t*                      method_id)
{
    if (!sock || !server || !exposure_id || !payload_out || !out_len) {
        return RGTP_ERR_INVALID_ARG;
    }

    rgtp_surface_t* surface = NULL;
    rgtp_error_t err = rgtp_pull_start(sock, server, exposure_id, cfg, &surface);
    if (err != RGTP_OK) return err;

    /* Reassemble all chunks */
    uint8_t* full_buf  = (uint8_t*)malloc(buf_size + SOMEIP_HDR_LEN);
    if (!full_buf) { rgtp_destroy_surface(surface); return RGTP_ERR_NOMEM; }

    uint8_t  chunk_buf[65536];
    size_t   chunk_len = 0;
    uint32_t chunk_idx = 0;
    size_t   total     = 0;

    while (rgtp_progress(surface) < 1.0f) {
        err = rgtp_pull_next(surface, chunk_buf, sizeof(chunk_buf),
                              &chunk_len, &chunk_idx);
        if (err == RGTP_ERR_TIMEOUT) break;
        if (err != RGTP_OK) {
            free(full_buf);
            rgtp_destroy_surface(surface);
            return err;
        }
        if (total + chunk_len > buf_size + SOMEIP_HDR_LEN) {
            free(full_buf);
            rgtp_destroy_surface(surface);
            return RGTP_ERR_INVALID_ARG;
        }
        memcpy(full_buf + total, chunk_buf, chunk_len);
        total += chunk_len;
    }
    rgtp_destroy_surface(surface);

    if (total < SOMEIP_HDR_LEN) {
        free(full_buf);
        return RGTP_ERR_TRUNCATED;
    }

    /* Parse SOME/IP header */
    if (service_id) *service_id = (uint16_t)((full_buf[0] << 8) | full_buf[1]);
    if (method_id)  *method_id  = (uint16_t)((full_buf[2] << 8) | full_buf[3]);

    size_t payload_len = total - SOMEIP_HDR_LEN;
    memcpy(payload_out, full_buf + SOMEIP_HDR_LEN, payload_len);
    *out_len = payload_len;

    free(full_buf);
    return RGTP_OK;
}

void rgtp_someip_service_destroy(rgtp_someip_service_t* svc) { free(svc); }
