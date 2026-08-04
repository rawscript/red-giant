#include "rgtp/rgtp.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>

static void test_satellite_init(void) {
    rgtp_config_t cfg = {
        .satellite_mode = true,
        .space_link_type = RGTP_SPACE_LINK_SBAND,
        .max_rtt_ms = 2000,
        .link_asymmetry = 5.0f,
        .min_snr_db = 10.0f,
        .max_ber = 0.001f,
        .store_and_forward = true,
        .ccsds_tm = true,
        .ccsds_tc = true,
        .apid = 0x3E0,
        .spacecraft_id = 42,
        .chunk_size = 1200,
        .window_size = 64,
        .fec_enabled = true,
    };
    
    rgtp_error_t err = rgtp_init();
    assert(err == RGTP_OK);
    
    rgtp_socket_t* sock = NULL;
    err = rgtp_socket_create(&cfg, &sock);
    assert(err == RGTP_OK);
    assert(sock != NULL);
    
    uint8_t test_data[4096];
    for (size_t i = 0; i < sizeof(test_data); i++) {
        test_data[i] = (uint8_t)i;
    }
    
    rgtp_surface_t* surface = NULL;
    err = rgtp_expose(sock, test_data, sizeof(test_data), &cfg, &surface);
    assert(err == RGTP_OK);
    assert(surface != NULL);
    
    rgtp_satellite_stats_t sat_stats;
    err = rgtp_get_satellite_stats(surface, &sat_stats);
    assert(err == RGTP_OK);
    
    assert(sat_stats.spacecraft_health == 100);
    assert(sat_stats.power_level == 100);
    
    rgtp_destroy_surface(surface);
    rgtp_socket_destroy(sock);
    rgtp_cleanup();
    
    printf("PASS: test_satellite_init\n");
}

static void test_link_quality_update(void) {
    rgtp_config_t cfg = {
        .satellite_mode = true,
        .space_link_type = RGTP_SPACE_LINK_XBAND,
        .max_rtt_ms = 1000,
        .min_snr_db = 12.0f,
        .max_ber = 0.0001f,
        .chunk_size = 1200,
        .window_size = 64,
    };
    
    rgtp_error_t err = rgtp_init();
    assert(err == RGTP_OK);
    
    rgtp_socket_t* sock = NULL;
    err = rgtp_socket_create(&cfg, &sock);
    assert(err == RGTP_OK);
    
    uint8_t test_data[1024];
    memset(test_data, 0xAA, sizeof(test_data));
    
    rgtp_surface_t* surface = NULL;
    err = rgtp_expose(sock, test_data, sizeof(test_data), &cfg, &surface);
    assert(err == RGTP_OK);
    
    err = rgtp_update_link_parameters(surface, 15.5f, 0.00005f, 1000);
    assert(err == RGTP_OK);
    
    rgtp_satellite_stats_t sat_stats;
    err = rgtp_get_satellite_stats(surface, &sat_stats);
    assert(err == RGTP_OK);
    
    assert(sat_stats.snr_db >= 15.0f && sat_stats.snr_db <= 16.0f);
    assert(sat_stats.ber <= 0.0001f);
    assert(sat_stats.doppler_offset_hz == 1000);
    
    rgtp_destroy_surface(surface);
    rgtp_socket_destroy(sock);
    rgtp_cleanup();
    
    printf("PASS: test_link_quality_update\n");
}

static void test_contact_scheduling(void) {
    rgtp_config_t cfg = {
        .satellite_mode = true,
        .space_link_type = RGTP_SPACE_LINK_SBAND,
        .chunk_size = 1200,
        .window_size = 64,
    };
    
    rgtp_error_t err = rgtp_init();
    assert(err == RGTP_OK);
    
    rgtp_socket_t* sock = NULL;
    err = rgtp_socket_create(&cfg, &sock);
    assert(err == RGTP_OK);
    
    uint8_t test_data[2048];
    memset(test_data, 0x55, sizeof(test_data));
    
    rgtp_surface_t* surface = NULL;
    err = rgtp_expose(sock, test_data, sizeof(test_data), &cfg, &surface);
    assert(err == RGTP_OK);
    
    uint64_t now = (uint64_t)time(NULL);
    uint64_t start_time = now + 300;
    uint64_t end_time = start_time + 600;
    
    err = rgtp_schedule_contact(surface, start_time, end_time, "GS-MADRID");
    assert(err == RGTP_OK);
    
    err = rgtp_schedule_contact(surface, end_time + 1000, end_time + 1600, "GS-WALLOPS");
    assert(err == RGTP_OK);
    
    rgtp_contact_window_t windows[10];
    size_t count = 0;
    err = rgtp_get_contact_windows(surface, windows, 10, &count);
    assert(err == RGTP_OK);
    assert(count == 2);
    
    assert(windows[0].start_time == start_time);
    assert(windows[0].end_time == end_time);
    assert(strcmp(windows[0].ground_station, "GS-MADRID") == 0);
    
    assert(windows[1].start_time == end_time + 1000);
    assert(strcmp(windows[1].ground_station, "GS-WALLOPS") == 0);
    
    bool in_contact;
    uint32_t time_to_contact, time_left;
    err = rgtp_check_contact_status(surface, &in_contact, &time_to_contact, &time_left);
    assert(err == RGTP_OK);
    assert(!in_contact);
    assert(time_to_contact > 0);
    
    rgtp_destroy_surface(surface);
    rgtp_socket_destroy(sock);
    rgtp_cleanup();
    
    printf("PASS: test_contact_scheduling\n");
}

