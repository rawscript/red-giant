#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main() {
    const char* filename = "large_test_file.dat";
    const size_t file_size = 100 * 1024 * 1024; // 100 MB file
    
    printf("Creating %zu MB test file: %s\n", file_size / (1024 * 1024), filename);
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("Error: Failed to create test file\n");
        return 1;
    }
    
    // Create a buffer with pattern data
    char* buffer = malloc(1024 * 1024); // 1MB buffer
    if (!buffer) {
        printf("Error: Failed to allocate buffer\n");
        fclose(file);
        return 1;
    }
    
    // Fill buffer with pattern
    for (int i = 0; i < 1024 * 1024; i++) {
        buffer[i] = (char)(i % 256);
    }
    
    // Write data to file
    size_t written = 0;
    while (written < file_size) {
        size_t to_write = file_size - written;
        if (to_write > 1024 * 1024) {
            to_write = 1024 * 1024;
        }
        
        size_t result = fwrite(buffer, 1, to_write, file);
        if (result != to_write) {
            printf("Error: Failed to write data to file\n");
            free(buffer);
            fclose(file);
            return 1;
        }
        
        written += result;
    }
    
    free(buffer);
    fclose(file);
    
    printf("Test file created successfully: %zu bytes\n", file_size);
    return 0;
}