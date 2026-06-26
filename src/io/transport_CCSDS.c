/* transport_ccsds.c — RGTP native CCSDS Space Packet I/O backend
 * License: MIT
 *
 * Integrates into RGTP's I/O layer. When compiled, replaces or
 * supplements the UDP backend. Uses the CCSDS APID 0x3E0.
 *
 * All RGTP packets are mapped 1:1 into CCSDS Space Packets (Version-1).
 * Large RGTP packets (e.g., due to FEC or jumbo chunks) are fragmented
 * across multiple Space Packets using the Sequence Flags field.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/uio.h>          /* for writev/readv if needed */
#include <time.h>

/* Internal RGTP headers — these paths depend on your actual source layout */
#include "rgtp_internal.h"    /* defines rgtp_transport_ops_t, rgtp_socket_t etc. */
#include "rgtp.h"             /* public types */

/* ------------------------------------------------------------------ */
/* CCSDS constants                                                   */
/* ------------------------------------------------------------------ */
#define CCSDS_APID_RGTP          0x3E0   /* 11-bit Application ID */
#define CCSDS_VERSION            0       /* Version-1 */
#define CCSDS_TYPE_TLM           0       /* Telemetry (directionless) */
#define CCSDS_SEC_HDR_FLAG       0       /* No secondary header */
#define CCSDS_PRIMARY_HDR_LEN    6

#define CCSDS_SEQ_STANDALONE     0x03    /* Sequence Flags: 11 = unsegmented */
#define CCSDS_SEQ_FIRST          0x01
#define CCSDS_SEQ_CONTINUE       0x00
#define CCSDS_SEQ_LAST           0x02

/* Maximum Space Packet data field size.
 * Many CCSDS links support up to 8920 bytes (AOS), but we default to a
 * conservative 1024 to fit most UHF/S-band serial links.
 * Adjust to your link's MTU, or set dynamically via config.
 */
#ifndef CCSDS_MAX_FRAME_DATA
#define CCSDS_MAX_FRAME_DATA     1024
#endif

/* ------------------------------------------------------------------ */
/* Transport state                                                   */
/* ------------------------------------------------------------------ */
typedef struct {
    int fd;                        /* device file descriptor (serial, spw, etc.) */
    uint16_t seq_count;            /* 14-bit sequence counter */
    /* Reassembly buffer for incoming fragmented packets */
    uint8_t *reasm_buf;
    size_t reasm_len;
    size_t reasm_alloc;
    uint16_t reasm_expected_seq;
    int reasm_active;             /* 1 if we are reassembling */
} ccsds_ctx_t;

/* ------------------------------------------------------------------ */
/* CCSDS header builders and parsers                                  */
/* ------------------------------------------------------------------ */
static inline void
build_ccsds_hdr(uint8_t hdr[CCSDS_PRIMARY_HDR_LEN],
                uint16_t apid, uint8_t seq_flags,
                uint16_t seq_count, uint16_t data_len)
{
    /* data_len is the number of data octets minus 1 (CCSDS convention) */
    uint16_t ccsds_len = (data_len > 0) ? (data_len - 1) : 0;

    hdr[0] = (CCSDS_VERSION << 5) |
             (CCSDS_TYPE_TLM << 4) |
             (CCSDS_SEC_HDR_FLAG << 3) |
             ((apid >> 8) & 0x07);
    hdr[1] = (apid & 0xFF);
    hdr[2] = (seq_flags << 6) |
             ((seq_count >> 8) & 0x3F);
    hdr[3] = (seq_count & 0xFF);
    hdr[4] = (ccsds_len >> 8);
    hdr[5] = (ccsds_len & 0xFF);
}

static inline void
parse_ccsds_hdr(const uint8_t hdr[CCSDS_PRIMARY_HDR_LEN],
                uint16_t *apid, uint8_t *seq_flags,
                uint16_t *seq_count, uint16_t *data_len)
{
    *apid = ((hdr[0] & 0x07) << 8) | hdr[1];
    *seq_flags = (hdr[2] >> 6) & 0x03;
    *seq_count = ((hdr[2] & 0x3F) << 8) | hdr[3];
    uint16_t ccsds_len = (hdr[4] << 8) | hdr[5];
    *data_len = ccsds_len + 1;  /* actual payload length */
}

