#include "../agentic/RawrXD_ToolRegistry.h"
#include "../core/self_host_engine.hpp"
#include "../core/shared_feature_dispatch.h"
#include "Win32IDE.h"
#include <atomic>
#include <string>
#include <windows.h>

// Handler for Self-Host Engine feature
CommandResult HandleSelfHostEngine(const CommandContext& ctx)
{
    Win32IDE* ide = static_cast<Win32IDE*>(ctx.idePtr);

    auto init = SelfHostEngine::instance().initialize();
    if (!init.success)
    {
        return CommandResult::error(init.detail.c_str(), init.errorCode);
    }

    static thread_local std::string status;
    status = "Self-Host Engine Active\n";
    status += "- Autonomous deployment\n";
    status += "- Self-maintenance\n";
    status += "- Resource self-management\n";
    status += "- Adaptive scaling\n";
    status += "- Self-healing systems";

    if (ctx.outputFn)
        ctx.outputLine(status);
    if (ctx.isGui && ide)
    {
        MessageBoxA(nullptr, status.c_str(), "Self-Host Engine", MB_ICONINFORMATION | MB_OK);
    }
    return CommandResult::ok(status.c_str());
}

// ============================================================================
// Headless server — Win32IDE build copy of the self-hosting entry points.
// The identical implementation lives in agentic_bridge_headless.cpp for the
// RawrEngine target.  Win32IDE cannot include that file (forbidden by the
// strict-agentic-reality gate), so the definitions are provided here.
// ============================================================================
namespace
{
std::atomic<bool> g_shHeadlessRunning{false};
HANDLE g_shHeadlessThread = nullptr;
uint16_t g_shHeadlessPort = 0;
HANDLE g_shHeadlessReadyEvent = nullptr;

DWORD WINAPI SelfHostServerThread(LPVOID param)
{
    const uint16_t port = *reinterpret_cast<uint16_t*>(param);
    delete reinterpret_cast<uint16_t*>(param);

    RawrXD::Agent::ToolRegistry::Instance().RegisterBuiltinMasmTools();

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        g_shHeadlessRunning.store(false);
        return 1;
    }

    const SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
        WSACleanup();
        g_shHeadlessRunning.store(false);
        return 1;
    }

    const int yes = 1;
    setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(listenSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 || listen(listenSock, SOMAXCONN) != 0)
    {
        closesocket(listenSock);
        WSACleanup();
        g_shHeadlessRunning.store(false);
        return 2;
    }

    if (g_shHeadlessReadyEvent)
    {
        SetEvent(g_shHeadlessReadyEvent);
    }

    while (g_shHeadlessRunning.load())
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listenSock, &rfds);
        timeval tv{0, 100000};
        if (select(0, &rfds, nullptr, nullptr, &tv) <= 0)
            continue;

        const SOCKET client = accept(listenSock, nullptr, nullptr);
        if (client == INVALID_SOCKET)
            continue;

        char buf[8192]{};
        const int n = recv(client, buf, sizeof(buf) - 1, 0);
        if (n > 0)
        {
            std::string body(buf, static_cast<size_t>(n));
            const size_t sep = body.find("\r\n\r\n");
            std::string output;
            if (sep != std::string::npos)
            {
                const std::string json = body.substr(sep + 4);
                // Extract "tool" string field
                auto extractStr = [&](const std::string& key) -> std::string
                {
                    const std::string needle = "\"" + key + "\":\"";
                    const size_t p = json.find(needle);
                    if (p == std::string::npos)
                        return {};
                    const size_t s = p + needle.size();
                    const size_t e = json.find('"', s);
                    return e == std::string::npos ? std::string{} : json.substr(s, e - s);
                };
                const std::string toolName = extractStr("tool");
                const size_t argsKey = json.find("\"args\":");
                const std::string toolArgs = argsKey != std::string::npos ? json.substr(argsKey + 7) : "{}";
                RawrXD::Agent::ToolRegistry::Instance().Execute(toolName, toolArgs, output);
            }
            else
            {
                output = R"({"error":"malformed request"})";
            }
            const std::string resp = "HTTP/1.1 200 OK\r\n"
                                     "Content-Type: application/json\r\n"
                                     "Content-Length: " +
                                     std::to_string(output.size()) +
                                     "\r\n"
                                     "Connection: close\r\n\r\n" +
                                     output;
            send(client, resp.c_str(), static_cast<int>(resp.size()), 0);
        }
        closesocket(client);
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}
}  // namespace

extern "C" bool RawrXD_StartHeadlessServer(uint16_t port)
{
    if (g_shHeadlessRunning.load())
        return true;
    if (port == 0)
        port = 51700;

    g_shHeadlessPort = port;
    g_shHeadlessRunning.store(true);

    const std::string evtName = "RawrXD_HeadlessReady_" + std::to_string(port);
    g_shHeadlessReadyEvent = CreateEventA(nullptr, TRUE, FALSE, evtName.c_str());

    auto* p = new uint16_t{port};
    g_shHeadlessThread = CreateThread(nullptr, 0, SelfHostServerThread, p, 0, nullptr);
    if (!g_shHeadlessThread)
    {
        g_shHeadlessRunning.store(false);
        return false;
    }
    if (g_shHeadlessReadyEvent)
        WaitForSingleObject(g_shHeadlessReadyEvent, 5000);
    return true;
}

extern "C" void RawrXD_StopHeadlessServer()
{
    g_shHeadlessRunning.store(false);
    if (g_shHeadlessThread)
    {
        WaitForSingleObject(g_shHeadlessThread, 10000);
        CloseHandle(g_shHeadlessThread);
        g_shHeadlessThread = nullptr;
    }
    if (g_shHeadlessReadyEvent)
    {
        CloseHandle(g_shHeadlessReadyEvent);
        g_shHeadlessReadyEvent = nullptr;
    }
    g_shHeadlessPort = 0;
}
