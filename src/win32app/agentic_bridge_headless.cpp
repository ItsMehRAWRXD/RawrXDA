// ============================================================================
// agentic_bridge_headless.cpp — Headless AgenticBridge (headless build variant) for RawrEngine
// ============================================================================
// RawrEngine has no Win32IDE; Win32IDE_AgenticBridge requires Win32IDE*.
// This file provides minimal implementations so AgentLoop and other agentic
// code link without the full Win32 GUI stack.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "Win32IDE_AgenticBridge.h"
#include "ui/agentic_bridge_api.h"
#include "Win32IDE_Phase16_AgenticController.h"
#include "Win32IDE_Phase17_AgenticProfiler.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <vector>
#include <windows.h>
#include <tlhelp32.h>

// Forward-declared; never dereferenced in headless
struct Win32IDE;

namespace
{
bool HeadlessRunCommand(const std::string& command, std::string& output)
{
    output.clear();
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe)
    {
        return false;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        output.append(buffer);
    }
    const int exitCode = _pclose(pipe);
    return exitCode == 0;
}

bool HeadlessPathExists(const std::string& path)
{
    if (path.empty())
    {
        return false;
    }
    const DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES;
}

bool HeadlessIsFile(const std::string& path)
{
    if (!HeadlessPathExists(path))
    {
        return false;
    }
    const DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string TrimCopy(const std::string& value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
    {
        ++begin;
    }
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }
    return value.substr(begin, end - begin);
}

bool StartsWith(const std::string& value, const std::string& prefix)
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string JsonEscape(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char c : value)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20)
                {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                    out += buf;
                }
                else
                {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

std::string AppendProfilerSummary(std::string text, uint32_t maxTools = 3)
{
    const std::string summary = AgenticProfilerTopSummary(maxTools);
    if (summary.empty())
    {
        return text;
    }
    if (!text.empty() && text.back() != '\n')
    {
        text.push_back('\n');
    }
    text.append("[phase17] ");
    text.append(summary);
    return text;
}
}  // namespace

AgenticBridge::AgenticBridge(Win32IDE* ide)
    : m_ide(ide), m_initialized(false), m_agentLoopRunning(false), m_hProcess(nullptr), m_hStdoutRead(nullptr),
      m_hStdoutWrite(nullptr), m_hStdinRead(nullptr), m_hStdinWrite(nullptr)
{
    (void)ide;
}

AgenticBridge::~AgenticBridge() {}

bool AgenticBridge::Initialize(const std::string& frameworkPath, const std::string& modelName)
{
    m_frameworkPath = frameworkPath;
    if (!modelName.empty())
    {
        m_modelName = modelName;
    }

    m_nativeEngine = RawrXD::CPUInferenceEngine::GetSharedInstance();
    if (!m_nativeAgent)
    {
        m_nativeAgent = std::make_unique<RawrXD::NativeAgent>(m_nativeEngine.get());
    }

    m_nativeAgent->SetDeepThink(m_deepThinking);
    m_nativeAgent->SetDeepResearch(m_deepResearch);
    m_nativeAgent->SetNoRefusal(m_noRefusal);
    m_nativeAgent->SetAutoCorrect(m_autoCorrect);
    m_nativeAgent->SetMaxMode(m_maxMode);
    m_nativeAgent->SetLanguageContext(m_languageContext);
    m_nativeAgent->SetFileContext(m_fileContext);
    if (m_outputCallback)
    {
        m_nativeAgent->SetOutputCallback(
            [this](const std::string& token)
            {
                if (m_outputCallback)
                {
                    m_outputCallback("agent", token);
                }
            });
    }

    m_initialized = true;
    if (!modelName.empty() && HeadlessIsFile(modelName))
    {
        return LoadModel(modelName);
    }
    return true;
}

AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt)
{
    if (!m_initialized && !Initialize(m_frameworkPath, m_modelName))
    {
        return {AgentResponseType::AGENT_ERROR, "Failed to initialize headless bridge"};
    }

    const std::string trimmed = TrimCopy(prompt);
    if (trimmed.empty())
    {
        return {AgentResponseType::AGENT_ERROR, "Empty prompt"};
    }

    // One-shot execution path: start a fresh profiler epoch per command.
    if (!m_agentLoopRunning)
    {
        AgenticProfilerBeginEpoch();
    }

    // --- Phase 16: Subagent tool dispatch ----------------------------------
    std::string toolResult;
    bool subagentDispatched = false;
    {
        Phase17ProfileGuard _p17("subagent_tool");  // Phase 17: profile subagent dispatch
        subagentDispatched = DispatchModelToolCalls(trimmed, toolResult);
    }
    if (subagentDispatched)
    {
        // Post-hoc notification: records tool execution in the Phase 16
        // state machine for latency tracking and aperture telemetry.
        AgenticNotifyToolStart("subagent_tool");
        AgenticNotifyToolEnd(true, 0);
        const std::string surfaced = AppendProfilerSummary(toolResult, 3);
        // CRITICAL: Stream tool result through callback for UI display
        if (m_outputCallback && !surfaced.empty())
        {
            m_outputCallback("stream", surfaced);
        }
        AgentResponse response{};
        response.type      = AgentResponseType::TOOL_CALL;
        response.content   = surfaced;
        response.toolName  = "subagent_tool";
        response.rawOutput = surfaced;
        return response;
    }

    // --- Phase 16: Named tool dispatch (gated + timed) -------------------
    if (StartsWith(trimmed, "dumpbin "))
    {
        const std::string arg = TrimCopy(trimmed.substr(8));
        if (!AgenticNotifyToolStart("dumpbin"))
        {
            return {AgentResponseType::AGENT_ERROR,
                    "[Phase16] dumpbin deferred: aperture saturated"};
        }
        Phase17ProfileGuard _p17("dumpbin");  // Phase 17: profile dumpbin dispatch
        const ULONGLONG ts  = GetTickCount64();
        const std::string out = RunDumpbin(arg, "/headers");
        AgenticNotifyToolEnd(!out.empty(),
                             static_cast<uint32_t>(GetTickCount64() - ts));
        const std::string surfaced = AppendProfilerSummary(out, 3);
        return {AgentResponseType::TOOL_CALL, surfaced, "dumpbin", arg, surfaced};
    }

    if (StartsWith(trimmed, "compile "))
    {
        const std::string arg = TrimCopy(trimmed.substr(8));
        if (!AgenticNotifyToolStart("compile"))
        {
            return {AgentResponseType::AGENT_ERROR,
                    "[Phase16] compile deferred: aperture saturated"};
        }
        Phase17ProfileGuard _p17("compile");  // Phase 17: profile compile dispatch
        const ULONGLONG ts  = GetTickCount64();
        const std::string out = RunCompiler(arg);
        AgenticNotifyToolEnd(!out.empty(),
                             static_cast<uint32_t>(GetTickCount64() - ts));
        const std::string surfaced = AppendProfilerSummary(out, 3);
        return {AgentResponseType::TOOL_CALL, surfaced, "compile", arg, surfaced};
    }

    if (StartsWith(trimmed, "codex "))
    {
        const std::string arg = TrimCopy(trimmed.substr(6));
        if (!AgenticNotifyToolStart("codex"))
        {
            return {AgentResponseType::AGENT_ERROR,
                    "[Phase16] codex deferred: aperture saturated"};
        }
        Phase17ProfileGuard _p17("codex");  // Phase 17: profile codex dispatch
        const ULONGLONG ts  = GetTickCount64();
        const std::string out = RunCodex(arg);
        AgenticNotifyToolEnd(!out.empty(),
                             static_cast<uint32_t>(GetTickCount64() - ts));
        const std::string surfaced = AppendProfilerSummary(out, 3);
        return {AgentResponseType::TOOL_CALL, surfaced, "codex", arg, surfaced};
    }

    if (StartsWith(trimmed, "!"))
    {
        if (!AgenticNotifyToolStart("shell"))
        {
            return {AgentResponseType::AGENT_ERROR,
                    "[Phase16] shell deferred: aperture saturated"};
        }
        Phase17ProfileGuard _p17("shell");  // Phase 17: profile shell dispatch
        const ULONGLONG ts = GetTickCount64();
        std::string shellOutput;
        const bool ok = HeadlessRunCommand(trimmed.substr(1), shellOutput);
        if (!ok && shellOutput.empty())
        {
            shellOutput = "Command failed with no output.";
        }
        AgenticNotifyToolEnd(ok, static_cast<uint32_t>(GetTickCount64() - ts));
        shellOutput = AppendProfilerSummary(shellOutput, 3);
        return {ok ? AgentResponseType::ANSWER : AgentResponseType::AGENT_ERROR,
                shellOutput};
    }

    if (m_nativeAgent && m_nativeEngine && m_nativeEngine->IsModelLoaded())
    {
        const std::string response = m_nativeAgent->Execute(trimmed);
        // CRITICAL: Stream response through callback so UI actually displays real inference output
        if (m_outputCallback && !response.empty())
        {
            m_outputCallback("stream", response);
        }
        return {AgentResponseType::ANSWER, response};
    }

    std::ostringstream oss;
    oss << "Headless agent ready. Prefix OS commands with '!'. Prompt received: " << trimmed;
    const std::string fallback = oss.str();
    // Stream fallback response through callback if available
    if (m_outputCallback && !fallback.empty())
    {
        m_outputCallback("stream", fallback);
    }
    return {AgentResponseType::ANSWER, fallback};
}

