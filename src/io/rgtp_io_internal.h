/**
 * @file rgtp_io_internal.h
 * @brief Internal I/O layer declarations.
 */

#ifndef RGTP_IO_INTERNAL_H
#define RGTP_IO_INTERNAL_H

#include "rgtp/rgtp.h"
#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
   typedef int socklen_t;
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Message descriptor for batch send/receive. */
typedef struct {
    void*                   buf;          /**< Data buffer */
    size_t                  len;          /**< Buffer size (send) or capacity (recv) */
    size_t                  received_len; /**< Bytes received (filled by recvmmsg) */
    struct sockaddr_storage* addr;        /**< Peer address */
    socklen_t               addr_len;
} rgtp_iovec_t;

/**
 * @brief Send @p count messages in a single syscall (sendmmsg on Linux).
 * @return Number of messages sent, or -1 on error.
 */
int rgtp_sendmmsg_batch(int sockfd, rgtp_iovec_t* msgs, int count);

/**
 * @brief Receive up to @p count messages in a single syscall (recvmmsg on Linux).
 * @return Number of messages received, or -1 on error.
 */
int rgtp_recvmmsg_batch(int sockfd, rgtp_iovec_t* msgs, int count);

/** Enable UDP_SEGMENT (GSO) on @p sockfd with the given segment size. */
void rgtp_socket_enable_gso(int sockfd, uint16_t segment_size);

#ifdef __cplusplus
}
#endif

#endif /* RGTP_IO_INTERNAL_H */
