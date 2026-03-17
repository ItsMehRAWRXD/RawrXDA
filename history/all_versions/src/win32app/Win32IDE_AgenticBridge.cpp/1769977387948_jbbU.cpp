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
    response.type = AgentResponseType::AGENT_ERROR;
    
    if (!m_initialized) {
        response.content = "Agentic framework not initialized";
        return response;
    }

    if (!m_nativeEngine) {
        response.content = "Native Engine not allocated";
        return response;
    }
    
    // Execute via Native C++ Engine
    std::string engineOutput = m_nativeEngine->processQuery(prompt);
    
    // Parse the raw LLM output
    response = ParseAgentResponse(engineOutput);
    response.rawOutput = engineOutput;
    
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
    if (m_nativeEngine) {
        m_nativeEngine->setModelName(modelName);
    }
}

void AgenticBridge::SetOllamaServer(const std::string& serverUrl) {
    m_ollamaServer = serverUrl;
    // Forward to native engine config if needed
}

void AgenticBridge::SetOutputCallback(OutputCallback callback) {
    m_outputCallback = callback;
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
