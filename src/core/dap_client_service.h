#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace RawrXD::Debug
{

struct DapLaunchConfig
{
    std::string program;
    std::string cwd;
    std::vector<std::string> args;
    std::string request = "launch";
    bool noDebug = false;
};

struct DapSourceBreakpoint
{
    int line = 1;
    std::optional<int> column;
    std::optional<std::string> condition;
};

/// One entry per requested breakpoint in setBreakpoints response order (DAP body.breakpoints).
struct DapBreakpointSetResult
{
    int id = -1;
    int line = 0;
    bool verified = true;
    std::string message;
};

struct DapStackFrame
{
    int id = 0;
    std::string name;
    std::string sourcePath;
    int line = 0;
    int column = 0;
};

class DapClientService
{
  public:
    DapClientService();
    ~DapClientService();

    DapClientService(const DapClientService&) = delete;
    DapClientService& operator=(const DapClientService&) = delete;

    bool startAdapter(const std::string& adapterExe, const std::vector<std::string>& args);
    void stopAdapter();
    bool isRunning() const;

    bool initialize(const std::string& clientName, const std::string& rootPath,
                    const std::string& adapterId = "cppdbg");
    bool launch(const DapLaunchConfig& cfg);
    bool attach(int processId);
    bool configurationDone();

    // DAP lifecycle: disconnect gracefully from the target
    // terminateDebuggee: whether to terminate the debuggee process on disconnect
    // restart: whether to restart instead of terminate (some adapters support this)
    bool disconnect(bool terminateDebuggee = false, bool restart = false);

    bool setBreakpoints(const std::string& sourcePath, const std::vector<DapSourceBreakpoint>& breakpoints,
                        std::vector<int>* outBreakpointIds = nullptr,
                        std::vector<DapBreakpointSetResult>* outBinding = nullptr);

    // Configure exception breakpoints (first/second chance, user-unhandled, etc.)
    // filters: array of exception filter IDs to enable (e.g., "cpp_throw", "cpp_catch", "user_unhandled")
    // Returns true if setExceptionBreakpoints succeeded
    bool setExceptionBreakpoints(const std::vector<std::string>& filters);

    bool continueExec(int threadId = 0);
    bool next(int threadId);
    bool stepIn(int threadId);
    bool stepOut(int threadId);
    bool pause(int threadId);

    std::vector<DapStackFrame> stackTrace(int threadId, int startFrame = 0, int levels = 20);
    nlohmann::json scopes(int frameId);
    nlohmann::json variables(int variablesReference);
    std::string evaluate(int frameId, const std::string& expression, const std::string& context = "watch",
                         uint32_t timeoutMs = 10000);

    // Read raw memory bytes from the target process.
    // Returns decoded bytes on success (base64 decoded), empty string on failure.
    // memoryReference is typically a hex address string ("0x...") or a named register.
    // offset is added to the reference before reading.
    std::string readMemory(const std::string& memoryReference, uint64_t offset, uint64_t count,
                           uint64_t* outBytesRead = nullptr);

    using EventCallback = std::function<void(const std::string& event, const nlohmann::json& body)>;
    void setEventCallback(EventCallback cb);

  private:
    void readerLoop();
    bool sendJson(const nlohmann::json& msg);
    bool recvJson(nlohmann::json& out, uint32_t timeoutMs);
    bool recvLine(std::string& line, uint32_t timeoutMs);
    bool recvExact(char* dst, size_t len, uint32_t timeoutMs);

    nlohmann::json request(const std::string& command, const nlohmann::json& arguments, uint32_t timeoutMs = 15000);

    bool waitForInitializedEvent(uint32_t timeoutMs);

    std::string quoteArg(const std::string& a) const;
    static std::string toUtf8(const std::wstring& ws);
    static std::wstring toWide(const std::string& s);

  private:
#ifdef _WIN32
    void* process_ = nullptr;
    void* thread_ = nullptr;
    void* stdinWrite_ = nullptr;
    void* stdoutRead_ = nullptr;
#endif

    mutable std::mutex ioMutex_;
    mutable std::mutex responseMutex_;
    std::condition_variable responseCv_;
    std::mutex initMutex_;
    std::condition_variable initCv_;
    bool initializedEventSeen_ = false;
    std::atomic<bool> running_{false};
    std::atomic<int> nextSeq_{1};
    std::thread readerThread_;
    std::unordered_map<int, nlohmann::json> pendingResponses_;

    EventCallback eventCallback_;
};

}  // namespace RawrXD::Debug
