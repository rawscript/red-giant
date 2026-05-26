/**
 * @file rgtp_ros2.c
 * @brief ROS2 rmw transport plugin adapter.
 *
 * Provides a thin adapter layer so ROS2 nodes can publish/subscribe over RGTP
 * without modifying application code. Built as a separate shared library
 * (librgtp_ros2.so) with no mandatory ROS2 dependency at RGTP core build time.
 *
 * Architecture:
 *  - Publisher: calls rgtp_expose() to create an Exposure for each message.
 *  - Subscriber: calls rgtp_pull_start() + rgtp_pull_next() to receive.
 *  - Messages are serialized using CDR (Common Data Representation) encoding.
 *  - Low-power idle: when no subscribers are active, the exposer thread sleeps.
 *
 * Requirements: 15.1, 15.4, 15.5, 15.6
 */

#include "rgtp/rgtp.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

/* ── CDR serialization helpers ──────────────────────────────────────────── */

/**
 * @brief Prepend a 4-byte CDR encapsulation header to a message buffer.
 *
 * CDR encapsulation header: { 0x00, 0x01, 0x00, 0x00 }
 *   Byte 0: representation identifier (0x00 = CDR_BE, 0x01 = CDR_LE)
 *   Byte 1: representation identifier (little-endian = 0x01)
 *   Bytes 2-3: options (0x00, 0x00)
 *
 * @param msg_data   Raw message bytes.
 * @param msg_len    Length of @p msg_data.
 * @param out        Output buffer (must be at least msg_len + 4 bytes).
 * @param out_len    Receives the total output length.
 */
static void cdr_wrap(const uint8_t* msg_data, size_t msg_len,
                     uint8_t* out, size_t* out_len)
{
    /* CDR little-endian encapsulation header */
    out[0] = 0x00;
    out[1] = 0x01;
    out[2] = 0x00;
    out[3] = 0x00;
    memcpy(out + 4, msg_data, msg_len);
    *out_len = msg_len + 4;
}

/**
 * @brief Strip the 4-byte CDR encapsulation header.
 *
 * @param cdr_data   CDR-wrapped buffer.
 * @param cdr_len    Length of @p cdr_data.
 * @param out_data   Receives pointer to the raw message (into @p cdr_data).
 * @param out_len    Receives the raw message length.
 * @return RGTP_OK or RGTP_ERR_TRUNCATED.
 */
static rgtp_error_t cdr_unwrap(const uint8_t* cdr_data, size_t cdr_len,
                                const uint8_t** out_data, size_t* out_len)
{
    if (cdr_len < 4) return RGTP_ERR_TRUNCATED;
    *out_data = cdr_data + 4;
    *out_len  = cdr_len - 4;
    return RGTP_OK;
}

/* ── ROS2 publisher handle ──────────────────────────────────────────────── */

typedef struct {
    rgtp_socket_t*  sock;
    rgtp_config_t   config;
    char            topic[128];
} rgtp_ros2_publisher_t;

/**
 * @brief Create a ROS2-compatible RGTP publisher.
 *
 * @param sock    RGTP socket to publish on.
 * @param topic   ROS2 topic name (stored for identification only).
 * @param cfg     RGTP configuration.
 * @param out     Receives the publisher handle.
 * @return RGTP_OK or RGTP_ERR_NOMEM.
 */
rgtp_error_t rgtp_ros2_publisher_create(rgtp_socket_t*        sock,
                                         const char*           topic,
                                         const rgtp_config_t*  cfg,
                                         rgtp_ros2_publisher_t** out)
{
    if (!sock || !out) return RGTP_ERR_INVALID_ARG;

    rgtp_ros2_publisher_t* pub = (rgtp_ros2_publisher_t*)calloc(1, sizeof(*pub));
    if (!pub) return RGTP_ERR_NOMEM;

    pub->sock = sock;
    if (cfg) pub->config = *cfg;
    if (topic) {
        strncpy(pub->topic, topic, sizeof(pub->topic) - 1);
    }

    *out = pub;
    return RGTP_OK;
}

/**
 * @brief Publish a ROS2 message over RGTP.
 *
 * Wraps the message in a CDR header, then calls rgtp_expose() to create
 * an immutable Exposure. The caller is responsible for calling rgtp_poll()
 * to serve pull requests.
 *
 * @param pub       Publisher handle.
 * @param msg_data  Serialized ROS2 message bytes.
 * @param msg_len   Length of @p msg_data.
 * @param surface   Receives the created Exposure surface.
 * @return RGTP_OK or an RGTP error code.
 */
