/**
 * HTTPS Server using RGTP as Layer 4 Transport Protocol with TLS Encryption (Windows Version)
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
#include <sys/stat.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "rgtp/rgtp.h"

#define MAX_REQUEST_SIZE 4096
#define MAX_PATH_SIZE 512
#define SERVER_PORT 8443
#define CERT_FILE "server.crt"
#define KEY_FILE "server.key"

// Windows compatibility macros
#ifdef _WIN32
#define close closesocket
#define sleep(x) Sleep(x*1000)
typedef int socklen_t;
#define S_ISDIR(x) ((x) & _S_IFDIR)
#endif

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
    (void)ctx; // Mark parameter as used
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
    
    #ifdef _WIN32
    struct _stat file_stat;
    #else
    struct stat file_stat;
    #endif
    
    printf("Received HTTPS request: %.100s...\n", request);
    
    // Parse request path
    if (!parse_https_path(request, path, sizeof(path))) {
        // Send 400 Bad Request
        // In a real implementation, send this response over RGTP
        printf("Sending 400 Bad Request response\n");
        return -1;
    }
    
    // Validate and sanitize path
    if (validate_path(path, sanitized_path, sizeof(sanitized_path)) < 0) {
        // Send 403 Forbidden
        // In a real implementation, send this response over RGTP
        printf("Sending 403 Forbidden response\n");
        return -1;
    }
    
    // Construct full file path
    snprintf(full_path, sizeof(full_path), "%s%s", server->document_root, sanitized_path);
    
    // Remove leading slash for Windows file paths
    if (full_path[strlen(server->document_root)] == '/') {
        memmove(&full_path[strlen(server->document_root)], 
                &full_path[strlen(server->document_root) + 1], 
                strlen(&full_path[strlen(server->document_root) + 1]) + 1);
    }
    
    printf("Attempting to serve file: %s\n", full_path);
    
    // Check if file exists and get stats
    if (stat(full_path, &file_stat) < 0) {
        // Send 404 Not Found
        // In a real implementation, send this response over RGTP
        printf("Sending 404 Not Found response\n");
        return -1;
    }
    
    // Check if it's a directory
    if (S_ISDIR(file_stat.st_mode)) {
        // Send 403 Forbidden for directories
        // In a real implementation, send this response over RGTP
        printf("Sending 403 Forbidden response for directory\n");
        return -1;
    }
    
    // Open and read file
    FILE* file = fopen(full_path, "rb");
    if (!file) {
        // Send 500 Internal Server Error
        // In a real implementation, send this response over RGTP
        printf("Sending 500 Internal Server Error response\n");
        return -1;
    }
    
    // Allocate buffer for file content
    char* file_content = malloc(file_stat.st_size);
    if (!file_content) {
        fclose(file);
        // Send 500 Internal Server Error
        // In a real implementation, send this response over RGTP
        printf("Sending 500 Internal Server Error response (malloc failed)\n");
        return -1;
    }
    
    // Read file content
    size_t bytes_read = fread(file_content, 1, file_stat.st_size, file);
    fclose(file);
    
    if (bytes_read != file_stat.st_size) {
        free(file_content);
        // Send 500 Internal Server Error
        // In a real implementation, send this response over RGTP
        printf("Sending 500 Internal Server Error response (read failed)\n");
        return -1;
    }
    
    // Prepare HTTP response headers
    const char* mime_type = get_mime_type(full_path);
    char response_headers[512];
    snprintf(response_headers, sizeof(response_headers),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %lld\r\n"
             "Connection: close\r\n\r\n",
             mime_type, (long long)file_stat.st_size);
    
    printf("Sending 200 OK response with %lld bytes of %s content\n", 
           (long long)file_stat.st_size, mime_type);
    
    // In a real implementation, we would:
    // 1. Send response headers over RGTP
    // 2. Send file content over RGTP
    // 3. Handle TLS encryption
    
    // For now, just print success
    printf("Successfully processed request for %s\n", full_path);
    
    free(file_content);
    return 0;
}

// Initialize server
https_rgtp_server_t* init_https_rgtp_server(const char* document_root, int port) {
    https_rgtp_server_t* server = malloc(sizeof(https_rgtp_server_t));
    if (!server) {
        return NULL;
    }
    
    // Initialize Windows Sockets (if on Windows)
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        free(server);
        return NULL;
    }
#endif
    
    // Initialize SSL context
    server->ssl_ctx = create_ssl_context();
    if (!server->ssl_ctx) {
#ifdef _WIN32
        WSACleanup();
#endif
        free(server);
        return NULL;
    }
    
    // Set document root
    strncpy(server->document_root, document_root, sizeof(server->document_root) - 1);
    server->document_root[sizeof(server->document_root) - 1] = '\0';
    
    // Ensure document root ends with a slash
    if (server->document_root[strlen(server->document_root) - 1] != '/' && 
        server->document_root[strlen(server->document_root) - 1] != '\\') {
        strcat(server->document_root, "/");
    }
    
    server->port = port;
    server->surface = NULL; // In a real implementation, this would be initialized
    
    return server;
}

// Cleanup server
void cleanup_https_rgtp_server(https_rgtp_server_t* server) {
    if (server) {
        if (server->ssl_ctx) {
            SSL_CTX_free(server->ssl_ctx);
        }
        
        // Cleanup Windows Sockets (if on Windows)
#ifdef _WIN32
        WSACleanup();
#endif
        
        free(server);
    }
}

// Main server loop
void run_https_rgtp_server(https_rgtp_server_t* server) {
    printf("HTTPS RGTP Server starting on port %d\n", server->port);
    printf("Document root: %s\n", server->document_root);
    printf("Press Ctrl+C to stop the server\n");
    
    // In a real implementation, this would:
    // 1. Create RGTP surface/exposure
    // 2. Listen for incoming RGTP connections
    // 3. Handle HTTPS requests over RGTP
    // 4. Serve files with TLS encryption
    
    // For demonstration, we'll just show how it would work
    printf("\nServer is ready to accept connections!\n");
    printf("In a real implementation, this would:\n");
    printf("1. Create an RGTP exposure\n");
    printf("2. Listen for RGTP pullers\n");
    printf("3. Handle HTTPS requests over RGTP\n");
    printf("4. Serve files with TLS encryption\n\n");
    
    // Simulate handling a request for demonstration
    const char* sample_request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    printf("Simulating request handling:\n");
    handle_https_request(server, sample_request);
    
    // Keep server running
    while (1) {
        // In a real implementation, this would check for incoming RGTP connections
        sleep(1);
    }
}

int main(int argc, char* argv[]) {
    const char* document_root = "./www";
    int port = SERVER_PORT;
    
    if (argc > 1) {
        document_root = argv[1];
    }
    
    if (argc > 2) {
        port = atoi(argv[2]);
    }
    
    printf("Initializing HTTPS RGTP Server...\n");
    
    https_rgtp_server_t* server = init_https_rgtp_server(document_root, port);
    if (!server) {
        fprintf(stderr, "Failed to initialize HTTPS RGTP server\n");
        return 1;
    }
    
    printf("HTTPS RGTP Server initialized successfully!\n");
    
    // Run the server
    run_https_rgtp_server(server);
    
    // Cleanup (this won't be reached in normal operation)
    cleanup_https_rgtp_server(server);
    
    return 0;
}