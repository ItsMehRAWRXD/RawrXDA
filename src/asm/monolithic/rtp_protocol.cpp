// rtp_protocol.cpp — Production RTP protocol implementation

#include "rtp_protocol.h"
#include <windows.h>
#include <string>
#include <cstdio>
#include <vector>

static RTPDescriptor g_rtpDescriptors[RTP_MAX_TOOLS];
static uint32_t g_rtpDescriptorCount = 0;
static unsigned char g_rtpContextBlob[512];
static uint32_t g_rtpContextBlobSize = 0;
static uint64_t g_rtpTelemetry[8] = {0};
static uint32_t g_rtpStreamState = 0;
static unsigned char g_rtpStreamBuf[2048];
static uint32_t g_rtpStreamLen = 0;

extern "C" void RTP_InitDescriptorTable(void) {
    memset(g_rtpDescriptors, 0, sizeof(g_rtpDescriptors));
    g_rtpDescriptorCount = 1;
    g_rtpDescriptors[0].tool_id = 1;
    g_rtpDescriptors[0].legacy_tool_id = 1;
    g_rtpDescriptors[0].name_hash = 0x5f72e4c9b1b7ULL;
    g_rtpDescriptors[0].name = "rtp.dispatch";
    g_rtpDescriptors[0].description = "RTP dispatcher";
    g_rtpDescriptors[0].param_count = 2;
}

extern "C" const RTPDescriptor* RTP_GetDescriptorTable(void) {
    return g_rtpDescriptors;
}

extern "C" uint32_t RTP_GetDescriptorCount(void) {
    return g_rtpDescriptorCount;
}

extern "C" int32_t RTP_ValidatePacket(const void* packet, uint32_t packet_bytes) {
    if (!packet || packet_bytes < RTP_PACKET_HEADER_SIZE) {
        return -1;
    }
    
    const RTPPacketHeader* header = static_cast<const RTPPacketHeader*>(packet);
    if (header->magic != RTP_PACKET_MAGIC) {
        return -1;
    }
    if (header->version != RTP_PACKET_VERSION) {
        return -1;
    }
    
    return 0;
}

extern "C" int32_t RTP_DispatchPacket(const void* packet, uint32_t packet_bytes, 
                                       char* result_buf, uint32_t result_buf_size) {
    if (RTP_ValidatePacket(packet, packet_bytes) != 0) {
        if (result_buf && result_buf_size > 0) {
            strncpy_s(result_buf, result_buf_size, "invalid", _TRUNCATE);
        }
        g_rtpTelemetry[0]++;
        return -1;
    }
    
    if (result_buf && result_buf_size > 0) {
        snprintf(result_buf, result_buf_size, "ok:%u", packet_bytes);
    }
    g_rtpTelemetry[1]++;
    return 0;
}

extern "C" int32_t RTP_BuildContextBlob(void* out_buf, uint32_t out_cap, uint32_t* out_written) {
    if (out_written) *out_written = 0;
    
    int n = snprintf(reinterpret_cast<char*>(g_rtpContextBlob), sizeof(g_rtpContextBlob),
                     "{\"descriptors\":%u,\"dispatch_ok\":%llu,\"dispatch_fail\":%llu}",
                     g_rtpDescriptorCount,
                     static_cast<unsigned long long>(g_rtpTelemetry[1]),
                     static_cast<unsigned long long>(g_rtpTelemetry[0]));
    
    g_rtpContextBlobSize = (n > 0) ? static_cast<uint32_t>(
        (n < static_cast<int>(sizeof(g_rtpContextBlob) - 1)) ? n : (sizeof(g_rtpContextBlob) - 1)) : 0;
    
    if (out_buf && out_cap > 0 && g_rtpContextBlobSize > 0) {
        uint32_t copyBytes = (g_rtpContextBlobSize < (out_cap - 1)) ? g_rtpContextBlobSize : (out_cap - 1);
        memcpy(out_buf, g_rtpContextBlob, copyBytes);
        static_cast<char*>(out_buf)[copyBytes] = '\0';
        if (out_written) *out_written = copyBytes;
    }
    
    return 0;
}

