/**
 * HTTP Server using RGTP as Layer 4 Transport Protocol
 * 
 * This example demonstrates how HTTP can run directly over RGTP instead of TCP.
 * RGTP replaces TCP entirely at Layer 4, providing:
 * - Natural multicast (one exposure serves multiple clients)
 * - Instant resume capability
 * - No head-of-line blocking
 * - Stateless operation (no connection management)
 * 
 * Network Stack:
 * Application Layer: HTTP
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
#include "rgtp/rgtp.h"

#define MAX_REQUEST_SIZE 4096
#define MAX_PATH_SIZE 512
#define SERVER_PORT 8080

typedef struct {
    rgtp_session_t* session;
    int port;
    char document_root[256];
} http_rgtp_server_t;

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
char* parse_http_path(const char* request, char* path, size_t path_size) {
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

// Handle HTTP request over RGTP
int handle_http_request(http_rgtp_server_t* server, const char* request) {
    char path[MAX_PATH_SIZE];
    char sanitized_path[MAX_PATH_SIZE];
    char full_path[512];
    struct stat file_stat;
    
    printf("Received HTTP request: %.100s...\n", request);
    
    // Parse request path
    if (!parse_http_path(request, path, sizeof(path))) {
        // Send 400 Bad Request
        const char* response = "HTTP/1.1 400 Bad Request\r\n"
                              "Content-Type: text/plain\r\n"
                              "Content-Length: 11\r\n"
                              "Connection: close\r\n\r\n"
                              "Bad Request";
        rgtp_expose_data(server->session, response, strlen(response));
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
        rgtp_expose_data(server->session, response, strlen(response));
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
        rgtp_expose_data(server->session, response, strlen(response));
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
        rgtp_expose_data(server->session, response, strlen(response));
        printf("Security: Attempted access to non-regular file: %s\n", full_path);
        return -1;
    }
    
    // Prepare HTTP response headers
    const char* mime_type = get_mime_type(full_path);
    char headers[1024];
    
    // RFC 7230: Content-Length MUST NOT be sent when Transfer-Encoding is present
    snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Transfer-Encoding: rgtp-chunked\r\n"
        "Accept-Ranges: bytes\r\n"
        "Cache-Control: public, max-age=3600\r\n"
        "Connection: close\r\n\r\n",
        mime_type);
    
    printf("Serving file: %s (%ld bytes, %s)\n", full_path, file_stat.st_size, mime_type);
    
    // Expose HTTP headers first
    rgtp_expose_data(server->session, headers, strlen(headers));
    
    // Expose file content using RGTP
    // This allows clients to:
    // - Resume interrupted downloads automatically
    // - Pull chunks out of order for better performance
    // - Share the same exposure with multiple clients (natural CDN behavior)
    if (rgtp_expose_file(server->session, full_path) != 0) {
        fprintf(stderr, "Failed to expose file: %s\n", full_path);
        return -1;
    }
    
    printf("File exposed successfully. Clients can now pull chunks on demand.\n");
    return 0;
}

// Initialize HTTP server
http_rgtp_server_t* create_http_server(int port, const char* document_root) {
    http_rgtp_server_t* server = malloc(sizeof(http_rgtp_server_t));
    if (!server) return NULL;
    
    // Create RGTP socket (Layer 4 protocol, replaces TCP socket)
    // RGTP operates directly over IP, no TCP/UDP underneath
    int rgtp_sockfd = rgtp_socket(AF_INET, RGTP_EXPOSER, 0);
    if (rgtp_sockfd < 0) {
        free(server);
        return NULL;
    }
    
    // Bind RGTP socket to IP address and port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (rgtp_bind(rgtp_sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        rgtp_close(rgtp_sockfd);
        free(server);
        return NULL;
    }
    
    // Configure RGTP exposure parameters
    rgtp_config_t config = {
        .chunk_size = 64 * 1024,        // 64KB chunks for web content
        .exposure_rate = 1000,          // 1000 chunks/second
        .adaptive_mode = 1,             // Enable adaptive rate control
        .multicast_enabled = 1,         // Allow multiple clients to pull same data
        .priority = RGTP_PRIORITY_NORMAL
    };
    
    rgtp_setsockopt(rgtp_sockfd, RGTP_SOL_RGTP, RGTP_CONFIG, &config, sizeof(config));
    
    server->session = rgtp_create_session_from_socket(rgtp_sockfd);
    server->port = port;
    strncpy(server->document_root, document_root, sizeof(server->document_root) - 1);
    server->document_root[sizeof(server->document_root) - 1] = '\0';
    
    printf("HTTP-over-RGTP server listening on port %d\n", port);
    printf("Document root: %s\n", document_root);
    
    return server;
}

// Main server loop
void run_server(http_rgtp_server_t* server) {
    char request_buffer[MAX_REQUEST_SIZE];
    
    printf("Server ready. Waiting for HTTP requests...\n");
    printf("Try: curl http://localhost:%d/\n", server->port);
    
    while (1) {
        // Wait for incoming request
        size_t request_size = sizeof(request_buffer) - 1;  // Reserve space for null terminator
        if (rgtp_receive_data(server->session, request_buffer, &request_size) == 0) {
            request_buffer[request_size] = '\0';
            
            // Handle the HTTP request
            handle_http_request(server, request_buffer);
        }
        
        // Small delay to prevent busy waiting
        usleep(1000);
    }
}

// Cleanup
void destroy_http_server(http_rgtp_server_t* server) {
    if (server) {
        if (server->session) {
            rgtp_destroy_session(server->session);
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
    
    printf("Starting HTTP-over-RGTP Server\n");
    printf("===============================\n");
    
    // Create server
    http_rgtp_server_t* server = create_http_server(port, document_root);
    if (!server) {
        fprintf(stderr, "Failed to create HTTP server\n");
        return 1;
    }
    
    // Run server
    run_server(server);
    
    // Cleanup (never reached in this example)
    destroy_http_server(server);
    
    return 0;
}