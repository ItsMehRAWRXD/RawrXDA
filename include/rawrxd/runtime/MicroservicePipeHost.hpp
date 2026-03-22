#pragma once

#include "RuntimeTypes.hpp"

#include <atomic>
#include <expected>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD::Runtime {

/// Named-pipe microservice host (one line = one JSON or text command). No directory scan.
class MicroservicePipeHost {
public:
    static MicroservicePipeHost& instance();

    using LineHandler = std::function<void(const std::string& line)>;

    /// Pipe path: `\\.\pipe\RawrXD_<suffix>` (suffix alphanumeric).
    [[nodiscard]] RuntimeResult start(const std::wstring& suffix, LineHandler handler);
    void stop();

    [[nodiscard]] bool running() const { return m_running.load(std::memory_order_acquire); }

private:
    MicroservicePipeHost() = default;
    ~MicroservicePipeHost() { stop(); }

    MicroservicePipeHost(const MicroservicePipeHost&) = delete;
    MicroservicePipeHost& operator=(const MicroservicePipeHost&) = delete;

    void serverThreadMain();

    std::mutex m_mutex{};
    std::wstring m_suffix{};
    LineHandler m_handler{};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop{false};
    std::thread m_thread{};
};

}  // namespace RawrXD::Runtime