/* ------------------------------------------------------------------ */
/* Transport operations                                               */
/* ------------------------------------------------------------------ */

/* Open the CCSDS device. 'cfg' may contain a device path string. */
static int
ccsds_init(rgtp_socket_t *sock, const rgtp_config_t *cfg)
{
    /* cfg->transport.device should be set by the caller (e.g., "/dev/spw0")
     * This expects a new field in the config, or we can pass it externally.
     * For demonstration, we'll retrieve it from an environment variable or
     * a dedicated setter. Since we will provide a separate socket creation
     * function, we can just store the fd in the socket's transport_ctx.
     */
    (void)sock;
    (void)cfg;
    /* The actual opening is done in rgtp_socket_create_ccsds() below. */
    return 0;
}

static void
ccsds_destroy(rgtp_socket_t *sock)
{
    ccsds_ctx_t *ctx = (ccsds_ctx_t *)sock->transport_ctx;
    if (ctx) {
        if (ctx->fd >= 0) close(ctx->fd);
        free(ctx->reasm_buf);
        free(ctx);
        sock->transport_ctx = NULL;
    }
}

/* Send a raw RGTP packet. This may fragment across multiple CCSDS frames. */
static int
ccsds_send_packet(rgtp_socket_t *sock,
                  const void *rgpt_pkt, size_t len,
                  const struct sockaddr *dst, socklen_t addrlen)
{
    (void)dst;      /* no addressing in point-to-point CCSDS */
    (void)addrlen;
    ccsds_ctx_t *ctx = (ccsds_ctx_t *)sock->transport_ctx;

    const uint8_t *src = (const uint8_t *)rgpt_pkt;
    size_t remaining = len;
    uint8_t seq_flags;
    uint16_t chunk;

    if (remaining == 0) return 0;

    if (len <= CCSDS_MAX_FRAME_DATA) {
        seq_flags = CCSDS_SEQ_STANDALONE;
        chunk = len;
    } else {
        seq_flags = CCSDS_SEQ_FIRST;
        chunk = CCSDS_MAX_FRAME_DATA;
    }

    while (remaining > 0) {
        size_t this_chunk = (remaining > CCSDS_MAX_FRAME_DATA) ?
                            CCSDS_MAX_FRAME_DATA : remaining;

        uint8_t buf[CCSDS_PRIMARY_HDR_LEN + CCSDS_MAX_FRAME_DATA];
        build_ccsds_hdr(buf, CCSDS_APID_RGTP, seq_flags,
                        ctx->seq_count, (uint16_t)this_chunk);
        memcpy(buf + CCSDS_PRIMARY_HDR_LEN, src, this_chunk);

        ssize_t n = write(ctx->fd, buf, CCSDS_PRIMARY_HDR_LEN + this_chunk);
        if (n < 0) return -1;
        /* On short writes you may need to loop; omitted for brevity */

        remaining -= this_chunk;
        src += this_chunk;

        if (remaining == 0)
            seq_flags = CCSDS_SEQ_LAST;
        else
            seq_flags = CCSDS_SEQ_CONTINUE;
    }

    ctx->seq_count = (ctx->seq_count + 1) & 0x3FFF;
    return 0;
}

/* Receive one complete RGTP packet. May block.
 * Returns 0 and sets *len if a packet was reassembled.
 * Returns 1 if no packet available yet (should not happen in blocking mode)
 * Returns -1 on error.
 */
static int
ccsds_recv_packet(rgtp_socket_t *sock,
                  void *rgpt_buf, size_t *len,
                  struct sockaddr *src, socklen_t *addrlen)
{
    (void)src;
    (void)addrlen;
    ccsds_ctx_t *ctx = (ccsds_ctx_t *)sock->transport_ctx;

    uint8_t hdr[CCSDS_PRIMARY_HDR_LEN];
    ssize_t n = read(ctx->fd, hdr, CCSDS_PRIMARY_HDR_LEN);
    if (n != CCSDS_PRIMARY_HDR_LEN) {
        if (n == 0) return 1;   /* EOF, treat as no packet */
        return -1;
    }

    uint16_t apid, data_len;
    uint8_t seq_flags;
    uint16_t seq_count;
    parse_ccsds_hdr(hdr, &apid, &seq_flags, &seq_count, &data_len);

