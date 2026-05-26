/**
 * @file rgtp_iocp.c
 * @brief IOCP (I/O Completion Ports) async socket backend for Windows.
 *
 * Guarded by _WIN32. On non-Windows platforms, all functions are stubs.
 *
 * When enabled:
 *  - Creates an IOCP handle associated with the UDP socket.
 *  - Posts WSARecvFrom overlapped operations.
 *  - Processes completions via GetQueuedCompletionStatus in rgtp_poll().
 *
 * Requirements: 9.4
 */

#include "rgtp_io_internal.h"

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <string.h>
#include <stdlib.h>

#define RGTP_IOCP_RECV_BUFS 64u
#define RGTP_IOCP_BUF_SIZE  65536u

typedef struct {
    OVERLAPPED          overlapped;
    WSABUF              wsabuf;
    char                buf[RGTP_IOCP_BUF_SIZE];
    struct sockaddr_storage from_addr;
    INT                 from_len;
    DWORD               flags;
} rgtp_iocp_recv_op_t;

typedef struct {
    HANDLE              iocp;
    SOCKET              sock;
    rgtp_iocp_recv_op_t ops[RGTP_IOCP_RECV_BUFS];
    bool                initialised;
} rgtp_iocp_ctx_t;

static rgtp_iocp_ctx_t g_iocp_ctx = { .initialised = false };

rgtp_error_t rgtp_iocp_init(SOCKET sock)
{
    if (g_iocp_ctx.initialised) return RGTP_OK;

    g_iocp_ctx.iocp = CreateIoCompletionPort((HANDLE)sock, NULL, 0, 0);
    if (g_iocp_ctx.iocp == NULL) return RGTP_ERR_SOCKET;

    g_iocp_ctx.sock = sock;
    g_iocp_ctx.initialised = true;

    /* Post initial receive operations */
    for (DWORD i = 0; i < RGTP_IOCP_RECV_BUFS; i++) {
        rgtp_iocp_recv_op_t* op = &g_iocp_ctx.ops[i];
        memset(op, 0, sizeof(*op));
        op->wsabuf.buf = op->buf;
        op->wsabuf.len = RGTP_IOCP_BUF_SIZE;
        op->from_len   = sizeof(op->from_addr);
        op->flags      = 0;

        WSARecvFrom(sock, &op->wsabuf, 1, NULL, &op->flags,
                    (struct sockaddr*)&op->from_addr, &op->from_len,
                    &op->overlapped, NULL);
    }
    return RGTP_OK;
}

void rgtp_iocp_cleanup(void)
{
    if (g_iocp_ctx.initialised) {
        CloseHandle(g_iocp_ctx.iocp);
        g_iocp_ctx.initialised = false;
    }
}

/**
 * @brief Poll for completed I/O operations.
 *
 * @param timeout_ms  Timeout in milliseconds (0 = non-blocking, INFINITE = block).
 * @param buf_out     Receives pointer to the completed receive buffer.
 * @param len_out     Receives the number of bytes received.
 * @param addr_out    Receives the sender address.
 * @return RGTP_OK if a completion was dequeued, RGTP_ERR_TIMEOUT if none.
 */
rgtp_error_t rgtp_iocp_poll(int timeout_ms,
                              const char** buf_out,
                              size_t*      len_out,
                              struct sockaddr_storage* addr_out)
{
    if (!g_iocp_ctx.initialised) return RGTP_ERR_NOT_SUPPORTED;

    DWORD       bytes_transferred = 0;
    ULONG_PTR   completion_key    = 0;
    OVERLAPPED* overlapped        = NULL;

    DWORD wait_ms = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    BOOL ok = GetQueuedCompletionStatus(g_iocp_ctx.iocp,
                                         &bytes_transferred,
                                         &completion_key,
                                         &overlapped,
                                         wait_ms);
    if (!ok || overlapped == NULL) return RGTP_ERR_TIMEOUT;

    /* Find the op that completed */
    rgtp_iocp_recv_op_t* op = (rgtp_iocp_recv_op_t*)overlapped;

    if (buf_out)  *buf_out  = op->buf;
    if (len_out)  *len_out  = (size_t)bytes_transferred;
    if (addr_out) memcpy(addr_out, &op->from_addr, sizeof(*addr_out));

    /* Re-post the receive operation */
    memset(&op->overlapped, 0, sizeof(op->overlapped));
    op->from_len = sizeof(op->from_addr);
    op->flags    = 0;
    WSARecvFrom(g_iocp_ctx.sock, &op->wsabuf, 1, NULL, &op->flags,
                (struct sockaddr*)&op->from_addr, &op->from_len,
                &op->overlapped, NULL);

    return RGTP_OK;
}

#else /* Non-Windows stubs */

rgtp_error_t rgtp_iocp_init(int sock)
{
    (void)sock;
    return RGTP_ERR_NOT_SUPPORTED;
}

void rgtp_iocp_cleanup(void) {}

rgtp_error_t rgtp_iocp_poll(int tm, const char** b, size_t* l,
                              struct sockaddr_storage* a)
{
    (void)tm; (void)b; (void)l; (void)a;
    return RGTP_ERR_NOT_SUPPORTED;
}

#endif /* _WIN32 */
