// Agentic Framework Bridge Implementation
// Connects Win32IDE to Native Agentic Framework

#include "Win32IDE_AgenticBridge.h"
#include "IDELogger.h"
#include "Win32IDE.h"
#include "../advanced_agent_features.hpp"
#include "../agentic_engine.h" 
#include "../cpu_inference_engine.h"
#include "../plugins/MemoryPlugin.hpp" // For Context Slider
#include "../vsix_native_converter.hpp"
#include <sstream>
#include <algorithm>
#include <memory>

// Global shared instances to persist across UI reloads if needed
static std::shared_ptr<RawrXD::CPUInference::CPUInferenceEngine> g_cpuEngine = nullptr;
static std::shared_ptr<AgenticEngine> g_agentEngine = nullptr;

AgenticBridge::AgenticBridge(Win32IDE* ide)
    : m_ide(ide), m_initialized(false), m_agentLoopRunning(false)
{
}

AgenticBridge::~AgenticBridge() {
}

bool AgenticBridge::Initialize(const std::string& frameworkPath, const std::string& modelName) {
    if (m_initialized) return true;

    LOG_INFO("Initializing Native Inference Stack...");

    if (!g_cpuEngine) {
        g_cpuEngine = std::make_shared<RawrXD::CPUInference::CPUInferenceEngine>();
        // Default context
        g_cpuEngine->SetContextLimit(4096);
    }

    if (!g_agentEngine) {
        g_agentEngine = std::make_shared<AgenticEngine>();
        g_agentEngine->setInferenceEngine(g_cpuEngine.get());
    }

    m_initialized = true;
    return true;
}

AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt) {
    if (!g_agentEngine) return {AgentResponseType::AGENT_ERROR, "Engine Not Initialized"};

    // Update Config
    AgenticEngine::GenerationConfig cfg;
    cfg.maxMode = m_maxMode;
    cfg.deepThinking = m_deepThinking;
    cfg.deepResearch = m_deepResearch;
    cfg.noRefusal = m_noRefusal;
    
    // Explicit /context override if slider set
    // m_contextSizeString handling?
    // We assume engine handles context resizing via SetContextSize call, 
    // config just passes flags.
    
    g_agentEngine->updateConfig(cfg);

    // Commands handling
    if (prompt.find("/react-server") == 0) {
        std::string name = (prompt.length() > 14) ? prompt.substr(14) : "react-app";
        // Call agent plan
        std::string result = g_agentEngine->planTask("Create React Server named " + name); 
        return {AgentResponseType::ANSWER, result};
    }
    
    if (prompt.find("/install_vsix ") == 0) {
        std::string path = prompt.substr(14);
        bool res = RawrXD::VsixNativeConverter::ConvertVsixToNative(path, "extensions/");
        return {AgentResponseType::ANSWER, res ? "VSIX Installed" : "VSIX Installation Failed"};
    }

    std::string response = g_agentEngine->chat(prompt);
    
    AgentResponse r;
    r.content = response;
    r.type = AgentResponseType::ANSWER;
    return r;
}

void AgenticBridge::SetMaxMode(bool enabled) { 
    m_maxMode = enabled; 
    // If Max Mode enabled, ensure context is at least 32k
    if (enabled && g_cpuEngine && g_cpuEngine->GetContextLimit() < 32768) {
        g_cpuEngine->SetContextLimit(32768);
    }
}
void AgenticBridge::SetDeepThinking(bool enabled) { m_deepThinking = enabled; }
void AgenticBridge::SetDeepResearch(bool enabled) { m_deepResearch = enabled; }
void AgenticBridge::SetNoRefusal(bool enabled) { m_noRefusal = enabled; }
void AgenticBridge::SetAutoCorrect(bool enabled) { m_autoCorrect = enabled; }

void AgenticBridge::SetContextSize(const std::string& sizeName) {
    if (!g_cpuEngine) return;
    
    size_t limit = 4096;
    std::string s = sizeName;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    
    if (s == "4k") limit = 4096;
    else if (s == "32k") limit = 32768;
    else if (s == "64k") limit = 65536;
    else if (s == "128k") limit = 131072;
    else if (s == "256k") limit = 262144;
    else if (s == "512k") limit = 524288;
    else if (s == "1m") limit = 1048576;
    
    g_cpuEngine->SetContextLimit(limit);
    
    if (m_ide) {
        // Log to output
        std::stringstream ss;
        ss << "Context Window resized to: " << sizeName << " (" << limit << " tokens)";
        // m_ide->appendToOutput(ss.str(), "Agent", OutputSeverity::Info); // Need to expose
    }
}

