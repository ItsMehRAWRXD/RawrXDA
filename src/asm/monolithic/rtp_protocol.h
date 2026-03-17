#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTP_MAX_TOOLS 44u
#define RTP_DESCRIPTOR_SIZE 64u
#define RTP_PACKET_MAGIC 0x21505452u /* 'RTP!' */
#define RTP_PACKET_VERSION 1u
#define RTP_PACKET_HEADER_SIZE 52u
#define RTP_RESULT_MAGIC 0x52505452u /* 'RTPR' */
#define RTP_RESULT_VERSION 1u
#define RTP_RESULT_HEADER_SIZE 24u

typedef struct RTPPacketHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint64_t call_id;
    uint64_t param_mask;
    uint32_t payload_size;
    uint32_t flags;
    uint8_t tool_uuid[16];
} RTPPacketHeader;

typedef struct RTPDescriptor {
    uint8_t tool_uuid[16];
    uint32_t tool_id;
    uint32_t legacy_tool_id;
    uint64_t name_hash;
    const char* name;
    const char* description;
    uint8_t param_count;
    uint8_t reserved[7];
    uint64_t handler_rva;
} RTPDescriptor;

typedef struct RTPResultHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint64_t call_id;
    int32_t status_code;
    uint32_t payload_size;
} RTPResultHeader;

void RTP_InitDescriptorTable(void);
const RTPDescriptor* RTP_GetDescriptorTable(void);
uint32_t RTP_GetDescriptorCount(void);
int32_t RTP_ValidatePacket(const void* packet, uint32_t packet_bytes);
int32_t RTP_DispatchPacket(const void* packet,
                           uint32_t packet_bytes,
                           char* result_buf,
                           uint32_t result_buf_size);
int32_t RTP_BuildContextBlob(void* out_buf, uint32_t out_cap, uint32_t* out_written);
const void* RTP_GetContextBlobPtr(void);
uint32_t RTP_GetContextBlobSize(void);
const void* RTP_GetTelemetrySnapshot(void);
int32_t RTP_AgentLoop_Run(const char* user_prompt_utf8,
                          char* out_buf,
                          uint32_t out_cap,
                          uint32_t max_iters);

void RTP_StreamParser_Init(void);
void RTP_StreamParser_Reset(void);
int32_t RTP_StreamParser_PushByte(uint8_t byte_value);
int32_t RTP_StreamParser_GetPacket(void* out_buf, uint32_t out_cap, uint32_t* out_written);
uint32_t RTP_StreamParser_GetState(void);

int32_t RTP_EncodeToolResultFrame(uint64_t call_id,
                                  int32_t status_code,
                                  const void* payload,
                                  uint32_t payload_size,
                                  void* out_buf,
                                  uint32_t out_cap,
                                  uint32_t* out_written);

#ifdef __cplusplus
}
#endif
