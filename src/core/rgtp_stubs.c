/**
 * @file rgtp_stubs.c
 * @brief Stub implementations for unimplemented API functions.
 *
 * These stubs allow the library to build and link even when full
 * implementations are not yet available. They return RGTP_ERR_NOT_SUPPORTED
 * for functionality that requires full implementation.
 */

#include "rgtp/rgtp.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Exposer API stubs
 * ═══════════════════════════════════════════════════════════════════════════ */

rgtp_error_t rgtp_expose(rgtp_socket_t* sock,
                          const void* data,
                          size_t size,
                          const rgtp_config_t* cfg,
                          rgtp_surface_t** out_surface)
{
    (void)sock; (void)data; (void)size; (void)cfg;
    if (!out_surface) return RGTP_ERR_INVALID_ARG;
    *out_surface = NULL;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_poll(rgtp_surface_t* surface, int timeout_ms)
{
    (void)surface; (void)timeout_ms;
    return RGTP_ERR_NOT_SUPPORTED;
}

void rgtp_destroy_surface(rgtp_surface_t* surface)
{
    (void)surface;
}

rgtp_error_t rgtp_get_exposure_id(const rgtp_surface_t* surface,
                                    uint8_t out_id[16])
{
    (void)surface;
    if (!out_id) return RGTP_ERR_INVALID_ARG;
    memset(out_id, 0, 16);
    return RGTP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Puller API stubs
 * ═══════════════════════════════════════════════════════════════════════════ */

rgtp_error_t rgtp_pull_start(rgtp_socket_t* sock,
                              const struct sockaddr_storage* server,
                              const uint8_t exposure_id[16],
                              const rgtp_config_t* cfg,
                              rgtp_surface_t** out_surface)
{
    (void)sock; (void)server; (void)exposure_id; (void)cfg;
    if (!out_surface) return RGTP_ERR_INVALID_ARG;
    *out_surface = NULL;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_pull_next(rgtp_surface_t* surface,
                              void* buffer,
                              size_t buf_size,
                              size_t* out_received,
                              uint32_t* out_chunk_index)
{
    (void)surface; (void)buffer; (void)buf_size;
    if (out_received) *out_received = 0;
    if (out_chunk_index) *out_chunk_index = 0;
    return RGTP_ERR_NOT_SUPPORTED;
}

float rgtp_progress(const rgtp_surface_t* surface)
{
    (void)surface;
    return 0.0f;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Statistics stubs
 * ═══════════════════════════════════════════════════════════════════════════ */

rgtp_error_t rgtp_get_stats(const rgtp_surface_t* surface,
                             rgtp_stats_t* out)
{
    (void)surface;
    if (!out) return RGTP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    return RGTP_OK;
}

rgtp_error_t rgtp_get_latency_stats(const rgtp_surface_t* surface,
                                      rgtp_latency_stats_t* out)
{
    (void)surface;
    if (!out) return RGTP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    return RGTP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Logging stubs
 * ═══════════════════════════════════════════════════════════════════════════ */

void rgtp_set_log_callback(rgtp_log_fn fn, void* ctx)
{
    (void)fn; (void)ctx;
}

void rgtp_set_log_level(int level)
{
    (void)level;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Satellite API stubs
 * ═══════════════════════════════════════════════════════════════════════════ */

rgtp_error_t rgtp_get_satellite_stats(const rgtp_surface_t* surface,
                                       rgtp_satellite_stats_t* out)
{
    (void)surface;
    if (!out) return RGTP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    return RGTP_OK;
}

rgtp_error_t rgtp_schedule_contact(rgtp_surface_t* surface,
                                    uint64_t start_time,
                                    uint64_t end_time,
                                    const char* ground_station)
{
    (void)surface; (void)start_time; (void)end_time; (void)ground_station;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_update_link_parameters(rgtp_surface_t* surface,
                                          float snr_db,
                                          float ber,
                                          int32_t doppler_hz)
{
    (void)surface; (void)snr_db; (void)ber; (void)doppler_hz;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_enable_store_forward(rgtp_surface_t* surface,
                                         uint64_t max_storage_bytes,
                                         uint32_t max_storage_time_s)
{
    (void)surface; (void)max_storage_bytes; (void)max_storage_time_s;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_get_contact_windows(const rgtp_surface_t* surface,
                                       rgtp_contact_window_t* windows,
                                       size_t max_windows,
                                       size_t* out_count)
{
    (void)surface;
    if (!out_count) return RGTP_ERR_INVALID_ARG;
    *out_count = 0;
    (void)windows; (void)max_windows;
    return RGTP_OK;
}

rgtp_error_t rgtp_configure_ccsds(rgtp_surface_t* surface,
                                   bool enable_tm,
                                   bool enable_tc,
                                   bool enable_aos,
                                   bool enable_cfdp)
{
    (void)surface; (void)enable_tm; (void)enable_tc; (void)enable_aos; (void)enable_cfdp;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_send_emergency_tc(rgtp_surface_t* surface,
                                     const void* tc_data,
                                     size_t tc_size,
                                     uint8_t priority)
{
    (void)surface; (void)tc_data; (void)tc_size; (void)priority;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_calculate_link_budget(const rgtp_surface_t* surface,
                                         float* out_margin_db,
                                         float* out_ebno_db)
{
    (void)surface;
    if (!out_margin_db || !out_ebno_db) return RGTP_ERR_INVALID_ARG;
    *out_margin_db = 0.0f;
    *out_ebno_db = 0.0f;
    return RGTP_OK;
}

rgtp_error_t rgtp_configure_doppler(rgtp_surface_t* surface,
                                     bool enable_compensation,
                                     uint32_t max_doppler_hz,
                                     float update_rate_hz)
{
    (void)surface; (void)enable_compensation; (void)max_doppler_hz; (void)update_rate_hz;
    return RGTP_ERR_NOT_SUPPORTED;
}

rgtp_error_t rgtp_check_contact_status(const rgtp_surface_t* surface,
                                        bool* out_in_contact,
                                        uint32_t* out_time_to_contact_s,
                                        uint32_t* out_time_left_s)
{
    (void)surface;
    if (!out_in_contact) return RGTP_ERR_INVALID_ARG;
    *out_in_contact = false;
    if (out_time_to_contact_s) *out_time_to_contact_s = 0;
    if (out_time_left_s) *out_time_left_s = 0;
    return RGTP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Allocator stub
 * ═══════════════════════════════════════════════════════════════════════════ */

rgtp_error_t rgtp_set_allocator(const rgtp_allocator_t* alloc)
{
    (void)alloc;
    return RGTP_ERR_NOT_SUPPORTED;
}