bool AgenticBridge::StartAgentLoop(const std::string& initialPrompt, int maxIterations)
{
    if (maxIterations <= 0)
    {
        return false;
    }
    if (!m_initialized && !Initialize(m_frameworkPath, m_modelName))
    {
        return false;
    }

    // Loop execution path: maintain one profiler epoch per loop session.
    AgenticProfilerBeginEpoch();
    m_agentLoopRunning = true;
    std::string nextPrompt = initialPrompt;

    for (int i = 0; i < maxIterations && m_agentLoopRunning; ++i)
    {
        AgentResponse response = ExecuteAgentCommand(nextPrompt);
        if (m_outputCallback)
        {
            m_outputCallback("loop", response.content);
        }

        if (response.type == AgentResponseType::AGENT_ERROR || response.type == AgentResponseType::ANSWER)
        {
            break;
        }
        nextPrompt = response.content;
    }

    m_agentLoopRunning = false;
    return true;
}

void AgenticBridge::StopAgentLoop()
{
    m_agentLoopRunning = false;
}

std::vector<std::string> AgenticBridge::GetAvailableTools()
{
    return {"dumpbin", "compile", "codex", "shell(!command)", "runSubagent", "chain", "hexmag_swarm"};
}

std::string AgenticBridge::GetAgentStatus()
{
    const std::string phase17TopTools = JsonEscape(AgenticProfilerTopSummary(3));
    const uint32_t phase17Epochs = Phase17Profiler::GetEpochCount();
    const uint64_t phase17ElapsedCycles = AgenticProfilerGetElapsed();
    const std::string modelEscaped = JsonEscape(m_modelName);
    const std::string frameworkEscaped = JsonEscape(m_frameworkPath);
    std::ostringstream oss;
    oss << "{"
        << "\"initialized\":" << (m_initialized ? "true" : "false") << ","
        << "\"agent_loop_running\":" << (m_agentLoopRunning ? "true" : "false") << ","
        << "\"model\":\"" << modelEscaped << "\","
        << "\"framework\":\"" << frameworkEscaped << "\","
        << "\"phase17_epochs\":" << phase17Epochs << ","
        << "\"phase17_elapsed_cycles\":" << phase17ElapsedCycles << ","
        << "\"phase17_top_tools\":\"" << phase17TopTools << "\","
        << "\"ghost_last_seq\":" << GetLastGhostSeq() << ","
        << "\"ghost_backtracks\":" << GetGhostSeqBacktracks() << ","
        << "\"ghost_gap_events\":" << GetGhostSeqGapEvents()
        << "}";
    return oss.str();
}

