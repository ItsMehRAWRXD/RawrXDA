// ============================================================================
// AssetExchangeManager_Internal.h
// Provides private implementation details for AssetExchangeManager
// ============================================================================
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <sstream>

namespace RawrXD {
namespace P2P {

enum NodeCapability : uint32_t {
    CAP_AVX2      = 1u << 0,
    CAP_AVX512    = 1u << 1,
    CAP_VULKAN    = 1u << 2,
    CAP_ENCRYPTED = 1u << 3,
};

struct IdentityAnnouncement {
    std::string nodeId;
    uint32_t capabilities{0};
    uint16_t port{0};
    uint64_t epochMs{0};
};

struct KernelProposalAnnouncement {
    std::string nodeId;
    std::string kernelName;
    uint64_t payloadSize{0};
    uint64_t epochMs{0};
};

inline std::string BuildIdentityMessage(const IdentityAnnouncement& msg) {
    std::ostringstream oss;
    oss << "RAWRXD_SOVEREIGN_NODE"
        << "|id=" << msg.nodeId
        << "|caps=" << msg.capabilities
        << "|port=" << msg.port
        << "|ts=" << msg.epochMs;
    return oss.str();
}

inline bool ParseIdentityMessage(const std::string& wire, IdentityAnnouncement& out) {
    const std::string prefix = "RAWRXD_SOVEREIGN_NODE|";
    if (wire.rfind(prefix, 0) != 0) {
        return false;
    }

    auto readField = [&](const std::string& key, std::string& value) -> bool {
        const std::string token = key + "=";
        const size_t begin = wire.find(token);
        if (begin == std::string::npos) return false;
        const size_t valueBegin = begin + token.size();
        size_t valueEnd = wire.find('|', valueBegin);
        if (valueEnd == std::string::npos) valueEnd = wire.size();
        value = wire.substr(valueBegin, valueEnd - valueBegin);
        return true;
    };

    std::string nodeId;
    std::string caps;
    std::string port;
    std::string ts;
    if (!readField("id", nodeId) || !readField("caps", caps) || !readField("port", port) || !readField("ts", ts)) {
        return false;
    }

    try {
        out.nodeId = nodeId;
        out.capabilities = static_cast<uint32_t>(std::stoul(caps));
        out.port = static_cast<uint16_t>(std::stoul(port));
        out.epochMs = static_cast<uint64_t>(std::stoull(ts));
    } catch (...) {
        return false;
    }

    return !out.nodeId.empty();
}

inline std::string BuildKernelProposalMessage(const KernelProposalAnnouncement& msg) {
    std::ostringstream oss;
    oss << "RAWRXD_KERNEL_PROPOSAL"
        << "|id=" << msg.nodeId
        << "|name=" << msg.kernelName
        << "|size=" << msg.payloadSize
        << "|ts=" << msg.epochMs;
    return oss.str();
}

inline bool ParseKernelProposalMessage(const std::string& wire, KernelProposalAnnouncement& out) {
    const std::string prefix = "RAWRXD_KERNEL_PROPOSAL|";
    if (wire.rfind(prefix, 0) != 0) {
        return false;
    }

    auto readField = [&](const std::string& key, std::string& value) -> bool {
        const std::string token = key + "=";
        const size_t begin = wire.find(token);
        if (begin == std::string::npos) return false;
        const size_t valueBegin = begin + token.size();
        size_t valueEnd = wire.find('|', valueBegin);
        if (valueEnd == std::string::npos) valueEnd = wire.size();
        value = wire.substr(valueBegin, valueEnd - valueBegin);
        return true;
    };

    std::string nodeId;
    std::string kernelName;
    std::string size;
    std::string ts;
    if (!readField("id", nodeId) || !readField("name", kernelName) || !readField("size", size) || !readField("ts", ts)) {
        return false;
    }

    try {
        out.nodeId = nodeId;
        out.kernelName = kernelName;
        out.payloadSize = static_cast<uint64_t>(std::stoull(size));
        out.epochMs = static_cast<uint64_t>(std::stoull(ts));
    } catch (...) {
        return false;
    }

    return !out.nodeId.empty() && !out.kernelName.empty();
}

// Simplified simulator for the "Truth" we expect from a kernel
inline uint64_t CalculateTruth(const std::string& kernel, uint64_t seed) {
    uint64_t t = seed;
    for (char c : kernel) {
        t = (t ^ (uint64_t)c) * 0x100000001b3ULL;
    }
    return t;
}

} // namespace P2P
} // namespace RawrXD