    /* Discard packets not belonging to RGTP */
    if (apid != CCSDS_APID_RGTP) {
        /* Skip over this packet's data */
        uint8_t dummy[CCSDS_MAX_FRAME_DATA];
        while (data_len > 0) {
            size_t skip = data_len > sizeof(dummy) ? sizeof(dummy) : data_len;
            if (read(ctx->fd, dummy, skip) != (ssize_t)skip) return -1;
            data_len -= skip;
        }
        return 1;   /* ignore and retry */
    }

    /* Read the payload */
    uint8_t payload[data_len];
    if (read(ctx->fd, payload, data_len) != (ssize_t)data_len)
        return -1;

    if (seq_flags == CCSDS_SEQ_STANDALONE) {
        if (data_len > *len) return -1; /* buffer too small */
        memcpy(rgpt_buf, payload, data_len);
        *len = data_len;
        return 0;
    }

    /* Fragment handling */
    if (!ctx->reasm_active) {
        /* Expecting first fragment */
        if (seq_flags != CCSDS_SEQ_FIRST) return -2;
        ctx->reasm_active = 1;
        ctx->reasm_len = 0;
        /* Ensure reassembly buffer large enough */
        if (ctx->reasm_alloc < data_len + 65536) {
            size_t new_alloc = data_len + 65536;
            uint8_t *tmp = realloc(ctx->reasm_buf, new_alloc);
            if (!tmp) return -1;
            ctx->reasm_buf = tmp;
            ctx->reasm_alloc = new_alloc;
        }
    } else {
        /* Check sequence continuity */
        if (seq_count != ctx->reasm_expected_seq)
            return -2;   /* missed a fragment */
    }

    /* Append payload */
    if (ctx->reasm_len + data_len > ctx->reasm_alloc) return -1;
    memcpy(ctx->reasm_buf + ctx->reasm_len, payload, data_len);
    ctx->reasm_len += data_len;
    ctx->reasm_expected_seq = (seq_count + 1) & 0x3FFF;

    if (seq_flags == CCSDS_SEQ_LAST) {
        /* Reassembly complete */
        if (ctx->reasm_len > *len) return -1;
        memcpy(rgpt_buf, ctx->reasm_buf, ctx->reasm_len);
        *len = ctx->reasm_len;
        ctx->reasm_active = 0;
        ctx->reasm_len = 0;
        return 0;
    } else {
        /* More fragments needed */
        return 1;   /* caller should loop */
    }
}

/* Transport operations vector */
const rgtp_transport_ops_t rgtp_transport_ccsds_ops = {
    .init           = ccsds_init,
    .destroy        = ccsds_destroy,
    .send_packet    = ccsds_send_packet,
    .recv_packet    = ccsds_recv_packet,
};

/* ------------------------------------------------------------------ */
/* Public API to create a CCSDS-bound socket                          */
/* ------------------------------------------------------------------ */
rgtp_socket_t *
rgtp_socket_create_ccsds(const char *device, const rgtp_config_t *cfg)
{
    ccsds_ctx_t *ctx = calloc(1, sizeof(ccsds_ctx_t));
    if (!ctx) return NULL;

    ctx->fd = open(device, O_RDWR | O_NOCTTY);
    if (ctx->fd < 0) {
        free(ctx);
        return NULL;
    }
    ctx->seq_count = 0;
    ctx->reasm_active = 0;
    ctx->reasm_buf = NULL;
    ctx->reasm_alloc = 0;

    /* The generic socket creation function needs modification to accept
     * a pre-defined transport ops and context. We'll assume rgtp_socket_t
     * has fields for ops and transport_ctx. Pseudocode:
     */
    rgtp_socket_t *sock = rgtp_socket_alloc();   /* internal alloc */
    if (!sock) {
        close(ctx->fd);
        free(ctx);
        return NULL;
    }
    sock->transport_ops = &rgtp_transport_ccsds_ops;
    sock->transport_ctx = ctx;

    /* Copy configuration */
    if (cfg) memcpy(&sock->config, cfg, sizeof(*cfg));

    /* Call generic init (sets up timers, buffers, etc.) */
    if (rgtp_socket_init(sock) != 0) {
        ccsds_destroy(sock);
        rgtp_socket_free(sock);
        return NULL;
    }

    return sock;
}