#include "../asm/monolithic/rtp_protocol.h"

#include <cstring>

extern "C" {

static RTPDescriptor g_rtp_descriptors[RTP_MAX_TOOLS] = {};
static uint32_t g_rtp_descriptor_count = 0;
static unsigned char g_rtp_context_blob[1] = {0};
static uint64_t g_rtp_telemetry[8] = {0};
static uint32_t g_rtp_stream_state = 0;

void RTP_InitDescriptorTable(void) {
    g_rtp_descriptor_count = 0;
}

const RTPDescriptor* RTP_GetDescriptorTable(void) {
    return g_rtp_descriptors;
}

uint32_t RTP_GetDescriptorCount(void) {
    return g_rtp_descriptor_count;
}

int32_t RTP_ValidatePacket(const void* packet, uint32_t packet_bytes) {
    if (packet == nullptr || packet_bytes < RTP_PACKET_HEADER_SIZE) {
        return -1;
    }
    return 0;
}

int32_t RTP_DispatchPacket(const void* packet, uint32_t packet_bytes, char* result_buf, uint32_t result_buf_size) {
    (void)packet;
    (void)packet_bytes;
    if (result_buf != nullptr && result_buf_size > 0) {
        result_buf[0] = '\0';
    }
    return -1;
}

int32_t RTP_BuildContextBlob(void* out_buf, uint32_t out_cap, uint32_t* out_written) {
    if (out_written != nullptr) {
        *out_written = 0;
    }
    if (out_buf != nullptr && out_cap > 0) {
        static const char kFallback[] = "{}";
        const uint32_t n = (out_cap > sizeof(kFallback)) ? (uint32_t)sizeof(kFallback) : (out_cap - 1);
        if (n > 0) {
            std::memcpy(out_buf, kFallback, n);
            static_cast<char*>(out_buf)[n] = '\0';
            if (out_written != nullptr) {
                *out_written = n;
            }
        }
    }
    return 0;
}

const void* RTP_GetContextBlobPtr(void) {
    return g_rtp_context_blob;
}

uint32_t RTP_GetContextBlobSize(void) {
    return 0;
}

const void* RTP_GetTelemetrySnapshot(void) {
    return g_rtp_telemetry;
}

int32_t RTP_AgentLoop_Run(const char* user_prompt_utf8, char* out_buf, uint32_t out_cap, uint32_t max_iters) {
    (void)user_prompt_utf8;
    (void)max_iters;
    if (out_buf != nullptr && out_cap > 0) {
        out_buf[0] = '\0';
    }
    return -1;
}

void RTP_StreamParser_Init(void) {
    g_rtp_stream_state = 0;
}

void RTP_StreamParser_Reset(void) {
    g_rtp_stream_state = 0;
}

int32_t RTP_StreamParser_PushByte(uint8_t byte_value) {
    (void)byte_value;
    g_rtp_stream_state = 1;
    return 0;
}

int32_t RTP_StreamParser_GetPacket(void* out_buf, uint32_t out_cap, uint32_t* out_written) {
    if (out_written != nullptr) {
        *out_written = 0;
    }
    if (out_buf != nullptr && out_cap > 0) {
        static_cast<unsigned char*>(out_buf)[0] = 0;
    }
    return -1;
}

uint32_t RTP_StreamParser_GetState(void) {
    return g_rtp_stream_state;
}

int32_t RTP_EncodeToolResultFrame(uint64_t call_id,
                                  int32_t status_code,
                                  const void* payload,
                                  uint32_t payload_size,
                                  void* out_buf,
                                  uint32_t out_cap,
                                  uint32_t* out_written) {
    (void)call_id;
    (void)status_code;
    (void)payload;
    (void)payload_size;
    if (out_written != nullptr) {
        *out_written = 0;
    }
    if (out_buf != nullptr && out_cap > 0) {
        static_cast<unsigned char*>(out_buf)[0] = 0;
    }
    return -1;
}

} // extern "C"
