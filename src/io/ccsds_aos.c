#include "rgtp_io_internal.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define CCSDS_AOS_TRANSFER_FRAME_PRIMARY_HEADER_LEN 6
#define CCSDS_AOS_TRANSFER_FRAME_DATA_ZONE_MAX 65536
#define CCSDS_AOS_MPDU_HEADER_LEN 2
#define CCSDS_AOS_FRAME_ERROR_CONTROL_LEN 2

typedef struct {
    uint16_t transfer_frame_version;
    uint16_t spacecraft_id;
    uint8_t virtual_channel_id;
    uint32_t virtual_channel_frame_count;
    uint8_t replay_flag;
    uint8_t frame_count_usage_flag;
    uint8_t reserved;
    uint16_t first_header_pointer;
} ccsds_aos_transfer_frame_primary_header_t;

typedef struct {
    uint16_t first_header_pointer;
    uint8_t* data_zone;
    size_t data_zone_length;
} ccsds_aos_transfer_frame_data_field_t;

typedef struct {
    ccsds_aos_transfer_frame_primary_header_t primary_header;
    ccsds_aos_transfer_frame_data_field_t data_field;
    uint16_t frame_error_control;
} ccsds_aos_transfer_frame_t;

static void encode_aos_primary_header(uint8_t* buffer, const ccsds_aos_transfer_frame_primary_header_t* hdr) {
    buffer[0] = (hdr->transfer_frame_version << 6) | ((hdr->spacecraft_id >> 2) & 0x3F);
    buffer[1] = ((hdr->spacecraft_id & 0x03) << 6) | (hdr->virtual_channel_id & 0x3F);
    buffer[2] = (hdr->virtual_channel_frame_count >> 16) & 0xFF;
    buffer[3] = (hdr->virtual_channel_frame_count >> 8) & 0xFF;
    buffer[4] = hdr->virtual_channel_frame_count & 0xFF;
    buffer[5] = (hdr->replay_flag << 7) | (hdr->frame_count_usage_flag << 6) | (hdr->reserved << 4) |
                ((hdr->first_header_pointer >> 8) & 0x07);
}

static void decode_aos_primary_header(const uint8_t* buffer, ccsds_aos_transfer_frame_primary_header_t* hdr) {
    hdr->transfer_frame_version = (buffer[0] >> 6) & 0x03;
    hdr->spacecraft_id = ((buffer[0] & 0x3F) << 2) | ((buffer[1] >> 6) & 0x03);
    hdr->virtual_channel_id = buffer[1] & 0x3F;
    hdr->virtual_channel_frame_count = ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 8) | buffer[4];
    hdr->replay_flag = (buffer[5] >> 7) & 0x01;
    hdr->frame_count_usage_flag = (buffer[5] >> 6) & 0x01;
    hdr->reserved = (buffer[5] >> 4) & 0x03;
    hdr->first_header_pointer = ((buffer[5] & 0x07) << 8) | buffer[6];
}

static uint16_t aos_calculate_crc(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

int ccsds_aos_encode_frame(uint8_t* output, size_t output_size,
                           uint16_t spacecraft_id, uint8_t virtual_channel_id,
                           uint32_t frame_count, const uint8_t* data, size_t data_len) {
    size_t total_len = CCSDS_AOS_TRANSFER_FRAME_PRIMARY_HEADER_LEN + data_len + CCSDS_AOS_FRAME_ERROR_CONTROL_LEN;
    
    if (output_size < total_len) {
        return -1;
    }
    
    ccsds_aos_transfer_frame_primary_header_t hdr = {
        .transfer_frame_version = 1,
        .spacecraft_id = spacecraft_id,
        .virtual_channel_id = virtual_channel_id,
        .virtual_channel_frame_count = frame_count,
        .replay_flag = 0,
        .frame_count_usage_flag = 0,
        .reserved = 0,
        .first_header_pointer = 0
    };
    
    encode_aos_primary_header(output, &hdr);
    
    if (data && data_len > 0) {
        memcpy(output + CCSDS_AOS_TRANSFER_FRAME_PRIMARY_HEADER_LEN, data, data_len);
    }
    
    uint16_t crc = aos_calculate_crc(output, total_len - CCSDS_AOS_FRAME_ERROR_CONTROL_LEN);
    output[total_len - 2] = (crc >> 8) & 0xFF;
    output[total_len - 1] = crc & 0xFF;
    
    return (int)total_len;
}

int ccsds_aos_decode_frame(const uint8_t* input, size_t input_len,
                           ccsds_aos_transfer_frame_primary_header_t* hdr,
                           uint8_t* data, size_t* data_len) {
    if (input_len < CCSDS_AOS_TRANSFER_FRAME_PRIMARY_HEADER_LEN + CCSDS_AOS_FRAME_ERROR_CONTROL_LEN) {
        return -1;
    }
    
    decode_aos_primary_header(input, hdr);
    
    size_t payload_len = input_len - CCSDS_AOS_TRANSFER_FRAME_PRIMARY_HEADER_LEN - CCSDS_AOS_FRAME_ERROR_CONTROL_LEN;
    
    if (data && data_len) {
        if (*data_len < payload_len) {
            return -1;
        }
        memcpy(data, input + CCSDS_AOS_TRANSFER_FRAME_PRIMARY_HEADER_LEN, payload_len);
        *data_len = payload_len;
    }
    
    uint16_t received_crc = (input[input_len - 2] << 8) | input[input_len - 1];
    uint16_t calculated_crc = aos_calculate_crc(input, input_len - CCSDS_AOS_FRAME_ERROR_CONTROL_LEN);
    
    if (received_crc != calculated_crc) {
        return -2;
    }
    
    return 0;
}

int ccsds_aos_multiplex_packets(uint8_t* output, size_t output_size,
                                const uint8_t** packets, const size_t* packet_lengths,
                                size_t packet_count) {
    size_t offset = 0;
    
    for (size_t i = 0; i < packet_count; i++) {
        if (offset + CCSDS_AOS_MPDU_HEADER_LEN + packet_lengths[i] > output_size) {
            return -1;
        }
        
        output[offset] = (packet_lengths[i] >> 8) & 0xFF;
        output[offset + 1] = packet_lengths[i] & 0xFF;
        offset += CCSDS_AOS_MPDU_HEADER_LEN;
        
        memcpy(output + offset, packets[i], packet_lengths[i]);
        offset += packet_lengths[i];
    }
    
    return (int)offset;
}

int ccsds_aos_demultiplex_packets(const uint8_t* input, size_t input_len,
                                  uint8_t** packets, size_t* packet_lengths,
                                  size_t max_packets) {
    size_t offset = 0;
    size_t packet_count = 0;
    
    while (offset + CCSDS_AOS_MPDU_HEADER_LEN <= input_len && packet_count < max_packets) {
        uint16_t packet_len = (input[offset] << 8) | input[offset + 1];
        offset += CCSDS_AOS_MPDU_HEADER_LEN;
        
        if (offset + packet_len > input_len) {
            return -1;
        }
        
        packets[packet_count] = (uint8_t*)input + offset;
        packet_lengths[packet_count] = packet_len;
        offset += packet_len;
        packet_count++;
    }
    
    return (int)packet_count;
}