void AgenticBridge::ObserveGhostStreamSeq(uint64_t seq)
{
    if (seq == 0)
    {
        return;
    }

    const uint64_t prev = m_lastGhostSeq.load(std::memory_order_relaxed);
    if (prev != 0)
    {
        if (seq <= prev)
        {
            m_ghostSeqBacktracks.fetch_add(1, std::memory_order_relaxed);
        }
        else if (seq > prev + 1)
        {
            m_ghostSeqGapEvents.fetch_add(1, std::memory_order_relaxed);
        }
    }

    uint64_t observed = prev;
    while (seq > observed &&
           !m_lastGhostSeq.compare_exchange_weak(observed, seq, std::memory_order_relaxed, std::memory_order_relaxed))
    {
    }
}

uint64_t AgenticBridge::GetLastGhostSeq() const
{
    return m_lastGhostSeq.load(std::memory_order_relaxed);
}

uint64_t AgenticBridge::GetGhostSeqBacktracks() const
{
    return m_ghostSeqBacktracks.load(std::memory_order_relaxed);
}

uint64_t AgenticBridge::GetGhostSeqGapEvents() const
{
    return m_ghostSeqGapEvents.load(std::memory_order_relaxed);
}

void AgenticBridge::ResetGhostSeqTelemetry()
{
    m_lastGhostSeq.store(0, std::memory_order_relaxed);
    m_ghostSeqBacktracks.store(0, std::memory_order_relaxed);
    m_ghostSeqGapEvents.store(0, std::memory_order_relaxed);
}

void AgenticBridge::SetModel(const std::string& modelName)
{
    m_modelName = modelName;
}

void AgenticBridge::SetOllamaServer(const std::string& serverUrl)
{
    m_ollamaServer = serverUrl;
}

void AgenticBridge::SetMaxMode(bool enabled)
{
    m_maxMode = enabled;
    if (m_nativeAgent)
    {
        m_nativeAgent->SetMaxMode(enabled);
    }
}

void AgenticBridge::SetDeepThinking(bool enabled)
{
    m_deepThinking = enabled;
    if (m_nativeAgent)
    {
        m_nativeAgent->SetDeepThink(enabled);
    }
}

void AgenticBridge::SetDeepResearch(bool enabled)
{
    m_deepResearch = enabled;
    if (m_nativeAgent)
    {
        m_nativeAgent->SetDeepResearch(enabled);
    }
}

void AgenticBridge::SetNoRefusal(bool enabled)
{
    m_noRefusal = enabled;
    if (m_nativeAgent)
    {
        m_nativeAgent->SetNoRefusal(enabled);
    }
}

void AgenticBridge::SetAutoCorrect(bool enabled)
{
    m_autoCorrect = enabled;
    if (m_nativeAgent)
    {
        m_nativeAgent->SetAutoCorrect(enabled);
    }
}

void AgenticBridge::SetContextSize(const std::string& sizeName)
{
    m_nativeEngine = RawrXD::CPUInferenceEngine::GetSharedInstance();

    size_t limit = 2048;
    if (sizeName == "small")
    {
        limit = 1024;
    }
    else if (sizeName == "medium")
    {
        limit = 2048;
    }
    else if (sizeName == "large")
    {
        limit = 4096;
    }
    else if (sizeName == "xlarge")
    {
        limit = 8192;
    }
    else
    {
        const unsigned long long parsed = std::strtoull(sizeName.c_str(), nullptr, 10);
        if (parsed > 0)
        {
            limit = static_cast<size_t>(parsed);
        }
    }
    m_nativeEngine->SetContextLimit(limit);
}