static void test_link_budget_calculation(void) {
    rgtp_config_t cfg = {
        .satellite_mode = true,
        .space_link_type = RGTP_SPACE_LINK_KABAND,
        .chunk_size = 1200,
        .window_size = 64,
    };
    
    rgtp_error_t err = rgtp_init();
    assert(err == RGTP_OK);
    
    rgtp_socket_t* sock = NULL;
    err = rgtp_socket_create(&cfg, &sock);
    assert(err == RGTP_OK);
    
    uint8_t test_data[512];
    memset(test_data, 0x00, sizeof(test_data));
    
    rgtp_surface_t* surface = NULL;
    err = rgtp_expose(sock, test_data, sizeof(test_data), &cfg, &surface);
    assert(err == RGTP_OK);
    
    float margin_db, ebno_db;
    err = rgtp_calculate_link_budget(surface, &margin_db, &ebno_db);
    assert(err == RGTP_OK);
    
    assert(margin_db > -100.0f && margin_db < 100.0f);
    assert(ebno_db > -50.0f && ebno_db < 50.0f);
    
    rgtp_destroy_surface(surface);
    rgtp_socket_destroy(sock);
    rgtp_cleanup();
    
    printf("PASS: test_link_budget_calculation\n");
}

static void test_doppler_compensation(void) {
    rgtp_config_t cfg = {
        .satellite_mode = true,
        .space_link_type = RGTP_SPACE_LINK_UHF,
        .doppler_shift_hz = 10000,
        .chunk_size = 1200,
        .window_size = 64,
    };
    
    rgtp_error_t err = rgtp_init();
    assert(err == RGTP_OK);
    
    rgtp_socket_t* sock = NULL;
    err = rgtp_socket_create(&cfg, &sock);
    assert(err == RGTP_OK);
    
    uint8_t test_data[1024];
    memset(test_data, 0xFF, sizeof(test_data));
    
    rgtp_surface_t* surface = NULL;
    err = rgtp_expose(sock, test_data, sizeof(test_data), &cfg, &surface);
    assert(err == RGTP_OK);
    
    err = rgtp_configure_doppler(surface, true, 10000, 10.0f);
    assert(err == RGTP_OK);
    
    err = rgtp_update_link_parameters(surface, 12.0f, 0.0001f, 5000);
    assert(err == RGTP_OK);
    
    err = rgtp_update_link_parameters(surface, 12.5f, 0.00008f, 5100);
    assert(err == RGTP_OK);
    
    err = rgtp_update_link_parameters(surface, 13.0f, 0.00006f, 5200);
    assert(err == RGTP_OK);
    
    rgtp_satellite_stats_t sat_stats;
    err = rgtp_get_satellite_stats(surface, &sat_stats);
    assert(err == RGTP_OK);
    
    assert(sat_stats.doppler_offset_hz >= 5000 && sat_stats.doppler_offset_hz <= 5500);
    
    rgtp_destroy_surface(surface);
    rgtp_socket_destroy(sock);
    rgtp_cleanup();
    
    printf("PASS: test_doppler_compensation\n");
}

static void test_store_and_forward(void) {
    rgtp_config_t cfg = {
        .satellite_mode = true,
        .space_link_type = RGTP_SPACE_LINK_SBAND,
        .store_and_forward = true,
        .chunk_size = 1200,
        .window_size = 64,
    };
    
    rgtp_error_t err = rgtp_init();
    assert(err == RGTP_OK);
    
    rgtp_socket_t* sock = NULL;
    err = rgtp_socket_create(&cfg, &sock);
    assert(err == RGTP_OK);
    
    uint8_t test_data[2048];
    for (size_t i = 0; i < sizeof(test_data); i++) {
        test_data[i] = (uint8_t)(i & 0xFF);
    }
    
    rgtp_surface_t* surface = NULL;
    err = rgtp_expose(sock, test_data, sizeof(test_data), &cfg, &surface);
    assert(err == RGTP_OK);
    
    err = rgtp_enable_store_forward(surface, 1048576, 3600);
    assert(err == RGTP_OK);
    
    rgtp_satellite_stats_t sat_stats;
    err = rgtp_get_satellite_stats(surface, &sat_stats);
    assert(err == RGTP_OK);
    
    assert(sat_stats.stored_chunks == 0);
    assert(sat_stats.stored_bytes == 0);
    
    rgtp_destroy_surface(surface);
    rgtp_socket_destroy(sock);
    rgtp_cleanup();
    
    printf("PASS: test_store_and_forward\n");
}

int main(void) {
    printf("Running satellite module tests...\n\n");
    
    test_satellite_init();
    test_link_quality_update();
    test_contact_scheduling();
    test_link_budget_calculation();
    test_doppler_compensation();
    test_store_and_forward();
    
    printf("\nAll satellite tests passed!\n");
    return 0;
}
