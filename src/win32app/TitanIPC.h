#pragma once
// TitanIPC.h — Inference process-isolation IPC protocol
//
// Architecture:
//   RawrXD-Win32IDE.exe  ←—named pipe—→  RawrXD-TitanHost.exe
//       TitanProxy (client)                TitanHostServer (server)
//                                          loads RawrXD_Titan.dll in-process
//
// If TitanHost crashes:
//   - The IDE pipe client gets ERROR_BROKEN_PIPE
//   - TitanProxy::Submit() returns an error string (not a crash)
//   - TitanProxy::EnsureHost() restarts TitanHost automatically
//
// Wire protocol: length-prefixed binary frames, no heap allocation in server.
//   [4 bytes: uint32_t payload_len] [payload_len bytes: JSON UTF-8]
//
// Named pipe: \\.\pipe\RawrXD_TitanHost_{PID_of_IDE}
//   Per-IDE-instance pipe name prevents cross-contamination.
//
// Frozen streaming transport contract (implementation lane):
//   see TitanStreamIPCContract.h for shared-memory ring + mailbox + event ABI.

#include <cstdint>
#include <functional>
#include <string>
#include <windows.h>

#include "TitanStreamIPCContract.h"


namespace RawrXD
{

// ============================================================================
// Protocol constants
// ============================================================================

// JSON command keys
//   Request:   {"cmd":"infer",  "prompt":"...", "max_tokens":256, "seq":N}
//   Response:  {"seq":N, "ok":true,  "completion":"...", "metadata":"..."}
//   Response:  {"seq":N, "ok":false, "error":"..."}                      -- inference error
//   Response:  {"seq":N, "ok":false, "error":"...", "fatal":true}       -- kernel fault; host is exiting
//   Request:   {"cmd":"load_model", "path":"D:/models/foo.gguf"}   -- UTF-8 path; loads weights in TitanHost
//   Response:  {"ok":true}  or  {"ok":false, "error":"..."}
//   (Preferred over relying on RAWRXD_NATIVE_MODEL_PATH alone; IDE calls this before infer.)
//
//   Request:   {"cmd":"ping"}
//   Response:  {"pong":true, "build":"..."}
//   Request:   {"cmd":"exit"}
//
// Tool-call round-trip (Writer/Thinker boundary):
//   TitanHost → IDE:  {"cmd":"tool_call", "seq":N, "round":R, "payload":"{\"name\":\"...\",\"args\":{...}}"}
//   IDE       → Host: {"cmd":"tool_result", "seq":N, "round":R, "result":"...result text..."}
// Host feeds result back into next inference round.  Up to 8 rounds per request.

static constexpr uint32_t TITAN_PIPE_FRAME_MAX = 8 * 1024 * 1024;  // 8 MB
static constexpr uint32_t TITAN_DEFAULT_TIMEOUT_MS = 120'000;      // 2 min
static constexpr const char* TITAN_HOST_EXE = "RawrXD-TitanHost.exe";

// ============================================================================
// TitanProxy — client side, lives inside the IDE process
// ============================================================================

class TitanProxy
{
  public:
    // Acquire singleton.
    static TitanProxy& instance();

    // Send an inference request.  Blocking up to timeoutMs.
    // Returns true on success; fills  completion+metadata.
    // Returns false; fills error string on failure/crash/timeout.
    bool submit(const std::string& prompt, uint32_t maxTokens, uint32_t timeoutMs, std::string& completion,
                std::string& metadata, std::string& error);

    // Optional streaming token callback.  Called on a worker thread.
    // If set, individual tokens are pushed before the final completion arrives.
    using TokenCb = std::function<void(const std::string& token)>;
    void setTokenCallback(TokenCb cb) { m_tokenCb = std::move(cb); }

    // Tool-call dispatch callback.  Called synchronously inside submit()
    // whenever TitanHost emits a {"cmd":"tool_call"} frame.
    // The callback receives the raw JSON payload string (the value of "payload"
    // in the tool_call frame) and must return the result string to feed back.
    // Wire-level framing is handled internally by TitanProxy.
    // Implement using AgenticExecutor::callTool() in the registration site.
    using ToolCallCb = std::function<std::string(const std::string& payloadJson)>;
    void setToolCallCallback(ToolCallCb cb) { m_toolCallCb = std::move(cb); }

    // Ping the host process (health check).
    bool ping(std::string& buildInfo);

    // Push model path to TitanHost over the pipe (UTF-8). Idempotent for the same path.
    // Returns false if the host reports failure or the pipe breaks.
    bool loadModel(const std::string& pathUtf8, std::string& error);

    // Drop cached path (e.g. after RAWRXD_NATIVE_MODEL_PATH is cleared / model unloaded).
    void clearLoadedModelCache();

    // Gracefully shut down the host process.
    void shutdown();

    // Force-restart the host process (e.g., after a crash detection).
    void restartHost();

    bool isConnected() const { return m_hPipe != INVALID_HANDLE_VALUE; }

  private:
    TitanProxy() = default;
    ~TitanProxy() { shutdown(); }
    TitanProxy(const TitanProxy&) = delete;
    TitanProxy& operator=(const TitanProxy&) = delete;

    // Spawn TitanHost.exe and connect the pipe.
    bool ensureHost();

    // Low-level frame I/O — not thread-safe, call with m_mu held.
    bool sendFrame(const std::string& json, std::string& outErr);
    bool recvFrame(std::string& json, uint32_t timeoutMs, std::string& outErr);

    // Kill the host process without sending "exit" (used after crash).
    void forceKillHost();

    HANDLE m_hPipe = INVALID_HANDLE_VALUE;
    HANDLE m_hProcess = nullptr;  // HANDLE to TitanHost.exe
    HANDLE m_hJob = nullptr;      // Job object keeping host alive
    uint32_t m_seq = 0;

    // Critical section for serialising concurrent callers.
    CRITICAL_SECTION m_cs{};
    bool m_csInit = false;

    TokenCb m_tokenCb;
    ToolCallCb m_toolCallCb;

    // Last path successfully sent to TitanHost (avoids redundant load_model frames).
    std::string m_lastLoadedModelPathUtf8;

    std::string pipeName() const;
};

}  // namespace RawrXD
