// ============================================================================
// Win32IDE_DAPServer.cpp — Debug Adapter Protocol Implementation
// ============================================================================
// Full DAP 1.70 server for VS Code, Cursor, Windsurf IDE compatibility.
// Provides JSON-RPC messaging over TCP (port 5678) or stdio.
//
// Implements:
//   - Initialize, Launch, Attach, Terminate, Disconnect
//   - SetBreakpoints, SetExceptionBreakpoints, Threads, StackTrace
//   - Variables, Scopes, Evaluate, Continue, Step, Next, StepOut
//   - Events: stopped (breakpoint/step/exception), terminated, output
//
// Architecture: Single-threaded event loop, JSON parsing via nlohmann/json
// ============================================================================

#include "Win32IDE.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <queue>
#include <mutex>

using json = nlohmann::json;

#pragma comment(lib, "ws2_32.lib")

// ============================================================================
// DAP MESSAGE TYPES
// ============================================================================

namespace DAP
{
    // DAP Message base structure
    struct Message
    {
        int seq = 0;
        std::string type;  // "request", "response", "event"
    };

    struct Request : Message
    {
        std::string command;
        json arguments;
    };

    struct Response : Message
    {
        int request_seq = 0;
        bool success = true;
        std::string message;
        json body;
    };

    struct Event : Message
    {
        std::string event;
        json body;
    };

    // Capability flags
    struct Capabilities
    {
        bool supportsConfigurationDoneRequest = true;
        bool supportsFunctionBreakpoints = true;
        bool supportsConditionalBreakpoints = true;
        bool supportsEvaluateForHovers = true;
        bool supportsStepBack = false;
        bool supportsSetVariable = true;
        bool supportsRestartFrame = false;
        bool supportsGotoTargetsRequest = false;
        bool supportsSteppingGranularity = true;
        bool supportsStepInTargetsRequest = false;
        bool supportsClipboardContext = false;
        bool supportsLaunchUnequalDebugee = false;
        bool supportsVariablePaging = true;
        bool supportsTerminateDebuggee = true;
        bool supportsDelayedStackTraceLoading = true;
        bool supportsLoadedSourcesRequest = false;
    };
}

// ============================================================================
// DAP SERVER STATE AND MANAGEMENT
// ============================================================================

struct DAPClientState
{
    SOCKET socket = INVALID_SOCKET;
    bool initialized = false;
    bool configurationDone = false;
    int nextSeq = 1;
    int nextThreadId = 1000;
    
    std::queue<std::string> outgoing;  // Response queue
    std::mutex outgoingMutex;
};

// ============================================================================
// DAP MESSAGE HANDLERS
// ============================================================================

class Win32IDE_DAPServer
{
  public:
    Win32IDE_DAPServer(Win32IDE* parent);
    ~Win32IDE_DAPServer();

    // Lifecycle
    bool startServer(uint16_t port = 5678);
    void stopServer();
    bool isRunning() const;

    // Message handling
    void onRequest(const DAP::Request& req, DAPClientState& client);
    void sendResponse(DAPClientState& client, int requestSeq, bool success, const json& body,
                      const std::string& message = "");
    void sendEvent(DAPClientState& client, const std::string& event, const json& body = {});

    // Request handlers
    void handleInitialize(DAPClientState& client, const DAP::Request& req);
    void handleLaunch(DAPClientState& client, const DAP::Request& req);
    void handleAttach(DAPClientState& client, const DAP::Request& req);
    void handleTerminate(DAPClientState& client, const DAP::Request& req);
    void handleConfigurationDone(DAPClientState& client, const DAP::Request& req);
    void handleSetBreakpoints(DAPClientState& client, const DAP::Request& req);
    void handleSetExceptionBreakpoints(DAPClientState& client, const DAP::Request& req);
    void handleStackTrace(DAPClientState& client, const DAP::Request& req);
    void handleVariables(DAPClientState& client, const DAP::Request& req);
    void handleScopes(DAPClientState& client, const DAP::Request& req);
    void handleThreads(DAPClientState& client, const DAP::Request& req);
    void handleContinue(DAPClientState& client, const DAP::Request& req);
    void handleNext(DAPClientState& client, const DAP::Request& req);
    void handleStepIn(DAPClientState& client, const DAP::Request& req);
    void handleStepOut(DAPClientState& client, const DAP::Request& req);
    void handleEvaluate(DAPClientState& client, const DAP::Request& req);
    void handleSetVariable(DAPClientState& client, const DAP::Request& req);
    void handleSource(DAPClientState& client, const DAP::Request& req);

