#include "../asm/monolithic/rtp_protocol.h"

#include <cstring>
#include <cstdio>

extern "C" {

static RTPDescriptor g_rtp_descriptors[RTP_MAX_TOOLS] = {};
static uint32_t g_rtp_descriptor_count = 0;
static unsigned char g_rtp_context_blob[512] = {};
static uint32_t g_rtp_context_blob_size = 0;
static uint64_t g_rtp_telemetry[8] = {0};
static uint32_t g_rtp_stream_state = 0;
static unsigned char g_rtp_stream_buf[2048] = {};
static uint32_t g_rtp_stream_len = 0;
static const char g_rtp_name_dispatch[] = "rtp.dispatch";
static const char g_rtp_desc_dispatch[] = "Fallback RTP dispatcher";

void RTP_InitDescriptorTable(void) {
    std::memset(g_rtp_descriptors, 0, sizeof(g_rtp_descriptors));
    g_rtp_descriptor_count = 1;
    g_rtp_descriptors[0].tool_id = 1;
    g_rtp_descriptors[0].legacy_tool_id = 1;
    g_rtp_descriptors[0].name_hash = 0x5f72e4c9b1b7ULL;
    g_rtp_descriptors[0].name = g_rtp_name_dispatch;
    g_rtp_descriptors[0].description = g_rtp_desc_dispatch;
    g_rtp_descriptors[0].param_count = 2;
    g_rtp_descriptors[0].handler_rva = 0;
    g_rtp_descriptors[0].tool_uuid[0] = 0x52;
    g_rtp_descriptors[0].tool_uuid[1] = 0x54;
    g_rtp_descriptors[0].tool_uuid[2] = 0x50;
    g_rtp_descriptors[0].tool_uuid[3] = 0x31;
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
    const int n = std::snprintf(reinterpret_cast<char*>(g_rtp_context_blob), sizeof(g_rtp_context_blob),
                                "{\"descriptors\":%u,\"dispatch_ok\":%llu,\"dispatch_fail\":%llu}",
                                g_rtp_descriptor_count,
                                static_cast<unsigned long long>(g_rtp_telemetry[1]),
                                static_cast<unsigned long long>(g_rtp_telemetry[0]));
    g_rtp_context_blob_size = (n > 0) ? static_cast<uint32_t>((n < static_cast<int>(sizeof(g_rtp_context_blob) - 1))
                                                                  ? n
                                                                  : (sizeof(g_rtp_context_blob) - 1))
                                      : 0;

    if (out_buf != nullptr && out_cap > 0 && g_rtp_context_blob_size > 0) {
        const uint32_t copyBytes = (g_rtp_context_blob_size < (out_cap - 1)) ? g_rtp_context_blob_size : (out_cap - 1);
        std::memcpy(out_buf, g_rtp_context_blob, copyBytes);
        static_cast<char*>(out_buf)[copyBytes] = '\0';
        if (out_written != nullptr) {
            *out_written = copyBytes;
        }
    }
    return 0;
}

const void* RTP_GetContextBlobPtr(void) {
    return g_rtp_context_blob;
}

uint32_t RTP_GetContextBlobSize(void) {
    return g_rtp_context_blob_size;
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
    g_rtp_stream_len = 0;
}

void RTP_StreamParser_Reset(void) {
    g_rtp_stream_state = 0;
    g_rtp_stream_len = 0;
}

int32_t RTP_StreamParser_PushByte(uint8_t byte_value) {
    if (g_rtp_stream_len >= sizeof(g_rtp_stream_buf)) {
        g_rtp_stream_state = 3; // overflow
        g_rtp_stream_len = 0;
        return -1;
    }
    g_rtp_stream_buf[g_rtp_stream_len++] = byte_value;
    g_rtp_stream_state = (byte_value == 0x7E || byte_value == '\n') ? 2 : 1;
    return 0;
}

int32_t RTP_StreamParser_GetPacket(void* out_buf, uint32_t out_cap, uint32_t* out_written) {
    if (out_written != nullptr) {
        *out_written = 0;
    }
    if (g_rtp_stream_state != 2) {
        return 1; // no packet available yet
    }
    if (out_buf == nullptr || out_cap == 0 || g_rtp_stream_len == 0) {
        return -1;
    }
    const uint32_t copyBytes = (g_rtp_stream_len < out_cap) ? g_rtp_stream_len : out_cap;
    std::memcpy(out_buf, g_rtp_stream_buf, copyBytes);
    if (out_written != nullptr) {
        *out_written = copyBytes;
    }
    g_rtp_stream_state = 0;
    g_rtp_stream_len = 0;
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
