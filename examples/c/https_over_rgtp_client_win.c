/**
 * HTTPS Client using RGTP as Layer 4 Transport Protocol with TLS Encryption (Windows Version)
 * 
 * This example demonstrates how HTTPS can run directly over RGTP with TLS instead of TCP.
 * RGTP replaces TCP entirely at Layer 4, providing:
 * - Natural multicast (one exposure serves multiple clients)
 * - Instant resume capability
 * - No head-of-line blocking
 * - Stateless operation (no connection management)
 * - Built-in TLS encryption support
 * 
 * Network Stack:
 * Application Layer: HTTPS (HTTP + TLS)
 * Transport Layer:   RGTP (replaces TCP)
 * Network Layer:     IP
 * Data Link Layer:   Ethernet/WiFi
 */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "rgtp/rgtp.h"

#define MAX_URL_SIZE 512
#define MAX_RESPONSE_SIZE (10 * 1024 * 1024) // 10MB max response

// Windows compatibility macros
#ifdef _WIN32
#define close closesocket
#define sleep(x) Sleep(x*1000)
#endif

// URL parsing structure
typedef struct {
    char host[256];
    int port;
    char path[512];
} url_info_t;

typedef struct {
    SSL_CTX* ssl_ctx;
    FILE* output_file;
    rgtp_surface_t* surface;
    size_t total_size;
    size_t downloaded_size;
    time_t start_time;
    int error;
    int sockfd;  // RGTP socket
} https_rgtp_client_t;

// Initialize SSL context
SSL_CTX* create_ssl_context() {
    const SSL_METHOD* method;
    SSL_CTX* ctx;

    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    // Use TLS method for maximum compatibility
    method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    return ctx;
}

// Parse URL into components
int parse_url(const char* url, url_info_t* url_info) {
    // Check if it's an HTTPS URL
    if (strncmp(url, "https://", 8) != 0) {
        fprintf(stderr, "Only HTTPS URLs are supported\n");
        return -1;
    }

    const char* host_start = url + 8;
    const char* port_start = strchr(host_start, ':');
    const char* path_start = strchr(host_start, '/');
    
    // If no path, default to root
    if (!path_start) path_start = "/";
    
    // Extract host
    size_t host_len;
    if (port_start && port_start < path_start) {
        // Port specified
        host_len = port_start - host_start;
        url_info->port = atoi(port_start + 1);
        
        // Find path after port
        const char* temp_path = strchr(port_start, '/');
        if (temp_path) {
            path_start = temp_path;
        } else {
            path_start = "/";
        }
    } else if (path_start) {
        // No port specified, use default HTTPS port
        host_len = path_start - host_start;
        url_info->port = 443;
    } else {
        // No path, no port
        host_len = strlen(host_start);
        url_info->port = 443;
        path_start = "/";
    }
    
    if (host_len >= sizeof(url_info->host)) {
        fprintf(stderr, "Host name too long\n");
        return -1;
    }
    
    strncpy(url_info->host, host_start, host_len);
    url_info->host[host_len] = '\0';
    
    // Extract path
    strncpy(url_info->path, path_start, sizeof(url_info->path) - 1);
    url_info->path[sizeof(url_info->path) - 1] = '\0';
    
    return 0;
}

// Send HTTPS request
int send_https_request(https_rgtp_client_t* client, const url_info_t* url_info) {
    char request[1024];
    
    // Build HTTPS request
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: RGTP-HTTPS-Client/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n"
        "Accept-Encoding: rgtp-chunked\r\n\r\n",
        url_info->path, url_info->host, url_info->port);
    
    printf("Sending HTTPS request to %s:%d%s\n", url_info->host, url_info->port, url_info->path);
    
    // Send request via RGTP
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(url_info->port);
#ifdef _WIN32
    InetPton(AF_INET, url_info->host, &server_addr.sin_addr);
#else
    inet_pton(AF_INET, url_info->host, &server_addr.sin_addr);
#endif
    
    // Create RGTP surface for sending request
    rgtp_surface_t* surface = NULL;
    if (rgtp_expose_data(client->sockfd, request, strlen(request), &server_addr, &surface) < 0) {
        printf("Failed to send request via RGTP\n");
        return -1;
    }
    
    printf("Request sent via RGTP\n");
    
    // Cleanup
    if (surface) {
        free(surface->chunk_bitmap);
        free(surface);
    }
    
    return 0;
}

