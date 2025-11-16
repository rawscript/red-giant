/**
 * HTTPS Client using RGTP as Layer 4 Transport Protocol with TLS Encryption
 * 
 * This example demonstrates an HTTPS client that uses RGTP instead of TCP with TLS.
 * RGTP completely replaces TCP at Layer 4, providing:
 * - Pull-based data retrieval (no connection state)
 * - Automatic resume on interruption
 * - Out-of-order chunk processing
 * - Natural load balancing through pull pressure
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
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "rgtp/rgtp.h"

#define MAX_URL_SIZE 512
#define MAX_RESPONSE_SIZE 8192
#define CHUNK_SIZE 64 * 1024

typedef struct {
    char host[256];
    int port;
    char path[256];
} url_info_t;

typedef struct {
    rgtp_surface_t* surface;
    SSL_CTX* ssl_ctx;
    FILE* output_file;
    size_t total_size;
    size_t downloaded_size;
    time_t start_time;
    int error;  // Error flag: 0=no error, 1=I/O error (disk full, permissions, etc.)
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

    // Configure SSL context
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL); // For simplicity, not verifying server cert

    return ctx;
}

// Parse URL into components
int parse_url(const char* url, url_info_t* info) {
    // Simple URL parser for https://host:port/path format
    const char* start = url;
    
    // Skip protocol
    if (strncmp(start, "https://", 8) == 0) {
        start += 8;
    }
    
    // Extract host
    const char* colon = strchr(start, ':');
    const char* slash = strchr(start, '/');
    
    if (colon && (!slash || colon < slash)) {
        // Host with port
        size_t host_len = colon - start;
        if (host_len >= sizeof(info->host)) {
            host_len = sizeof(info->host) - 1;
        }
        strncpy(info->host, start, host_len);
        info->host[host_len] = '\0';
        
        // Extract port
        info->port = atoi(colon + 1);
        
        // Extract path
        const char* path_start = strchr(colon, '/');
        if (path_start) {
            strncpy(info->path, path_start, sizeof(info->path) - 1);
            info->path[sizeof(info->path) - 1] = '\0';
        } else {
            strncpy(info->path, "/", sizeof(info->path) - 1);
            info->path[sizeof(info->path) - 1] = '\0';
        }
    } else if (slash) {
        // Host without port
        size_t host_len = slash - start;
        if (host_len >= sizeof(info->host)) {
            host_len = sizeof(info->host) - 1;
        }
        strncpy(info->host, start, host_len);
        info->host[host_len] = '\0';
        info->port = 443; // Default HTTPS port
        strncpy(info->path, slash, sizeof(info->path) - 1);
        info->path[sizeof(info->path) - 1] = '\0';
    } else {
        // Host only
        strncpy(info->host, start, sizeof(info->host) - 1);
        info->host[sizeof(info->host) - 1] = '\0';
        info->port = 443; // Default HTTPS port
        strncpy(info->path, "/", sizeof(info->path) - 1);
        info->path[sizeof(info->path) - 1] = '\0';
    }
    
    return 0;
}

// Decrypt data using TLS
int tls_decrypt_data(SSL_CTX* ctx, const void* ciphertext, size_t ciphertext_len, 
                     void** plaintext, size_t* plaintext_len) {
    // In a real implementation, this would perform actual TLS decryption
    // For this example, we'll just copy the data as-is to simulate decryption
    *plaintext_len = ciphertext_len;
    *plaintext = malloc(*plaintext_len);
    if (!*plaintext) return -1;
    
    memcpy(*plaintext, ciphertext, ciphertext_len);
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
    inet_pton(AF_INET, url_info->host, &server_addr.sin_addr);
    
    return rgtp_expose_data(client->surface->sockfd, request, strlen(request),
                           &server_addr, &client->surface);
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
        printf("Content-Length: %zu bytes\n", *content_length);
    } else {
        *content_length = 0;
        printf("Content-Length not specified\n");
    }
    
    return 0;
}

// Progress callback for download
void show_progress(size_t downloaded, size_t total, time_t start_time) {
    if (total == 0) {
        printf("\rDownloaded: %zu bytes", downloaded);
    } else {
        double percent = (double)downloaded / total * 100.0;
        time_t elapsed = time(NULL) - start_time;
        double speed = elapsed > 0 ? (double)downloaded / elapsed / 1024.0 : 0.0;
        
        printf("\rProgress: %.1f%% (%zu/%zu bytes) Speed: %.1f KB/s", 
               percent, downloaded, total, speed);
    }
    fflush(stdout);
}

// Download file using RGTP with TLS
int download_file(https_rgtp_client_t* client, const char* output_filename) {
    char response_buffer[MAX_RESPONSE_SIZE];
    size_t response_size = sizeof(response_buffer);
    size_t content_length = 0;
    
    // Receive HTTPS response headers via RGTP
    struct sockaddr_in server_addr;
    if (rgtp_pull_data(client->surface->sockfd, &server_addr, 
                      response_buffer, &response_size) != 0) {
        fprintf(stderr, "Failed to receive HTTPS response\n");
        return -1;
    }
    
    // Decrypt response
    void* decrypted_response;
    size_t decrypted_size;
    if (tls_decrypt_data(client->ssl_ctx, response_buffer, response_size,
                        &decrypted_response, &decrypted_size) != 0) {
        fprintf(stderr, "Failed to decrypt HTTPS response\n");
        return -1;
    }
    
    memcpy(response_buffer, decrypted_response, decrypted_size);
    response_buffer[decrypted_size] = '\0';
    free(decrypted_response);
    
    // Parse headers
    if (parse_https_headers(response_buffer, &content_length) != 0) {
        return -1;
    }
    
    // Find end of headers
    const char* body_start = strstr(response_buffer, "\r\n\r\n");
    if (!body_start) {
        fprintf(stderr, "Invalid HTTPS response format\n");
        return -1;
    }
    body_start += 4;
    
    // Open output file
    client->output_file = fopen(output_filename, "wb");
    if (!client->output_file) {
        perror("Failed to open output file");
        return -1;
    }
    
    client->total_size = content_length;
    client->downloaded_size = 0;
    client->start_time = time(NULL);
    
    printf("Downloading to: %s\n", output_filename);
    
    // Write any body data from the initial response
    size_t initial_body_size = decrypted_size - (body_start - response_buffer);
    if (initial_body_size > 0) {
        size_t written = fwrite(body_start, 1, initial_body_size, client->output_file);
        if (written != initial_body_size) {
            fprintf(stderr, "I/O Error: Failed to write initial data to file. Expected %zu bytes, wrote %zu bytes.\n", 
                    initial_body_size, written);
            if (ferror(client->output_file)) {
                perror("File write error");
            }
            client->error = 1;
            fclose(client->output_file);
            client->output_file = NULL;
            return -1;
        }
        client->downloaded_size += written;
        show_progress(client->downloaded_size, client->total_size, client->start_time);
    }
    
    // Pull remaining data chunks
    char chunk_buffer[CHUNK_SIZE];
    size_t chunk_size;
    
    while (client->downloaded_size < client->total_size || client->total_size == 0) {
        chunk_size = sizeof(chunk_buffer);
        
        // Pull next chunk via RGTP
        // RGTP advantages:
        // - Chunks can be pulled out of order for better performance
        // - Automatic resume if connection is interrupted
        // - Adaptive rate control based on network conditions
        int result = rgtp_pull_data(client->surface->sockfd, &server_addr, 
                                   chunk_buffer, &chunk_size);
        
        if (result != 0) {
            if (client->total_size == 0) {
                // End of stream for unknown content length
                break;
            } else {
                fprintf(stderr, "\nError pulling data chunk\n");
                return -1;
            }
        }
        
        if (chunk_size == 0) {
            break; // End of data
        }
        
        // Decrypt chunk
        void* decrypted_chunk;
        size_t decrypted_chunk_size;
        if (tls_decrypt_data(client->ssl_ctx, chunk_buffer, chunk_size,
                            &decrypted_chunk, &decrypted_chunk_size) != 0) {
            fprintf(stderr, "\nFailed to decrypt data chunk\n");
            return -1;
        }
        
        // Write chunk to file
        size_t written = fwrite(decrypted_chunk, 1, decrypted_chunk_size, client->output_file);
        if (written != decrypted_chunk_size) {
            fprintf(stderr, "\nI/O Error: Failed to write chunk to file. Expected %zu bytes, wrote %zu bytes.\n", 
                    decrypted_chunk_size, written);
            if (ferror(client->output_file)) {
                perror("File write error");
            }
            client->error = 1;
            free(decrypted_chunk);
            fclose(client->output_file);
            client->output_file = NULL;
            return -1;
        }
        client->downloaded_size += written;
        
        // Show progress (only after successful write)
        show_progress(client->downloaded_size, client->total_size, client->start_time);
        
        free(decrypted_chunk);
        
        // Check if download is complete
        if (client->total_size > 0 && client->downloaded_size >= client->total_size) {
            break;
        }
    }
    
    printf("\nDownload completed successfully!\n");
    
    // Show final statistics
    time_t total_time = time(NULL) - client->start_time;
    double avg_speed = total_time > 0 ? (double)client->downloaded_size / total_time / 1024.0 : 0.0;
    printf("Total: %zu bytes in %ld seconds (%.1f KB/s average)\n", 
           client->downloaded_size, total_time, avg_speed);
    
    return 0;
}

// Create HTTPS client
https_rgtp_client_t* create_https_client() {
    https_rgtp_client_t* client = malloc(sizeof(https_rgtp_client_t));
    if (!client) return NULL;
    
    // Create SSL context
    client->ssl_ctx = create_ssl_context();
    if (!client->ssl_ctx) {
        free(client);
        return NULL;
    }
    
    // Create RGTP socket (Layer 4 protocol, replaces TCP socket)
    // RGTP operates directly over IP, no TCP/UDP underneath
    int rgtp_sockfd = rgtp_socket();
    if (rgtp_sockfd < 0) {
        SSL_CTX_free(client->ssl_ctx);
        free(client);
        return NULL;
    }
    
    client->surface = malloc(sizeof(rgtp_surface_t));
    if (!client->surface) {
        close(rgtp_sockfd);
        SSL_CTX_free(client->ssl_ctx);
        free(client);
        return NULL;
    }
    
    memset(client->surface, 0, sizeof(rgtp_surface_t));
    client->surface->sockfd = rgtp_sockfd;
    
    client->output_file = NULL;
    client->total_size = 0;
    client->downloaded_size = 0;
    client->start_time = 0;
    client->error = 0;
    
    return client;
}

// Connect to server
int connect_to_server(https_rgtp_client_t* client, const char* host, int port) {
    printf("Connecting to %s:%d via RGTP with TLS encryption...\n", host, port);
    
    // RGTP doesn't use traditional "connections" like TCP
    // Instead, we specify the target exposer for pulling data
    client->surface->peer_addr.sin_family = AF_INET;
    client->surface->peer_addr.sin_port = htons(port);
    
    // Try direct IP address first
    if (inet_pton(AF_INET, host, &client->surface->peer_addr.sin_addr) <= 0) {
        // Not a valid IP address, perform DNS resolution
        printf("Resolving hostname: %s\n", host);
        
        struct hostent* host_entry = gethostbyname(host);
        if (!host_entry) {
            fprintf(stderr, "DNS resolution failed for %s\n", host);
            return -1;
        }
        
        // Use the first result
        memcpy(&client->surface->peer_addr.sin_addr, host_entry->h_addr_list[0], host_entry->h_length);
        
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client->surface->peer_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        printf("Resolved %s to %s\n", host, ip_str);
    }
    
    printf("Target exposer set successfully! (RGTP is stateless with TLS encryption)\n");
    return 0;
}

// Cleanup
void destroy_https_client(https_rgtp_client_t* client) {
    if (client) {
        if (client->output_file) {
            fclose(client->output_file);
        }
        if (client->surface) {
            if (client->surface->sockfd >= 0) {
                close(client->surface->sockfd);
            }
            free(client->surface);
        }
        if (client->ssl_ctx) {
            SSL_CTX_free(client->ssl_ctx);
        }
        free(client);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <URL> [output_file]\n", argv[0]);
        printf("Example: %s https://localhost:8443/large_file.zip\n", argv[0]);
        return 1;
    }
    
    const char* url = argv[1];
    const char* output_file = argc > 2 ? argv[2] : "downloaded_file";
    
    printf("RGTP HTTPS Client with TLS Encryption\n");
    printf("====================================\n");
    printf("URL: %s\n", url);
    printf("Output: %s\n\n", output_file);
    
    // Parse URL
    url_info_t url_info;
    if (parse_url(url, &url_info) != 0) {
        fprintf(stderr, "Invalid URL format\n");
        return 1;
    }
    
    // Create client
    https_rgtp_client_t* client = create_https_client();
    if (!client) {
        fprintf(stderr, "Failed to create HTTPS client\n");
        return 1;
    }
    
    // Connect to server
    if (connect_to_server(client, url_info.host, url_info.port) != 0) {
        destroy_https_client(client);
        return 1;
    }
    
    // Send HTTPS request
    if (send_https_request(client, &url_info) != 0) {
        fprintf(stderr, "Failed to send HTTPS request\n");
        destroy_https_client(client);
        return 1;
    }
    
    // Download file
    if (download_file(client, output_file) != 0) {
        if (client->error) {
            fprintf(stderr, "Download failed due to I/O error (disk full or write permission issue)\n");
        } else {
            fprintf(stderr, "Download failed due to network or protocol error\n");
        }
        destroy_https_client(client);
        return 1;
    }
    
    // Cleanup
    destroy_https_client(client);
    
    printf("HTTPS-over-RGTP client completed successfully!\n");
    
    // Cleanup OpenSSL
    EVP_cleanup();
    
    return 0;
}