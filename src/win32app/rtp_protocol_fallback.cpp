#include "../asm/monolithic/rtp_protocol.h"

#include <cstring>
#include <cstdio>

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
    if (RTP_ValidatePacket(packet, packet_bytes) != 0) {
        if (result_buf != nullptr && result_buf_size > 0) {
            std::snprintf(result_buf, result_buf_size, "invalid");
        }
        g_rtp_telemetry[0] += 1; // validation failures
        return -1;
    }
    if (result_buf != nullptr && result_buf_size > 0) {
        std::snprintf(result_buf, result_buf_size, "ok:%u", packet_bytes);
    }
    g_rtp_telemetry[1] += 1; // dispatch successes
    return 0;
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
    if (user_prompt_utf8 == nullptr || max_iters == 0) {
        if (out_buf != nullptr && out_cap > 0) {
            out_buf[0] = '\0';
        }
        g_rtp_telemetry[2] += 1; // agent-loop invalid input
        return -1;
    }
    if (out_buf != nullptr && out_cap > 0) {
        std::snprintf(out_buf, out_cap, "agent-loop:%u:%s", max_iters, user_prompt_utf8);
    }
    g_rtp_telemetry[3] += 1; // agent-loop runs
    return 0;
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
    if (g_rtp_stream_state == 0) {
        return 1; // no packet available yet
    }
    if (out_buf == nullptr || out_cap == 0) {
        return -1;
    }
    static_cast<unsigned char*>(out_buf)[0] = 0x7E;
    if (out_written != nullptr) {
        *out_written = 1;
    }
    g_rtp_stream_state = 0;
    return 0;
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
    if (out_written != nullptr) {
        *out_written = 0;
    }
    if (out_buf == nullptr || out_cap < 16) {
        return -1;
    }
    unsigned char* dst = static_cast<unsigned char*>(out_buf);
    std::memset(dst, 0, out_cap);
    std::memcpy(dst, &call_id, sizeof(call_id));
    std::memcpy(dst + sizeof(call_id), &status_code, sizeof(status_code));
    const uint32_t copyBytes =
        (payload != nullptr) ? ((payload_size <= (out_cap - 16)) ? payload_size : (out_cap - 16)) : 0;
    std::memcpy(dst + 12, &copyBytes, sizeof(copyBytes));
    if (copyBytes > 0) {
        std::memcpy(dst + 16, payload, copyBytes);
    }
    if (out_written != nullptr) {
        *out_written = 16 + copyBytes;
    }
    g_rtp_telemetry[4] += 1; // encoded frames
    return 0;
}

} // extern "C"
