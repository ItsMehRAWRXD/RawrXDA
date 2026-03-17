#include "mcp_server_host.hpp"
#include "license_enforcement.h"
#include <windows.h>
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>

namespace RawrXD {

class MCPServerHost::Impl {
public:
    Impl(MCPServerHost* owner) : m_owner(owner), m_hStopEvent(CreateEvent(NULL, TRUE, FALSE, NULL)) {}
    ~Impl() {
        stop();
        CloseHandle(m_hStopEvent);
    }

    bool start(const std::string& serverName) {
        // Enterprise license enforcement gate for MCP Server
        if (!LicenseEnforcementGate::getInstance().gateMCPServer(serverName).success) {
            if (m_owner->m_onErrorOccurred)
                m_owner->m_onErrorOccurred("MCP Server requires Professional+ license");
            return false;
        }
        m_pipeName = "\\\\.\\pipe\\" + serverName;
        m_serverThread = std::thread(&Impl::serverLoop, this);
        return true;
    }

    void stop() {
        SetEvent(m_hStopEvent);
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
    }

private:
    void serverLoop() {
        while (WaitForSingleObject(m_hStopEvent, 0) != WAIT_OBJECT_0) {
            HANDLE hPipe = CreateNamedPipeA(
                m_pipeName.c_str(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                4096, 4096, 0, NULL
            );

            if (hPipe == INVALID_HANDLE_VALUE) {
                if (m_owner->m_onErrorOccurred) m_owner->m_onErrorOccurred("Failed to create named pipe");
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            OVERLAPPED overlapped = { 0 };
            overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            
            bool connected = ConnectNamedPipe(hPipe, &overlapped);
            if (!connected && GetLastError() == ERROR_IO_PENDING) {
                HANDLE waitHandles[] = { m_hStopEvent, overlapped.hEvent };
                DWORD wait = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                if (wait == WAIT_OBJECT_0) {
                    CancelIo(hPipe);
                    CloseHandle(hPipe);
                    CloseHandle(overlapped.hEvent);
                    break;
                }
                connected = true;
            }

            if (connected || GetLastError() == ERROR_PIPE_CONNECTED) {
                if (m_owner->m_onClientConnected) m_owner->m_onClientConnected("client");
                std::thread(&Impl::clientHandler, this, hPipe).detach();
            } else {
                CloseHandle(hPipe);
            }
            CloseHandle(overlapped.hEvent);
        }
    }

    void clientHandler(HANDLE hPipe) {
        char buffer[4096];
        DWORD bytesRead;
        while (WaitForSingleObject(m_hStopEvent, 0) != WAIT_OBJECT_0) {
            if (ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                try {
                    auto j = nlohmann::json::parse(std::string(buffer, bytesRead));
                    m_owner->processRequest(hPipe, j);
                } catch (...) {
                    // JSON Parse error
                }
            } else {
                break;
            }
        }
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
        if (m_owner->m_onClientDisconnected) m_owner->m_onClientDisconnected("client");
    }

    MCPServerHost* m_owner;
    std::string m_pipeName;
    std::thread m_serverThread;
    HANDLE m_hStopEvent;
};

MCPServerHost::MCPServerHost(ToolExecutionEngine* engine)
    : m_engine(engine), m_impl(std::make_unique<Impl>(this)) {}

MCPServerHost::~MCPServerHost() { stop(); }

bool MCPServerHost::start(const std::string& serverName) {
    m_serverName = serverName;
    m_running = true;
    return m_impl->start(serverName);
}

void MCPServerHost::stop() {
    m_running = false;
    m_impl->stop();
}

void MCPServerHost::processRequest(void* handle, const nlohmann::json& request) {
    HANDLE hPipe = (HANDLE)handle;
    std::string method = request.value("method", "");
    auto id = request["id"];

    if (method == "initialize") {
        nlohmann::json res = {
            {"id", id},
            {"jsonrpc", "2.0"},
            {"result", {
                {"protocolVersion", "2024-11-05"},
                {"serverInfo", {{"name", "RawrXD-MCP"}, {"version", "1.0.0"}}}
            }}
        };
        sendResponse(hPipe, res);
    } else if (method == "tools/list") {
        nlohmann::json tools = nlohmann::json::array();
        
        // Populate tools... (reusing the logic from the old version but with nlohmann)
        tools.push_back({
            {"name", "searchFiles"},
            {"description", "Search project files"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {{"pattern", {{"type", "string"}}}, {"path", {{"type", "string"}}}}},
                {"required", {"pattern"}}
            }}
        });

        tools.push_back({
            {"name", "readFiles"},
            {"description", "Read file contents"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {{"path", {{"type", "string"}}}}},
                {"required", {"path"}}
            }}
        });

        nlohmann::json res = {
            {"id", id},
            {"jsonrpc", "2.0"},
            {"result", {{"tools", tools}}}
        };
        sendResponse(hPipe, res);
    } else if (method == "tools/call") {
        auto params = request.value("params", nlohmann::json::object());
        std::string toolName = params.value("name", "");
        auto toolArgs = params.value("arguments", nlohmann::json::object());

        // Call engine
        auto toolRes = m_engine->executeTool(toolName, toolArgs);

        nlohmann::json res = {
            {"id", id},
            {"jsonrpc", "2.0"},
            {"result", {
                {"content", {{{"type", "text"}, {"text", toolRes.output}}}},
                {"isError", !toolRes.success}
            }}
        };
        sendResponse(hPipe, res);
    }
}

void MCPServerHost::sendResponse(void* handle, const nlohmann::json& response) {
    HANDLE hPipe = (HANDLE)handle;
    std::string s = response.dump();
    DWORD bytesWritten;
    WriteFile(hPipe, s.c_str(), (DWORD)s.size(), &bytesWritten, NULL);
}

void MCPServerHost::sendError(void* handle, const nlohmann::json& id, int code, const std::string& message) {
    nlohmann::json err = {
        {"id", id},
        {"jsonrpc", "2.0"},
        {"error", {{"code", code}, {"message", message}}}
    };
    sendResponse(handle, err);
}

} // namespace RawrXD
