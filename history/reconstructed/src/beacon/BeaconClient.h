#pragma once
// ============================================================================
// BeaconClient.h — Win32 HTTP Beacon Client (Qt-Free, WinHTTP-based)
// ============================================================================
// Wraps WinHTTP for HTTP beacon communication (outbound to beacon-manager.js).
// Uses the existing BeaconHub/BeaconKind enum system from circular_beacon_system.h.
// The old BeaconClient in src/BeaconClient.h is a MASM-linked C++ wrapper;
// this is a pure C++20 + WinHTTP replacement for beacon/gui_pane_beacon_wiring.
//
// No Qt. No OpenSSL. No external deps beyond Windows SDK.
// ============================================================================

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include "../RawrXD_SignalSlot.h"
#include "../../include/circular_beacon_system.h"
#pragma comment(lib, "winhttp.lib")

namespace RawrXD {

// ============================================================================
// WinHTTPBeaconClient — Stateful HTTP client for the beacon mesh
// ============================================================================
class WinHTTPBeaconClient {
    HINTERNET m_hSession = nullptr;
    HINTERNET m_hConnect = nullptr;
    std::wstring m_serverAddr;
    int m_port;
    std::atomic<bool> m_running{false};
    std::thread m_pollThread;

    // Signal fired for every inbound message from the poll loop
    Signal<const BeaconMessage&> m_onMessage;

    // Command → handler dispatch table
    std::unordered_map<std::string, std::function<void(const BeaconMessage&)>> m_handlers;
    std::mutex m_handlerMutex;

public:
    explicit WinHTTPBeaconClient(const std::wstring& addr = L"localhost", int port = 9001)
        : m_serverAddr(addr), m_port(port) {}

    ~WinHTTPBeaconClient() { Stop(); }

    // Non-copyable
    WinHTTPBeaconClient(const WinHTTPBeaconClient&) = delete;
    WinHTTPBeaconClient& operator=(const WinHTTPBeaconClient&) = delete;

