#include "../asm/monolithic/rtp_protocol.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr int32_t RTP_OK = 0;
constexpr int32_t RTP_ERR_INVALID_ARG = -1;
constexpr int32_t RTP_ERR_INVALID_PACKET = -2;
constexpr int32_t RTP_ERR_UNKNOWN_TOOL = -3;
constexpr int32_t RTP_ERR_BUFFER_TOO_SMALL = -4;
constexpr int32_t RTP_ERR_POLICY_BLOCK = 23;

enum TelemetryIndex : size_t {
    TEL_PACKETS_VALID = 0,
    TEL_DISPATCH_OK = 1,
    TEL_DISPATCH_ERR = 2,
    TEL_ALIAS_HITS = 3,
    TEL_POLICY_BLOCKS = 4,
    TEL_AGENT_LOOP_ROUNDS = 5,
};

static RTPDescriptor g_rtp_descriptors[RTP_MAX_TOOLS] = {};
static uint32_t g_rtp_descriptor_count = 0;
static std::array<char, 4096> g_rtp_context_blob = {};
static uint32_t g_rtp_context_size = 0;
static std::array<uint64_t, 8> g_rtp_telemetry = {};
static std::array<uint8_t, 8192> g_stream_packet = {};
static uint32_t g_stream_packet_size = 0;
static uint32_t g_stream_state = 0; // 0=idle, 1=collecting, 2=ready

static void resetStreamState() {
    g_stream_packet.fill(0);
    g_stream_packet_size = 0;
    g_stream_state = 0;
}

