/**
 * @file rgtp_socket.c
 * @brief RGTP socket creation, binding, and I/O backend selection.
 *
 * Creates a UDP socket (IPv4/IPv6 dual-stack) or an AF_PACKET raw Ethernet
 * socket, selects the appropriate I/O backend (io_uring, IOCP, sendmmsg,
 * or plain sendto/recvfrom), and wraps it in an opaque rgtp_socket_t handle.
 *
 * Requirements: 9.8, 12.3, 12.4, 21.4
 */

#include "rgtp/rgtp.h"
#include "rgtp_io_internal.h"
#include "../core/rgtp_alloc_internal.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  define INVALID_SOCKET_FD  INVALID_SOCKET
#  define CLOSE_SOCKET(fd)   closesocket(fd)
   typedef SOCKET socket_fd_t;
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <unistd.h>
#  include <fcntl.h>
#  ifdef RGTP_ENABLE_RAW_ETHERNET
#    include <net/if.h>
#    include <linux/if_packet.h>
#    include <linux/if_ether.h>
#    include <sys/ioctl.h>
#  endif
#  define INVALID_SOCKET_FD  (-1)
#  define CLOSE_SOCKET(fd)   close(fd)
   typedef int socket_fd_t;
#endif

/* ── I/O backend tag ────────────────────────────────────────────────────── */

typedef enum {
    RGTP_IO_SENDMMSG   = 0,   /**< sendmmsg/recvmmsg (Linux 4.14+) */
    RGTP_IO_IOURING    = 1,   /**< io_uring (Linux 5.1+) */
    RGTP_IO_IOCP       = 2,   /**< IOCP (Windows) */
    RGTP_IO_RAWETH     = 3,   /**< AF_PACKET raw Ethernet */
    RGTP_IO_BASIC      = 4,   /**< sendto/recvfrom fallback */
} rgtp_io_backend_t;

/* ── Internal socket structure ──────────────────────────────────────────── */

struct rgtp_socket_s {
    socket_fd_t       fd;
    rgtp_io_backend_t backend;
    uint16_t          bound_port;
    bool              is_raw_ethernet;
    rgtp_config_t     config;
};

/* ── Helpers ────────────────────────────────────────────────────────────── */

static int set_nonblocking(socket_fd_t fd)
{
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

static int set_reuseaddr(socket_fd_t fd)
{
    int opt = 1;
#ifdef _WIN32
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                      (const char*)&opt, sizeof(opt));
#else
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
}

/* ── UDP socket creation ────────────────────────────────────────────────── */

static rgtp_error_t create_udp_socket(const rgtp_config_t *cfg,
                                       socket_fd_t *out_fd,
                                       uint16_t    *out_port)
{
    /* Try IPv6 dual-stack first, fall back to IPv4 */
    socket_fd_t fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    int         family = AF_INET6;

    if (fd == INVALID_SOCKET_FD) {
        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        family = AF_INET;
    }
    if (fd == INVALID_SOCKET_FD) return RGTP_ERR_SOCKET;

    /* Enable dual-stack on IPv6 socket */
    if (family == AF_INET6) {
        int off = 0;
#ifdef _WIN32
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&off, sizeof(off));
#else
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
#endif
    }

    set_reuseaddr(fd);
    set_nonblocking(fd);

    /* Bind to configured port (0 = auto-assign) */
    uint16_t port = (cfg && cfg->port) ? cfg->port : 0;

    if (family == AF_INET6) {
        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_addr   = in6addr_any;
        addr.sin6_port   = htons(port);
        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            CLOSE_SOCKET(fd);
            return RGTP_ERR_SOCKET;
        }
        /* Retrieve actual port */
        socklen_t len = sizeof(addr);
        getsockname(fd, (struct sockaddr*)&addr, &len);
        *out_port = ntohs(addr.sin6_port);
    } else {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(port);
        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            CLOSE_SOCKET(fd);
            return RGTP_ERR_SOCKET;
        }
        socklen_t len = sizeof(addr);
        getsockname(fd, (struct sockaddr*)&addr, &len);
        *out_port = ntohs(addr.sin_port);
    }

    *out_fd = fd;
    return RGTP_OK;
}