bool AgenticBridge::LoadModel(const std::string& path)
{
    m_lastModelLoadError.clear();
    if (path.empty())
    {
        m_lastModelLoadError = "empty model path";
        if (m_modelLoadErrorCallback)
        {
            m_modelLoadErrorCallback(m_lastModelLoadError);
        }
        return false;
    }

    m_nativeEngine = RawrXD::CPUInferenceEngine::GetSharedInstance();
    if (!m_nativeAgent)
    {
        m_nativeAgent = std::make_unique<RawrXD::NativeAgent>(m_nativeEngine.get());
    }

    bool loaded = true;
    if (HeadlessIsFile(path))
    {
        loaded = m_nativeEngine->LoadModel(path);
        if (!loaded)
        {
            m_lastModelLoadError = m_nativeEngine->GetLastLoadErrorMessage();
            if (m_lastModelLoadError.empty())
            {
                m_lastModelLoadError = "native engine load failed without detailed error";
            }
            if (m_modelLoadErrorCallback)
            {
                m_modelLoadErrorCallback(m_lastModelLoadError);
            }
            if (m_errorCallback)
            {
                m_errorCallback("Model load failed: " + m_lastModelLoadError);
            }
        }
    }
    else
    {
        m_lastModelLoadError = "path not found or not a file: " + path;
        loaded = false;
        if (m_modelLoadErrorCallback)
        {
            m_modelLoadErrorCallback(m_lastModelLoadError);
        }
    }

    if (loaded)
    {
        m_modelName = path;
        m_initialized = true;
    }
    return loaded;
}

void AgenticBridge::SetWorkspaceRoot(const std::string& workspaceRoot)
{
    m_workspaceRoot = workspaceRoot;
}

void AgenticBridge::SetLanguageContext(const std::string& language, const std::string& filePath)
{
    m_languageContext = language;
    m_fileContext = filePath;

    if (m_nativeAgent)
    {
        m_nativeAgent->SetLanguageContext(language);
        m_nativeAgent->SetFileContext(filePath);
    }
}

void AgenticBridge::SetOutputCallback(OutputCallback callback)
{
    m_outputCallback = std::move(callback);
    if (m_nativeAgent && m_outputCallback)
    {
        m_nativeAgent->SetOutputCallback(
            [this](const std::string& token)
            {
                if (m_outputCallback)
                {
                    m_outputCallback("agent", token);
                }
            });
    }
}

std::string AgenticBridge::RunDumpbin(const std::string& path, const std::string& mode)
{
    if (!HeadlessIsFile(path))
    {
        return "dumpbin: file not found: " + path;
    }

    const std::string modeArg = mode.empty() ? "/headers" : mode;
    std::string output;
    std::string command = "dumpbin " + modeArg + " \"" + path + "\" 2>&1";
    if (!HeadlessRunCommand(command, output))
    {
        command = "llvm-objdump -x \"" + path + "\" 2>&1";
        HeadlessRunCommand(command, output);
    }
    if (output.empty())
    {
        output = "dumpbin produced no output.";
    }
    return output;
}

std::string AgenticBridge::RunCodex(const std::string& path)
{
    if (!HeadlessPathExists(path))
    {
        return "codex: path not found: " + path;
    }

    std::string output;
    if (HeadlessIsFile(path))
    {
        const std::string command = "type \"" + path + "\" 2>&1";
        HeadlessRunCommand(command, output);
    }
    else
    {
        const std::string command = "dir /b \"" + path + "\" 2>&1";
        HeadlessRunCommand(command, output);
    }
    if (output.empty())
    {
        output = "codex: no output";
    }
    return output;
}

std::string AgenticBridge::RunCompiler(const std::string& path)
{
    if (!HeadlessIsFile(path))
    {
        return "compiler: file not found: " + path;
    }

    std::string output;
    std::string command = "cl /nologo /c \"" + path + "\" 2>&1";
    if (!HeadlessRunCommand(command, output))
    {
        command = "g++ -c \"" + path + "\" -o NUL 2>&1";
        HeadlessRunCommand(command, output);
    }
    if (output.empty())
    {
        output = "compiler: no output";
    }
    return output;
}

SubAgentManager* AgenticBridge::GetSubAgentManager()
{
    if (!m_subAgentManager)
    {
        m_subAgentManager = std::make_unique<SubAgentManager>(nullptr);
    }
    return m_subAgentManager.get();
}

