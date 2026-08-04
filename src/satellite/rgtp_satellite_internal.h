#ifndef RGTP_SATELLITE_INTERNAL_H
#define RGTP_SATELLITE_INTERNAL_H

#include "rgtp/rgtp.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RGTP_MAX_CONTACT_WINDOWS 64
#define RGTP_MAX_GROUND_STATIONS 32
#define RGTP_STORE_FORWARD_MAX_CHUNKS 16384
#define RGTP_DOPPLER_HISTORY_SIZE 256

typedef enum {
    RGTP_LINK_STATUS_DOWN       = 0,
    RGTP_LINK_STATUS_ACQUIRING  = 1,
    RGTP_LINK_STATUS_CARRIER    = 2,
    RGTP_LINK_STATUS_LOCK       = 3,
    RGTP_LINK_STATUS_DEGRADED   = 4,
} rgtp_link_status_t;

typedef enum {
    RGTP_ANTENNA_DOWN     = 0,
    RGTP_ANTENNA_POINTING = 1,
    RGTP_ANTENNA_TRACKING = 2,
} rgtp_antenna_status_t;

typedef struct {
    uint64_t start_time;
    uint64_t end_time;
    char ground_station[32];
    float max_elevation_deg;
    float aos_azimuth_deg;
    float los_azimuth_deg;
    bool active;
    uint32_t data_transferred_bytes;
} rgtp_contact_window_internal_t;

typedef struct {
    uint32_t chunk_index;
    uint8_t* data;
    size_t size;
    uint64_t timestamp_us;
    uint8_t priority;
} rgtp_stored_chunk_t;

typedef struct {
    rgtp_stored_chunk_t chunks[RGTP_STORE_FORWARD_MAX_CHUNKS];
    uint32_t count;
    uint32_t head;
    uint32_t tail;
    uint64_t total_bytes;
    uint64_t max_bytes;
    uint32_t max_time_s;
    _Atomic uint32_t overflow_count;
} rgtp_store_forward_buffer_t;

typedef struct {
    int32_t offset_hz;
    uint64_t timestamp_us;
    float rate_hz_per_sec;
} rgtp_doppler_sample_t;

typedef struct {
    rgtp_doppler_sample_t history[RGTP_DOPPLER_HISTORY_SIZE];
    uint32_t head;
    uint32_t count;
    bool compensation_enabled;
    uint32_t max_doppler_hz;
    float update_rate_hz;
    int32_t current_offset_hz;
    float predicted_offset_hz;
} rgtp_doppler_state_t;

typedef struct {
    float snr_db;
    float ber;
    float link_margin_db;
    float eb_no_db;
    uint64_t timestamp_us;
    uint32_t sample_count;
} rgtp_link_quality_t;

typedef struct {
    rgtp_contact_window_internal_t windows[RGTP_MAX_CONTACT_WINDOWS];
    uint32_t count;
    uint32_t active_index;
    _Atomic uint32_t contact_attempts;
    _Atomic uint32_t successful_contacts;
    uint64_t last_contact_start_us;
    uint64_t last_contact_end_us;
} rgtp_contact_schedule_t;

typedef struct {
    uint8_t health;
    uint8_t power_level;
    rgtp_antenna_status_t antenna_status;
    float temperature_c;
    uint32_t uptime_s;
    uint64_t last_update_us;
} rgtp_spacecraft_state_t;

typedef struct {
    _Atomic uint32_t tm_frames_sent;
    _Atomic uint32_t tm_frames_received;
    _Atomic uint32_t tc_frames_sent;
    _Atomic uint32_t tc_frames_received;
    _Atomic uint32_t aos_frames_sent;
    _Atomic uint32_t aos_frames_received;
    _Atomic uint32_t cfdp_pdus_sent;
    _Atomic uint32_t cfdp_pdus_received;
    _Atomic uint32_t frame_errors;
    _Atomic uint32_t vcdu_count;
} rgtp_ccsds_stats_t;

typedef struct {
    bool initialized;
    bool satellite_mode;
    uint8_t space_link_type;
    uint32_t max_rtt_ms;
    float link_asymmetry;
    
    rgtp_link_status_t link_status;
    rgtp_link_quality_t link_quality;
    rgtp_doppler_state_t doppler;
    rgtp_contact_schedule_t schedule;
    rgtp_spacecraft_state_t spacecraft;
    rgtp_ccsds_stats_t ccsds_stats;
    
    rgtp_store_forward_buffer_t store_forward;
    bool store_forward_enabled;
    
    bool ccsds_tm_enabled;
    bool ccsds_tc_enabled;
    bool ccsds_aos_enabled;
    bool ccsds_cfdp_enabled;
    uint16_t apid;
    uint8_t spacecraft_id;
    
    float min_snr_db;
    float max_ber;
    
    _Atomic uint64_t bytes_transmitted;
    _Atomic uint64_t bytes_received;
} rgtp_satellite_context_t;

rgtp_error_t rgtp_satellite_init(rgtp_satellite_context_t* ctx, const rgtp_config_t* cfg);
void rgtp_satellite_destroy(rgtp_satellite_context_t* ctx);

rgtp_error_t rgtp_satellite_update_link_quality(rgtp_satellite_context_t* ctx, 
                                                 float snr_db, 
                                                 float ber, 
                                                 int32_t doppler_hz);

rgtp_error_t rgtp_satellite_schedule_contact(rgtp_satellite_context_t* ctx,
                                              uint64_t start_time,
                                              uint64_t end_time,
                                              const char* ground_station);

bool rgtp_satellite_in_contact_window(const rgtp_satellite_context_t* ctx, uint64_t now_us);

rgtp_error_t rgtp_satellite_store_chunk(rgtp_satellite_context_t* ctx,
                                         uint32_t chunk_index,
                                         const uint8_t* data,
                                         size_t size,
                                         uint8_t priority);

rgtp_error_t rgtp_satellite_retrieve_chunk(rgtp_satellite_context_t* ctx,
                                            uint32_t* out_chunk_index,
                                            uint8_t* buffer,
                                            size_t* out_size);

rgtp_error_t rgtp_satellite_calculate_link_budget(const rgtp_satellite_context_t* ctx,
                                                   const rgtp_config_t* cfg,
                                                   float* out_margin_db,
                                                   float* out_ebno_db);

void rgtp_satellite_update_spacecraft_state(rgtp_satellite_context_t* ctx,
                                             uint8_t health,
                                             uint8_t power,
                                             rgtp_antenna_status_t antenna);

#ifdef __cplusplus
}
#endif

#endif