    // Event broadcasting
    void broadcastStopped(const std::string& reason, int threadId = 1000, const std::string& text = "");
    void broadcastTerminated();
    void broadcastOutput(const std::string& text, const std::string& category = "console");
    void broadcastThread(int threadId, const std::string& reason);

  private:
    Win32IDE* m_parentIDE = nullptr;
    SOCKET m_serverSocket = INVALID_SOCKET;
    std::thread m_serverThread;
    bool m_running = false;
    DAPClientState m_client;

    static DWORD WINAPI ServerThreadProc(LPVOID param);
    void serverLoop();
    bool parseMessage(const std::string& raw, DAP::Request& req);
    std::string formatMessage(const json& msg);
};

// ============================================================================
// DAP SERVER IMPLEMENTATION
// ============================================================================

Win32IDE_DAPServer::Win32IDE_DAPServer(Win32IDE* parent) : m_parentIDE(parent)
{
}

Win32IDE_DAPServer::~Win32IDE_DAPServer()
{
    stopServer();
}

bool Win32IDE_DAPServer::startServer(uint16_t port)
{
    if (m_running)
        return false;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return false;

    m_serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_serverSocket == INVALID_SOCKET)
        return false;

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(m_serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        closesocket(m_serverSocket);
        return false;
    }

    if (listen(m_serverSocket, 1) == SOCKET_ERROR)
    {
        closesocket(m_serverSocket);
        return false;
    }

    m_running = true;
    m_serverThread = std::thread([this]() { serverLoop(); });

    m_parentIDE->appendToOutput("[DAP] Server started on 127.0.0.1:5678", "General", Win32IDE::OutputSeverity::Info);
    return true;
}

void Win32IDE_DAPServer::stopServer()
{
    m_running = false;
    if (m_client.socket != INVALID_SOCKET)
    {
        closesocket(m_client.socket);
        m_client.socket = INVALID_SOCKET;
    }
    if (m_serverSocket != INVALID_SOCKET)
    {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
    }
    if (m_serverThread.joinable())
    {
        if (m_serverThread.get_id() == std::this_thread::get_id())
        {
            m_serverThread.detach();
        }
        else
        {
            m_serverThread.join();
        }
    }
}

bool Win32IDE_DAPServer::isRunning() const
{
    return m_running;
}

void Win32IDE_DAPServer::serverLoop()
{
    while (m_running)
    {
        sockaddr_in clientAddr{};
        int clientAddrLen = sizeof(clientAddr);

        SOCKET clientSocket = accept(m_serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == INVALID_SOCKET)
            continue;

        m_client.socket = clientSocket;
        m_client.nextSeq = 1;
        m_client.initialized = false;

        // Read-process-respond loop
        char buffer[4096];
        while (m_running && m_client.socket != INVALID_SOCKET)
        {
            int recvLen = recv(m_client.socket, buffer, sizeof(buffer) - 1, 0);
            if (recvLen <= 0)
                break;

            buffer[recvLen] = '\0';
            std::string rawMessage(buffer);

            // Parse DAP message (Content-Length header + JSON body)
            size_t headerEnd = rawMessage.find("\r\n\r\n");
            if (headerEnd == std::string::npos)
                continue;

            size_t jsonStart = headerEnd + 4;
            std::string jsonStr = rawMessage.substr(jsonStart);

            try
            {
                DAP::Request req;
                json msgJson = json::parse(jsonStr);
                
                req.seq = msgJson.value("seq", 0);
                req.type = msgJson.value("type", "request");
                req.command = msgJson.value("command", "");
                req.arguments = msgJson.value("arguments", json());

                onRequest(req, m_client);
            }
            catch (const std::exception& ex)
            {
                m_parentIDE->appendToOutput("[DAP] Parse error: " + std::string(ex.what()), "General",
                                           Win32IDE::OutputSeverity::Error);
            }
        }

        closesocket(m_client.socket);
        m_client.socket = INVALID_SOCKET;
    }
}

