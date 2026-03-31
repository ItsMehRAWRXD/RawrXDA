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

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <vector>

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

    if (!m_nativeEngine)
    {
        m_nativeEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
    }
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

    std::string toolResult;
    if (DispatchModelToolCalls(trimmed, toolResult))
    {
        AgentResponse response{};
        response.type = AgentResponseType::TOOL_CALL;
        response.content = toolResult;
        response.toolName = "subagent_tool";
        response.rawOutput = toolResult;
        return response;
    }

    if (StartsWith(trimmed, "dumpbin "))
    {
        const std::string arg = TrimCopy(trimmed.substr(8));
        const std::string out = RunDumpbin(arg, "/headers");
        return {AgentResponseType::TOOL_CALL, out, "dumpbin", arg, out};
    }

    if (StartsWith(trimmed, "compile "))
    {
        const std::string arg = TrimCopy(trimmed.substr(8));
        const std::string out = RunCompiler(arg);
        return {AgentResponseType::TOOL_CALL, out, "compile", arg, out};
    }

    if (StartsWith(trimmed, "codex "))
    {
        const std::string arg = TrimCopy(trimmed.substr(6));
        const std::string out = RunCodex(arg);
        return {AgentResponseType::TOOL_CALL, out, "codex", arg, out};
    }

    if (StartsWith(trimmed, "!"))
    {
        std::string shellOutput;
        const bool ok = HeadlessRunCommand(trimmed.substr(1), shellOutput);
        if (!ok && shellOutput.empty())
        {
            shellOutput = "Command failed with no output.";
        }
        return {ok ? AgentResponseType::ANSWER : AgentResponseType::AGENT_ERROR, shellOutput};
    }

    if (m_nativeAgent && m_nativeEngine && m_nativeEngine->IsModelLoaded())
    {
        const std::string response = m_nativeAgent->Execute(trimmed);
        return {AgentResponseType::ANSWER, response};
    }

    std::ostringstream oss;
    oss << "Headless agent ready. Prefix OS commands with '!'. Prompt received: " << trimmed;
    return {AgentResponseType::ANSWER, oss.str()};
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
    std::ostringstream oss;
    oss << "{"
        << "\"initialized\":" << (m_initialized ? "true" : "false") << ","
        << "\"agent_loop_running\":" << (m_agentLoopRunning ? "true" : "false") << ","
        << "\"model\":\"" << m_modelName << "\","
        << "\"framework\":\"" << m_frameworkPath << "\""
        << "}";
    return oss.str();
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
    if (!m_nativeEngine)
    {
        m_nativeEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
    }

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

    if (!m_nativeEngine)
    {
        m_nativeEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
    }
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

struct HeadlessWatchEntry
{
    uint64_t address = 0;
    uint64_t length = 0;
    std::string name;
};
std::vector<HeadlessWatchEntry> g_headlessWatches;
}  // namespace

extern "C" void RawrXD_Disasm_HandleReq(void* req)
{
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    g_headlessDisasm.clear();

    rawrxd::ipc::MsgDisasmChunk chunk{};
    if (req != nullptr)
    {
        chunk.address = *reinterpret_cast<const uint64_t*>(req);
    }
    std::strncpy(chunk.mnemonic, "headless_stub", sizeof(chunk.mnemonic) - 1);
    chunk.length = 1;
    chunk.raw_bytes[0] = 0x90;  // NOP
    g_headlessDisasm.push_back(chunk);
}

extern "C" void RawrXD_Symbol_HandleReq(void* req)
{
    std::lock_guard<std::mutex> lock(g_headlessIpcMutex);
    g_headlessSymbolAddress = (req != nullptr) ? *reinterpret_cast<const uint64_t*>(req) : 0;
}

extern "C" void RawrXD_Module_HandleReq(void* req)
{
    (void)req;
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
    if (outEntries)
    {
        *outEntries = nullptr;
    }
    return 0;
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