/* ── Raw Ethernet socket creation ───────────────────────────────────────── */

#ifdef RGTP_ENABLE_RAW_ETHERNET

static rgtp_error_t create_raweth_socket(const rgtp_config_t *cfg,
                                          socket_fd_t *out_fd)
{
#ifdef _WIN32
    /* Windows requires WinPcap/Npcap — not implemented in this stub */
    (void)cfg; (void)out_fd;
    return RGTP_ERR_NOT_SUPPORTED;
#else
    /* AF_PACKET / SOCK_DGRAM — EtherType 0x88B5 (local/experimental) */
    socket_fd_t fd = socket(AF_PACKET, SOCK_DGRAM, htons(0x88B5));
    if (fd == INVALID_SOCKET_FD) return RGTP_ERR_SOCKET;

    /* Bind to the specified interface */
    const char *iface = (cfg && cfg->eth_iface[0])
                        ? (const char*)cfg->eth_iface : "eth0";

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        close(fd);
        return RGTP_ERR_SOCKET;
    }

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family   = AF_PACKET;
    sll.sll_protocol = htons(0x88B5);
    sll.sll_ifindex  = ifr.ifr_ifindex;

    if (bind(fd, (struct sockaddr*)&sll, sizeof(sll)) != 0) {
        close(fd);
        return RGTP_ERR_SOCKET;
    }

    set_nonblocking(fd);
    *out_fd = fd;
    return RGTP_OK;
#endif
}

#endif /* RGTP_ENABLE_RAW_ETHERNET */

/* ── Backend selection ──────────────────────────────────────────────────── */

static rgtp_io_backend_t select_backend(bool raw_ethernet)
{
    if (raw_ethernet) return RGTP_IO_RAWETH;

#ifdef _WIN32
    return RGTP_IO_IOCP;
#elif defined(RGTP_ENABLE_IOURING)
    /* io_uring requires kernel 5.1+ — checked at runtime */
    return RGTP_IO_IOURING;
#elif defined(__linux__)
    return RGTP_IO_SENDMMSG;
#else
    return RGTP_IO_BASIC;
#endif
}

/* ── Public API ─────────────────────────────────────────────────────────── */

rgtp_error_t rgtp_socket_create(const rgtp_config_t *cfg,
                                 rgtp_socket_t      **out)
{
    if (!out) return RGTP_ERR_INVALID_ARG;

    struct rgtp_socket_s *sock = rgtp_malloc(sizeof(*sock));
    if (!sock) return RGTP_ERR_NOMEM;
    memset(sock, 0, sizeof(*sock));

    if (cfg) {
        sock->config = *cfg;
    }

    bool raw_eth = cfg && cfg->raw_ethernet;
    sock->is_raw_ethernet = raw_eth;
    sock->backend = select_backend(raw_eth);

    rgtp_error_t err;

#ifdef RGTP_ENABLE_RAW_ETHERNET
    if (raw_eth) {
        err = create_raweth_socket(cfg, &sock->fd);
        if (err != RGTP_OK) {
            rgtp_free(sock);
            return err;
        }
        sock->bound_port = 0;
        *out = sock;
        return RGTP_OK;
    }
#else
    if (raw_eth) {
        rgtp_free(sock);
        return RGTP_ERR_NOT_SUPPORTED;
    }
#endif

    err = create_udp_socket(cfg, &sock->fd, &sock->bound_port);
    if (err != RGTP_OK) {
        rgtp_free(sock);
        return err;
    }

    /* Enable GSO on Linux when available */
#if defined(__linux__) && defined(UDP_SEGMENT)
    if (sock->backend == RGTP_IO_SENDMMSG) {
        uint16_t seg = cfg ? (uint16_t)cfg->chunk_size : 1200;
        if (seg == 0) seg = 1200;
        rgtp_socket_enable_gso(sock->fd, seg);
    }
#endif

    *out = sock;
    return RGTP_OK;
}

void rgtp_socket_destroy(rgtp_socket_t *sock)
{
    if (!sock) return;
    if (sock->fd != INVALID_SOCKET_FD) {
        CLOSE_SOCKET(sock->fd);
        sock->fd = INVALID_SOCKET_FD;
    }
    rgtp_free(sock);
}
