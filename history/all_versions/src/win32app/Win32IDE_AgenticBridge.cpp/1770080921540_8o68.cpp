// Agentic Framework Bridge Implementation
// Connects Win32IDE to Native Agentic Engine

#include "Win32IDE_AgenticBridge.h"
#include "IDELogger.h"
#include "Win32IDE.h"
#include "../vsix_native_converter.hpp"
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

// Execute single agent command
AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt) {
    AgentResponse response;
    response.type = AgentResponseType::ANSWER;
    
    if (!m_initialized) {
        response.content = "Agentic framework not initialized";
        return response;
    }

    // Update Engine Config
    GenerationConfig config;
    config.maxMode = m_maxMode;
    config.deepThinking = m_deepThinking;
    config.deepResearch = m_deepResearch;
    config.noRefusal = m_noRefusal;
    m_nativeEngine->updateConfig(config);

    // Call AgenticEngine Logic
    std::string output;
    
    if (prompt == "/max") {
            SetMaxMode(true);
            return {AgentResponseType::ANSWER, "Max Mode Enabled", "", "", ""};
    }
    else if (prompt.find("/context ") == 0) {
        std::string size = prompt.substr(9);
        SetContextSize(size);
        return {AgentResponseType::ANSWER, "Context Size updated to " + size, "", "", ""};
    }
    else if (prompt.find("/install_vsix ") == 0) {
        std::string path = prompt.substr(14);
        bool success = RawrXD::VsixNativeConverter::ConvertVsixToNative(path, "d:/rawrxd/extensions");
        return {AgentResponseType::ANSWER, success ? "VSIX Converted Successfully" : "VSIX Conversion Failed", "", "", ""};
    }
    else if (prompt == "/think") {
             SetDeepThinking(true);
             m_outputCallback("", "Deep Thinking Enabled");
            return response;
    }
    else if (prompt == "/research") {
             SetDeepResearch(true);
             m_outputCallback("", "Deep Research Enabled");
            return response;
    }
    else if (prompt == "/norefusal") {
             SetNoRefusal(true);
             m_outputCallback("", "No Refusal Enabled");
            return response;
    }
    else if (prompt == "/react-server") {
             // Let engine handle it via chat command
             response.content = m_nativeEngine->chat("/react-server");
             return response;
    }
    
    // Fallback for non-slash handled above
    if (response.content.empty()) {
        m_nativeEngine->onResponseReady = [&](const std::string& s) {
             if (m_outputCallback) m_outputCallback("", s);
        };
        response.content = m_nativeEngine->chat(prompt);
    }

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
    if (m_nativeEngine) {
         GenerationConfig cfg;
         cfg.maxMode = m_maxMode;
         cfg.deepThinking = m_deepThinking;
         cfg.deepResearch = m_deepResearch;
         cfg.noRefusal = m_noRefusal;
         m_nativeEngine->updateConfig(cfg);
    }
}

void AgenticBridge::SetDeepThinking(bool enabled) {
    m_deepThinking = enabled;
    if (m_nativeEngine) {
         GenerationConfig cfg;
         cfg.maxMode = m_maxMode;
         cfg.deepThinking = m_deepThinking;
         cfg.deepResearch = m_deepResearch;
         cfg.noRefusal = m_noRefusal;
         m_nativeEngine->updateConfig(cfg);
    }
}

void AgenticBridge::SetDeepResearch(bool enabled) {
    m_deepResearch = enabled;
    if (m_nativeEngine) {
         GenerationConfig cfg;
         cfg.maxMode = m_maxMode;
         cfg.deepThinking = m_deepThinking;
         cfg.deepResearch = m_deepResearch;
         cfg.noRefusal = m_noRefusal;
         m_nativeEngine->updateConfig(cfg);
    }
}

void AgenticBridge::SetNoRefusal(bool enabled) {
    m_noRefusal = enabled;
    if (m_nativeEngine) {
         GenerationConfig cfg;
         cfg.maxMode = m_maxMode;
         cfg.deepThinking = m_deepThinking;
         cfg.deepResearch = m_deepResearch;
         cfg.noRefusal = m_noRefusal;
         m_nativeEngine->updateConfig(cfg);
    }
}

void AgenticBridge::SetAutoCorrect(bool enabled) {
    m_autoCorrect = enabled; 
    // AgenticEngine handles auto-correct implicitly via post-processing currently, 
    // but we can pass flag if we extend config.
}