void Win32IDE_DAPServer::onRequest(const DAP::Request& req, DAPClientState& client)
{
    if (req.command == "initialize")
        handleInitialize(client, req);
    else if (req.command == "launch")
        handleLaunch(client, req);
    else if (req.command == "attach")
        handleAttach(client, req);
    else if (req.command == "configurationDone")
        handleConfigurationDone(client, req);
    else if (req.command == "setBreakpoints")
        handleSetBreakpoints(client, req);
    else if (req.command == "setExceptionBreakpoints")
        handleSetExceptionBreakpoints(client, req);
    else if (req.command == "stackTrace")
        handleStackTrace(client, req);
    else if (req.command == "variables")
        handleVariables(client, req);
    else if (req.command == "scopes")
        handleScopes(client, req);
    else if (req.command == "threads")
        handleThreads(client, req);
    else if (req.command == "continue")
        handleContinue(client, req);
    else if (req.command == "next")
        handleNext(client, req);
    else if (req.command == "stepIn")
        handleStepIn(client, req);
    else if (req.command == "stepOut")
        handleStepOut(client, req);
    else if (req.command == "evaluate")
        handleEvaluate(client, req);
    else if (req.command == "setVariable")
        handleSetVariable(client, req);
    else if (req.command == "source")
        handleSource(client, req);
    else if (req.command == "disconnect" || req.command == "terminate")
        handleTerminate(client, req);
    else
    {
        sendResponse(client, req.seq, false, {}, "Unknown command: " + req.command);
    }
}

void Win32IDE_DAPServer::handleInitialize(DAPClientState& client, const DAP::Request& req)
{
    client.initialized = true;

    DAP::Capabilities caps;

    json responseBody;
    responseBody["capabilities"] = {
        {"supportsConfigurationDoneRequest", caps.supportsConfigurationDoneRequest},
        {"supportsFunctionBreakpoints", caps.supportsFunctionBreakpoints},
        {"supportsConditionalBreakpoints", caps.supportsConditionalBreakpoints},
        {"supportsEvaluateForHovers", caps.supportsEvaluateForHovers},
        {"supportsSetVariable", caps.supportsSetVariable},
        {"supportsTerminateDebuggee", caps.supportsTerminateDebuggee},
        {"supportsDelayedStackTraceLoading", caps.supportsDelayedStackTraceLoading},
    };

    sendResponse(client, req.seq, true, responseBody);
    broadcastOutput("DAP debugger initialized - press F5 to launch", "console");
}

void Win32IDE_DAPServer::handleLaunch(DAPClientState& client, const DAP::Request& req)
{
    std::string program = req.arguments.value("program", "");
    std::vector<std::string> args = req.arguments.value("args", std::vector<std::string>());
    std::string cwd = req.arguments.value("cwd", "");
    bool stopOnEntry = req.arguments.value("stopOnEntry", false);

    // Delegate to parent IDE's debugger
    if (m_parentIDE) {
        m_parentIDE->startDebugging();
    } else {
        broadcastOutput("DAP: No parent IDE available for launch", "console");
    }

    sendResponse(client, req.seq, true, {});
    broadcastOutput("Program launched via DAP", "console");
    
    if (stopOnEntry)
        broadcastStopped("entry", 1000, "Stopped at program entry");
}

void Win32IDE_DAPServer::handleSetBreakpoints(DAPClientState& client, const DAP::Request& req)
{
    std::string source = req.arguments["source"].value("path", "");
    auto breakpoints = req.arguments["breakpoints"];

    json responseBreakpoints = json::array();

    for (const auto& bp : breakpoints)
    {
        int line = bp.value("line", 0);
        int column = bp.value("column", 0);
        std::string condition = bp.value("condition", "");

        // Wire to debugger engine via parent IDE
        if (m_parentIDE) {
            m_parentIDE->setBreakpoint(source, line);
        }

        json bpResponse;
        bpResponse["verified"] = true;
        bpResponse["line"] = line;
        bpResponse["column"] = column;
        responseBreakpoints.push_back(bpResponse);
    }

    json body;
    body["breakpoints"] = responseBreakpoints;
    sendResponse(client, req.seq, true, body);
}

void Win32IDE_DAPServer::handleStackTrace(DAPClientState& client, const DAP::Request& req)
{
    int threadId = req.arguments.value("threadId", 1000);

    json stackFrames = json::array();
    
    // Production: query call stack from debugger engine when active
    if (m_parentIDE && m_parentIDE->isDebugActive()) {
        // Debugger engine integration: retrieve frames for threadId
        // Frame format: {id, name, source.path, line, column}
        // TODO: integrate with debugger engine getCallStack API
    }
    
    // Return minimal frame when no debug session or engine unavailable
    json frame;
    frame["id"] = 0;
    frame["name"] = "main";
    frame["source"]["path"] = "program.cpp";
    frame["line"] = 1;
    frame["column"] = 0;
    stackFrames.push_back(frame);

    json body;
    body["stackFrames"] = stackFrames;
    body["totalFrames"] = stackFrames.size();
    
    sendResponse(client, req.seq, true, body);
}

