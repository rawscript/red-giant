#include "rgtp/rgtp.h"
#include "rgtp_satellite_internal.h"
#include "../transport/rgtp_surface_internal.h"
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

static uint64_t api_get_time_us(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t tmp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    tmp -= 116444736000000000ULL;
    return tmp / 10;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
#endif
}

rgtp_error_t rgtp_get_satellite_stats(const rgtp_surface_t* surface,
                                      rgtp_satellite_stats_t* out) {
    if (!surface || !out) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!surface->config.satellite_mode) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_satellite_context_t* sat_ctx = (rgtp_satellite_context_t*)surface->sat_context;
    if (!sat_ctx || !sat_ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    memset(out, 0, sizeof(*out));
    
    out->snr_db = sat_ctx->link_quality.snr_db;
    out->ber = sat_ctx->link_quality.ber;
    out->link_margin_db = sat_ctx->link_quality.link_margin_db;
    out->doppler_offset_hz = sat_ctx->doppler.current_offset_hz;
    
    out->contact_attempts = atomic_load(&sat_ctx->schedule.contact_attempts);
    out->successful_contacts = atomic_load(&sat_ctx->schedule.successful_contacts);
    
    uint64_t now_us = api_get_time_us();
    if (sat_ctx->schedule.active_index < sat_ctx->schedule.count) {
        rgtp_contact_window_internal_t* active = 
            &sat_ctx->schedule.windows[sat_ctx->schedule.active_index];
        uint64_t start_us = active->start_time * 1000000ULL;
        uint64_t end_us = active->end_time * 1000000ULL;
        
        if (now_us >= start_us && now_us < end_us) {
            out->contact_duration_s = (uint32_t)((now_us - start_us) / 1000000ULL);
        }
    }
    
    uint64_t next_contact_us = UINT64_MAX;
    for (uint32_t i = 0; i < sat_ctx->schedule.count; i++) {
        uint64_t start_us = sat_ctx->schedule.windows[i].start_time * 1000000ULL;
        if (start_us > now_us && start_us < next_contact_us) {
            next_contact_us = start_us;
        }
    }
    
    if (next_contact_us != UINT64_MAX) {
        out->time_to_contact_s = (uint32_t)((next_contact_us - now_us) / 1000000ULL);
    } else {
        out->time_to_contact_s = UINT32_MAX;
    }
    
    out->stored_bytes = sat_ctx->store_forward.total_bytes;
    out->stored_chunks = sat_ctx->store_forward.count;
    out->store_overflow_count = atomic_load(&sat_ctx->store_forward.overflow_count);
    
    out->spacecraft_health = sat_ctx->spacecraft.health;
    out->power_level = sat_ctx->spacecraft.power_level;
    out->antenna_status = sat_ctx->spacecraft.antenna_status;
    
    out->ccsds_frames_sent = atomic_load(&sat_ctx->ccsds_stats.tm_frames_sent) +
                             atomic_load(&sat_ctx->ccsds_stats.aos_frames_sent);
    out->ccsds_frames_received = atomic_load(&sat_ctx->ccsds_stats.tm_frames_received) +
                                 atomic_load(&sat_ctx->ccsds_stats.aos_frames_received);
    out->ccsds_frame_errors = atomic_load(&sat_ctx->ccsds_stats.frame_errors);
    out->ccsds_vcdu_count = atomic_load(&sat_ctx->ccsds_stats.vcdu_count);
    
    return RGTP_OK;
}

rgtp_error_t rgtp_schedule_contact(rgtp_surface_t* surface,
                                   uint64_t start_time,
                                   uint64_t end_time,
                                   const char* ground_station) {
    if (!surface || !ground_station) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!surface->config.satellite_mode) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_satellite_context_t* sat_ctx = (rgtp_satellite_context_t*)surface->sat_context;
    if (!sat_ctx || !sat_ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    return rgtp_satellite_schedule_contact(sat_ctx, start_time, end_time, ground_station);
}

rgtp_error_t rgtp_update_link_parameters(rgtp_surface_t* surface,
                                         float snr_db,
                                         float ber,
                                         int32_t doppler_hz) {
    if (!surface) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!surface->config.satellite_mode) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_satellite_context_t* sat_ctx = (rgtp_satellite_context_t*)surface->sat_context;
    if (!sat_ctx || !sat_ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    return rgtp_satellite_update_link_quality(sat_ctx, snr_db, ber, doppler_hz);
}

rgtp_error_t rgtp_enable_store_forward(rgtp_surface_t* surface,
                                       uint64_t max_storage_bytes,
                                       uint32_t max_storage_time_s) {
    if (!surface) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!surface->config.satellite_mode) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_satellite_context_t* sat_ctx = (rgtp_satellite_context_t*)surface->sat_context;
    if (!sat_ctx || !sat_ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    sat_ctx->store_forward_enabled = true;
    sat_ctx->store_forward.max_bytes = max_storage_bytes;
    sat_ctx->store_forward.max_time_s = max_storage_time_s;
    
    return RGTP_OK;
}

