#include "lsp_client.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <windows.h>
#include "nlohmann/json.hpp"

namespace RawrXD {

class Win32PipeTransport : public JsonRpcTransport {
public:
    Win32PipeTransport() : hRead(NULL), hWrite(NULL), hProcess(NULL) {}
    
    ~Win32PipeTransport() {
        disconnect();
    }
    
    bool connect(const std::string& cmd, const std::vector<std::string>& args) override {
        SECURITY_Attributes sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;
        
        HANDLE hChildStd_IN_Rd = NULL;
        HANDLE hChildStd_IN_Wr = NULL;
        HANDLE hChildStd_OUT_Rd = NULL;
        HANDLE hChildStd_OUT_Wr = NULL;
        
        // Create a pipe for the child process's STDOUT.
        if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &sa, 0)) return false;
        if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) return false;
        
        // Create a pipe for the child process's STDIN.
        if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &sa, 0)) return false;
        if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) return false;
        
        // CreateProcess
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.hStdError = hChildStd_OUT_Wr; // Capture stderr too? Or separate? Usually stderr logs, stdout is JSON-RPC
        si.hStdOutput = hChildStd_OUT_Wr;
        si.hStdInput = hChildStd_IN_Rd;
        si.dwFlags |= STARTF_USESTDHANDLES;
        
        ZeroMemory(&pi, sizeof(pi));
        
        std::string commandLine = cmd;
        for(const auto& arg : args) {
            commandLine += " " + arg;
        }
        
        // NOTE: CreateProcess modifies commandLine buffer if not const char*
        std::vector<char> cmdVec(commandLine.begin(), commandLine.end());
        cmdVec.push_back(0);
        
        if (!CreateProcessA(NULL, cmdVec.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            return false;
        }
        
        hProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        
        // Close handles to the pipes that are just for child
        CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_IN_Rd);
        
        hRead = hChildStd_OUT_Rd;
        hWrite = hChildStd_IN_Wr;
        
        return true;
    }
    
    void disconnect() {
        if (hWrite) CloseHandle(hWrite);
        if (hRead) CloseHandle(hRead);
        if (hProcess) {
            TerminateProcess(hProcess, 0);
            CloseHandle(hProcess);
        }
        hWrite = NULL;
        hRead = NULL;
        hProcess = NULL;
    }
    
    void send(const nlohmann::json& msg) override {
        if (!hWrite) return;
        std::string payload = msg.dump();
        std::string header = "Content-Length: " + std::to_string(payload.size()) + "\r\n\r\n";
        std::string packet = header + payload;
        
        DWORD written;
        WriteFile(hWrite, packet.c_str(), packet.size(), &written, NULL);
    }
    
    nlohmann::json receive() override {
        if (!hRead) return nullptr;
        
        // Basic implementation of Content-Length reading
        // Need to read headers byte by byte until \r\n\r\n
        std::string headers;
        char c;
        DWORD read;
        while (ReadFile(hRead, &c, 1, &read, NULL) && read > 0) {
            headers += c;
            if (headers.size() >= 4 && headers.substr(headers.size()-4) == "\r\n\r\n") {
                 break;
            }
        }
        
        // Parse Content-Length
        size_t lenInfo = headers.find("Content-Length: ");
        if (lenInfo == std::string::npos) return nullptr;
        
        size_t lenStart = lenInfo + 16;
        size_t lenEnd = headers.find("\r", lenStart);
        int contentLength = std::stoi(headers.substr(lenStart, lenEnd - lenStart));
        
        if (contentLength <= 0) return nullptr;
        
        // Read body
        std::vector<char> buffer(contentLength);
        DWORD totalRead = 0;
        while(totalRead < contentLength) {
             if(!ReadFile(hRead, buffer.data() + totalRead, contentLength - totalRead, &read, NULL)) break;
             totalRead += read;
        }
        
        std::string body(buffer.begin(), buffer.end());
        try {
            return nlohmann::json::parse(body);
        } catch(...) {
            return nullptr;
        }
    }
    
    bool isConnected() const override {
        return hProcess != NULL;
    }

private:
    HANDLE hRead;
    HANDLE hWrite;
    HANDLE hProcess;
    typedef struct _SECURITY_ATTRIBUTES {
        DWORD nLength;
        LPVOID lpSecurityDescriptor;
        BOOL bInheritHandle;
    } SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;
};

// LSPClient Implementation

LSPClient::LSPClient(const LSPConfig& config) : m_config(config) {
    m_transport = std::make_unique<Win32PipeTransport>();
}

LSPClient::~LSPClient() {
    stop();
}

bool LSPClient::start() {
    return m_transport->connect(m_config.command, m_config.args);
}

void LSPClient::stop() {
    // Dynamic cast or just destruct
    // m_transport destructor handles cleanup
    m_transport.reset();
}

nlohmann::json LSPClient::createRequest(const std::string& method, const nlohmann::json& params) {
    return {
        {"jsonrpc", "2.0"},
        {"id", ++m_requestId},
        {"method", method},
        {"params", params}
    };
}

nlohmann::json LSPClient::createNotification(const std::string& method, const nlohmann::json& params) {
    return {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", params}
    };
}

std::future<nlohmann::json> LSPClient::initialize() {
    return std::async(std::launch::async, [this]() {
        nlohmann::json params = {
            {"processId", GetCurrentProcessId()},
            {"rootUri", "file:///" + m_config.rootPath},
            {"capabilities", {}}
        };
        m_transport->send(createRequest("initialize", params));
        
        // Block wait for response (simple sync implementation for now)
        return m_transport->receive();
    });
}

void LSPClient::didOpen(const std::string& uri, const std::string& text) {
     nlohmann::json params = {
        {"textDocument", {
            {"uri", uri},
            {"languageId", m_config.languageId},
            {"version", 1},
            {"text", text}
        }}
    };
    m_transport->send(createNotification("textDocument/didOpen", params));
}

void LSPClient::didChange(const std::string& uri, const std::string& text) {
    nlohmann::json params = {
        {"textDocument", {
            {"uri", uri},
            {"version", 2}
        }},
        {"contentChanges", {{{"text", text}}}}
    };
    m_transport->send(createNotification("textDocument/didChange", params));
}

std::future<nlohmann::json> LSPClient::completion(const std::string& uri, int line, int character) {
    return std::async(std::launch::async, [this, uri, line, character]() {
        nlohmann::json params = {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", line}, {"character", character}}}
        };
        
        int id = m_requestId + 1;
        m_transport->send(createRequest("textDocument/completion", params));
        
        // VERY NAIVE loop to find matching response
        // In reality, you'd have a message pump thread handling this
        while (true) {
            auto msg = m_transport->receive();
            if (msg.contains("id") && msg["id"] == id) {
                return msg["result"];
            }
        }
    });
}

std::future<nlohmann::json> LSPClient::definition(const std::string& uri, int line, int character) {
     return std::async(std::launch::async, [this, uri, line, character]() {
        nlohmann::json params = {
            {"textDocument", {{"uri", uri}}},
            {"position", {{"line", line}, {"character", character}}}
        };
        int id = m_requestId + 1;
        m_transport->send(createRequest("textDocument/definition", params));
        
        while (true) {
            auto msg = m_transport->receive();
            if (msg.contains("id") && msg["id"] == id) {
                return msg["result"];
            }
        }
    });
}

} // namespace RawrXD
