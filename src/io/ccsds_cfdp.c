#include "rgtp_io_internal.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define CCSDS_CFDP_HEADER_LEN 4
#define CCSDS_CFDP_MAX_ENTITY_ID_LEN 8
#define CCSDS_CFDP_MAX_TRANSACTION_SEQ_LEN 8

typedef enum {
    CCSDS_CFDP_PDU_FILE_DIRECTIVE = 0,
    CCSDS_CFDP_PDU_FILE_DATA = 1
} ccsds_cfdp_pdu_type_t;

typedef enum {
    CCSDS_CFDP_DIRECTIVE_EOF = 0x04,
    CCSDS_CFDP_DIRECTIVE_FINISHED = 0x05,
    CCSDS_CFDP_DIRECTIVE_ACK = 0x06,
    CCSDS_CFDP_DIRECTIVE_METADATA = 0x07,
    CCSDS_CFDP_DIRECTIVE_NAK = 0x08,
    CCSDS_CFDP_DIRECTIVE_PROMPT = 0x09,
    CCSDS_CFDP_DIRECTIVE_KEEP_ALIVE = 0x0C
} ccsds_cfdp_directive_code_t;

typedef struct {
    uint8_t version;
    ccsds_cfdp_pdu_type_t pdu_type;
    uint8_t direction;
    uint8_t transmission_mode;
    uint8_t crc_flag;
    uint16_t pdu_data_field_length;
    uint8_t entity_id_length;
    uint8_t transaction_seq_num_length;
    uint64_t source_entity_id;
    uint64_t transaction_sequence_number;
    uint64_t destination_entity_id;
} ccsds_cfdp_pdu_header_t;

typedef struct {
    ccsds_cfdp_directive_code_t directive_code;
    uint8_t* directive_parameter_field;
    size_t directive_parameter_field_length;
} ccsds_cfdp_file_directive_t;

typedef struct {
    uint64_t offset;
    uint8_t* file_data;
    size_t file_data_length;
} ccsds_cfdp_file_data_t;

typedef struct {
    uint8_t condition_code;
    uint64_t file_size;
    uint32_t checksum;
} ccsds_cfdp_eof_pdu_t;

typedef struct {
    uint64_t file_size;
    uint8_t* source_file_name;
    size_t source_file_name_length;
    uint8_t* destination_file_name;
    size_t destination_file_name_length;
} ccsds_cfdp_metadata_pdu_t;

static void encode_cfdp_header(uint8_t* buffer, const ccsds_cfdp_pdu_header_t* hdr) {
    buffer[0] = (hdr->version << 5) | (hdr->pdu_type << 4) | (hdr->direction << 3) |
                (hdr->transmission_mode << 2) | (hdr->crc_flag << 1);
    buffer[1] = (hdr->pdu_data_field_length >> 8) & 0xFF;
    buffer[2] = hdr->pdu_data_field_length & 0xFF;
    buffer[3] = ((hdr->entity_id_length & 0x07) << 4) | (hdr->transaction_seq_num_length & 0x07);
}

static void decode_cfdp_header(const uint8_t* buffer, ccsds_cfdp_pdu_header_t* hdr) {
    hdr->version = (buffer[0] >> 5) & 0x07;
    hdr->pdu_type = (buffer[0] >> 4) & 0x01;
    hdr->direction = (buffer[0] >> 3) & 0x01;
    hdr->transmission_mode = (buffer[0] >> 2) & 0x01;
    hdr->crc_flag = (buffer[0] >> 1) & 0x01;
    hdr->pdu_data_field_length = (buffer[1] << 8) | buffer[2];
    hdr->entity_id_length = (buffer[3] >> 4) & 0x07;
    hdr->transaction_seq_num_length = buffer[3] & 0x07;
}

