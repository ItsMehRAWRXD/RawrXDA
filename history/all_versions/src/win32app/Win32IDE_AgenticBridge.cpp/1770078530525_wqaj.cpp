// Agentic Framework Bridge Implementation
// Connects Win32IDE to Native Agentic Engine

#include "Win32IDE_AgenticBridge.h"
#include "IDELogger.h"
#include "Win32IDE.h"
#include <sstream>
#include <algorithm>
#include <regex>

AgenticBridge::AgenticBridge(Win32IDE* ide)
    : m_ide(ide)
    , m_initialized(false)
    , m_agentLoopRunning(false)
    , m_modelName("titan-embedded")
    , m_ollamaServer("http://localhost:11434")
    , m_nativeEngine(std::make_shared<AgenticEngine>())
{
}

AgenticBridge::~AgenticBridge() {
    StopAgentLoop();
     if (m_nativeEngine) {
        m_nativeEngine->shutdown();
    }
}

bool AgenticBridge::Initialize(const std::string& frameworkPath, const std::string& modelName) {
    if (m_initialized) {
        LOG_WARNING("AgenticBridge already initialized");
        return true;
    }
    
    // Initialize Native Engine
    if (m_nativeEngine) {
        m_nativeEngine->initialize();
        m_nativeEngine->setModelName(modelName.empty() ? m_modelName : modelName);
        
        // Register callbacks
        m_nativeEngine->onResponseReady = [this](const std::string& response) {
            if (m_outputCallback) {
                m_outputCallback("Agent Stream", response);
            }
        };
    }

    // Set model if provided
    if (!modelName.empty()) {
        m_modelName = modelName;
    }
    
    m_initialized = true;
    LOG_INFO("Native Agentic Bridge Initialized");
    return true;
}

AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt) {
    AgentResponse response;
    response.type = AgentResponseType::ANSWER;
    
    if (!m_initialized) {
        response.content = "Agentic framework not initialized";
        return response;
    }

    if (!m_nativeAgent) { // fallback lazy init
         if (m_nativeEngine) {
             m_nativeAgent = std::make_shared<RawrXD::NativeAgent>(m_nativeEngine->getInferenceEngine());
         }
         else { response.content = "Native Agent not ready"; return response; }
    }

    // Handle Slash Commands
    if (prompt.find("/bugreport ") == 0) {
            std::string path = prompt.substr(11);
            m_nativeAgent->BugReport(path); 
            return response; 
    }
    else if (prompt.find("/suggest ") == 0) {
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
    
    // Accumulate output into response.content for return
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

bool AgenticBridge::StartAgentLoop(const std::string& initialPrompt, int maxIterations) {
    if (!m_initialized) {
        return false;
    }
    
    if (m_agentLoopRunning) {
        LOG_WARNING("Agent loop already running");
        return false;
    }
    
    m_agentLoopRunning = true;
    
    // Execute agent command (Blocking for now, can move to thread if needed)
    // The native engine supports async but for the bridge interface we keep it simple
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
    m_agentLoopRunning = false;
    // Signal engine to stop if running async
}

std::vector<std::string> AgenticBridge::GetAvailableTools() {
    // Return tools available in the native registry
    // This could also query m_nativeEngine->getAvailableTools() if implemented
    return {
        "access_fs", "read_file", "write_file", 
        "web_fetch", "list_directory", "git_ops", "code_analysis"
    };
}

std::string AgenticBridge::GetAgentStatus() {
    std::stringstream status;
    status << "Native Agentic Framework Status:\n";
    status << "  Initialized: " << (m_initialized ? "Yes" : "No") << "\n";
    status << "  Model: " << m_modelName << "\n";
    status << "  Engine State: " << (m_nativeEngine ? "Active" : "Null") << "\n";
    status << "  Loop Running: " << (m_agentLoopRunning ? "Yes" : "No") << "\n";
    return status.str();
}

void AgenticBridge::SetModel(const std::string& modelName) {
    m_modelName = modelName;
    // Potentially reload model here
}

void AgenticBridge::SetOllamaServer(const std::string& serverUrl) {
    m_ollamaServer = serverUrl;
}

void AgenticBridge::SetMaxMode(bool enabled) {
    m_maxMode = enabled;
    if (m_nativeAgent) m_nativeAgent->SetMaxMode(enabled);
}

void AgenticBridge::SetDeepThinking(bool enabled) {
    m_deepThinking = enabled;
    if (m_nativeAgent) m_nativeAgent->SetDeepThink(enabled);
}

void AgenticBridge::SetDeepResearch(bool enabled) {
    m_deepResearch = enabled;
    if (m_nativeAgent) m_nativeAgent->SetDeepResearch(enabled);
}

void AgenticBridge::SetNoRefusal(bool enabled) {
    m_noRefusal = enabled;
    if (m_nativeAgent) m_nativeAgent->SetNoRefusal(enabled);
}

void AgenticBridge::SetAutoCorrect(bool enabled) {
    m_autoCorrect = enabled;
    // AutoCorrect might be handled by bridge loop or passed to agent if it supports it
    // For now we store it.
}

AgentResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput) {
    AgentResponse response;
    response.rawOutput = rawOutput;
    response.type = AgentResponseType::ANSWER; // 
    
    // Basic Heuristic Parsing for Native Model Output
    if (rawOutput.find("Tool Call:") != std::string::npos) {
        response.type = AgentResponseType::TOOL_CALL;
        // Simple extraction logic
        size_t start = rawOutput.find("Tool Call:");
        response.content = rawOutput.substr(start);
    } else {
        response.content = rawOutput;
    }
    
    return response;
}

std::string AgenticBridge::ResolveFrameworkPath() {
    // Legacy: Was used for PowerShell scripts.
    // Now we use native C++ logic, but return current dir for compatibility.
    return ".";
}

std::string AgenticBridge::ResolveToolsModulePath() {
    return ".\\modules";
}

bool AgenticBridge::IsToolCall(const std::string& line) {
    return line.find("Tool Call:") != std::string::npos;
}

bool AgenticBridge::IsAnswer(const std::string& line) {
    return !IsToolCall(line);
}

    CloseHandle(pi.hThread);

    return true;
}

bool AgenticBridge::ReadProcessOutput(std::string& output, DWORD timeoutMs) {

    output.clear();
    
    if (!m_hStdoutRead) {

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

    return !output.empty();
}

void AgenticBridge::KillPowerShellProcess() {
    if (m_hProcess) {
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;

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

            return path;
        }
    }
    
    LOG_WARNING("Agentic-Framework.ps1 not found, using default path");
    return "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield\\Agentic-Framework.ps1";
}

std::string AgenticBridge::ResolveToolsModulePath() {
    // Tools module should be in same directory as framework
    std::string frameworkDir = m_frameworkPath.substr(0, m_frameworkPath.find_last_of("\\/"));
    return frameworkDir + "\\tools\\AgentTools.psm1";
}