rgtp_error_t rgtp_get_contact_windows(const rgtp_surface_t* surface,
                                      struct rgtp_contact_window* windows,
                                      size_t max_windows,
                                      size_t* out_count) {
    if (!surface || !windows || !out_count) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!surface->config.satellite_mode) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_satellite_context_t* sat_ctx = (rgtp_satellite_context_t*)surface->sat_context;
    if (!sat_ctx || !sat_ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    size_t count = sat_ctx->schedule.count < max_windows ? 
                   sat_ctx->schedule.count : max_windows;
    
    for (size_t i = 0; i < count; i++) {
        windows[i].start_time = sat_ctx->schedule.windows[i].start_time;
        windows[i].end_time = sat_ctx->schedule.windows[i].end_time;
        strncpy(windows[i].ground_station, 
                sat_ctx->schedule.windows[i].ground_station,
                sizeof(windows[i].ground_station) - 1);
        windows[i].ground_station[sizeof(windows[i].ground_station) - 1] = '\0';
    }
    
    *out_count = count;
    
    return RGTP_OK;
}

rgtp_error_t rgtp_configure_ccsds(rgtp_surface_t* surface,
                                  bool enable_tm,
                                  bool enable_tc,
                                  bool enable_aos,
                                  bool enable_cfdp) {
    if (!surface) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!surface->config.satellite_mode) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_satellite_context_t* sat_ctx = (rgtp_satellite_context_t*)surface->sat_context;
    if (!sat_ctx || !sat_ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    sat_ctx->ccsds_tm_enabled = enable_tm;
    sat_ctx->ccsds_tc_enabled = enable_tc;
    sat_ctx->ccsds_aos_enabled = enable_aos;
    sat_ctx->ccsds_cfdp_enabled = enable_cfdp;
    
    return RGTP_OK;
}

rgtp_error_t rgtp_send_emergency_tc(rgtp_surface_t* surface,
                                    const void* tc_data,
                                    size_t tc_size,
                                    uint8_t priority) {
    if (!surface || !tc_data || tc_size == 0) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!surface->config.satellite_mode) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_satellite_context_t* sat_ctx = (rgtp_satellite_context_t*)surface->sat_context;
    if (!sat_ctx || !sat_ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!sat_ctx->ccsds_tc_enabled) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    uint64_t now_us = api_get_time_us();
    if (!rgtp_satellite_in_contact_window(sat_ctx, now_us)) {
        if (!sat_ctx->store_forward_enabled) {
            return RGTP_ERR_TIMEOUT;
        }
        
        rgtp_error_t err = rgtp_satellite_store_chunk(sat_ctx, 0, tc_data, tc_size, priority);
        if (err != RGTP_OK) {
            return err;
        }
        
        return RGTP_OK;
    }
    
    atomic_fetch_add(&sat_ctx->ccsds_stats.tc_frames_sent, 1);
    atomic_fetch_add(&sat_ctx->bytes_transmitted, tc_size);
    
    return RGTP_OK;
}

rgtp_error_t rgtp_calculate_link_budget(const rgtp_surface_t* surface,
                                        float* out_margin_db,
                                        float* out_ebno_db) {
    if (!surface || !out_margin_db || !out_ebno_db) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!surface->config.satellite_mode) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_satellite_context_t* sat_ctx = (rgtp_satellite_context_t*)surface->sat_context;
    if (!sat_ctx || !sat_ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    return rgtp_satellite_calculate_link_budget(sat_ctx, &surface->config, 
                                                out_margin_db, out_ebno_db);
}

rgtp_error_t rgtp_configure_doppler(rgtp_surface_t* surface,
                                    bool enable_compensation,
                                    uint32_t max_doppler_hz,
                                    float update_rate_hz) {
    if (!surface) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!surface->config.satellite_mode) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_satellite_context_t* sat_ctx = (rgtp_satellite_context_t*)surface->sat_context;
    if (!sat_ctx || !sat_ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    sat_ctx->doppler.compensation_enabled = enable_compensation;
    sat_ctx->doppler.max_doppler_hz = max_doppler_hz;
    sat_ctx->doppler.update_rate_hz = update_rate_hz;
    
    return RGTP_OK;
}

rgtp_error_t rgtp_check_contact_status(const rgtp_surface_t* surface,
                                       bool* out_in_contact,
                                       uint32_t* out_time_to_contact_s,
                                       uint32_t* out_time_left_s) {
    if (!surface || !out_in_contact || !out_time_to_contact_s || !out_time_left_s) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!surface->config.satellite_mode) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_satellite_context_t* sat_ctx = (rgtp_satellite_context_t*)surface->sat_context;
    if (!sat_ctx || !sat_ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    uint64_t now_us = api_get_time_us();
    uint64_t now_s = now_us / 1000000ULL;
    
    *out_in_contact = false;
    *out_time_to_contact_s = UINT32_MAX;
    *out_time_left_s = 0;
    
    uint64_t next_contact_s = UINT64_MAX;
    
    for (uint32_t i = 0; i < sat_ctx->schedule.count; i++) {
        rgtp_contact_window_internal_t* window = &sat_ctx->schedule.windows[i];
        
        if (now_s >= window->start_time && now_s < window->end_time) {
            *out_in_contact = true;
            *out_time_left_s = (uint32_t)(window->end_time - now_s);
            return RGTP_OK;
        }
        
        if (window->start_time > now_s && window->start_time < next_contact_s) {
            next_contact_s = window->start_time;
        }
    }
    
    if (next_contact_s != UINT64_MAX) {
        *out_time_to_contact_s = (uint32_t)(next_contact_s - now_s);
    }
    
    return RGTP_OK;
}