// Stubs for interface compliance
bool AgenticBridge::StartAgentLoop(const std::string& prompt, int max) {
    ExecuteAgentCommand(prompt);
    return true;
}
void AgenticBridge::StopAgentLoop() {}
std::vector<std::string> AgenticBridge::GetAvailableTools() { return {}; }
std::string AgenticBridge::GetAgentStatus() { 
    return g_cpuEngine && g_cpuEngine->IsModelLoaded() ? "Ready" : "No Model"; 
}
void AgenticBridge::SetModel(const std::string& name) {
    if (g_cpuEngine) g_cpuEngine->LoadModel(name);
}
void AgenticBridge::SetOllamaServer(const std::string&) {}
void AgenticBridge::SetOutputCallback(OutputCallback cb) { m_outputCallback = cb; }

// Internal helpers
AgentResponse AgenticBridge::ParseAgentResponse(const std::string& r) { return {AgentResponseType::ANSWER, r}; }
bool AgenticBridge::IsToolCall(const std::string&) { return false; }
bool AgenticBridge::IsAnswer(const std::string&) { return true; }
std::string AgenticBridge::ResolveFrameworkPath() { return ""; }
std::string AgenticBridge::ResolveToolsModulePath() { return ""; }
             std::string path = prompt.substr(9);
             m_nativeAgent->Suggest(path);
             return response;
        }
        else if (prompt.find("/plan ") == 0) {
             std::string task = prompt.substr(6);
             m_nativeAgent->Plan(task);
             return response;
        }
        else if (prompt.find("/patch ") == 0) {
             std::string path = prompt.substr(7);
             m_nativeAgent->HotPatch(path);
             return response;
        }
        else if (prompt.find("/edit ") == 0) {
             // simplified parsing: /edit file instructions...
             std::string rest = prompt.substr(6);
             size_t space = rest.find(' ');
             if (space != std::string::npos) {
                 std::string file = rest.substr(0, space);
                 std::string inst = rest.substr(space + 1);
                 m_nativeAgent->Edit(file, inst);
             }
             return response; 
        }
        else if (prompt == "/max") {
             m_nativeAgent->SetMaxMode(true);
             m_outputCallback("", "Max Mode Enabled (Native Threads)");
             return response;
        }
        else if (prompt == "/think") {
             static bool t = false; t = !t;
             m_nativeAgent->SetDeepThink(t);
             m_outputCallback("", std::string("Deep Thinking ") + (t ? "Enabled" : "Disabled"));
             return response;
        }
        else if (prompt == "/research") {
             static bool r = false; r = !r;
             m_nativeAgent->SetDeepResearch(r);
             m_outputCallback("", std::string("Deep Research ") + (r ? "Enabled" : "Disabled"));
             return response;
        }
        else if (prompt == "/norefusal") {
             static bool n = false; n = !n;
             m_nativeAgent->SetNoRefusal(n);
             m_outputCallback("", std::string("No Refusal Mode ") + (n ? "Enabled" : "Disabled"));
             return response;
        }
        else if (prompt == "/react-server") {
             m_outputCallback("", "Generating React Server Architecture...");
             if (RawrXD::ReactServerGenerator::Generate("react-server", "rawrxd-app")) {
                 m_outputCallback("", "React Server generated in ./react-server");
                 response.content = "React Server Generated.";
             } else {
                 m_outputCallback("", "Failed to generate React Server.");
                 response.type = AgentResponseType::AGENT_ERROR;
             }
             return response;
        }
        
        // Accumulate output into response.content for return, though callback handles streaming
        std::string fullAccumulator;
        m_nativeAgent->SetOutputCallback([&](const std::string& s) {
             fullAccumulator += s;
             if (m_outputCallback) m_outputCallback("", s);
        });

        // Trigger generation
        m_nativeAgent->Ask(prompt);
        
        response.content = fullAccumulator;
        return response;
    }

    // Fallback to legacy PowerShell if native not ready
    response.type = AgentResponseType::AGENT_ERROR;
    
    if (!m_initialized) {
        response.content = "Agentic framework not initialized";
        LOG_ERROR(response.content);
        return response;
    }
    
    // Build PowerShell command
    std::stringstream cmd;
    cmd << "& \"" << m_frameworkPath << "\" -Prompt \"" << prompt << "\" -Model \"" 
        << m_modelName << "\" -OllamaServer \"" << m_ollamaServer << "\" -MaxIterations 10";
    
    std::string psCommand = cmd.str();
    LOG_DEBUG("PowerShell command: " + psCommand);
    
    // Execute via PowerShell
    if (!SpawnPowerShellProcess("powershell.exe", "-NoProfile -ExecutionPolicy Bypass -Command \"" + psCommand + "\"")) {
        response.content = "Failed to spawn PowerShell process";
        LOG_ERROR(response.content);
        return response;
    }
    
    // Read output
    std::string output;
    if (ReadProcessOutput(output, 30000)) {
        response = ParseAgentResponse(output);
        response.rawOutput = output;
        LOG_DEBUG("Agent response received: " + std::to_string(output.length()) + " bytes");
    } else {

        response.content = "Failed to read agent output";
        LOG_ERROR(response.content);
    }
    
    KillPowerShellProcess();
    return response;
}