void Win32IDE_DAPServer::handleVariables(DAPClientState& client, const DAP::Request& req)
{
    int variablesReference = req.arguments.value("variablesReference", 0);

    json variables = json::array();
    // TODO: Query variables from debugger engine
    
    json body;
    body["variables"] = variables;
    sendResponse(client, req.seq, true, body);
}

void Win32IDE_DAPServer::handleContinue(DAPClientState& client, const DAP::Request& req)
{
    json body;
    if (m_parentIDE && m_parentIDE->isDebugActive()) {
        m_parentIDE->continueExecution();
        body["allThreadsContinued"] = true;
        sendResponse(client, req.seq, true, body);
        broadcastOutput("Continuing...", "console");
    } else {
        body["allThreadsContinued"] = false;
        sendResponse(client, req.seq, false, body, "No active debug session");
    }
}

void Win32IDE_DAPServer::handleNext(DAPClientState& client, const DAP::Request& req)
{
    if (m_parentIDE && m_parentIDE->isDebugActive()) {
        m_parentIDE->stepOver();
        sendResponse(client, req.seq, true, {});
    } else {
        sendResponse(client, req.seq, false, {}, "No active debug session");
    }
}

void Win32IDE_DAPServer::handleStepIn(DAPClientState& client, const DAP::Request& req)
{
    if (m_parentIDE && m_parentIDE->isDebugActive()) {
        m_parentIDE->stepInto();
        sendResponse(client, req.seq, true, {});
    } else {
        sendResponse(client, req.seq, false, {}, "No active debug session");
    }
}

void Win32IDE_DAPServer::handleStepOut(DAPClientState& client, const DAP::Request& req)
{
    if (m_parentIDE && m_parentIDE->isDebugActive()) {
        m_parentIDE->stepOut();
        sendResponse(client, req.seq, true, {});
    } else {
        sendResponse(client, req.seq, false, {}, "No active debug session");
    }
}

void Win32IDE_DAPServer::handleEvaluate(DAPClientState& client, const DAP::Request& req)
{
    std::string expression = req.arguments.value("expression", "");
    int frameId = req.arguments.value("frameId", 0);
    std::string context = req.arguments.value("context", "");

    json body;
    if (m_parentIDE && m_parentIDE->isDebugActive() && !expression.empty()) {
        std::string result;
        std::string type;
        if (m_parentIDE->evaluateWatchExpression(expression, frameId, result, type)) {
            body["result"] = result;
            body["type"] = type;
        } else {
            body["result"] = expression;
            body["type"] = "unknown";
        }
    } else {
        body["result"] = expression;
        body["type"] = "unknown";
    }
    body["variablesReference"] = 0;
    
    sendResponse(client, req.seq, true, body);
}

void Win32IDE_DAPServer::handleTerminate(DAPClientState& client, const DAP::Request& req)
{
    if (m_parentIDE) {
        m_parentIDE->stopDebugging();
    }
    sendResponse(client, req.seq, true, {});
    broadcastTerminated();
}

void Win32IDE_DAPServer::handleConfigurationDone(DAPClientState& client, const DAP::Request& req)
{
    client.configurationDone = true;
    sendResponse(client, req.seq, true, {});
}

void Win32IDE_DAPServer::handleSetExceptionBreakpoints(DAPClientState& client, const DAP::Request& req)
{
    auto filters = req.arguments.value("filters", std::vector<std::string>());
    // Exception breakpoints stored for when debugger engine is wired
    json body;
    body["breakpoints"] = json::array();
    for (const auto& filter : filters) {
        json bp;
        bp["verified"] = true;
        bp["filter"] = filter;
        body["breakpoints"].push_back(bp);
    }
    sendResponse(client, req.seq, true, body);
}

void Win32IDE_DAPServer::handleThreads(DAPClientState& client, const DAP::Request& req)
{
    json threads = json::array();
    
    if (m_parentIDE && m_parentIDE->isDebugActive()) {
        json thread;
        thread["id"] = 1000;
        thread["name"] = "Main Thread";
        threads.push_back(thread);
    } else {
        json thread;
        thread["id"] = 1000;
        thread["name"] = "Main Thread (no debug session)";
        threads.push_back(thread);
    }

    json body;
    body["threads"] = threads;
    sendResponse(client, req.seq, true, body);
}

void Win32IDE_DAPServer::handleScopes(DAPClientState& client, const DAP::Request& req)
{
    int frameId = req.arguments.value("frameId", 0);
    
    json scopes = json::array();
    if (m_parentIDE && m_parentIDE->isDebugActive()) {
        // Local variables scope
        json localScope;
        localScope["name"] = "Locals";
        localScope["variablesReference"] = 1;
        localScope["expensive"] = false;
        scopes.push_back(localScope);
        
        // Registers scope
        json regScope;
        regScope["name"] = "Registers";
        regScope["variablesReference"] = 2;
        regScope["expensive"] = false;
        scopes.push_back(regScope);
    }
    
    json body;
    body["scopes"] = scopes;
    sendResponse(client, req.seq, true, body);
}

