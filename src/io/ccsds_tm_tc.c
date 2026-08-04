#include "rgtp_io_internal.h"
#include "../satellite/rgtp_satellite_internal.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define CCSDS_TM_PRIMARY_HEADER_LEN 6
#define CCSDS_TC_PRIMARY_HEADER_LEN 5
#define CCSDS_TM_SECONDARY_HEADER_LEN 10
#define CCSDS_TC_SECONDARY_HEADER_LEN 6

typedef struct {
    uint8_t version;
    uint8_t type;
    uint8_t sec_hdr_flag;
    uint16_t apid;
    uint8_t seq_flags;
    uint16_t seq_count;
    uint16_t data_length;
} ccsds_primary_header_t;

typedef struct {
    uint8_t pus_version;
    uint8_t service_type;
    uint8_t service_subtype;
    uint8_t source_id;
    uint32_t timestamp_coarse;
    uint16_t timestamp_fine;
} ccsds_tm_secondary_header_t;

typedef struct {
    uint8_t ccsds_version;
    uint8_t ack_flags;
    uint8_t service_type;
    uint8_t service_subtype;
    uint8_t source_id;
} ccsds_tc_secondary_header_t;

static uint16_t ccsds_calculate_crc(const uint8_t* data, size_t length) {
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

static void encode_tm_primary_header(uint8_t* buffer, const ccsds_primary_header_t* hdr) {
    buffer[0] = (hdr->version << 5) | (hdr->type << 4) | (hdr->sec_hdr_flag << 3) | ((hdr->apid >> 8) & 0x07);
    buffer[1] = hdr->apid & 0xFF;
    buffer[2] = (hdr->seq_flags << 6) | ((hdr->seq_count >> 8) & 0x3F);
    buffer[3] = hdr->seq_count & 0xFF;
    buffer[4] = (hdr->data_length >> 8) & 0xFF;
    buffer[5] = hdr->data_length & 0xFF;
}

static void decode_tm_primary_header(const uint8_t* buffer, ccsds_primary_header_t* hdr) {
    hdr->version = (buffer[0] >> 5) & 0x07;
    hdr->type = (buffer[0] >> 4) & 0x01;
    hdr->sec_hdr_flag = (buffer[0] >> 3) & 0x01;
    hdr->apid = ((buffer[0] & 0x07) << 8) | buffer[1];
    hdr->seq_flags = (buffer[2] >> 6) & 0x03;
    hdr->seq_count = ((buffer[2] & 0x3F) << 8) | buffer[3];
    hdr->data_length = (buffer[4] << 8) | buffer[5];
}

static void encode_tm_secondary_header(uint8_t* buffer, const ccsds_tm_secondary_header_t* hdr) {
    buffer[0] = (hdr->pus_version << 4) | 0x0F;
    buffer[1] = hdr->service_type;
    buffer[2] = hdr->service_subtype;
    buffer[3] = hdr->source_id;
    buffer[4] = (hdr->timestamp_coarse >> 24) & 0xFF;
    buffer[5] = (hdr->timestamp_coarse >> 16) & 0xFF;
    buffer[6] = (hdr->timestamp_coarse >> 8) & 0xFF;
    buffer[7] = hdr->timestamp_coarse & 0xFF;
    buffer[8] = (hdr->timestamp_fine >> 8) & 0xFF;
    buffer[9] = hdr->timestamp_fine & 0xFF;
}

static void decode_tm_secondary_header(const uint8_t* buffer, ccsds_tm_secondary_header_t* hdr) {
    hdr->pus_version = (buffer[0] >> 4) & 0x0F;
    hdr->service_type = buffer[1];
    hdr->service_subtype = buffer[2];
    hdr->source_id = buffer[3];
    hdr->timestamp_coarse = ((uint32_t)buffer[4] << 24) | ((uint32_t)buffer[5] << 16) |
                            ((uint32_t)buffer[6] << 8) | buffer[7];
    hdr->timestamp_fine = (buffer[8] << 8) | buffer[9];
}

int ccsds_encode_tm_packet(uint8_t* output, size_t output_size,
                           uint16_t apid, uint16_t seq_count,
                           uint8_t service_type, uint8_t service_subtype,
                           const uint8_t* data, size_t data_len) {
    size_t total_len = CCSDS_TM_PRIMARY_HEADER_LEN + 
                       CCSDS_TM_SECONDARY_HEADER_LEN + 
                       data_len + 2;
    
    if (output_size < total_len) {
        return -1;
    }
    
    ccsds_primary_header_t prim_hdr = {
        .version = 0,
        .type = 0,
        .sec_hdr_flag = 1,
        .apid = apid,
        .seq_flags = 3,
        .seq_count = seq_count,
        .data_length = (uint16_t)(CCSDS_TM_SECONDARY_HEADER_LEN + data_len + 2 - 1)
    };
    
    encode_tm_primary_header(output, &prim_hdr);
    
    ccsds_tm_secondary_header_t sec_hdr = {
        .pus_version = 2,
        .service_type = service_type,
        .service_subtype = service_subtype,
        .source_id = 0,
        .timestamp_coarse = 0,
        .timestamp_fine = 0
    };
    
    encode_tm_secondary_header(output + CCSDS_TM_PRIMARY_HEADER_LEN, &sec_hdr);
    
    if (data && data_len > 0) {
        memcpy(output + CCSDS_TM_PRIMARY_HEADER_LEN + CCSDS_TM_SECONDARY_HEADER_LEN,
               data, data_len);
    }
    
    uint16_t crc = ccsds_calculate_crc(output, total_len - 2);
    output[total_len - 2] = (crc >> 8) & 0xFF;
    output[total_len - 1] = crc & 0xFF;
    
    return (int)total_len;
}

int ccsds_decode_tm_packet(const uint8_t* input, size_t input_len,
                           ccsds_primary_header_t* prim_hdr,
                           ccsds_tm_secondary_header_t* sec_hdr,
                           uint8_t* data, size_t* data_len) {
    if (input_len < CCSDS_TM_PRIMARY_HEADER_LEN + CCSDS_TM_SECONDARY_HEADER_LEN + 2) {
        return -1;
    }
    
    decode_tm_primary_header(input, prim_hdr);
    
    if (prim_hdr->sec_hdr_flag) {
        decode_tm_secondary_header(input + CCSDS_TM_PRIMARY_HEADER_LEN, sec_hdr);
    }
    
    size_t payload_len = prim_hdr->data_length + 1 - CCSDS_TM_SECONDARY_HEADER_LEN - 2;
    
    if (data && data_len) {
        if (*data_len < payload_len) {
            return -1;
        }
        memcpy(data, input + CCSDS_TM_PRIMARY_HEADER_LEN + CCSDS_TM_SECONDARY_HEADER_LEN,
               payload_len);
        *data_len = payload_len;
    }
    
    uint16_t received_crc = (input[input_len - 2] << 8) | input[input_len - 1];
    uint16_t calculated_crc = ccsds_calculate_crc(input, input_len - 2);
    
    if (received_crc != calculated_crc) {
        return -2;
    }
    
    return 0;
}

static void encode_tc_primary_header(uint8_t* buffer, const ccsds_primary_header_t* hdr) {
    buffer[0] = (hdr->version << 5) | (hdr->type << 4) | (hdr->sec_hdr_flag << 3) | ((hdr->apid >> 8) & 0x07);
    buffer[1] = hdr->apid & 0xFF;
    buffer[2] = (hdr->seq_flags << 6) | ((hdr->seq_count >> 8) & 0x3F);
    buffer[3] = hdr->seq_count & 0xFF;
    buffer[4] = (hdr->data_length >> 8) & 0xFF;
}

static void encode_tc_secondary_header(uint8_t* buffer, const ccsds_tc_secondary_header_t* hdr) {
    buffer[0] = (hdr->ccsds_version << 4) | (hdr->ack_flags & 0x0F);
    buffer[1] = hdr->service_type;
    buffer[2] = hdr->service_subtype;
    buffer[3] = hdr->source_id;
}

int ccsds_encode_tc_packet(uint8_t* output, size_t output_size,
                           uint16_t apid, uint16_t seq_count,
                           uint8_t service_type, uint8_t service_subtype,
                           const uint8_t* data, size_t data_len) {
    size_t total_len = CCSDS_TC_PRIMARY_HEADER_LEN + 
                       CCSDS_TC_SECONDARY_HEADER_LEN + 
                       data_len + 2;
    
    if (output_size < total_len) {
        return -1;
    }
    
    ccsds_primary_header_t prim_hdr = {
        .version = 0,
        .type = 1,
        .sec_hdr_flag = 1,
        .apid = apid,
        .seq_flags = 3,
        .seq_count = seq_count,
        .data_length = (uint16_t)(CCSDS_TC_SECONDARY_HEADER_LEN + data_len + 2 - 1)
    };
    
    encode_tc_primary_header(output, &prim_hdr);
    
    ccsds_tc_secondary_header_t sec_hdr = {
        .ccsds_version = 0,
        .ack_flags = 0x0F,
        .service_type = service_type,
        .service_subtype = service_subtype,
        .source_id = 0
    };
    
    encode_tc_secondary_header(output + CCSDS_TC_PRIMARY_HEADER_LEN, &sec_hdr);
    
    if (data && data_len > 0) {
        memcpy(output + CCSDS_TC_PRIMARY_HEADER_LEN + CCSDS_TC_SECONDARY_HEADER_LEN,
               data, data_len);
    }
    
    uint16_t crc = ccsds_calculate_crc(output, total_len - 2);
    output[total_len - 2] = (crc >> 8) & 0xFF;
    output[total_len - 1] = crc & 0xFF;
    
    return (int)total_len;
}