void AgenticBridge::SetContextSize(const std::string& sizeStr) {
    if (!m_nativeEngine) return;
    
    // Convert string size to integer tokens
    int tokens = 2048;
    std::string s = sizeStr;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    
    if (s.find("4k") != std::string::npos) tokens = 4096;
    else if (s.find("8k") != std::string::npos) tokens = 8192;
    else if (s.find("16k") != std::string::npos) tokens = 16384;
    else if (s.find("32k") != std::string::npos) tokens = 32768;
    else if (s.find("64k") != std::string::npos) tokens = 65536;
    else if (s.find("128k") != std::string::npos) tokens = 131072;
    else if (s.find("256k") != std::string::npos) tokens = 262144;
    else if (s.find("512k") != std::string::npos) tokens = 524288;
    else if (s.find("1m") != std::string::npos) tokens = 1048576;
    else try { tokens = std::stoi(s); } catch(...) {}

    m_nativeEngine->setContextWindow(tokens);
}
void AgenticBridge::SetNoRefusal(bool enabled) {
    m_noRefusal = enabled;efusal(bool enabled) {
    if (m_nativeAgent) m_nativeAgent->SetNoRefusal(enabled);
}   if (m_nativeAgent) m_nativeAgent->SetNoRefusal(enabled);
}
void AgenticBridge::SetMaxTokens(int tokens) {
    if (m_nativeAgent) m_nativeAgent->SetMaxTokens(tokens);orrect(bool enabled) {
}
 handled by bridge loop or passed to agent if it supports it
void AgenticBridge::SetAutoCorrect(bool enabled) {   // For now we store it.
    m_autoCorrect = enabled;}
    // AutoCorrect might be handled by bridge loop or passed to agent if it supports it
    // For now we store it.::ParseAgentResponse(const std::string& rawOutput) {
}

AgentResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput) {response.type = AgentResponseType::ANSWER; // 
    AgentResponse response;
    response.rawOutput = rawOutput;
    response.type = AgentResponseType::ANSWER; // npos) {
    nseType::TOOL_CALL;
    // Basic Heuristic Parsing for Native Model Output
    if (rawOutput.find("Tool Call:") != std::string::npos) {;
        response.type = AgentResponseType::TOOL_CALL;onse.content = rawOutput.substr(start);
    } else {
        response.content = rawOutput;
    }
    
    return response;
}