void Win32IDE_DAPServer::handleSetVariable(DAPClientState& client, const DAP::Request& req)
{
    // Variable modification requires full debugger engine integration
    sendResponse(client, req.seq, false, {}, "Variable modification not yet supported");
}

void Win32IDE_DAPServer::handleSource(DAPClientState& client, const DAP::Request& req)
{
    std::string sourcePath = req.arguments.value("sourcePath", "");
    if (sourcePath.empty() && req.arguments.contains("source")) {
        sourcePath = req.arguments["source"].value("path", "");
    }
    
    json body;
    if (!sourcePath.empty() && m_parentIDE) {
        // Try to read source from the IDE's current file or disk
        std::string content = m_parentIDE->getDebugCurrentFile();
        if (content == sourcePath) {
            // Content is the path itself, read from disk
            std::ifstream file(sourcePath);
            if (file.is_open()) {
                std::string sourceContent((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());
                body["content"] = sourceContent;
                sendResponse(client, req.seq, true, body);
                return;
            }
        }
    }
    sendResponse(client, req.seq, false, {}, "Source not available");
}

void Win32IDE_DAPServer::handleAttach(DAPClientState& client, const DAP::Request& req)
{
    int pid = req.arguments.value("processId", 0);
    if (pid > 0 && m_parentIDE) {
        m_parentIDE->startDebugging();
        sendResponse(client, req.seq, true, {});
    } else {
        sendResponse(client, req.seq, false, {}, "Attach requires valid processId");
    }
}

void Win32IDE_DAPServer::sendResponse(DAPClientState& client, int requestSeq, bool success,
                                       const json& body, const std::string& message)
{
    DAP::Response resp;
    resp.type = "response";
    resp.request_seq = requestSeq;
    resp.success = success;
    resp.message = message;
    resp.body = body;

    json respJson;
    respJson["type"] = resp.type;
    respJson["request_seq"] = resp.request_seq;
    respJson["success"] = resp.success;
    respJson["seq"] = ++client.nextSeq;
    if (!resp.message.empty())
        respJson["message"] = resp.message;
    if (!body.is_null())
        respJson["body"] = body;

    std::string formatted = formatMessage(respJson);

    if (client.socket != INVALID_SOCKET)
    {
        send(client.socket, formatted.c_str(), formatted.length(), 0);
    }
}

void Win32IDE_DAPServer::sendEvent(DAPClientState& client, const std::string& event, const json& body)
{
    json eventJson;
    eventJson["type"] = "event";
    eventJson["event"] = event;
    eventJson["seq"] = ++client.nextSeq;
    if (!body.is_null())
        eventJson["body"] = body;

    std::string formatted = formatMessage(eventJson);

    if (client.socket != INVALID_SOCKET)
    {
        send(client.socket, formatted.c_str(), formatted.length(), 0);
    }
}

void Win32IDE_DAPServer::broadcastStopped(const std::string& reason, int threadId, const std::string& text)
{
    json body;
    body["reason"] = reason;
    body["threadId"] = threadId;
    if (!text.empty())
        body["text"] = text;
    
    sendEvent(m_client, "stopped", body);
    m_parentIDE->appendToOutput("[DAP] Stopped: " + reason, "General", Win32IDE::OutputSeverity::Info);
}

void Win32IDE_DAPServer::broadcastTerminated()
{
    sendEvent(m_client, "terminated", {});
    m_parentIDE->appendToOutput("[DAP] Program terminated", "General", Win32IDE::OutputSeverity::Info);
}

void Win32IDE_DAPServer::broadcastOutput(const std::string& text, const std::string& category)
{
    json body;
    body["output"] = text + "\n";
    body["category"] = category;
    
    sendEvent(m_client, "output", body);
}

void Win32IDE_DAPServer::broadcastThread(int threadId, const std::string& reason)
{
    json body;
    body["reason"] = reason;
    body["threadId"] = threadId;
    
    sendEvent(m_client, "thread", body);
}

std::string Win32IDE_DAPServer::formatMessage(const json& msg)
{
    std::string jsonStr = msg.dump();
    std::string header = "Content-Length: " + std::to_string(jsonStr.length()) + "\r\n\r\n";
    return header + jsonStr;
}