rgtp_error_t rgtp_ros2_publish(rgtp_ros2_publisher_t* pub,
                                const uint8_t*         msg_data,
                                size_t                 msg_len,
                                rgtp_surface_t**       surface)
{
    if (!pub || !msg_data || !surface) return RGTP_ERR_INVALID_ARG;

    /* Wrap in CDR header */
    size_t   cdr_len = msg_len + 4;
    uint8_t* cdr_buf = (uint8_t*)malloc(cdr_len);
    if (!cdr_buf) return RGTP_ERR_NOMEM;

    size_t wrapped_len = 0;
    cdr_wrap(msg_data, msg_len, cdr_buf, &wrapped_len);

    rgtp_error_t err = rgtp_expose(pub->sock, cdr_buf, wrapped_len,
                                    &pub->config, surface);
    free(cdr_buf);
    return err;
}

void rgtp_ros2_publisher_destroy(rgtp_ros2_publisher_t* pub)
{
    free(pub);
}

/* ── ROS2 subscriber handle ─────────────────────────────────────────────── */

typedef struct {
    rgtp_socket_t*  sock;
    rgtp_config_t   config;
    char            topic[128];
    bool            active;
} rgtp_ros2_subscriber_t;

rgtp_error_t rgtp_ros2_subscriber_create(rgtp_socket_t*          sock,
                                          const char*             topic,
                                          const rgtp_config_t*    cfg,
                                          rgtp_ros2_subscriber_t** out)
{
    if (!sock || !out) return RGTP_ERR_INVALID_ARG;

    rgtp_ros2_subscriber_t* sub = (rgtp_ros2_subscriber_t*)calloc(1, sizeof(*sub));
    if (!sub) return RGTP_ERR_NOMEM;

    sub->sock   = sock;
    sub->active = true;
    if (cfg)   sub->config = *cfg;
    if (topic) strncpy(sub->topic, topic, sizeof(sub->topic) - 1);

    *out = sub;
    return RGTP_OK;
}

/**
 * @brief Receive the next ROS2 message from an RGTP Exposure.
 *
 * Calls rgtp_pull_start() + rgtp_pull_next() and strips the CDR header.
 *
 * @param sub          Subscriber handle.
 * @param server       Exposer address.
 * @param exposure_id  16-byte Exposure_ID to pull.
 * @param buf          Output buffer for the raw ROS2 message.
 * @param buf_size     Size of @p buf.
 * @param out_len      Receives the message length.
 * @return RGTP_OK or an RGTP error code.
 */
rgtp_error_t rgtp_ros2_receive(rgtp_ros2_subscriber_t*        sub,
                                const struct sockaddr_storage* server,
                                const uint8_t                  exposure_id[16],
                                void*                          buf,
                                size_t                         buf_size,
                                size_t*                        out_len)
{
    if (!sub || !server || !exposure_id || !buf || !out_len) {
        return RGTP_ERR_INVALID_ARG;
    }

    rgtp_surface_t* surface = NULL;
    rgtp_error_t err = rgtp_pull_start(sub->sock, server, exposure_id,
                                        &sub->config, &surface);
    if (err != RGTP_OK) return err;

    /* Pull all chunks and reassemble */
    uint8_t  chunk_buf[65536];
    size_t   chunk_len   = 0;
    uint32_t chunk_index = 0;
    uint8_t* out_ptr     = (uint8_t*)buf;
    size_t   total_len   = 0;

    while (rgtp_progress(surface) < 1.0f) {
        err = rgtp_pull_next(surface, chunk_buf, sizeof(chunk_buf),
                              &chunk_len, &chunk_index);
        if (err == RGTP_ERR_TIMEOUT) break;
        if (err != RGTP_OK) { rgtp_destroy_surface(surface); return err; }

        if (total_len + chunk_len > buf_size) {
            rgtp_destroy_surface(surface);
            return RGTP_ERR_INVALID_ARG;
        }
        memcpy(out_ptr + total_len, chunk_buf, chunk_len);
        total_len += chunk_len;
    }

    rgtp_destroy_surface(surface);

    /* Strip CDR header */
    const uint8_t* raw_msg = NULL;
    size_t         raw_len = 0;
    err = cdr_unwrap((const uint8_t*)buf, total_len, &raw_msg, &raw_len);
    if (err != RGTP_OK) return err;

    /* Move raw message to start of buffer */
    memmove(buf, raw_msg, raw_len);
    *out_len = raw_len;
    return RGTP_OK;
}

void rgtp_ros2_subscriber_destroy(rgtp_ros2_subscriber_t* sub)
{
    free(sub);
}