static bool containsCaseInsensitive(const char* haystack, const char* needle) {
    if (!haystack || !needle || needle[0] == '\0') {
        return false;
    }
    const size_t needleLen = std::strlen(needle);
    const size_t hayLen = std::strlen(haystack);
    if (needleLen > hayLen) {
        return false;
    }
    for (size_t i = 0; i + needleLen <= hayLen; ++i) {
        bool match = true;
        for (size_t j = 0; j < needleLen; ++j) {
            const unsigned char hc = static_cast<unsigned char>(haystack[i + j]);
            const unsigned char nc = static_cast<unsigned char>(needle[j]);
            if (std::tolower(hc) != std::tolower(nc)) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

static const RTPDescriptor* findDescriptorByUUID(const uint8_t uuid[16]) {
    for (uint32_t i = 0; i < g_rtp_descriptor_count; ++i) {
        if (std::memcmp(g_rtp_descriptors[i].tool_uuid, uuid, 16) == 0) {
            return &g_rtp_descriptors[i];
        }
    }
    return nullptr;
}

static void writeSafeResult(char* out, uint32_t cap, const char* text) {
    if (!out || cap == 0) {
        return;
    }
    if (!text) {
        out[0] = '\0';
        return;
    }
    std::snprintf(out, cap, "%s", text);
}

} // namespace

extern "C" {

void RTP_InitDescriptorTable(void) {
    std::memset(g_rtp_descriptors, 0, sizeof(g_rtp_descriptors));
    g_rtp_descriptor_count = 4;

    // tool[0] open_file
    g_rtp_descriptors[0].tool_id = 1;
    g_rtp_descriptors[0].legacy_tool_id = 1001;
    g_rtp_descriptors[0].name_hash = 0x6f70656e5f66696cULL; // "open_fil" packed
    g_rtp_descriptors[0].name = "open_file";
    g_rtp_descriptors[0].description = "Open a workspace file";
    g_rtp_descriptors[0].param_count = 1;
    const uint8_t uuid0[16] = {0x10, 0x01, 0xAA, 0xBB, 0x00, 0x00, 0x40, 0x10, 0x80, 0, 0, 0, 0, 0, 0, 1};
    std::memcpy(g_rtp_descriptors[0].tool_uuid, uuid0, 16);

    // tool[1] search_workspace
    g_rtp_descriptors[1].tool_id = 2;
    g_rtp_descriptors[1].legacy_tool_id = 1002;
    g_rtp_descriptors[1].name_hash = 0x7365617263685f77ULL; // "search_w"
    g_rtp_descriptors[1].name = "search_workspace";
    g_rtp_descriptors[1].description = "Search workspace contents";
    g_rtp_descriptors[1].param_count = 1;
    const uint8_t uuid1[16] = {0x10, 0x02, 0xAA, 0xBB, 0x00, 0x00, 0x40, 0x10, 0x80, 0, 0, 0, 0, 0, 0, 2};
    std::memcpy(g_rtp_descriptors[1].tool_uuid, uuid1, 16);

    // tool[2] edit_file
    g_rtp_descriptors[2].tool_id = 3;
    g_rtp_descriptors[2].legacy_tool_id = 1003;
    g_rtp_descriptors[2].name_hash = 0x656469745f66696cULL; // "edit_fil"
    g_rtp_descriptors[2].name = "edit_file";
    g_rtp_descriptors[2].description = "Apply workspace edit";
    g_rtp_descriptors[2].param_count = 2;
    const uint8_t uuid2[16] = {0x10, 0x03, 0xAA, 0xBB, 0x00, 0x00, 0x40, 0x10, 0x80, 0, 0, 0, 0, 0, 0, 3};
    std::memcpy(g_rtp_descriptors[2].tool_uuid, uuid2, 16);

    // tool[3] execute_command (used by policy smoke test)
    g_rtp_descriptors[3].tool_id = 4;
    g_rtp_descriptors[3].legacy_tool_id = 1004;
    g_rtp_descriptors[3].name_hash = 0x657865637574655fULL; // "execute_"
    g_rtp_descriptors[3].name = "execute_command";
    g_rtp_descriptors[3].description = "Execute shell command with policy gate";
    g_rtp_descriptors[3].param_count = 1;
    const uint8_t uuid3[16] = {0x10, 0x04, 0xAA, 0xBB, 0x00, 0x00, 0x40, 0x10, 0x80, 0, 0, 0, 0, 0, 0, 4};
    std::memcpy(g_rtp_descriptors[3].tool_uuid, uuid3, 16);
}

const RTPDescriptor* RTP_GetDescriptorTable(void) {
    if (g_rtp_descriptor_count == 0) {
        RTP_InitDescriptorTable();
    }
    return g_rtp_descriptors;
}

uint32_t RTP_GetDescriptorCount(void) {
    if (g_rtp_descriptor_count == 0) {
        RTP_InitDescriptorTable();
    }
    return g_rtp_descriptor_count;
}

int32_t RTP_ValidatePacket(const void* packet, uint32_t packet_bytes) {
    const uint32_t nativeHeaderSize = static_cast<uint32_t>(sizeof(RTPPacketHeader));
    if (!packet || packet_bytes < nativeHeaderSize) {
        return RTP_ERR_INVALID_ARG;
    }
    RTPPacketHeader hdr{};
    std::memcpy(&hdr, packet, sizeof(hdr));
    if (hdr.magic != RTP_PACKET_MAGIC || hdr.version != RTP_PACKET_VERSION) {
        return RTP_ERR_INVALID_PACKET;
    }
    const uint32_t declaredHeader =
        (hdr.header_size == RTP_PACKET_HEADER_SIZE || hdr.header_size == nativeHeaderSize)
        ? hdr.header_size
        : nativeHeaderSize;
    const uint32_t payloadOffset = std::min(declaredHeader, nativeHeaderSize);
    const uint64_t fullSize = static_cast<uint64_t>(payloadOffset) + static_cast<uint64_t>(hdr.payload_size);
    if (packet_bytes < fullSize) {
        return RTP_ERR_INVALID_PACKET;
    }
    if (g_rtp_descriptor_count == 0) {
        RTP_InitDescriptorTable();
    }
    if (!findDescriptorByUUID(hdr.tool_uuid)) {
        return RTP_ERR_UNKNOWN_TOOL;
    }
    g_rtp_telemetry[TEL_PACKETS_VALID]++;
    return RTP_OK;
}

int32_t RTP_DispatchPacket(const void* packet,
                           uint32_t packet_bytes,
                           char* result_buf,
                           uint32_t result_buf_size) {
    if (!packet || !result_buf || result_buf_size == 0) {
        return RTP_ERR_INVALID_ARG;
    }
    const int32_t valid = RTP_ValidatePacket(packet, packet_bytes);
    if (valid != RTP_OK) {
        g_rtp_telemetry[TEL_DISPATCH_ERR]++;
        writeSafeResult(result_buf, result_buf_size, "dispatch_error: invalid packet");
        return valid;
    }

    RTPPacketHeader hdr{};
    std::memcpy(&hdr, packet, sizeof(hdr));
    const RTPDescriptor* desc = findDescriptorByUUID(hdr.tool_uuid);
    if (!desc) {
        g_rtp_telemetry[TEL_DISPATCH_ERR]++;
        writeSafeResult(result_buf, result_buf_size, "dispatch_error: unknown tool");
        return RTP_ERR_UNKNOWN_TOOL;
    }

    const uint32_t payloadOffset = std::min<uint32_t>(hdr.header_size, static_cast<uint32_t>(sizeof(RTPPacketHeader)));
    const char* payload = reinterpret_cast<const char*>(static_cast<const uint8_t*>(packet) + payloadOffset);
    std::string payloadCopy;
    payloadCopy.assign(payload, payload + hdr.payload_size);

    // Simple policy gate for command execution lane.
    if ((desc->tool_id == 4 || desc->legacy_tool_id == 1004) &&
        (containsCaseInsensitive(payloadCopy.c_str(), "del /q") ||
         containsCaseInsensitive(payloadCopy.c_str(), "rm -rf") ||
         containsCaseInsensitive(payloadCopy.c_str(), "format "))) {
        g_rtp_telemetry[TEL_POLICY_BLOCKS]++;
        g_rtp_telemetry[TEL_DISPATCH_ERR]++;
        writeSafeResult(result_buf, result_buf_size, "policy_block: command denied");
        return RTP_ERR_POLICY_BLOCK;
    }

    char out[512] = {};
    std::snprintf(out, sizeof(out),
                  "dispatch_ok tool=%s call_id=%llu payload_bytes=%u",
                  desc->name ? desc->name : "unknown",
                  static_cast<unsigned long long>(hdr.call_id),
                  hdr.payload_size);
    writeSafeResult(result_buf, result_buf_size, out);
    g_rtp_telemetry[TEL_DISPATCH_OK]++;
    return RTP_OK;
}

int32_t RTP_BuildContextBlob(void* out_buf, uint32_t out_cap, uint32_t* out_written) {
    if (g_rtp_descriptor_count == 0) {
        RTP_InitDescriptorTable();
    }
    if (out_written) {
        *out_written = 0;
    }

    int n = std::snprintf(
        g_rtp_context_blob.data(), g_rtp_context_blob.size(),
        "{\"version\":%u,\"descriptorCount\":%u,\"tools\":[\"%s\",\"%s\",\"%s\",\"%s\"]}",
        RTP_PACKET_VERSION,
        g_rtp_descriptor_count,
        g_rtp_descriptors[0].name ? g_rtp_descriptors[0].name : "",
        g_rtp_descriptors[1].name ? g_rtp_descriptors[1].name : "",
        g_rtp_descriptors[2].name ? g_rtp_descriptors[2].name : "",
        g_rtp_descriptors[3].name ? g_rtp_descriptors[3].name : "");
    if (n < 0) {
        return RTP_ERR_INVALID_PACKET;
    }
    g_rtp_context_size = static_cast<uint32_t>(std::min<int>(n, static_cast<int>(g_rtp_context_blob.size() - 1)));

    if (out_buf && out_cap > 0) {
        const uint32_t copyLen = std::min(out_cap - 1, g_rtp_context_size);
        std::memcpy(out_buf, g_rtp_context_blob.data(), copyLen);
        static_cast<char*>(out_buf)[copyLen] = '\0';
        if (out_written) {
            *out_written = copyLen;
        }
    }
    return RTP_OK;
}

const void* RTP_GetContextBlobPtr(void) {
    if (g_rtp_context_size == 0) {
        uint32_t ignored = 0;
        RTP_BuildContextBlob(nullptr, 0, &ignored);
    }
    return g_rtp_context_blob.data();
}

uint32_t RTP_GetContextBlobSize(void) {
    if (g_rtp_context_size == 0) {
        uint32_t ignored = 0;
        RTP_BuildContextBlob(nullptr, 0, &ignored);
    }
    return g_rtp_context_size;
}

const void* RTP_GetTelemetrySnapshot(void) {
    return g_rtp_telemetry.data();
}

int32_t RTP_AgentLoop_Run(const char* user_prompt_utf8,
                          char* out_buf,
                          uint32_t out_cap,
                          uint32_t max_iters) {
    if (!out_buf || out_cap == 0) {
        return RTP_ERR_INVALID_ARG;
    }
    const uint32_t rounds = std::max<uint32_t>(1, std::min<uint32_t>(max_iters, 8));
    g_rtp_telemetry[TEL_AGENT_LOOP_ROUNDS] += rounds;
    const char* prompt = (user_prompt_utf8 && user_prompt_utf8[0] != '\0')
        ? user_prompt_utf8
        : "no_prompt";
    std::snprintf(out_buf, out_cap, "RTP agent loop ready; rounds=%u; prompt=%s", rounds, prompt);
    return RTP_OK;
}

void RTP_StreamParser_Init(void) {
    resetStreamState();
}

void RTP_StreamParser_Reset(void) {
    resetStreamState();
}

int32_t RTP_StreamParser_PushByte(uint8_t byte_value) {
    if (g_stream_packet_size >= g_stream_packet.size()) {
        resetStreamState();
        return RTP_ERR_BUFFER_TOO_SMALL;
    }

    g_stream_packet[g_stream_packet_size++] = byte_value;
    g_stream_state = 1;

    if (g_stream_packet_size < sizeof(RTPPacketHeader)) {
        return RTP_OK;
    }

    RTPPacketHeader hdr{};
    std::memcpy(&hdr, g_stream_packet.data(), sizeof(hdr));
    if (hdr.magic != RTP_PACKET_MAGIC || hdr.version != RTP_PACKET_VERSION) {
        resetStreamState();
        return RTP_ERR_INVALID_PACKET;
    }

    const uint32_t payloadOffset = std::min<uint32_t>(hdr.header_size, static_cast<uint32_t>(sizeof(RTPPacketHeader)));
    const uint64_t fullSize = static_cast<uint64_t>(payloadOffset) + static_cast<uint64_t>(hdr.payload_size);
    if (fullSize > g_stream_packet.size()) {
        resetStreamState();
        return RTP_ERR_BUFFER_TOO_SMALL;
    }
    if (g_stream_packet_size >= fullSize) {
        g_stream_packet_size = static_cast<uint32_t>(fullSize);
        g_stream_state = 2;
        return 1;
    }

    return RTP_OK;
}

int32_t RTP_StreamParser_GetPacket(void* out_buf, uint32_t out_cap, uint32_t* out_written) {
    if (out_written) {
        *out_written = 0;
    }
    if (g_stream_state != 2) {
        return RTP_ERR_INVALID_PACKET;
    }
    if (!out_buf || out_cap < g_stream_packet_size) {
        return RTP_ERR_BUFFER_TOO_SMALL;
    }
    std::memcpy(out_buf, g_stream_packet.data(), g_stream_packet_size);
    if (out_written) {
        *out_written = g_stream_packet_size;
    }
    resetStreamState();
    return RTP_OK;
}

uint32_t RTP_StreamParser_GetState(void) {
    return g_stream_state;
}

int32_t RTP_EncodeToolResultFrame(uint64_t call_id,
                                  int32_t status_code,
                                  const void* payload,
                                  uint32_t payload_size,
                                  void* out_buf,
                                  uint32_t out_cap,
                                  uint32_t* out_written) {
    if (out_written) {
        *out_written = 0;
    }
    if (!out_buf || out_cap < RTP_RESULT_HEADER_SIZE) {
        return RTP_ERR_BUFFER_TOO_SMALL;
    }

    const uint64_t fullSize = static_cast<uint64_t>(RTP_RESULT_HEADER_SIZE) + static_cast<uint64_t>(payload_size);
    if (out_cap < fullSize) {
        return RTP_ERR_BUFFER_TOO_SMALL;
    }

    RTPResultHeader hdr{};
    hdr.magic = RTP_RESULT_MAGIC;
    hdr.version = RTP_RESULT_VERSION;
    hdr.header_size = RTP_RESULT_HEADER_SIZE;
    hdr.call_id = call_id;
    hdr.status_code = status_code;
    hdr.payload_size = payload_size;

    std::memcpy(out_buf, &hdr, sizeof(hdr));
    if (payload && payload_size > 0) {
        std::memcpy(static_cast<uint8_t*>(out_buf) + sizeof(hdr), payload, payload_size);
    }
    if (out_written) {
        *out_written = static_cast<uint32_t>(fullSize);
    }
    return RTP_OK;
}

} // extern "C"