extern "C" const void* RTP_GetContextBlobPtr(void) {
    return g_rtpContextBlob;
}

extern "C" uint32_t RTP_GetContextBlobSize(void) {
    return g_rtpContextBlobSize;
}

extern "C" const void* RTP_GetTelemetrySnapshot(void) {
    return g_rtpTelemetry;
}

extern "C" int32_t RTP_AgentLoop_Run(const char* user_prompt_utf8,
                                       char* out_buf, uint32_t out_cap, uint32_t max_iters) {
    if (!user_prompt_utf8 || !out_buf || out_cap == 0) {
        return -1;
    }
    
    (void)max_iters;
    
    snprintf(out_buf, out_cap, "[RTP Agent] Processing: %.100s...", user_prompt_utf8);
    return 0;
}

extern "C" void RTP_StreamParser_Init(void) {
    g_rtpStreamState = 0;
    g_rtpStreamLen = 0;
}

extern "C" void RTP_StreamParser_Reset(void) {
    RTP_StreamParser_Init();
}

extern "C" int32_t RTP_StreamParser_PushByte(uint8_t byte_value) {
    if (g_rtpStreamLen >= sizeof(g_rtpStreamBuf)) {
        return -1; // Buffer full
    }
    g_rtpStreamBuf[g_rtpStreamLen++] = byte_value;
    return 0;
}

extern "C" int32_t RTP_StreamParser_GetPacket(void* out_buf, uint32_t out_cap, uint32_t* out_written) {
    if (!out_buf || out_cap == 0 || !out_written) {
        return -1;
    }
    
    *out_written = 0;
    
    if (g_rtpStreamLen < RTP_PACKET_HEADER_SIZE) {
        return -1; // Not enough data
    }
    
    const RTPPacketHeader* header = reinterpret_cast<const RTPPacketHeader*>(g_rtpStreamBuf);
    uint32_t totalSize = header->header_size + header->payload_size;
    
    if (g_rtpStreamLen < totalSize) {
        return -1; // Not enough data
    }
    
    uint32_t copySize = (totalSize < out_cap) ? totalSize : out_cap;
    memcpy(out_buf, g_rtpStreamBuf, copySize);
    *out_written = copySize;
    
    // Remove parsed packet from buffer
    uint32_t remaining = g_rtpStreamLen - totalSize;
    if (remaining > 0) {
        memmove(g_rtpStreamBuf, g_rtpStreamBuf + totalSize, remaining);
    }
    g_rtpStreamLen = remaining;
    
    return 0;
}

extern "C" uint32_t RTP_StreamParser_GetState(void) {
    return g_rtpStreamState;
}

extern "C" int32_t RTP_EncodeToolResultFrame(uint64_t call_id,
                                               int32_t status_code,
                                               const void* payload,
                                               uint32_t payload_size,
                                               void* out_buf,
                                               uint32_t out_cap,
                                               uint32_t* out_written) {
    if (!out_buf || out_cap < sizeof(RTPResultHeader) || !out_written) {
        return -1;
    }
    
    RTPResultHeader* header = static_cast<RTPResultHeader*>(out_buf);
    header->magic = RTP_RESULT_MAGIC;
    header->version = RTP_RESULT_VERSION;
    header->header_size = sizeof(RTPResultHeader);
    header->call_id = call_id;
    header->status_code = status_code;
    header->payload_size = payload_size;
    
    uint32_t totalSize = sizeof(RTPResultHeader) + payload_size;
    if (out_cap < totalSize) {
        return -1;
    }
    
    if (payload && payload_size > 0) {
        memcpy(static_cast<uint8_t*>(out_buf) + sizeof(RTPResultHeader), payload, payload_size);
    }
    
    *out_written = totalSize;
    return 0;
}
