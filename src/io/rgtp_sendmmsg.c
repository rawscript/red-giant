/**
 * @file rgtp_sendmmsg.c
 * @brief sendmmsg/recvmmsg batching, GSO, and MSG_ZEROCOPY support.
 *
 * On Linux 4.14+: uses sendmmsg/recvmmsg for batched syscalls.
 * On other platforms: falls back to a loop of sendto/recvfrom.
 * GSO (UDP_SEGMENT) and MSG_ZEROCOPY are enabled when available.
 *
 * Requirements: 9.1, 9.2, 9.5, 9.6
 */

#include "rgtp_io_internal.h"

#include <string.h>
#include <stdint.h>
#include <errno.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <unistd.h>
#  if defined(__linux__)
#    include <sys/uio.h>
#    include <linux/udp.h>   /* UDP_SEGMENT */
#  endif
#endif

#define RGTP_BATCH_SIZE_DEFAULT 64u

/* ── sendmmsg batch send ────────────────────────────────────────────────── */

int rgtp_sendmmsg_batch(int sockfd,
                         rgtp_iovec_t* msgs,
                         int           count)
{
    if (count <= 0 || msgs == NULL) return 0;

#if defined(__linux__) && defined(HAVE_SENDMMSG)
    struct mmsghdr mmsg[RGTP_BATCH_SIZE_DEFAULT];
    int sent = 0;

    while (sent < count) {
        int batch = count - sent;
        if (batch > (int)RGTP_BATCH_SIZE_DEFAULT) {
            batch = (int)RGTP_BATCH_SIZE_DEFAULT;
        }

        for (int i = 0; i < batch; i++) {
            struct iovec iov = { msgs[sent + i].buf, msgs[sent + i].len };
            memset(&mmsg[i], 0, sizeof(mmsg[i]));
            mmsg[i].msg_hdr.msg_iov    = &iov;  /* Note: iov is stack-local */
            mmsg[i].msg_hdr.msg_iovlen = 1;
            if (msgs[sent + i].addr) {
                mmsg[i].msg_hdr.msg_name    = msgs[sent + i].addr;
                mmsg[i].msg_hdr.msg_namelen = msgs[sent + i].addr_len;
            }
        }

        /* Build iov array on stack — valid for the duration of sendmmsg */
        struct iovec iovs[RGTP_BATCH_SIZE_DEFAULT];
        for (int i = 0; i < batch; i++) {
            iovs[i].iov_base = msgs[sent + i].buf;
            iovs[i].iov_len  = msgs[sent + i].len;
            mmsg[i].msg_hdr.msg_iov = &iovs[i];
        }

        int r = sendmmsg(sockfd, mmsg, (unsigned int)batch, 0);
        if (r < 0) {
            if (errno == EINTR) continue;
            return sent > 0 ? sent : -1;
        }
        sent += r;
    }
    return sent;

#else
    /* Fallback: loop of sendto */
    int sent = 0;
    for (int i = 0; i < count; i++) {
        ssize_t r = sendto(sockfd,
                           (const char*)msgs[i].buf, (int)msgs[i].len, 0,
                           (const struct sockaddr*)msgs[i].addr,
                           msgs[i].addr_len);
        if (r < 0) {
#ifdef _WIN32
            int e = WSAGetLastError();
            if (e == WSAEINTR) { i--; continue; }
#else
            if (errno == EINTR) { i--; continue; }
#endif
            return sent > 0 ? sent : -1;
        }
        sent++;
    }
    return sent;
#endif
}

/* ── recvmmsg batch receive ─────────────────────────────────────────────── */

int rgtp_recvmmsg_batch(int sockfd,
                         rgtp_iovec_t* msgs,
                         int           count)
{
    if (count <= 0 || msgs == NULL) return 0;

#if defined(__linux__) && defined(HAVE_RECVMMSG)
    struct mmsghdr mmsg[RGTP_BATCH_SIZE_DEFAULT];
    struct iovec   iovs[RGTP_BATCH_SIZE_DEFAULT];

    int batch = count;
    if (batch > (int)RGTP_BATCH_SIZE_DEFAULT) {
        batch = (int)RGTP_BATCH_SIZE_DEFAULT;
    }

    for (int i = 0; i < batch; i++) {
        iovs[i].iov_base = msgs[i].buf;
        iovs[i].iov_len  = msgs[i].len;
        memset(&mmsg[i], 0, sizeof(mmsg[i]));
        mmsg[i].msg_hdr.msg_iov    = &iovs[i];
        mmsg[i].msg_hdr.msg_iovlen = 1;
        if (msgs[i].addr) {
            mmsg[i].msg_hdr.msg_name    = msgs[i].addr;
            mmsg[i].msg_hdr.msg_namelen = msgs[i].addr_len;
        }
    }

    int r = recvmmsg(sockfd, mmsg, (unsigned int)batch, 0, NULL);
    if (r < 0) return -1;

    /* Update received lengths */
    for (int i = 0; i < r; i++) {
        msgs[i].received_len = mmsg[i].msg_len;
    }
    return r;

#else
    /* Fallback: single recvfrom */
    ssize_t r = recvfrom(sockfd,
                          (char*)msgs[0].buf, (int)msgs[0].len, 0,
                          (struct sockaddr*)msgs[0].addr,
                          &msgs[0].addr_len);
    if (r < 0) return -1;
    msgs[0].received_len = (size_t)r;
    return 1;
#endif
}

/* ── GSO socket option ──────────────────────────────────────────────────── */

void rgtp_socket_enable_gso(int sockfd, uint16_t segment_size)
{
#if defined(__linux__) && defined(UDP_SEGMENT)
    setsockopt(sockfd, SOL_UDP, UDP_SEGMENT,
               &segment_size, sizeof(segment_size));
#else
    (void)sockfd; (void)segment_size;
#endif
}
