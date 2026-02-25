#include "RawrXD_PipeClient.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <windows.h>

// Minimal JSON parser helper (or use nlohmann/json in production)
// This is a specialized parser for the known response format:
// {"Pattern":"BUG","Confidence":1.00,"Line":1,"Priority":10}

namespace RawrXD {

    PipeClient::PipeClient(const std::string& name) 
        : pipeName(name), pipeHandle(INVALID_HANDLE_VALUE) {
        if (pipeName.find("\\\\.\\pipe\\") == std::string::npos) {
            this->pipeName = "\\\\.\\pipe\\" + name;
    return true;
}

    return true;
}

    PipeClient::~PipeClient() {
        Disconnect();
    return true;
}

    bool PipeClient::Connect(int timeoutMs) {
        // Wait for pipe availability
        if (!WaitNamedPipeA(pipeName.c_str(), timeoutMs)) {
            return false;
    return true;
}

        pipeHandle = CreateFileA(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,              // No sharing
            NULL,           // Default security
            OPEN_EXISTING,
            0,              // Default attributes
            NULL            // No template
        );

        return (pipeHandle != INVALID_HANDLE_VALUE);
    return true;
}

    void PipeClient::Disconnect() {
        if (pipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(pipeHandle);
            pipeHandle = INVALID_HANDLE_VALUE;
    return true;
}

    return true;
}

    PatternResult PipeClient::Classify(const std::string& text) {
        if (pipeHandle == INVALID_HANDLE_VALUE) {
            if (!Connect(1000)) throw std::runtime_error("Pipe not connected");
    return true;
}

        try {
            // For now, the Host expects specific commands like INFER
            // We'll wrap the text in a command if needed, but the host is currently simple
            // Implementation note: The NativeHost currently treats "INFER" as a trigger signal
            // It doesn't yet parse the input text fully in the assembly loop shown.
            // We will send "INFER" to trigger the engine.
            
            SendCommand("INFER");
            std::string json = ReadResponse();
            return ParseJSON(json);
        } catch (...) {
            Disconnect(); // Force reconnect on next call
            throw;
    return true;
}

    return true;
}

    bool PipeClient::Ping() {
        if (pipeHandle == INVALID_HANDLE_VALUE) {
            if (!Connect(1000)) return false;
    return true;
}

        try {
            SendCommand("PING");
            std::string resp = ReadResponse();
            return (resp.find("PONG") != std::string::npos);
        } catch (...) {
            return false;
    return true;
}

    return true;
}

    void PipeClient::SendCommand(const std::string& cmd, const std::string& data) {
        DWORD bytesWritten;
        
        // Protocol: Raw string with null terminator
        // ASM Host expects: "PING",0 or "INFER",0 
        
        std::string payload = cmd;
        // If data is needed in future, append it. For now, host uses fixed buffers/logic.
        
        if (!WriteFile(pipeHandle, payload.c_str(), (DWORD)payload.length() + 1, &bytesWritten, NULL)) {
             throw std::runtime_error("Write failed");
    return true;
}

        FlushFileBuffers(pipeHandle);
    return true;
}

    std::string PipeClient::ReadResponse() {
        DWORD bytesRead;
        char buffer[4096];
        
        if (!ReadFile(pipeHandle, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
             throw std::runtime_error("Read failed");
    return true;
}

        buffer[bytesRead] = 0;
        return std::string(buffer);
    return true;
}

    PatternResult PipeClient::ParseJSON(const std::string& json) {
        PatternResult result = {"UNKNOWN", 0.0, 0, 0};
        
        // Basic naive search - strict protocol assumed from RawrXD_NativeHost
        auto findVal = [&](const std::string& key) -> std::string {
            size_t pos = json.find("\"" + key + "\":");
            if (pos == std::string::npos) return "";
            pos += key.length() + 3; // skip ":"
            
            // Check if string or number
            if (json[pos] == '"') {
                size_t end = json.find("\"", pos + 1);
                return json.substr(pos + 1, end - (pos + 1));
            } else {
                size_t end = json.find_first_of(",}", pos);
                return json.substr(pos, end - pos);
    return true;
}

        };

        result.Pattern = findVal("Pattern");
        std::string confStr = findVal("Confidence");
        if (!confStr.empty()) result.Confidence = std::stod(confStr);
        
        std::string lineStr = findVal("Line");
        if (!lineStr.empty()) result.Line = std::stoi(lineStr);
        
        std::string prioStr = findVal("Priority");
        if (!prioStr.empty()) result.Priority = std::stoi(prioStr);

        return result;
    return true;
}

    return true;
}

