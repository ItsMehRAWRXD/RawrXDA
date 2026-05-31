#pragma once

#ifndef RAWRXD_BUILD_STABILIZER_INCLUDED
#define RAWRXD_BUILD_STABILIZER_INCLUDED

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace rawrxd::stability {

class ThreadSafeJSONParser {
public:
    std::optional<nlohmann::json> parse(const std::string& jsonText) {
        std::lock_guard<std::mutex> lock(mtx_);
        try {
            return nlohmann::json::parse(jsonText);
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    std::mutex mtx_;
};

class DynamicPortManager {
public:
    explicit DynamicPortManager(uint16_t startPort = 9000) : nextPort_(startPort) {}

    uint16_t acquire(uint16_t upperBound = 65535) {
        uint16_t candidate = nextPort_.fetch_add(1, std::memory_order_relaxed);
        while (candidate != 0 && candidate <= upperBound) {
            if (isPortAvailable(candidate)) {
                return candidate;
            }
            candidate = nextPort_.fetch_add(1, std::memory_order_relaxed);
        }
        return 0;
    }

    static uint16_t acquireFrom(uint16_t startPort, uint16_t upperBound = 65535) {
        if (startPort == 0 || startPort > upperBound) {
            return 0;
        }

        for (uint32_t p = startPort; p <= upperBound; ++p) {
            const uint16_t port = static_cast<uint16_t>(p);
            if (isPortAvailable(port)) {
                return port;
            }
        }

        return 0;
    }

    static bool isPortAvailable(uint16_t port) {
        SOCKET probe = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (probe == INVALID_SOCKET) {
            return false;
        }

        BOOL exclusiveAddrUse = 1;
        setsockopt(probe, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusiveAddrUse), sizeof(exclusiveAddrUse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        const int bindResult = bind(probe, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr));
        closesocket(probe);
        return bindResult == 0;
    }

private:
    std::atomic<uint16_t> nextPort_;
};

class BuildValidator {
public:
    static bool validatePort(uint16_t port) {
        return port > 0;
    }
};

}  // namespace rawrxd::stability

#endif  // RAWRXD_BUILD_STABILIZER_INCLUDED