std::string AgenticBridge::RunSubAgent(const std::string& description, const std::string& prompt)
{
    SubAgentManager* manager = GetSubAgentManager();
    if (!manager)
    {
        return "subagent manager unavailable";
    }

    const std::string subAgentId = manager->spawnSubAgent("headless", description, prompt);
    if (subAgentId.empty())
    {
        return "failed to spawn subagent";
    }

    manager->waitForSubAgent(subAgentId, 30000);
    return manager->getSubAgentResult(subAgentId);
}

std::string AgenticBridge::ExecuteChain(const std::vector<std::string>& steps, const std::string& initialInput)
{
    SubAgentManager* manager = GetSubAgentManager();
    if (!manager)
    {
        return "subagent manager unavailable";
    }
    return manager->executeChain("headless", steps, initialInput);
}

std::string AgenticBridge::ExecuteSwarm(const std::vector<std::string>& prompts, const std::string& mergeStrategy,
                                        int maxParallel)
{
    SubAgentManager* manager = GetSubAgentManager();
    if (!manager)
    {
        return "subagent manager unavailable";
    }

    SwarmConfig config{};
    config.maxParallel = std::max(1, maxParallel);
    if (!mergeStrategy.empty())
    {
        config.mergeStrategy = mergeStrategy;
    }
    return manager->executeSwarm("headless", prompts, config);
}

void AgenticBridge::CancelAllSubAgents()
{
    if (m_subAgentManager)
    {
        m_subAgentManager->cancelAll();
    }
}

std::string AgenticBridge::GetSubAgentStatus() const
{
    if (!m_subAgentManager)
    {
        return "no-subagents";
    }
    return m_subAgentManager->getStatusSummary();
}

bool AgenticBridge::DispatchModelToolCalls(const std::string& modelOutput, std::string& toolResult)
{
    if (modelOutput.empty())
    {
        return false;
    }
    SubAgentManager* manager = GetSubAgentManager();
    if (!manager)
    {
        return false;
    }
    return manager->dispatchToolCall("headless", modelOutput, toolResult);
}

namespace
{
std::mutex g_headlessIpcMutex;
std::vector<rawrxd::ipc::MsgDisasmChunk> g_headlessDisasm;
uint64_t g_headlessSymbolAddress = 0;
std::vector<std::pair<std::string, uint64_t>> g_headlessSymbolCache;
std::vector<std::pair<std::string, std::pair<uint64_t, uint64_t>>> g_headlessModules;

struct HeadlessWatchEntry
{
    uint64_t address = 0;
    uint64_t length = 0;
    std::string name;
};
std::vector<HeadlessWatchEntry> g_headlessWatches;

// Simple x86-64 instruction disassembler for headless mode (when capstone unavailable)
struct SimpleInstruction {
    uint64_t address;
    uint8_t bytes[15];
    uint8_t length;
    std::string mnemonic;
};

// Decode basic x86-64 instructions without external dependency
[[nodiscard]] bool DisassembleX64Simple(const uint8_t* code, size_t size, uint64_t address, SimpleInstruction& out)
{
    if (!code || size == 0) return false;
    
    uint8_t byte1 = code[0];
    out.address = address;
    out.length = 1;
    out.bytes[0] = byte1;
    
    // Basic single-byte instructions
    if (byte1 == 0x90) {
        out.mnemonic = "nop";
        return true;
    }
    if (byte1 == 0xc3) {
        out.mnemonic = "ret";
        return true;
    }
    if (byte1 == 0xcc) {
        out.mnemonic = "int 3";
        return true;
    }
    if (byte1 == 0x55) {
        out.mnemonic = "push rbp";
        return true;
    }
    
    // MOV rax, ...
    if ((byte1 & 0xf0) == 0xb0 || byte1 == 0x48) {
        if (size >= 2 && byte1 == 0x48 && code[1] == 0xc7) {
            if (size >= 7 && code[2] == 0xc0) {
                out.length = 7;
                for (int i = 0; i < 7; ++i) out.bytes[i] = code[i];
                out.mnemonic = "mov rax, imm32";
                return true;
            }
        }
    }
    
    // Conditional jumps
    if (byte1 == 0x74) {
        out.length = 2;
        out.bytes[1] = size > 1 ? code[1] : 0;
        out.mnemonic = "jz rel8";
        return true;
    }
    if (byte1 == 0x75) {
        out.length = 2;
        out.bytes[1] = size > 1 ? code[1] : 0;
        out.mnemonic = "jnz rel8";
        return true;
    }
    
    // JMP
    if (byte1 == 0xe9) {
        out.length = 5;
        for (int i = 0; i < 5 && i < size; ++i) out.bytes[i] = code[i];
        out.mnemonic = "jmp rel32";
        return true;
    }
    
    // Default: unknown instruction
    out.length = 1;
    out.mnemonic = "unknown";
    return false;
}

[[nodiscard]] bool IsReadableAddressRange(uint64_t address, size_t size)
{
    if (address == 0 || size == 0)
    {
        return false;
    }

    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0)
    {
        return false;
    }

