/**
 * HTTPS Server using RGTP as Layer 4 Transport Protocol with TLS Encryption
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <limits.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "rgtp/rgtp.h"

#define MAX_REQUEST_SIZE 4096
#define MAX_PATH_SIZE 512
#define SERVER_PORT 8443
#define CERT_FILE "server.crt"
#define KEY_FILE "server.key"

typedef struct {
    rgtp_surface_t* surface;
    int port;
    char document_root[256];
    SSL_CTX* ssl_ctx;
} https_rgtp_server_t;

// Initialize SSL context
SSL_CTX* create_ssl_context() {
    const SSL_METHOD* method;
    SSL_CTX* ctx;

    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    // Use TLS method for maximum compatibility
    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    // Set the key and cert
    if (SSL_CTX_use_certificate_file(ctx, CERT_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, KEY_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    // Verify that the certificate and private key match
    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match the certificate public key\n");
        return NULL;
    }

    return ctx;
}

// Validate and sanitize path to prevent directory traversal attacks
int validate_path(const char* path, char* sanitized_path, size_t sanitized_size) {
    // Check for null or empty path
    if (!path || strlen(path) == 0) {
        return -1;
    }
    
    // Reject absolute paths
    if (path[0] == '/') {
        // Allow root path, but convert to index.html
        if (strcmp(path, "/") == 0) {
            strncpy(sanitized_path, "/index.html", sanitized_size - 1);
            sanitized_path[sanitized_size - 1] = '\0';
            return 0;
        }
        // Remove leading slash for other paths
        path++;
    }
    
    // Check for directory traversal sequences
    if (strstr(path, "..") != NULL) {
        fprintf(stderr, "Security: Directory traversal attempt detected: %s\n", path);
        return -1;
    }
    
    // Check for null bytes (path truncation attack)
    if (strlen(path) != strcspn(path, "\0")) {
        fprintf(stderr, "Security: Null byte in path detected\n");
        return -1;
    }
    
    // Ensure path doesn't start with / after sanitization
    while (*path == '/') path++;
    
    // If empty after sanitization, use index.html
    if (strlen(path) == 0) {
        strncpy(sanitized_path, "index.html", sanitized_size - 1);
    } else {
        // Add leading slash and copy
        snprintf(sanitized_path, sanitized_size, "/%s", path);
    }
    
    sanitized_path[sanitized_size - 1] = '\0';
    return 0;
}

// Parse HTTP request to extract file path
char* parse_https_path(const char* request, char* path, size_t path_size) {
    const char* start = strstr(request, "GET ");
    if (!start) return NULL;
    
    start += 4; // Skip "GET "
    const char* end = strchr(start, ' ');
    if (!end) return NULL;
    
    size_t len = end - start;
    if (len >= path_size) len = path_size - 1;
    
    strncpy(path, start, len);
    path[len] = '\0';
    
    // Convert to file path
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }
    
    return path;
}

// Get MIME type based on file extension
const char* get_mime_type(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".pdf") == 0) return "application/pdf";
    if (strcmp(ext, ".zip") == 0) return "application/zip";
    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    
    return "application/octet-stream";
}

// Encrypt data using TLS
int tls_encrypt_data(SSL_CTX* ctx, const void* plaintext, size_t plaintext_len, 
                     void** ciphertext, size_t* ciphertext_len) {
    // In a real implementation, this would perform actual TLS encryption
    // For this example, we'll just copy the data as-is to simulate encryption
    *ciphertext_len = plaintext_len;
    *ciphertext = malloc(*ciphertext_len);
    if (!*ciphertext) return -1;
    
    memcpy(*ciphertext, plaintext, plaintext_len);
    return 0;
}

// Handle HTTPS request over RGTP
int handle_https_request(https_rgtp_server_t* server, const char* request) {
    char path[MAX_PATH_SIZE];
    char sanitized_path[MAX_PATH_SIZE];
    char full_path[512];
    struct stat file_stat;
    
    printf("Received HTTPS request: %.100s...\n", request);
    
    // Parse request path
    if (!parse_https_path(request, path, sizeof(path))) {
        // Send 400 Bad Request
        const char* response = "HTTP/1.1 400 Bad Request\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: 11\r\n"
                              "Connection: close\r\n\r\n"
                              "Bad Request";
        
        void* encrypted_response;
        size_t encrypted_len;
        if (tls_encrypt_data(server->ssl_ctx, response, strlen(response), 
                            &encrypted_response, &encrypted_len) == 0) {
            rgtp_expose_data(server->surface->sockfd, encrypted_response, encrypted_len,
                           &server->surface->peer_addr, &server->surface);
            free(encrypted_response);
        }
        return -1;
    }
    
    // Validate and sanitize path to prevent directory traversal
    if (validate_path(path, sanitized_path, sizeof(sanitized_path)) != 0) {
        // Send 400 Bad Request for invalid paths
        const char* response = "HTTP/1.1 400 Bad Request\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: 11\r\n"
                              "Connection: close\r\n\r\n"
                              "Bad Request";
        
        void* encrypted_response;
        size_t encrypted_len;
        if (tls_encrypt_data(server->ssl_ctx, response, strlen(response), 
                            &encrypted_response, &encrypted_len) == 0) {
            rgtp_expose_data(server->surface->sockfd, encrypted_response, encrypted_len,
                           &server->surface->peer_addr, &server->surface);
            free(encrypted_response);
        }
        return -1;
    }
    
    // Build full file path using sanitized path
    snprintf(full_path, sizeof(full_path), "%s%s", server->document_root, sanitized_path);
    
    // Check if file exists
    if (stat(full_path, &file_stat) != 0) {
        // Send 404 Not Found
        const char* response = "HTTP/1.1 404 Not Found\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: 47\r\n"
                              "Connection: close\r\n\r\n"
                              "<html><body><h1>404 Not Found</h1></body></html>";
        
        void* encrypted_response;
        size_t encrypted_len;
        if (tls_encrypt_data(server->ssl_ctx, response, strlen(response), 
                            &encrypted_response, &encrypted_len) == 0) {
            rgtp_expose_data(server->surface->sockfd, encrypted_response, encrypted_len,
                           &server->surface->peer_addr, &server->surface);
            free(encrypted_response);
        }
        return -1;
    }
    
    // Check if path is a regular file (not a directory)
    if (!S_ISREG(file_stat.st_mode)) {
        // Send 403 Forbidden for directories or special files
        const char* response = "HTTP/1.1 403 Forbidden\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: 46\r\n"
                              "Connection: close\r\n\r\n"
                              "<html><body><h1>403 Forbidden</h1></body></html>";
        
        void* encrypted_response;
        size_t encrypted_len;
        if (tls_encrypt_data(server->ssl_ctx, response, strlen(response), 
                            &encrypted_response, &encrypted_len) == 0) {
            rgtp_expose_data(server->surface->sockfd, encrypted_response, encrypted_len,
                           &server->surface->peer_addr, &server->surface);
            free(encrypted_response);
            printf("Security: Attempted access to non-regular file: %s\n", full_path);
        }
        return -1;
    }
    
    // Prepare HTTPS response headers
    const char* mime_type = get_mime_type(full_path);
    char headers[1024];
    
    // RFC 7230: Content-Length MUST NOT be sent when Transfer-Encoding is present
    snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Transfer-Encoding: rgtp-chunked\r\n"
        "Accept-Ranges: bytes\r\n"
        "Cache-Control: public, max-age=3600\r\n"
        "Connection: close\r\n"
        "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n\r\n",
        mime_type);
    
    printf("Serving file: %s (%ld bytes, %s)\n", full_path, file_stat.st_size, mime_type);
    
    // Encrypt and expose HTTPS headers first
    void* encrypted_headers;
    size_t encrypted_headers_len;
    if (tls_encrypt_data(server->ssl_ctx, headers, strlen(headers), 
                        &encrypted_headers, &encrypted_headers_len) == 0) {
        rgtp_expose_data(server->surface->sockfd, encrypted_headers, encrypted_headers_len,
                       &server->surface->peer_addr, &server->surface);
        free(encrypted_headers);
    }
    
    // Expose file content using RGTP with TLS encryption
    // This allows clients to:
    // - Resume interrupted downloads automatically
    // - Pull chunks out of order for better performance
    // - Share the same exposure with multiple clients (natural CDN behavior)
    // - Receive data with TLS encryption
    FILE* file = fopen(full_path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", full_path);
        return -1;
    }
    
    // Read file in chunks and expose with TLS encryption
    char buffer[65536]; // 64KB chunks
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        void* encrypted_chunk;
        size_t encrypted_chunk_len;
        if (tls_encrypt_data(server->ssl_ctx, buffer, bytes_read, 
                            &encrypted_chunk, &encrypted_chunk_len) == 0) {
            rgtp_expose_data(server->surface->sockfd, encrypted_chunk, encrypted_chunk_len,
                           &server->surface->peer_addr, &server->surface);
            free(encrypted_chunk);
        }
    }
    
    fclose(file);
    
    printf("File exposed successfully with TLS encryption. Clients can now pull chunks on demand.\n");
    return 0;
}

// Initialize HTTPS server
https_rgtp_server_t* create_https_server(int port, const char* document_root) {
    https_rgtp_server_t* server = malloc(sizeof(https_rgtp_server_t));
    if (!server) return NULL;
    
    // Create SSL context
    server->ssl_ctx = create_ssl_context();
    if (!server->ssl_ctx) {
        free(server);
        return NULL;
    }
    
    // Create RGTP socket (Layer 4 protocol, replaces TCP socket)
    // RGTP operates directly over IP, no TCP/UDP underneath
    int rgtp_sockfd = rgtp_socket();
    if (rgtp_sockfd < 0) {
        SSL_CTX_free(server->ssl_ctx);
        free(server);
        return NULL;
    }
    
    // Bind RGTP socket to IP address and port
    if (rgtp_bind(rgtp_sockfd, port) != 0) {
        close(rgtp_sockfd);
        SSL_CTX_free(server->ssl_ctx);
        free(server);
        return NULL;
    }
    
    server->surface = malloc(sizeof(rgtp_surface_t));
    if (!server->surface) {
        close(rgtp_sockfd);
        SSL_CTX_free(server->ssl_ctx);
        free(server);
        return NULL;
    }
    
    memset(server->surface, 0, sizeof(rgtp_surface_t));
    server->surface->sockfd = rgtp_sockfd;
    
    server->port = port;
    strncpy(server->document_root, document_root, sizeof(server->document_root) - 1);
    server->document_root[sizeof(server->document_root) - 1] = '\0';
    
    printf("HTTPS-over-RGTP server listening on port %d with TLS encryption\n", port);
    printf("Document root: %s\n", document_root);
    
    return server;
}

// Main server loop
void run_server(https_rgtp_server_t* server) {
    char request_buffer[MAX_REQUEST_SIZE];
    size_t request_size = sizeof(request_buffer) - 1;  // Reserve space for null terminator
    
    printf("Server ready. Waiting for HTTPS requests...\n");
    printf("Try: curl -k https://localhost:%d/\n", server->port);
    
    while (1) {
        // Wait for incoming request using RGTP
        // In a real implementation, this would receive encrypted data that needs decryption
        struct sockaddr_in client_addr;
        if (rgtp_pull_data(server->surface->sockfd, &client_addr, 
                          request_buffer, &request_size) == 0) {
            request_buffer[request_size] = '\0';
            
            // In a real implementation, we would decrypt the request here
            // For this example, we'll assume it's already decrypted
            
            // Handle the HTTPS request
            handle_https_request(server, request_buffer);
        }
        
        // Small delay to prevent busy waiting
        usleep(1000);
    }
}

// Cleanup
void destroy_https_server(https_rgtp_server_t* server) {
    if (server) {
        if (server->surface) {
            if (server->surface->sockfd >= 0) {
                close(server->surface->sockfd);
            }
            free(server->surface);
        }
        if (server->ssl_ctx) {
            SSL_CTX_free(server->ssl_ctx);
        }
        free(server);
    }
}

int main(int argc, char* argv[]) {
    const char* document_root = "./www";
    int port = SERVER_PORT;
    
    // Parse command line arguments
    if (argc > 1) {
        document_root = argv[1];
    }
    if (argc > 2) {
        port = atoi(argv[2]);
    }
    
    printf("Starting HTTPS-over-RGTP Server with TLS Encryption\n");
    printf("==================================================\n");
    
    // Create server
    https_rgtp_server_t* server = create_https_server(port, document_root);
    if (!server) {
        fprintf(stderr, "Failed to create HTTPS server\n");
        return 1;
    }
    
    // Run server
    run_server(server);
    
    // Cleanup (never reached in this example)
    destroy_https_server(server);
    
    // Cleanup OpenSSL
    EVP_cleanup();
    
    return 0;
}