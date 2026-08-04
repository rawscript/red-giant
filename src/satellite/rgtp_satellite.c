#include "rgtp_satellite_internal.h"
#include "rgtp/rgtp.h"
#include "../core/rgtp_alloc_internal.h"
#include "../observability/rgtp_log_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

static uint64_t get_time_us(void) {
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

static float calculate_link_margin_internal(uint8_t link_type, float snr_db, float distance_km) {
    float required_snr_db;
    
    switch (link_type) {
        case RGTP_SPACE_LINK_UHF:
            required_snr_db = 10.0f;
            break;
        case RGTP_SPACE_LINK_SBAND:
            required_snr_db = 12.0f;
            break;
        case RGTP_SPACE_LINK_XBAND:
            required_snr_db = 14.0f;
            break;
        case RGTP_SPACE_LINK_KABAND:
            required_snr_db = 16.0f;
            break;
        case RGTP_SPACE_LINK_OPTICAL:
            required_snr_db = 20.0f;
            break;
        default:
            required_snr_db = 10.0f;
    }
    
    float path_loss_db = 20.0f * log10f(distance_km) + 92.45f;
    float margin = snr_db - required_snr_db - (path_loss_db * 0.01f);
    
    return margin;
}

static float calculate_eb_no(float snr_db, float bit_rate_bps, float bandwidth_hz) {
    float snr_linear = powf(10.0f, snr_db / 10.0f);
    float eb_no_linear = snr_linear * (bandwidth_hz / bit_rate_bps);
    return 10.0f * log10f(eb_no_linear);
}

rgtp_error_t rgtp_satellite_init(rgtp_satellite_context_t* ctx, const rgtp_config_t* cfg) {
    if (!ctx || !cfg) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->initialized = true;
    ctx->satellite_mode = cfg->satellite_mode;
    ctx->space_link_type = cfg->space_link_type;
    ctx->max_rtt_ms = cfg->max_rtt_ms;
    ctx->link_asymmetry = cfg->link_asymmetry;
    ctx->min_snr_db = cfg->min_snr_db;
    ctx->max_ber = cfg->max_ber;
    
    ctx->ccsds_tm_enabled = cfg->ccsds_tm;
    ctx->ccsds_tc_enabled = cfg->ccsds_tc;
    ctx->ccsds_aos_enabled = cfg->ccsds_aos;
    ctx->ccsds_cfdp_enabled = cfg->ccsds_cfdp;
    ctx->apid = cfg->apid;
    ctx->spacecraft_id = cfg->spacecraft_id;
    
    ctx->link_status = RGTP_LINK_STATUS_DOWN;
    ctx->link_quality.snr_db = 0.0f;
    ctx->link_quality.ber = 1.0f;
    ctx->link_quality.link_margin_db = -100.0f;
    ctx->link_quality.timestamp_us = get_time_us();
    
    ctx->doppler.compensation_enabled = false;
    ctx->doppler.max_doppler_hz = cfg->doppler_shift_hz;
    ctx->doppler.current_offset_hz = 0;
    ctx->doppler.head = 0;
    ctx->doppler.count = 0;
    
    ctx->schedule.count = 0;
    ctx->schedule.active_index = UINT32_MAX;
    atomic_init(&ctx->schedule.contact_attempts, 0);
    atomic_init(&ctx->schedule.successful_contacts, 0);
    
    ctx->spacecraft.health = 100;
    ctx->spacecraft.power_level = 100;
    ctx->spacecraft.antenna_status = RGTP_ANTENNA_DOWN;
    ctx->spacecraft.temperature_c = 20.0f;
    ctx->spacecraft.uptime_s = 0;
    ctx->spacecraft.last_update_us = get_time_us();
    
    memset(&ctx->store_forward, 0, sizeof(ctx->store_forward));
    ctx->store_forward_enabled = cfg->store_and_forward;
    ctx->store_forward.max_bytes = 0;
    ctx->store_forward.max_time_s = 0;
    atomic_init(&ctx->store_forward.overflow_count, 0);
    
    atomic_init(&ctx->ccsds_stats.tm_frames_sent, 0);
    atomic_init(&ctx->ccsds_stats.tm_frames_received, 0);
    atomic_init(&ctx->ccsds_stats.tc_frames_sent, 0);
    atomic_init(&ctx->ccsds_stats.tc_frames_received, 0);
    atomic_init(&ctx->ccsds_stats.aos_frames_sent, 0);
    atomic_init(&ctx->ccsds_stats.aos_frames_received, 0);
    atomic_init(&ctx->ccsds_stats.cfdp_pdus_sent, 0);
    atomic_init(&ctx->ccsds_stats.cfdp_pdus_received, 0);
    atomic_init(&ctx->ccsds_stats.frame_errors, 0);
    atomic_init(&ctx->ccsds_stats.vcdu_count, 0);
    
    atomic_init(&ctx->bytes_transmitted, 0);
    atomic_init(&ctx->bytes_received, 0);
    
    return RGTP_OK;
}

void rgtp_satellite_destroy(rgtp_satellite_context_t* ctx) {
    if (!ctx || !ctx->initialized) {
        return;
    }
    
    for (uint32_t i = 0; i < ctx->store_forward.count; i++) {
        uint32_t idx = (ctx->store_forward.head + i) % RGTP_STORE_FORWARD_MAX_CHUNKS;
        if (ctx->store_forward.chunks[idx].data) {
            free(ctx->store_forward.chunks[idx].data);
            ctx->store_forward.chunks[idx].data = NULL;
        }
    }
    
    memset(ctx, 0, sizeof(*ctx));
}

rgtp_error_t rgtp_satellite_update_link_quality(rgtp_satellite_context_t* ctx,
                                                 float snr_db,
                                                 float ber,
                                                 int32_t doppler_hz) {
    if (!ctx || !ctx->initialized) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    uint64_t now_us = get_time_us();
    
    ctx->link_quality.snr_db = snr_db;
    ctx->link_quality.ber = ber;
    ctx->link_quality.timestamp_us = now_us;
    ctx->link_quality.sample_count++;
    
    ctx->link_quality.link_margin_db = calculate_link_margin_internal(
        ctx->space_link_type, snr_db, 1000.0f
    );
    
    ctx->link_quality.eb_no_db = calculate_eb_no(snr_db, 1000000.0f, 10000000.0f);
    
    if (snr_db >= ctx->min_snr_db && ber <= ctx->max_ber) {
        if (ctx->link_status == RGTP_LINK_STATUS_DOWN || 
            ctx->link_status == RGTP_LINK_STATUS_ACQUIRING) {
            ctx->link_status = RGTP_LINK_STATUS_LOCK;
        }
    } else if (snr_db >= ctx->min_snr_db * 0.7f) {
        ctx->link_status = RGTP_LINK_STATUS_DEGRADED;
    } else {
        ctx->link_status = RGTP_LINK_STATUS_DOWN;
    }
    
    if (ctx->doppler.compensation_enabled) {
        uint32_t idx = ctx->doppler.head;
        ctx->doppler.history[idx].offset_hz = doppler_hz;
        ctx->doppler.history[idx].timestamp_us = now_us;
        
        if (ctx->doppler.count > 1) {
            uint32_t prev_idx = (idx + RGTP_DOPPLER_HISTORY_SIZE - 1) % RGTP_DOPPLER_HISTORY_SIZE;
            uint64_t dt_us = now_us - ctx->doppler.history[prev_idx].timestamp_us;
            if (dt_us > 0) {
                float dt_s = dt_us / 1000000.0f;
                ctx->doppler.history[idx].rate_hz_per_sec = 
                    (doppler_hz - ctx->doppler.history[prev_idx].offset_hz) / dt_s;
            }
        }
        
        ctx->doppler.current_offset_hz = doppler_hz;
        ctx->doppler.head = (idx + 1) % RGTP_DOPPLER_HISTORY_SIZE;
        if (ctx->doppler.count < RGTP_DOPPLER_HISTORY_SIZE) {
            ctx->doppler.count++;
        }
        
        if (ctx->doppler.count >= 3) {
            float sum_rate = 0.0f;
            for (uint32_t i = 0; i < 3; i++) {
                uint32_t sample_idx = (ctx->doppler.head + RGTP_DOPPLER_HISTORY_SIZE - 1 - i) 
                                     % RGTP_DOPPLER_HISTORY_SIZE;
                sum_rate += ctx->doppler.history[sample_idx].rate_hz_per_sec;
            }
            float avg_rate = sum_rate / 3.0f;
            ctx->doppler.predicted_offset_hz = doppler_hz + (avg_rate * 0.1f);
        }
    }
    
    return RGTP_OK;
}

rgtp_error_t rgtp_satellite_schedule_contact(rgtp_satellite_context_t* ctx,
                                              uint64_t start_time,
                                              uint64_t end_time,
                                              const char* ground_station) {
    if (!ctx || !ctx->initialized || !ground_station) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (ctx->schedule.count >= RGTP_MAX_CONTACT_WINDOWS) {
        return RGTP_ERR_NOMEM;
    }
    
    if (start_time >= end_time) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    uint32_t idx = ctx->schedule.count;
    rgtp_contact_window_internal_t* window = &ctx->schedule.windows[idx];
    
    window->start_time = start_time;
    window->end_time = end_time;
    strncpy(window->ground_station, ground_station, sizeof(window->ground_station) - 1);
    window->ground_station[sizeof(window->ground_station) - 1] = '\0';
    window->max_elevation_deg = 0.0f;
    window->aos_azimuth_deg = 0.0f;
    window->los_azimuth_deg = 0.0f;
    window->active = false;
    window->data_transferred_bytes = 0;
    
    ctx->schedule.count++;
    
    return RGTP_OK;
}

bool rgtp_satellite_in_contact_window(const rgtp_satellite_context_t* ctx, uint64_t now_us) {
    if (!ctx || !ctx->initialized) {
        return false;
    }
    
    uint64_t now_s = now_us / 1000000ULL;
    
    for (uint32_t i = 0; i < ctx->schedule.count; i++) {
        const rgtp_contact_window_internal_t* window = &ctx->schedule.windows[i];
        if (now_s >= window->start_time && now_s < window->end_time) {
            return true;
        }
    }
    
    return false;
}

rgtp_error_t rgtp_satellite_store_chunk(rgtp_satellite_context_t* ctx,
                                         uint32_t chunk_index,
                                         const uint8_t* data,
                                         size_t size,
                                         uint8_t priority) {
    if (!ctx || !ctx->initialized || !data || size == 0) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!ctx->store_forward_enabled) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_store_forward_buffer_t* buf = &ctx->store_forward;
    
    if (buf->count >= RGTP_STORE_FORWARD_MAX_CHUNKS) {
        atomic_fetch_add(&buf->overflow_count, 1);
        return RGTP_ERR_NOMEM;
    }
    
    if (buf->max_bytes > 0 && (buf->total_bytes + size) > buf->max_bytes) {
        atomic_fetch_add(&buf->overflow_count, 1);
        return RGTP_ERR_NOMEM;
    }
    
    uint8_t* chunk_data = malloc(size);
    if (!chunk_data) {
        return RGTP_ERR_NOMEM;
    }
    
    memcpy(chunk_data, data, size);
    
    uint32_t idx = buf->tail;
    buf->chunks[idx].chunk_index = chunk_index;
    buf->chunks[idx].data = chunk_data;
    buf->chunks[idx].size = size;
    buf->chunks[idx].timestamp_us = get_time_us();
    buf->chunks[idx].priority = priority;
    
    buf->tail = (buf->tail + 1) % RGTP_STORE_FORWARD_MAX_CHUNKS;
    buf->count++;
    buf->total_bytes += size;
    
    return RGTP_OK;
}