int ccsds_cfdp_encode_metadata_pdu(uint8_t* output, size_t output_size,
                                   uint64_t source_entity_id,
                                   uint64_t dest_entity_id,
                                   uint64_t transaction_seq,
                                   uint64_t file_size,
                                   const char* source_filename,
                                   const char* dest_filename) {
    size_t src_len = strlen(source_filename);
    size_t dst_len = strlen(dest_filename);
    size_t total_len = CCSDS_CFDP_HEADER_LEN + 1 + 8 + 1 + src_len + 1 + dst_len + 8 + 8;
    
    if (output_size < total_len) {
        return -1;
    }
    
    ccsds_cfdp_pdu_header_t hdr = {
        .version = 1,
        .pdu_type = CCSDS_CFDP_PDU_FILE_DIRECTIVE,
        .direction = 0,
        .transmission_mode = 0,
        .crc_flag = 0,
        .pdu_data_field_length = (uint16_t)(total_len - CCSDS_CFDP_HEADER_LEN),
        .entity_id_length = 4,
        .transaction_seq_num_length = 4,
        .source_entity_id = source_entity_id,
        .transaction_sequence_number = transaction_seq,
        .destination_entity_id = dest_entity_id
    };
    
    encode_cfdp_header(output, &hdr);
    
    size_t offset = CCSDS_CFDP_HEADER_LEN;
    
    output[offset++] = CCSDS_CFDP_DIRECTIVE_METADATA;
    
    output[offset++] = (file_size >> 56) & 0xFF;
    output[offset++] = (file_size >> 48) & 0xFF;
    output[offset++] = (file_size >> 40) & 0xFF;
    output[offset++] = (file_size >> 32) & 0xFF;
    output[offset++] = (file_size >> 24) & 0xFF;
    output[offset++] = (file_size >> 16) & 0xFF;
    output[offset++] = (file_size >> 8) & 0xFF;
    output[offset++] = file_size & 0xFF;
    
    output[offset++] = (uint8_t)src_len;
    memcpy(output + offset, source_filename, src_len);
    offset += src_len;
    
    output[offset++] = (uint8_t)dst_len;
    memcpy(output + offset, dest_filename, dst_len);
    offset += dst_len;
    
    output[offset++] = (source_entity_id >> 24) & 0xFF;
    output[offset++] = (source_entity_id >> 16) & 0xFF;
    output[offset++] = (source_entity_id >> 8) & 0xFF;
    output[offset++] = source_entity_id & 0xFF;
    
    output[offset++] = (transaction_seq >> 24) & 0xFF;
    output[offset++] = (transaction_seq >> 16) & 0xFF;
    output[offset++] = (transaction_seq >> 8) & 0xFF;
    output[offset++] = transaction_seq & 0xFF;
    
    output[offset++] = (dest_entity_id >> 24) & 0xFF;
    output[offset++] = (dest_entity_id >> 16) & 0xFF;
    output[offset++] = (dest_entity_id >> 8) & 0xFF;
    output[offset++] = dest_entity_id & 0xFF;
    
    return (int)offset;
}

int ccsds_cfdp_encode_file_data_pdu(uint8_t* output, size_t output_size,
                                    uint64_t source_entity_id,
                                    uint64_t dest_entity_id,
                                    uint64_t transaction_seq,
                                    uint64_t offset,
                                    const uint8_t* file_data,
                                    size_t file_data_len) {
    size_t total_len = CCSDS_CFDP_HEADER_LEN + 8 + 8 + 8 + file_data_len;
    
    if (output_size < total_len) {
        return -1;
    }
    
    ccsds_cfdp_pdu_header_t hdr = {
        .version = 1,
        .pdu_type = CCSDS_CFDP_PDU_FILE_DATA,
        .direction = 0,
        .transmission_mode = 0,
        .crc_flag = 0,
        .pdu_data_field_length = (uint16_t)(total_len - CCSDS_CFDP_HEADER_LEN),
        .entity_id_length = 4,
        .transaction_seq_num_length = 4,
        .source_entity_id = source_entity_id,
        .transaction_sequence_number = transaction_seq,
        .destination_entity_id = dest_entity_id
    };
    
    encode_cfdp_header(output, &hdr);
    
    size_t pos = CCSDS_CFDP_HEADER_LEN;
    
    output[pos++] = (source_entity_id >> 24) & 0xFF;
    output[pos++] = (source_entity_id >> 16) & 0xFF;
    output[pos++] = (source_entity_id >> 8) & 0xFF;
    output[pos++] = source_entity_id & 0xFF;
    
    output[pos++] = (transaction_seq >> 24) & 0xFF;
    output[pos++] = (transaction_seq >> 16) & 0xFF;
    output[pos++] = (transaction_seq >> 8) & 0xFF;
    output[pos++] = transaction_seq & 0xFF;
    
    output[pos++] = (dest_entity_id >> 24) & 0xFF;
    output[pos++] = (dest_entity_id >> 16) & 0xFF;
    output[pos++] = (dest_entity_id >> 8) & 0xFF;
    output[pos++] = dest_entity_id & 0xFF;
    
    output[pos++] = (offset >> 56) & 0xFF;
    output[pos++] = (offset >> 48) & 0xFF;
    output[pos++] = (offset >> 40) & 0xFF;
    output[pos++] = (offset >> 32) & 0xFF;
    output[pos++] = (offset >> 24) & 0xFF;
    output[pos++] = (offset >> 16) & 0xFF;
    output[pos++] = (offset >> 8) & 0xFF;
    output[pos++] = offset & 0xFF;
    
    memcpy(output + pos, file_data, file_data_len);
    pos += file_data_len;
    
    return (int)pos;
}