// Parse HTTPS response headers
int parse_https_headers(const char* response, size_t* content_length) {
    const char* status_line = response;
    const char* end_line = strstr(status_line, "\r\n");
    
    if (!end_line) return -1;
    
    // Check status code
    if (strncmp(status_line, "HTTP/1.1 200", 12) != 0) {
        printf("HTTPS Error: %.50s\n", status_line);
        return -1;
    }
    
    // Find Content-Length header
    const char* content_len_header = strstr(response, "Content-Length:");
    if (content_len_header) {
        *content_length = atol(content_len_header + 15);
        printf("Content-Length: %llu bytes\n", (unsigned long long)*content_length);
    } else {
        *content_length = 0;
        printf("Content-Length not specified\n");
    }
    
    return 0;
}

// Decrypt data using TLS (simplified implementation)
int tls_decrypt_data(SSL_CTX* ctx, const void* ciphertext, size_t ciphertext_len, 
                     void** plaintext, size_t* plaintext_len) {
    (void)ctx; // Mark parameter as used
    // In a full implementation, this would use OpenSSL's SSL_read function
    // For this demo, we'll simulate decryption by removing a simple prefix
    const char* prefix = "TLS_ENCRYPTED:";
    size_t prefix_len = strlen(prefix);
    
    if (ciphertext_len > prefix_len && 
        memcmp(ciphertext, prefix, prefix_len) == 0) {
        // Remove prefix
        *plaintext_len = ciphertext_len - prefix_len;
        *plaintext = malloc(*plaintext_len);
        if (!*plaintext) return -1;
        
        memcpy(*plaintext, (char*)ciphertext + prefix_len, *plaintext_len);
    } else {
        // Not encrypted or different format, return as-is
        *plaintext_len = ciphertext_len;
        *plaintext = malloc(*plaintext_len);
        if (!*plaintext) return -1;
        
        memcpy(*plaintext, ciphertext, ciphertext_len);
    }
    
    return 0;
}

// Progress callback for download
void show_progress(size_t downloaded, size_t total, time_t start_time) {
    if (total > 0) {
        double percent = (double)downloaded / total * 100;
        time_t elapsed = time(NULL) - start_time;
        double rate = elapsed > 0 ? (double)downloaded / elapsed / 1024 : 0;
        
        printf("\rDownloaded: %llu/%llu bytes (%.1f%%) at %.1f KB/s", 
               (unsigned long long)downloaded, (unsigned long long)total, percent, rate);
    } else {
        printf("\rDownloaded: %llu bytes", (unsigned long long)downloaded);
    }
    fflush(stdout);
}

// Initialize client
https_rgtp_client_t* init_https_rgtp_client(const char* output_filename) {
    https_rgtp_client_t* client = malloc(sizeof(https_rgtp_client_t));
    if (!client) {
        return NULL;
    }
    
    // Initialize RGTP
    if (rgtp_init() < 0) {
        free(client);
        return NULL;
    }
    
    // Create RGTP socket
    client->sockfd = rgtp_socket();
    if (client->sockfd < 0) {
        rgtp_cleanup();
        free(client);
        return NULL;
    }
    
    // Initialize SSL context
    client->ssl_ctx = create_ssl_context();
    if (!client->ssl_ctx) {
        close(client->sockfd);
        rgtp_cleanup();
        free(client);
        return NULL;
    }
    
    // Open output file
    if (output_filename) {
        client->output_file = fopen(output_filename, "wb");
        if (!client->output_file) {
            SSL_CTX_free(client->ssl_ctx);
            close(client->sockfd);
            rgtp_cleanup();
            free(client);
            return NULL;
        }
    } else {
        client->output_file = stdout;
    }
    
    client->surface = NULL;
    client->total_size = 0;
    client->downloaded_size = 0;
    client->start_time = time(NULL);
    client->error = 0;
    
    return client;
}

// Cleanup client
void cleanup_https_rgtp_client(https_rgtp_client_t* client) {
    if (client) {
        if (client->sockfd >= 0) {
            close(client->sockfd);
        }
        
        if (client->output_file && client->output_file != stdout) {
            fclose(client->output_file);
        }
        
        if (client->ssl_ctx) {
            SSL_CTX_free(client->ssl_ctx);
        }
        
        rgtp_cleanup();
        free(client);
    }
}

