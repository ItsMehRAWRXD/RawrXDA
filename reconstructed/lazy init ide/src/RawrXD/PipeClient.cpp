#include "RawrXD_PipeClient.h"
#include <iostream>
#include <sstream>
#include <vector>

// Minimal JSON parser helper (or use nlohmann/json in production)
// This is a specialized parser for the known response format:
// {"Pattern":"BUG","Confidence":1.00,"Line":1,"Priority":10}

namespace RawrXD {

    PipeClient::PipeClient(const std::string& name) 
        : pipeName(name), pipeHandle(INVALID_HANDLE_VALUE) {
        if (pipeName.find("\\\\.\\pipe\\") == std::string::npos) {
            this->pipeName = "\\\\.\\pipe\\" + name;
        }
    }

    PipeClient::~PipeClient() {
        Disconnect();
    }

    bool PipeClient::Connect(int timeoutMs) {
        // Wait for pipe availability
        if (!WaitNamedPipeA(pipeName.c_str(), timeoutMs)) {
            return false;
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
    }

    void PipeClient::Disconnect() {
        if (pipeHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(pipeHandle);
            pipeHandle = INVALID_HANDLE_VALUE;
        }
    }

    PatternResult PipeClient::Classify(const std::string& text) {
        if (pipeHandle == INVALID_HANDLE_VALUE) {
            if (!Connect()) throw std::runtime_error("Pipe not connected");
        }

        try {
            SendCommand("CLASSIFY", text);
            std::string json = ReadResponse();
            return ParseJSON(json);
        } catch (...) {
            Disconnect(); // Force reconnect on next call
            throw;
        }
    }

    bool PipeClient::Ping() {
        if (pipeHandle == INVALID_HANDLE_VALUE) {
            if (!Connect()) return false;
        }
        
        try {
            SendCommand("PING");
            std::string resp = ReadResponse();
            return (resp.find("PONG") != std::string::npos);
        } catch (...) {
            return false;
        }
    }

    void PipeClient::SendCommand(const std::string& cmd, const std::string& data) {
        DWORD bytesWritten;
        
        // Protocol: [4-byte cmd len][cmd bytes][4-byte data len][data bytes]
        int cmdLen = (int)cmd.length();
        if (!WriteFile(pipeHandle, &cmdLen, 4, &bytesWritten, NULL)) throw std::runtime_error("Write failed");
        if (!WriteFile(pipeHandle, cmd.c_str(), cmdLen, &bytesWritten, NULL)) throw std::runtime_error("Write failed");

        if (!data.empty()) {
            int dataLen = (int)data.length();
            if (!WriteFile(pipeHandle, &dataLen, 4, &bytesWritten, NULL)) throw std::runtime_error("Write failed");
            if (!WriteFile(pipeHandle, data.c_str(), dataLen, &bytesWritten, NULL)) throw std::runtime_error("Write failed");
        }
    }

    std::string PipeClient::ReadResponse() {
        DWORD bytesRead;
        int respLen;
        
        // Read length first
        if (!ReadFile(pipeHandle, &respLen, 4, &bytesRead, NULL) || bytesRead != 4) {
             throw std::runtime_error("Read length failed");
        }

        if (respLen <= 0 || respLen > 65536) throw std::runtime_error("Invalid response length");

        std::vector<char> buffer(respLen + 1);
        if (!ReadFile(pipeHandle, buffer.data(), respLen, &bytesRead, NULL) || bytesRead != (DWORD)respLen) {
            throw std::runtime_error("Read data failed");
        }
        buffer[respLen] = '\0';
        
        return std::string(buffer.data());
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
    }
}
