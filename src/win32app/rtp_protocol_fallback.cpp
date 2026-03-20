#include "../asm/monolithic/rtp_protocol.h"

#include <cstdio>
#include <cstring>

extern "C" {

static RTPDescriptor g_rtp_descriptors[RTP_MAX_TOOLS] = {};
static uint32_t g_rtp_descriptor_count = 0;
static unsigned char g_rtp_context_blob[1] = {0};
static uint32_t g_rtp_context_blob_size = 1;
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
    if (packet == nullptr || packet_bytes < RTP_PACKET_HEADER_SIZE || result_buf == nullptr || result_buf_size == 0) {
        return -1;
    }
    const RTPPacketHeader* hdr = static_cast<const RTPPacketHeader*>(packet);
    if (hdr->magic != RTP_PACKET_MAGIC || hdr->version != RTP_PACKET_VERSION) {
        return -1;
    }
    int written = std::snprintf(result_buf,
                                result_buf_size,
                                "{\"call_id\":%llu,\"payload_size\":%u,\"status\":\"ok\"}",
                                static_cast<unsigned long long>(hdr->call_id),
                                hdr->payload_size);
    if (written < 0) {
        return -1;
    }
    if (static_cast<uint32_t>(written) >= result_buf_size) {
        result_buf[result_buf_size - 1] = '\0';
    }
    g_rtp_telemetry[0] += 1;
    g_rtp_telemetry[1] += packet_bytes;
    if (result_buf != nullptr && result_buf_size > 0) {
        g_rtp_context_blob[0] = static_cast<unsigned char>(result_buf[0]);
    }
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
    return g_rtp_context_blob_size;
}

const void* RTP_GetTelemetrySnapshot(void) {
    return g_rtp_telemetry;
}

int32_t RTP_AgentLoop_Run(const char* user_prompt_utf8, char* out_buf, uint32_t out_cap, uint32_t max_iters) {
    if (out_buf == nullptr || out_cap == 0) {
        return -1;
    }
    const char* prompt = (user_prompt_utf8 != nullptr) ? user_prompt_utf8 : "";
    int written = std::snprintf(out_buf,
                                out_cap,
                                "{\"iters\":%u,\"echo\":\"%.48s\"}",
                                max_iters,
                                prompt);
    if (written < 0) {
        out_buf[0] = '\0';
        return -1;
    }
    if (static_cast<uint32_t>(written) >= out_cap) {
        out_buf[out_cap - 1] = '\0';
    }
    g_rtp_telemetry[2] += 1;
    g_rtp_telemetry[3] += max_iters;
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
    if (out_buf == nullptr || out_cap == 0) {
        return -1;
    }
    if (g_rtp_stream_state == 0) {
        static_cast<unsigned char*>(out_buf)[0] = 0;
        return -1;
    }
    static_cast<unsigned char*>(out_buf)[0] = static_cast<unsigned char>(RTP_PACKET_MAGIC & 0xFFu);
    if (out_written != nullptr) {
        *out_written = 1;
    }
    g_rtp_stream_state = 0;
    g_rtp_telemetry[4] += 1;
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
    if (out_buf == nullptr || out_cap < RTP_RESULT_HEADER_SIZE) {
        return -1;
    }
    if (payload != nullptr && payload_size > out_cap - RTP_RESULT_HEADER_SIZE) {
        return -1;
    }

    RTPResultHeader header{};
    header.magic = RTP_RESULT_MAGIC;
    header.version = RTP_RESULT_VERSION;
    header.header_size = static_cast<uint16_t>(RTP_RESULT_HEADER_SIZE);
    header.call_id = call_id;
    header.status_code = status_code;
    header.payload_size = payload_size;

    std::memcpy(out_buf, &header, sizeof(header));
    if (payload != nullptr && payload_size > 0) {
        std::memcpy(static_cast<unsigned char*>(out_buf) + RTP_RESULT_HEADER_SIZE, payload, payload_size);
    }

    if (out_written != nullptr) {
        *out_written = RTP_RESULT_HEADER_SIZE + payload_size;
    }
    g_rtp_telemetry[5] += 1;
    g_rtp_telemetry[6] += payload_size;
    return 0;
}

} // extern "C"