    if (mbi.State != MEM_COMMIT)
    {
        return false;
    }

    const DWORD prot = mbi.Protect & 0xFF;
    const bool readable = (prot == PAGE_READONLY || prot == PAGE_READWRITE || prot == PAGE_WRITECOPY ||
                           prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE ||
                           prot == PAGE_EXECUTE_WRITECOPY);
    if (!readable)
    {
        return false;
    }

    const auto regionStart = reinterpret_cast<uint64_t>(mbi.BaseAddress);
    const auto regionEnd = regionStart + mbi.RegionSize;
    return address >= regionStart && (address + size) <= regionEnd;
}

void RefreshModuleCacheUnlocked()
{
    g_headlessModules.clear();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return;
    }

    MODULEENTRY32W mod{};
    mod.dwSize = sizeof(mod);
    if (Module32FirstW(snapshot, &mod))
    {
        do
        {
            std::wstring ws(mod.szModule);
            std::string name(ws.begin(), ws.end());
            const uint64_t base = reinterpret_cast<uint64_t>(mod.modBaseAddr);
            const uint64_t size = static_cast<uint64_t>(mod.modBaseSize);
            g_headlessModules.push_back({name, {base, size}});
        } while (Module32NextW(snapshot, &mod));
    }
    CloseHandle(snapshot);
}

void BuildSymbolCacheUnlocked()
{
    g_headlessSymbolCache.clear();
    for (const auto& module : g_headlessModules)
    {
        g_headlessSymbolCache.push_back({module.first + "!base", module.second.first});
    }
}
}  // namespace

extern "C" void RawrXD_Disasm_HandleReq(void* req)
{
    if (!req) return;
    
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    g_headlessDisasm.clear();
    
    const uint64_t address = *reinterpret_cast<const uint64_t*>(req);
    
    uint8_t buffer[32] = {0};

    if (IsReadableAddressRange(address, sizeof(buffer)))
    {
        std::memcpy(buffer, reinterpret_cast<const void*>(address), sizeof(buffer));
    }
    else
    {
        // Graceful fallback for invalid or inaccessible addresses.
        buffer[0] = 0xCC;
    }
    
    // Disassemble the buffer
    SimpleInstruction decoded[3];
    for (int i = 0; i < 3; ++i) {
        uint64_t currentAddr = address + i;
        if (DisassembleX64Simple(buffer + i, sizeof(buffer) - i, currentAddr, decoded[i])) {
            rawrxd::ipc::MsgDisasmChunk chunk{};
            chunk.address = decoded[i].address;
            chunk.length = decoded[i].length;
            std::memcpy(chunk.raw_bytes, decoded[i].bytes, std::min<size_t>(decoded[i].length, 15));
            std::strncpy(chunk.mnemonic, decoded[i].mnemonic.c_str(), sizeof(chunk.mnemonic) - 1);
            g_headlessDisasm.push_back(chunk);
            i += (decoded[i].length - 1);  // Skip to next instruction
        }
    }
}