int ccsds_cfdp_encode_eof_pdu(uint8_t* output, size_t output_size,
                              uint64_t source_entity_id,
                              uint64_t dest_entity_id,
                              uint64_t transaction_seq,
                              uint8_t condition_code,
                              uint64_t file_size,
                              uint32_t checksum) {
    size_t total_len = CCSDS_CFDP_HEADER_LEN + 1 + 1 + 8 + 4 + 8 + 8;
    
    if (output_size < total_len) {
        return -1;
    }
    
    ccsds_cfdp_pdu_header_t hdr = {
        .version = 1,
        .pdu_type = CCSDS_CFDP_PDU_FILE_DIRECTIVE,
        .direction = 0,
        .transmission_mode = 0,
        .crc_flag = 0,
        .pdu_data_field_length = (uint16_t)(total_len - CCSDS_CFDP_HEADER_LEN),
        .entity_id_length = 4,
        .transaction_seq_num_length = 4,
        .source_entity_id = source_entity_id,
        .transaction_sequence_number = transaction_seq,
        .destination_entity_id = dest_entity_id
    };
    
    encode_cfdp_header(output, &hdr);
    
    size_t pos = CCSDS_CFDP_HEADER_LEN;
    
    output[pos++] = CCSDS_CFDP_DIRECTIVE_EOF;
    output[pos++] = condition_code & 0x0F;
    
    output[pos++] = (file_size >> 56) & 0xFF;
    output[pos++] = (file_size >> 48) & 0xFF;
    output[pos++] = (file_size >> 40) & 0xFF;
    output[pos++] = (file_size >> 32) & 0xFF;
    output[pos++] = (file_size >> 24) & 0xFF;
    output[pos++] = (file_size >> 16) & 0xFF;
    output[pos++] = (file_size >> 8) & 0xFF;
    output[pos++] = file_size & 0xFF;
    
    output[pos++] = (checksum >> 24) & 0xFF;
    output[pos++] = (checksum >> 16) & 0xFF;
    output[pos++] = (checksum >> 8) & 0xFF;
    output[pos++] = checksum & 0xFF;
    
    output[pos++] = (source_entity_id >> 24) & 0xFF;
    output[pos++] = (source_entity_id >> 16) & 0xFF;
    output[pos++] = (source_entity_id >> 8) & 0xFF;
    output[pos++] = source_entity_id & 0xFF;
    
    output[pos++] = (transaction_seq >> 24) & 0xFF;
    output[pos++] = (transaction_seq >> 16) & 0xFF;
    output[pos++] = (transaction_seq >> 8) & 0xFF;
    output[pos++] = transaction_seq & 0xFF;
    
    output[pos++] = (dest_entity_id >> 24) & 0xFF;
    output[pos++] = (dest_entity_id >> 16) & 0xFF;
    output[pos++] = (dest_entity_id >> 8) & 0xFF;
    output[pos++] = dest_entity_id & 0xFF;
    
    return (int)pos;
}
