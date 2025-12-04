#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "rgtp/rgtp.h"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#include <iphlpapi.h>   // for getting local IP
#pragma comment(lib, "iphlpapi.lib")
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#endif

static volatile int running = 1;
void sigint(int s) { (void)s; running = 0; }

// Helper: get first non-loopback IPv4 address (Windows + Linux)
static void get_local_ip(char* out_ip) {
    out_ip[0] = 0;
#ifdef _WIN32
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent* he = gethostbyname(hostname);
        if (he) {
            for (int i = 0; he->h_addr_list[i]; i++) {
                struct in_addr addr = *(struct in_addr*)he->h_addr_list[i];
                if (addr.s_addr != htonl(INADDR_LOOPBACK)) {
                    strcpy(out_ip, inet_ntoa(addr));
                    return;
                }
            }
        }
    }
#else
    struct ifaddrs* ifap, * ifa;
    if (getifaddrs(&ifap) == 0) {
        for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
                if (sa->sin_addr.s_addr != htonl(INADDR_LOOPBACK) &&
                    strcmp(ifa->ifa_name, "lo") != 0) {
                    inet_ntop(AF_INET, &sa->sin_addr, out_ip, 16);
                    break;
                }
            }
        }
        freeifaddrs(ifap);
    }
#endif
    if (!out_ip[0]) strcpy(out_ip, "127.0.0.1");
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <file-to-expose>\n", argv[0]);
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    rgtp_init();
    signal(SIGINT, sigint);
#ifdef SIGBREAK
    signal(SIGBREAK, sigint);
#endif

    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void* data = malloc(size);
    if (!data || fread(data, 1, size, f) != size) {
        perror("read file");
        fclose(f); free(data); return 1;
    }
    fclose(f);

    int sock = rgtp_socket();
    if (sock < 0) {
        fprintf(stderr, "Failed to create socket\n");
        free(data); return 1;
    }

    // Get actual bound port (fixes "UDP 0" on Windows)
    struct sockaddr_in local = { 0 };
    socklen_t len = sizeof(local);
    getsockname(sock, (struct sockaddr*)&local, &len);
    uint16_t port = ntohs(local.sin_port);

    struct sockaddr_in any = { 0 };
    any.sin_family = AF_INET;
    any.sin_addr.s_addr = INADDR_ANY;

    rgtp_surface_t* surface = NULL;
    if (rgtp_expose_data(sock, data, size, &any, &surface) != 0 || !surface) {
        fprintf(stderr, "Failed to expose file\n");
        close(sock); free(data); return 1;
    }

    char local_ip[16];
    get_local_ip(local_ip);

    printf("\nRED GIANT UDP EXPOSER v2.0 - FINAL\n");
    printf("File         : %s\n", argv[1]);
    printf("Size         : %.3f GB\n", size / 1e9);
    printf("Exposure ID  : %016llx%016llx\n",
        (unsigned long long)surface->exposure_id[0],
        (unsigned long long)surface->exposure_id[1]);
    printf("Serving on   : UDP %u  →  %s:%u\n", port, local_ip, port);
    printf("Pull from LAN: udp_pull_file %s %s\n", local_ip, argv[1]);
    printf("Pull from Web: paste ID into browser demo\n\n");

    while (running) {
        rgtp_poll(surface, 100);
        printf("\rSent: %.3f GB | Active pullers: %u    ",
            surface->bytes_sent / 1e9, surface->pull_pressure);
        fflush(stdout);

#ifdef _WIN32
        if (_kbhit() && (_getch() == 27 || _getch() == 3)) break;
        Sleep(200);
#else
        usleep(200000);
#endif
    }

    printf("\n\nShutting down gracefully...\n");
    rgtp_destroy_surface(surface);
    free(data);
    close(sock);

#ifdef _WIN32
    WSACleanup();
#endif
    rgtp_cleanup();
    return 0;
}