extern "C" void RawrXD_Symbol_HandleReq(void* req)
{
    if (!req) return;
    
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);

    const uint64_t address = *reinterpret_cast<const uint64_t*>(req);
    RefreshModuleCacheUnlocked();
    BuildSymbolCacheUnlocked();
    
    // Find closest symbol to address
    g_headlessSymbolAddress = 0;
    uint64_t closestDistance = UINT64_MAX;
    
    for (const auto& [symbol, addr] : g_headlessSymbolCache) {
        if (addr <= address) {
            uint64_t distance = address - addr;
            if (distance < closestDistance) {
                closestDistance = distance;
                g_headlessSymbolAddress = addr;
            }
        }
    }
}

extern "C" void RawrXD_Module_HandleReq(void* req)
{
    (void)req;
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    RefreshModuleCacheUnlocked();
}

extern "C" const rawrxd::ipc::MsgDisasmChunk* RawrXD_Disasm_GetLastResult(uint32_t* outCount)
{
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    if (outCount)
    {
        *outCount = static_cast<uint32_t>(g_headlessDisasm.size());
    }
    return g_headlessDisasm.empty() ? nullptr : g_headlessDisasm.data();
}

extern "C" uint64_t RawrXD_Symbol_GetLastResult(void)
{
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    return g_headlessSymbolAddress;
}

extern "C" uint32_t RawrXD_Module_GetLastResult(const void** outEntries)
{
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    RefreshModuleCacheUnlocked();
    
    if (outEntries)
    {
        // Return module list as string array (simplified for headless)
        static std::string moduleList;
        moduleList.clear();
        for (const auto& [name, range] : g_headlessModules) {
            if (!moduleList.empty()) moduleList += "\n";
            std::ostringstream oss;
            oss << name << " @ 0x" << std::hex << range.first << " (0x" << range.second << " bytes)";
            moduleList += oss.str();
        }
        *outEntries = moduleList.c_str();
    }
    
    return static_cast<uint32_t>(g_headlessModules.size());
}

extern "C" void RawrXD_Debugger_AddWatch(uint64_t addr, uint64_t len, const char* name)
{
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    HeadlessWatchEntry entry;
    entry.address = addr;
    entry.length = len;
    if (name)
    {
        entry.name = name;
    }
    g_headlessWatches.push_back(std::move(entry));
}

extern "C" void RawrXD_Debugger_RemoveWatch(uint64_t addr)
{
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    g_headlessWatches.erase(std::remove_if(g_headlessWatches.begin(), g_headlessWatches.end(),
                                           [addr](const HeadlessWatchEntry& entry) { return entry.address == addr; }),
                            g_headlessWatches.end());
}

extern "C" void RawrXD_Debugger_RemoveWatchString(const char* name)
{
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    if (!name)
    {
        return;
    }
    const std::string needle(name);
    g_headlessWatches.erase(std::remove_if(g_headlessWatches.begin(), g_headlessWatches.end(),
                                           [&needle](const HeadlessWatchEntry& entry) { return entry.name == needle; }),
                            g_headlessWatches.end());
}

extern "C" uint32_t RawrXD_Debugger_GetWatchCount(void)
{
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    return static_cast<uint32_t>(g_headlessWatches.size());
}

extern "C" int RawrXD_Debugger_GetWatchAt(uint32_t index, uint64_t* outAddr, uint64_t* outLen, char* outName,
                                          uint32_t nameBufSize)
{
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    if (index >= g_headlessWatches.size())
    {
        return 0;
    }

    const auto& entry = g_headlessWatches[index];
    if (outAddr)
    {
        *outAddr = entry.address;
    }
    if (outLen)
    {
        *outLen = entry.length;
    }
    if (outName && nameBufSize > 0)
    {
        const size_t copyLen = std::min<size_t>(entry.name.size(), nameBufSize - 1);
        std::memcpy(outName, entry.name.data(), copyLen);
        outName[copyLen] = '\0';
    }
    return 1;
}
