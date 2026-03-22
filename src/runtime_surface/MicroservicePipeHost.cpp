#include "rawrxd/runtime/MicroservicePipeHost.hpp"

#include "../logging/Logger.h"

#include <mutex>
#include <vector>

namespace RawrXD::Runtime {

MicroservicePipeHost& MicroservicePipeHost::instance() {
    static MicroservicePipeHost s;
    return s;
}

RuntimeResult MicroservicePipeHost::start(const std::wstring& suffix, LineHandler handler) {
    if (suffix.empty()) {
        return std::unexpected("MicroservicePipeHost: empty suffix");
    }
    if (!handler) {
        return std::unexpected("MicroservicePipeHost: null handler");
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_running.load(std::memory_order_acquire)) {
        return std::unexpected("MicroservicePipeHost: already running");
    }
    m_suffix = suffix;
    m_handler = std::move(handler);
    m_stop.store(false, std::memory_order_release);
    m_running.store(true, std::memory_order_release);
    lock.unlock();
    m_thread = std::thread([this] { serverThreadMain(); });
    RawrXD::Logging::Logger::instance().info("[MicroservicePipeHost] started pipe RawrXD service thread",
                                             "RuntimeSurface");
    return {};
}

void MicroservicePipeHost::stop() {
    m_stop.store(true, std::memory_order_release);
    std::thread t;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_thread.joinable()) {
            t = std::move(m_thread);
        }
    }
    if (t.joinable()) {
        t.join();
    }
    m_running.store(false, std::memory_order_release);
}

void MicroservicePipeHost::serverThreadMain() {
    std::wstring pipePath = L"\\\\.\\pipe\\RawrXD_";
    pipePath += m_suffix;

    // Synchronous pipe (ConnectNamedPipe/ReadFile). stop() takes effect between client sessions.
    while (!m_stop.load(std::memory_order_acquire)) {
        HANDLE pipe = CreateNamedPipeW(
            pipePath.c_str(),
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            65536,
            65536,
            0,
            nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            RawrXD::Logging::Logger::instance().error("[MicroservicePipeHost] CreateNamedPipe failed",
                                                        "RuntimeSurface");
            break;
        }

        const BOOL connected = ConnectNamedPipe(pipe, nullptr);
        if (!connected) {
            const DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED) {
                CloseHandle(pipe);
                continue;
            }
        }

        std::string line;
        std::vector<char> buf(4096);
        for (;;) {
            if (m_stop.load(std::memory_order_acquire)) {
                break;
            }
            DWORD nread = 0;
            if (!ReadFile(pipe, buf.data(), static_cast<DWORD>(buf.size()), &nread, nullptr) || nread == 0) {
                break;
            }
            line.append(buf.data(), buf.data() + nread);
            for (;;) {
                const auto pos = line.find('\n');
                if (pos == std::string::npos) {
                    break;
                }
                std::string one = line.substr(0, pos);
                line.erase(0, pos + 1);
                if (!one.empty() && one.back() == '\r') {
                    one.pop_back();
                }
                LineHandler h;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    h = m_handler;
                }
                if (h) {
                    h(one);
                }
            }
        }

        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }

    m_running.store(false, std::memory_order_release);
}

}  // namespace RawrXD::Runtime