bool AgenticBridge::StartAgentLoop(const std::string& initialPrompt, int maxIterations) {
    LOG_INFO("StartAgentLoop: " + initialPrompt);
    
    if (!m_initialized) {
        LOG_ERROR("Cannot start agent loop - not initialized");
        return false;
    }
    
    if (m_agentLoopRunning) {
        LOG_WARNING("Agent loop already running");
        return false;
    }
    
    m_agentLoopRunning = true;
    
    // Execute agent command
    AgentResponse response = ExecuteAgentCommand(initialPrompt);
    
    // Send to output callback
    if (m_outputCallback) {
        m_outputCallback("Agent Response", response.content);
        if (!response.rawOutput.empty()) {
            m_outputCallback("Agent Debug", response.rawOutput);
        }
    }
    
    m_agentLoopRunning = false;
    return true;
}

void AgenticBridge::StopAgentLoop() {
    LOG_INFO("StopAgentLoop called");
    m_agentLoopRunning = false;
    KillPowerShellProcess();
}

std::vector<std::string> AgenticBridge::GetAvailableTools() {
    // Return default tool list
    return {
        "shell", "powershell", "read_file", "write_file", 
        "web_search", "list_dir", "git_status", "task_orchestrator"
    };
}

std::string AgenticBridge::GetAgentStatus() {
    std::stringstream status;
    status << "Agentic Framework Status:\n";
    status << "  Initialized: " << (m_initialized ? "Yes" : "No") << "\n";
    status << "  Model: " << m_modelName << "\n";
    status << "  Ollama Server: " << m_ollamaServer << "\n";
    status << "  Framework Path: " << m_frameworkPath << "\n";
    status << "  Loop Running: " << (m_agentLoopRunning ? "Yes" : "No") << "\n";
    return status.str();
}

void AgenticBridge::SetModel(const std::string& modelName) {
    m_modelName = modelName;
    LOG_INFO("Model set to: " + modelName);
}

void AgenticBridge::SetOllamaServer(const std::string& serverUrl) {
    m_ollamaServer = serverUrl;
    LOG_INFO("Ollama server set to: " + serverUrl);
}

void AgenticBridge::SetOutputCallback(OutputCallback callback) {
    m_outputCallback = callback;
}