rgtp_error_t rgtp_satellite_retrieve_chunk(rgtp_satellite_context_t* ctx,
                                            uint32_t* out_chunk_index,
                                            uint8_t* buffer,
                                            size_t* out_size) {
    if (!ctx || !ctx->initialized || !out_chunk_index || !buffer || !out_size) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    if (!ctx->store_forward_enabled) {
        return RGTP_ERR_NOT_SUPPORTED;
    }
    
    rgtp_store_forward_buffer_t* buf = &ctx->store_forward;
    
    if (buf->count == 0) {
        return RGTP_ERR_TIMEOUT;
    }
    
    uint32_t highest_priority = 0;
    uint32_t selected_idx = buf->head;
    
    for (uint32_t i = 0; i < buf->count; i++) {
        uint32_t idx = (buf->head + i) % RGTP_STORE_FORWARD_MAX_CHUNKS;
        if (buf->chunks[idx].priority > highest_priority) {
            highest_priority = buf->chunks[idx].priority;
            selected_idx = idx;
        }
    }
    
    rgtp_stored_chunk_t* chunk = &buf->chunks[selected_idx];
    
    if (*out_size < chunk->size) {
        *out_size = chunk->size;
        return RGTP_ERR_NOMEM;
    }
    
    memcpy(buffer, chunk->data, chunk->size);
    *out_chunk_index = chunk->chunk_index;
    *out_size = chunk->size;
    
    free(chunk->data);
    chunk->data = NULL;
    
    if (selected_idx == buf->head) {
        buf->head = (buf->head + 1) % RGTP_STORE_FORWARD_MAX_CHUNKS;
    } else {
        memmove(&buf->chunks[selected_idx], &buf->chunks[selected_idx + 1],
                (buf->tail - selected_idx - 1) * sizeof(rgtp_stored_chunk_t));
        if (buf->tail > 0) {
            buf->tail--;
        }
    }
    
    buf->count--;
    buf->total_bytes -= chunk->size;
    
    return RGTP_OK;
}