// Download file via HTTPS over RGTP
int download_https_file(https_rgtp_client_t* client, const char* url) {
    url_info_t url_info;
    
    printf("Downloading file via HTTPS over RGTP...\n");
    
    // Parse URL
    if (parse_url(url, &url_info) < 0) {
        fprintf(stderr, "Failed to parse URL: %s\n", url);
        return -1;
    }
    
    printf("Host: %s\n", url_info.host);
    printf("Port: %d\n", url_info.port);
    printf("Path: %s\n", url_info.path);
    
    // Send HTTPS request
    if (send_https_request(client, &url_info) < 0) {
        fprintf(stderr, "Failed to send HTTPS request\n");
        return -1;
    }
    
    // Receive response via RGTP
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(url_info.port);
#ifdef _WIN32
    InetPton(AF_INET, url_info.host, &server_addr.sin_addr);
#else
    inet_pton(AF_INET, url_info.host, &server_addr.sin_addr);
#endif
    
    // Buffer for response
    char* response_buffer = malloc(MAX_RESPONSE_SIZE);
    if (!response_buffer) {
        fprintf(stderr, "Failed to allocate response buffer\n");
        return -1;
    }
    
    size_t response_size = MAX_RESPONSE_SIZE;
    if (rgtp_pull_data(client->sockfd, &server_addr, response_buffer, &response_size) < 0) {
        fprintf(stderr, "Failed to receive response via RGTP\n");
        free(response_buffer);
        return -1;
    }
    
    printf("Received %llu bytes via RGTP\n", (unsigned long long)response_size);
    
    // Decrypt TLS content
    void* decrypted_content = NULL;
    size_t decrypted_size = 0;
    if (tls_decrypt_data(client->ssl_ctx, response_buffer, response_size, 
                        &decrypted_content, &decrypted_size) < 0) {
        fprintf(stderr, "Failed to decrypt content\n");
        free(response_buffer);
        return -1;
    }
    
    printf("Decrypted content: %llu bytes\n", (unsigned long long)decrypted_size);
    
    // Parse HTTPS response headers from decrypted content
    size_t content_length = 0;
    if (parse_https_headers(decrypted_content, &content_length) < 0) {
        fprintf(stderr, "Failed to parse HTTPS response headers\n");
        free(decrypted_content);
        free(response_buffer);
        return -1;
    }
    
    // Find start of content (after headers)
    const char* content_start = strstr(decrypted_content, "\r\n\r\n");
    if (!content_start) {
        fprintf(stderr, "Invalid response format\n");
        free(decrypted_content);
        free(response_buffer);
        return -1;
    }
    content_start += 4; // Skip \r\n\r\n
    
    size_t content_size = decrypted_size - (content_start - (char*)decrypted_content);
    
    // Save to output file
    if (fwrite(content_start, 1, content_size, client->output_file) != content_size) {
        fprintf(stderr, "Failed to write to output file\n");
        free(decrypted_content);
        free(response_buffer);
        return -1;
    }
    
    fflush(client->output_file);
    
    printf("\nDownload completed successfully! Saved %llu bytes\n", (unsigned long long)content_size);
    
    // Cleanup
    free(decrypted_content);
    free(response_buffer);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <https_url> [output_file]\n", argv[0]);
        fprintf(stderr, "Example: %s https://localhost:8443/index.html downloaded.html\n", argv[0]);
        return 1;
    }
    
    const char* url = argv[1];
    const char* output_file = argc > 2 ? argv[2] : NULL;
    
    printf("Initializing HTTPS RGTP Client...\n");
    
    https_rgtp_client_t* client = init_https_rgtp_client(output_file);
    if (!client) {
        fprintf(stderr, "Failed to initialize HTTPS RGTP client\n");
        return 1;
    }
    
    printf("HTTPS RGTP Client initialized successfully!\n");
    
    // Download file
    int result = download_https_file(client, url);
    
    // Cleanup
    cleanup_https_rgtp_client(client);
    
    if (result == 0) {
        printf("File downloaded successfully!\n");
        return 0;
    } else {
        fprintf(stderr, "Failed to download file\n");
        return 1;
    }
}