// Agentic Framework Bridge Implementation
// Connects Win32IDE to PowerShell-based agentic framework

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
    , m_modelName("bigdaddyg-personalized-agentic:latest")
    , m_ollamaServer("http://localhost:11434")
    , m_hProcess(nullptr)
    , m_hStdoutRead(nullptr)
    , m_hStdoutWrite(nullptr)
    , m_hStdinRead(nullptr)
    , m_hStdinWrite(nullptr)
{

}

AgenticBridge::~AgenticBridge() {
    KillPowerShellProcess();

}

bool AgenticBridge::Initialize(const std::string& frameworkPath, const std::string& modelName) {

    if (m_initialized) {
        LOG_WARNING("AgenticBridge already initialized");
        return true;
    }
    
    // Resolve framework path
    if (frameworkPath.empty()) {
        m_frameworkPath = ResolveFrameworkPath();
    } else {
        m_frameworkPath = frameworkPath;
    }
    
    // Check if framework script exists
    DWORD fileAttr = GetFileAttributesA(m_frameworkPath.c_str());
    if (fileAttr == INVALID_FILE_ATTRIBUTES) {

        return false;
    }
    
    // Resolve tools module path
    m_toolsModulePath = ResolveToolsModulePath();
    
    // Set model if provided
    if (!modelName.empty()) {
        m_modelName = modelName;
    }
    
    m_initialized = true;

    return true;
}

AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt) {

    AgentResponse response;
    response.type = AgentResponseType::AGENT_ERROR;
    
    if (!m_initialized) {
        response.content = "Agentic framework not initialized";

        return response;
    }
    
    // Build PowerShell command
    std::stringstream cmd;
    cmd << "& \"" << m_frameworkPath << "\" -Prompt \"" << prompt << "\" -Model \"" 
        << m_modelName << "\" -OllamaServer \"" << m_ollamaServer << "\" -MaxIterations 10";
    
    std::string psCommand = cmd.str();

    // Execute via PowerShell
    if (!SpawnPowerShellProcess("powershell.exe", "-NoProfile -ExecutionPolicy Bypass -Command \"" + psCommand + "\"")) {
        response.content = "Failed to spawn PowerShell process";

        return response;
    }
    
    // Read output
    std::string output;
    if (ReadProcessOutput(output, 30000)) {
        response = ParseAgentResponse(output);
        response.rawOutput = output;

    } else {
        response.content = "Failed to read agent output";

    }
    
    KillPowerShellProcess();
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

}

void AgenticBridge::SetOllamaServer(const std::string& serverUrl) {
    m_ollamaServer = serverUrl;

}

void AgenticBridge::SetOutputCallback(OutputCallback callback) {
    m_outputCallback = callback;
}

bool AgenticBridge::SpawnPowerShellProcess(const std::string& scriptPath, const std::string& arguments) {

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    // Create stdout pipe
    if (!CreatePipe(&m_hStdoutRead, &m_hStdoutWrite, &sa, 0)) {

        return false;
    }
    SetHandleInformation(m_hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    
    // Create stdin pipe
    if (!CreatePipe(&m_hStdinRead, &m_hStdinWrite, &sa, 0)) {

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

        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        CloseHandle(m_hStdinRead);
        CloseHandle(m_hStdinWrite);
        return false;
    }
    
    m_hProcess = pi.hProcess;
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