bool AgenticBridge::SpawnPowerShellProcess(const std::string& scriptPath, const std::string& arguments) {
    LOG_DEBUG("Spawning PowerShell: " + scriptPath + " " + arguments);
    
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    // Create stdout pipe
    if (!CreatePipe(&m_hStdoutRead, &m_hStdoutWrite, &sa, 0)) {
        LOG_ERROR("Failed to create stdout pipe");
        return false;
    }
    SetHandleInformation(m_hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    
    // Create stdin pipe
    if (!CreatePipe(&m_hStdinRead, &m_hStdinWrite, &sa, 0)) {
        LOG_ERROR("Failed to create stdin pipe");
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        return false;
    }
    SetHandleInformation(m_hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    
    // Setup process
    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = m_hStdoutWrite;
    si.hStdError = m_hStdoutWrite;
    si.hStdInput = m_hStdinRead;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    
    std::string cmdLine = scriptPath + " " + arguments;
    
    BOOL success = CreateProcessA(
        NULL,
        const_cast<char*>(cmdLine.c_str()),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );
    
    if (!success) {
        LOG_ERROR("Failed to create PowerShell process");
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        CloseHandle(m_hStdinRead);
        CloseHandle(m_hStdinWrite);
        return false;
    }
    
    m_hProcess = pi.hProcess;
    CloseHandle(pi.hThread);
    
    LOG_DEBUG("PowerShell process spawned successfully");
    return true;
}

bool AgenticBridge::ReadProcessOutput(std::string& output, DWORD timeoutMs) {
    LOG_DEBUG("Reading process output");
    
    output.clear();
    
    if (!m_hStdoutRead) {
        LOG_ERROR("No stdout handle");
        return false;
    }
    
    // Close write handle so ReadFile won't block indefinitely
    CloseHandle(m_hStdoutWrite);
    m_hStdoutWrite = nullptr;
    
    char buffer[4096];
    DWORD bytesRead;
    DWORD startTime = GetTickCount();
    
    while (true) {
        DWORD available = 0;
        if (!PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL)) {
            break;
        }
        
        if (available > 0) {
            if (ReadFile(m_hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                output += buffer;
            } else {
                break;
            }
        }
        
        // Check timeout
        if (GetTickCount() - startTime > timeoutMs) {
            LOG_WARNING("ReadProcessOutput timeout");
            break;
        }
        
        // Check if process ended
        DWORD exitCode;
        if (GetExitCodeProcess(m_hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            // Read any remaining output
            while (PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL) && available > 0) {
                if (ReadFile(m_hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    output += buffer;
                }
            }
            break;
        }
        
        Sleep(100);
    }
    
    LOG_DEBUG("Read " + std::to_string(output.length()) + " bytes from process");
    return !output.empty();
}

void AgenticBridge::KillPowerShellProcess() {
    if (m_hProcess) {
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
        LOG_DEBUG("PowerShell process terminated");
    }
    
    if (m_hStdoutRead) { CloseHandle(m_hStdoutRead); m_hStdoutRead = nullptr; }
    if (m_hStdoutWrite) { CloseHandle(m_hStdoutWrite); m_hStdoutWrite = nullptr; }
    if (m_hStdinRead) { CloseHandle(m_hStdinRead); m_hStdinRead = nullptr; }
    if (m_hStdinWrite) { CloseHandle(m_hStdinWrite); m_hStdinWrite = nullptr; }
}

AgentResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput) {
    AgentResponse response;
    response.type = AgentResponseType::THINKING;
    response.rawOutput = rawOutput;
    
    // Split into lines
    std::istringstream stream(rawOutput);
    std::string line;
    std::string fullContent;
    
    while (std::getline(stream, line)) {
        if (IsToolCall(line)) {
            response.type = AgentResponseType::TOOL_CALL;
            // Parse TOOL:name:args
            size_t firstColon = line.find(':');
            size_t secondColon = line.find(':', firstColon + 1);
            if (secondColon != std::string::npos) {
                response.toolName = line.substr(firstColon + 1, secondColon - firstColon - 1);
                response.toolArgs = line.substr(secondColon + 1);
            }
        } else if (IsAnswer(line)) {
            response.type = AgentResponseType::ANSWER;
            response.content = line.substr(line.find(':') + 1);
            // Trim whitespace
            response.content.erase(0, response.content.find_first_not_of(" \t\n\r"));
            response.content.erase(response.content.find_last_not_of(" \t\n\r") + 1);
        }
        fullContent += line + "\n";
    }
    
    if (response.content.empty()) {
        response.content = fullContent;
    }
    
    return response;
}

bool AgenticBridge::IsToolCall(const std::string& line) {
    return line.find("TOOL:") == 0;
}

bool AgenticBridge::IsAnswer(const std::string& line) {
    return line.find("ANSWER:") == 0;
}

std::string AgenticBridge::ResolveFrameworkPath() {
    // Try multiple locations
    std::vector<std::string> searchPaths = {
        "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield\\Agentic-Framework.ps1",
        "..\\..\\..\\..\\Powershield\\Agentic-Framework.ps1",
        "Agentic-Framework.ps1"
    };
    
    for (const auto& path : searchPaths) {
        DWORD attr = GetFileAttributesA(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            LOG_INFO("Found Agentic-Framework.ps1 at: " + path);
            return path;
        }
    }
    
    LOG_WARNING("Agentic-Framework.ps1 not found, using default path");
    return "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield\\Agentic-Framework.ps1";
}

std::string AgenticBridge::ResolveToolsModulePath() { return ""; }

void AgenticBridge::SetMaxMode(bool enabled) { m_maxMode = enabled; }
void AgenticBridge::SetDeepThinking(bool enabled) { m_deepThinking = enabled; }
void AgenticBridge::SetDeepResearch(bool enabled) { m_deepResearch = enabled; }
void AgenticBridge::SetNoRefusal(bool enabled) { m_noRefusal = enabled; }
