// Red Giant Protocol - C Core Implementation
#include "red_giant.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

rg_exposure_surface_t* rg_create_surface(const rg_manifest_t* manifest) {
    rg_exposure_surface_t* surface = calloc(1, sizeof(rg_exposure_surface_t));
    if (!surface) return NULL;
    
    // Copy manifest
    memcpy(&surface->manifest, manifest, sizeof(rg_manifest_t));
    
    // Allocate chunk array
    surface->chunks = calloc(manifest->total_chunks, sizeof(rg_chunk_t));
    if (!surface->chunks) {
        free(surface);
        return NULL;
    }
    
    // Initialize chunks
    for (uint32_t i = 0; i < manifest->total_chunks; i++) {
        surface->chunks[i].sequence_id = i;
        surface->chunks[i].offset = i * manifest->chunk_size;
        surface->chunks[i].data_size = (i == manifest->total_chunks - 1) ? 
            manifest->total_size % manifest->chunk_size : manifest->chunk_size;
        surface->chunks[i].is_exposed = false;
    }
    
    // Allocate shared buffer for exposure
    surface->buffer_size = manifest->chunk_size * 2; // Double buffer
    surface->shared_buffer = malloc(surface->buffer_size);
    
    return surface;
}

void rg_destroy_surface(rg_exposure_surface_t* surface) {
    if (!surface) return;
    
    free(surface->chunks);
    free(surface->shared_buffer);
    free(surface);
}

bool rg_expose_chunk(rg_exposure_surface_t* surface, uint32_t chunk_id, const void* data) {
    if (!surface || chunk_id >= surface->manifest.total_chunks) return false;
    
    rg_chunk_t* chunk = &surface->chunks[chunk_id];
    
    // Copy data to shared buffer region
    size_t offset = (chunk_id % 2) * surface->manifest.chunk_size;
    chunk->data_ptr = (uint8_t*)surface->shared_buffer + offset;
    memcpy(chunk->data_ptr, data, chunk->data_size);
    
    // Mark as exposed
    chunk->is_exposed = true;
    surface->exposed_count++;
    
    printf("[EXPOSE] Chunk %u exposed at offset %lu\n", chunk_id, chunk->offset);
    return true;
}

const rg_chunk_t* rg_peek_chunk(rg_exposure_surface_t* surface, uint32_t chunk_id) {
    if (!surface || chunk_id >= surface->manifest.total_chunks) return NULL;
    
    rg_chunk_t* chunk = &surface->chunks[chunk_id];
    return chunk->is_exposed ? chunk : NULL;
}

bool rg_pull_chunk(rg_exposure_surface_t* surface, uint32_t chunk_id, void* dest_buffer) {
    const rg_chunk_t* chunk = rg_peek_chunk(surface, chunk_id);
    if (!chunk) return false;
    
    memcpy(dest_buffer, chunk->data_ptr, chunk->data_size);
    printf("[PULL] Chunk %u pulled (%u bytes)\n", chunk_id, chunk->data_size);
    return true;
}

void rg_raise_red_flag(rg_exposure_surface_t* surface) {
    if (!surface) return;
    surface->red_flag_raised = true;
    printf("[RED FLAG] 🚩 All chunks exposed - transmission complete!\n");
}

bool rg_is_complete(const rg_exposure_surface_t* surface) {
    return surface && surface->red_flag_raised && 
           surface->exposed_count == surface->manifest.total_chunks;
}