    // ── Lifecycle ──
    bool Start() {
        m_hSession = WinHttpOpen(L"RawrXD-Beacon/1.0",
                                  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
        if (!m_hSession) return false;

        m_hConnect = WinHttpConnect(m_hSession, m_serverAddr.c_str(),
                                    static_cast<INTERNET_PORT>(m_port), 0);
        if (!m_hConnect) {
            WinHttpCloseHandle(m_hSession);
            m_hSession = nullptr;
            return false;
        }

        m_running = true;
        m_pollThread = std::thread(&WinHTTPBeaconClient::PollLoop, this);

        // Register IDE beacon on the HTTP server
        PostJSON(L"/api/beacon/register",
                 "{\"type\":\"IDE\",\"id\":\"rawrxd_ide\",\"version\":\"3.0\"}");
        return true;
    }

    void Stop() {
        m_running = false;
        if (m_pollThread.joinable()) m_pollThread.join();
        if (m_hConnect) { WinHttpCloseHandle(m_hConnect); m_hConnect = nullptr; }
        if (m_hSession) { WinHttpCloseHandle(m_hSession); m_hSession = nullptr; }
    }

    bool IsRunning() const { return m_running.load(); }

    // ── Handler Registration ──
    void RegisterHandler(const std::string& verb,
                         std::function<void(const BeaconMessage&)> handler)
    {
        std::lock_guard<std::mutex> lock(m_handlerMutex);
        m_handlers[verb] = std::move(handler);
    }

    // ── Outbound Send ──
    bool Send(BeaconKind target, const char* verb,
              const char* payload = nullptr, size_t payloadLen = 0)
    {
        // Build JSON body
        std::string body = "{\"source\":\"IDE\",\"target\":";
        body += std::to_string(static_cast<uint32_t>(target));
        body += ",\"verb\":\"";
        body += verb;
        body += "\"";
        if (payload && payloadLen > 0) {
            body += ",\"payload\":\"";
            body.append(payload, payloadLen);
            body += "\"";
        }
        body += "}";

        return PostJSON(L"/api/beacon/message", body);
    }

    // Convenience overload with string payload
    bool Send(BeaconKind target, const char* verb, const std::string& payload) {
        return Send(target, verb, payload.c_str(), payload.size());
    }

    // ── Signal Access ──
    Signal<const BeaconMessage&>& MessageSignal() { return m_onMessage; }

private:
    // HTTP POST helper
    bool PostJSON(const std::wstring& path, const std::string& body) {
        if (!m_hConnect) return false;

        HINTERNET hReq = WinHttpOpenRequest(m_hConnect, L"POST", path.c_str(),
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_ESCAPE_PERCENT);
        if (!hReq) return false;

        LPCWSTR headers = L"Content-Type: application/json\r\n";
        bool ok = WinHttpSendRequest(hReq, headers, static_cast<DWORD>(-1),
                                      const_cast<char*>(body.c_str()),
                                      static_cast<DWORD>(body.size()),
                                      static_cast<DWORD>(body.size()), 0) != 0;
        if (ok) WinHttpReceiveResponse(hReq, nullptr);
        WinHttpCloseHandle(hReq);
        return ok;
    }

    // HTTP GET helper
    std::string GetJSON(const std::wstring& path) {
        if (!m_hConnect) return {};

        HINTERNET hReq = WinHttpOpenRequest(m_hConnect, L"GET", path.c_str(),
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hReq) return {};

        std::string result;
        if (WinHttpSendRequest(hReq, nullptr, 0, nullptr, 0, 0, 0) &&
            WinHttpReceiveResponse(hReq, nullptr)) {
            DWORD available = 0;
            while (WinHttpQueryDataAvailable(hReq, &available) && available > 0) {
                std::vector<char> buf(available + 1);
                DWORD read = 0;
                WinHttpReadData(hReq, buf.data(), available, &read);
                result.append(buf.data(), read);
            }
        }
        WinHttpCloseHandle(hReq);
        return result;
    }

    // Poll loop — 20Hz check for inbound messages
    void PollLoop() {
        while (m_running) {
            Sleep(50);
            CheckMessages();
        }
    }

    void CheckMessages() {
        auto json = GetJSON(L"/api/beacon/poll/ide");
        if (json.empty() || json == "[]" || json == "{}") return;
        ParseAndDispatch(json);
    }

    void ParseAndDispatch(const std::string& json) {
        // Extract verb from JSON response (minimal parse)
        std::string verb;
        auto vpos = json.find("\"verb\":\"");
        if (vpos != std::string::npos) {
            vpos += 8;
            auto vend = json.find('"', vpos);
            if (vend != std::string::npos)
                verb = json.substr(vpos, vend - vpos);
        }

        // Extract payload
        std::string payload;
        auto ppos = json.find("\"payload\":\"");
        if (ppos != std::string::npos) {
            ppos += 11;
            auto pend = json.find('"', ppos);
            if (pend != std::string::npos)
                payload = json.substr(ppos, pend - ppos);
        }

        // Build BeaconMessage using the real struct from circular_beacon_system.h
        BeaconMessage msg = {};
        msg.messageId = GetTickCount() & 0xFFFFFFFF;
        msg.sourceKind = BeaconKind::Unknown;
        msg.targetKind = BeaconKind::IDECore;
        msg.direction = BeaconDirection::Forward;
        msg.verb = verb.c_str();
        msg.payload = payload.c_str();
        msg.payloadLen = payload.size();
        msg.flags = BeaconMessage::FLAG_FIRE_AND_FORGET;

        // Dispatch to registered handler
        {
            std::lock_guard<std::mutex> lock(m_handlerMutex);
            auto it = m_handlers.find(verb);
            if (it != m_handlers.end()) it->second(msg);
        }

        // Fire signal for global listeners
        m_onMessage.emit(msg);
    }
};

} // namespace RawrXD