rgtp_error_t rgtp_satellite_calculate_link_budget(const rgtp_satellite_context_t* ctx,
                                                   const rgtp_config_t* cfg,
                                                   float* out_margin_db,
                                                   float* out_ebno_db) {
    if (!ctx || !ctx->initialized || !cfg || !out_margin_db || !out_ebno_db) {
        return RGTP_ERR_INVALID_ARG;
    }
    
    float tx_power_dbw;
    float frequency_ghz;
    float bandwidth_mhz;
    
    switch (ctx->space_link_type) {
        case RGTP_SPACE_LINK_UHF:
            tx_power_dbw = 40.0f;
            frequency_ghz = 0.4f;
            bandwidth_mhz = 1.0f;
            break;
        case RGTP_SPACE_LINK_SBAND:
            tx_power_dbw = 43.0f;
            frequency_ghz = 2.2f;
            bandwidth_mhz = 5.0f;
            break;
        case RGTP_SPACE_LINK_XBAND:
            tx_power_dbw = 46.0f;
            frequency_ghz = 8.4f;
            bandwidth_mhz = 50.0f;
            break;
        case RGTP_SPACE_LINK_KABAND:
            tx_power_dbw = 48.0f;
            frequency_ghz = 32.0f;
            bandwidth_mhz = 500.0f;
            break;
        case RGTP_SPACE_LINK_OPTICAL:
            tx_power_dbw = 30.0f;
            frequency_ghz = 200000.0f;
            bandwidth_mhz = 10000.0f;
            break;
        default:
            return RGTP_ERR_INVALID_ARG;
    }
    
    float distance_km = 1000.0f;
    float tx_antenna_gain_dbi = 30.0f;
    float rx_antenna_gain_dbi = 45.0f;
    float system_losses_db = 3.0f;
    float atmospheric_loss_db = 1.0f;
    
    float fspl_db = 20.0f * log10f(distance_km) + 
                    20.0f * log10f(frequency_ghz * 1000.0f) + 
                    92.45f;
    
    float eirp_dbw = tx_power_dbw + tx_antenna_gain_dbi;
    
    float received_power_dbw = eirp_dbw + 
                               rx_antenna_gain_dbi - 
                               fspl_db - 
                               system_losses_db - 
                               atmospheric_loss_db;
    
    float noise_temp_k = 150.0f;
    float boltzmann_dbw_k_hz = -228.6f;
    float noise_power_dbw = boltzmann_dbw_k_hz + 
                            10.0f * log10f(noise_temp_k) + 
                            10.0f * log10f(bandwidth_mhz * 1e6f);
    
    float cnr_db = received_power_dbw - noise_power_dbw;
    
    float required_ebno_db;
    switch (ctx->space_link_type) {
        case RGTP_SPACE_LINK_UHF:
            required_ebno_db = 10.0f;
            break;
        case RGTP_SPACE_LINK_SBAND:
            required_ebno_db = 12.0f;
            break;
        case RGTP_SPACE_LINK_XBAND:
            required_ebno_db = 14.0f;
            break;
        case RGTP_SPACE_LINK_KABAND:
            required_ebno_db = 16.0f;
            break;
        case RGTP_SPACE_LINK_OPTICAL:
            required_ebno_db = 20.0f;
            break;
        default:
            required_ebno_db = 10.0f;
    }
    
    float bit_rate_mbps = bandwidth_mhz * 0.8f;
    *out_ebno_db = cnr_db - 10.0f * log10f(bandwidth_mhz / bit_rate_mbps);
    *out_margin_db = *out_ebno_db - required_ebno_db;
    
    return RGTP_OK;
}

void rgtp_satellite_update_spacecraft_state(rgtp_satellite_context_t* ctx,
                                             uint8_t health,
                                             uint8_t power,
                                             rgtp_antenna_status_t antenna) {
    if (!ctx || !ctx->initialized) {
        return;
    }
    
    ctx->spacecraft.health = health;
    ctx->spacecraft.power_level = power;
    ctx->spacecraft.antenna_status = antenna;
    ctx->spacecraft.last_update_us = get_time_us();
}
