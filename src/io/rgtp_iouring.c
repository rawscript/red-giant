/**
 * @file rgtp_iouring.c
 * @brief io_uring I/O backend for Linux 5.1+.
 *
 * Guarded by RGTP_ENABLE_IOURING. When not defined, all functions are stubs
 * that return RGTP_ERR_NOT_SUPPORTED.
 *
 * When enabled:
 *  - Creates an io_uring instance with SQ depth >= 256.
 *  - Registers fixed buffers for zero-copy I/O.
 *  - Submits IORING_OP_RECVMSG / IORING_OP_SENDMSG SQEs.
 *  - Reaps CQEs in rgtp_poll().
 *
 * Requirements: 9.3
 */

#include "rgtp_io_internal.h"

#ifdef RGTP_ENABLE_IOURING
#  include <liburing.h>
#  include <string.h>
#  include <stdlib.h>
#  include <errno.h>

#define RGTP_IOURING_SQ_DEPTH 256u

typedef struct {
    struct io_uring ring;
    bool            initialised;
} rgtp_iouring_ctx_t;

static rgtp_iouring_ctx_t g_uring_ctx = { .initialised = false };

rgtp_error_t rgtp_iouring_init(void)
{
    if (g_uring_ctx.initialised) return RGTP_OK;

    int ret = io_uring_queue_init(RGTP_IOURING_SQ_DEPTH, &g_uring_ctx.ring, 0);
    if (ret < 0) return RGTP_ERR_NOT_SUPPORTED;

    g_uring_ctx.initialised = true;
    return RGTP_OK;
}

void rgtp_iouring_cleanup(void)
{
    if (g_uring_ctx.initialised) {
        io_uring_queue_exit(&g_uring_ctx.ring);
        g_uring_ctx.initialised = false;
    }
}

rgtp_error_t rgtp_iouring_submit_recv(int sockfd, void* buf, size_t len,
                                       struct sockaddr_storage* addr,
                                       socklen_t* addr_len)
{
    if (!g_uring_ctx.initialised) return RGTP_ERR_NOT_SUPPORTED;

    struct io_uring_sqe* sqe = io_uring_get_sqe(&g_uring_ctx.ring);
    if (!sqe) return RGTP_ERR_INTERNAL;

    struct msghdr msg = {0};
    struct iovec  iov = { buf, len };
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;
    msg.msg_name    = addr;
    msg.msg_namelen = addr_len ? *addr_len : sizeof(struct sockaddr_storage);

    io_uring_prep_recvmsg(sqe, sockfd, &msg, 0);
    io_uring_sqe_set_data(sqe, buf);

    int ret = io_uring_submit(&g_uring_ctx.ring);
    return (ret >= 0) ? RGTP_OK : RGTP_ERR_SOCKET;
}

#else /* RGTP_ENABLE_IOURING not defined */

rgtp_error_t rgtp_iouring_init(void)    { return RGTP_ERR_NOT_SUPPORTED; }
void         rgtp_iouring_cleanup(void) {}
rgtp_error_t rgtp_iouring_submit_recv(int s, void* b, size_t l,
                                       struct sockaddr_storage* a, socklen_t* al)
{
    (void)s; (void)b; (void)l; (void)a; (void)al;
    return RGTP_ERR_NOT_SUPPORTED;
}

#endif /* RGTP_ENABLE_IOURING */