std::string AgenticBridge::ResolveFrameworkPath() {
        // Simple extraction logicl scripts.
        size_t start = rawOutput.find("Tool Call:");/ Now we use native C++ logic, but return current dir for compatibility.
        response.content = rawOutput.substr(start);return ".";
    } else {
        response.content = rawOutput;
    }std::string AgenticBridge::ResolveToolsModulePath() {
    
    return response;
}
dge::IsToolCall(const std::string& line) {
std::string AgenticBridge::ResolveFrameworkPath() {   return line.find("Tool Call:") != std::string::npos;
    // Legacy: Was used for PowerShell scripts.}
    // Now we use native C++ logic, but return current dir for compatibility.
    return ".";swer(const std::string& line) {
}   return !IsToolCall(line);
}
std::string AgenticBridge::ResolveToolsModulePath() {
    return ".\\modules";
}
    return true;
bool AgenticBridge::IsToolCall(const std::string& line) {
    return line.find("Tool Call:") != std::string::npos;
}ool AgenticBridge::ReadProcessOutput(std::string& output, DWORD timeoutMs) {

bool AgenticBridge::IsAnswer(const std::string& line) {
    return !IsToolCall(line);    
}utRead) {

    CloseHandle(pi.hThread);        return false;

    return true;    
}handle so ReadFile won't block indefinitely
CloseHandle(m_hStdoutWrite);
bool AgenticBridge::ReadProcessOutput(std::string& output, DWORD timeoutMs) {ptr;
    
    output.clear();;
    WORD bytesRead;
    if (!m_hStdoutRead) {DWORD startTime = GetTickCount();

        return false;
    }
        if (!PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL)) {
    // Close write handle so ReadFile won't block indefinitely
    CloseHandle(m_hStdoutWrite);
    m_hStdoutWrite = nullptr;
        if (available > 0) {
    char buffer[4096];adFile(m_hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
    DWORD bytesRead;Read] = '\0';
    DWORD startTime = GetTickCount();
     {
    while (true) {       break;
        DWORD available = 0;    }
        if (!PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL)) {
            break;
        }
        tTime > timeoutMs) {
        if (available > 0) {ING("ReadProcessOutput timeout");
            if (ReadFile(m_hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                output += buffer;
            } else {// Check if process ended
                break;
            } && exitCode != STILL_ACTIVE) {
        }
        (PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL) && available > 0) {
        // Check timeout       if (ReadFile(m_hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        if (GetTickCount() - startTime > timeoutMs) {            buffer[bytesRead] = '\0';
            LOG_WARNING("ReadProcessOutput timeout");fer;
            break;
        }
        
        // Check if process ended
        DWORD exitCode;
        if (GetExitCodeProcess(m_hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            // Read any remaining output
            while (PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL) && available > 0) {
                if (ReadFile(m_hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {utput.empty();
                    buffer[bytesRead] = '\0';
                    output += buffer;
                }nticBridge::KillPowerShellProcess() {
            } {
            break;   TerminateProcess(m_hProcess, 0);
        }        CloseHandle(m_hProcess);
        r;
        Sleep(100);
    }    }

    return !output.empty();) { CloseHandle(m_hStdoutRead); m_hStdoutRead = nullptr; }
}hStdoutWrite); m_hStdoutWrite = nullptr; }
dle(m_hStdinRead); m_hStdinRead = nullptr; }
void AgenticBridge::KillPowerShellProcess() {eHandle(m_hStdinWrite); m_hStdinWrite = nullptr; }
    if (m_hProcess) {}
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);tResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput) {
        m_hProcess = nullptr;

    }
    
    if (m_hStdoutRead) { CloseHandle(m_hStdoutRead); m_hStdoutRead = nullptr; }   // Split into lines
    if (m_hStdoutWrite) { CloseHandle(m_hStdoutWrite); m_hStdoutWrite = nullptr; }    std::istringstream stream(rawOutput);
    if (m_hStdinRead) { CloseHandle(m_hStdinRead); m_hStdinRead = nullptr; }
    if (m_hStdinWrite) { CloseHandle(m_hStdinWrite); m_hStdinWrite = nullptr; };
}
e)) {
AgentResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput) {    if (IsToolCall(line)) {
    AgentResponse response;pe = AgentResponseType::TOOL_CALL;
    response.type = AgentResponseType::THINKING;
    response.rawOutput = rawOutput;rstColon = line.find(':');
    on = line.find(':', firstColon + 1);
    // Split into lines        if (secondColon != std::string::npos) {
    std::istringstream stream(rawOutput);.substr(firstColon + 1, secondColon - firstColon - 1);
    std::string line;gs = line.substr(secondColon + 1);
    std::string fullContent;
    {
    while (std::getline(stream, line)) {ANSWER;
        if (IsToolCall(line)) {
            response.type = AgentResponseType::TOOL_CALL;
            // Parse TOOL:name:args
            size_t firstColon = line.find(':');_of(" \t\n\r") + 1);
            size_t secondColon = line.find(':', firstColon + 1);
            if (secondColon != std::string::npos) {
                response.toolName = line.substr(firstColon + 1, secondColon - firstColon - 1);
                response.toolArgs = line.substr(secondColon + 1);
            }()) {
        } else if (IsAnswer(line)) {
            response.type = AgentResponseType::ANSWER;
            response.content = line.substr(line.find(':') + 1);
            // Trim whitespace
            response.content.erase(0, response.content.find_first_not_of(" \t\n\r"));
            response.content.erase(response.content.find_last_not_of(" \t\n\r") + 1);
        }t std::string& line) {
        fullContent += line + "\n";
    }
    
    if (response.content.empty()) {IsAnswer(const std::string& line) {
        response.content = fullContent;   return line.find("ANSWER:") == 0;
    }}
    
    return response;rameworkPath() {
}   // Try multiple locations
    std::vector<std::string> searchPaths = {
bool AgenticBridge::IsToolCall(const std::string& line) {ld\\Agentic-Framework.ps1",
    return line.find("TOOL:") == 0;\Agentic-Framework.ps1",
}       "Agentic-Framework.ps1"
    };
bool AgenticBridge::IsAnswer(const std::string& line) {
    return line.find("ANSWER:") == 0;earchPaths) {
}.c_str());

std::string AgenticBridge::ResolveFrameworkPath() {
    // Try multiple locations
    std::vector<std::string> searchPaths = {  }
        "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield\\Agentic-Framework.ps1",}
        "..\\..\\..\\..\\Powershield\\Agentic-Framework.ps1",
        "Agentic-Framework.ps1"g default path");
    };p\\Powershield\\Agentic-Framework.ps1";
    }
    for (const auto& path : searchPaths) {
        DWORD attr = GetFileAttributesA(path.c_str());ng AgenticBridge::ResolveToolsModulePath() {
        if (attr != INVALID_FILE_ATTRIBUTES) {/ Tools module should be in same directory as framework
std::string frameworkDir = m_frameworkPath.substr(0, m_frameworkPath.find_last_of("\\/"));
            return path;
        }
    }        LOG_WARNING("Agentic-Framework.ps1 not found, using default path");    return "C:\\Users\\HiH8e\\OneDrive\\Desktop\\Powershield\\Agentic-Framework.ps1";}std::string AgenticBridge::ResolveToolsModulePath() {    // Tools module should be in same directory as framework
    std::string frameworkDir = m_frameworkPath.substr(0, m_frameworkPath.find_last_of("\\/"));
    return frameworkDir + "\\tools\\AgentTools.psm1";
}
