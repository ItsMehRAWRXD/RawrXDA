// mcp_model_smoke_test.cpp
// Smoke test: exercises read_file and list_dir MCP tools, then optionally
// routes a prompt through the IDE's embedded inference server (port 11435)
// or external Ollama to verify the full model→tool→result loop.
//
// Build target: MCP-ModelSmoke
// Usage: MCP-ModelSmoke.exe [--model NAME] [--port PORT]

#include <windows.h>
#include <winhttp.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "runtime/SovereignMCPBridge.h"

using RawrXD::Runtime::SovereignMCPBridge;
using nlohmann::json;

#pragma comment(lib, "winhttp.lib")

// ───────────────────── helpers ─────────────────────
static int g_pass = 0, g_fail = 0;

static void check(const char* label, bool ok, const char* detail = nullptr)
{
    if (ok) { ++g_pass; printf("[PASS] %s", label); }
    else    { ++g_fail; printf("[FAIL] %s", label); }
    if (detail) printf(" — %s", detail);
    putchar('\n');
}

static std::string findMCPServer()
{
    // Prefer build-ninja, then build
    const char* candidates[] = {
        "d:\\rawrxd\\build-ninja\\bin\\RawrXD-MCPServer.exe",
        "d:\\rawrxd\\build\\bin\\RawrXD-MCPServer.exe",
    };
    for (auto* c : candidates)
    {
        if (GetFileAttributesA(c) != INVALID_FILE_ATTRIBUTES)
            return c;
    }
    return {};
}

// ───────────────────── MCP tool tests ─────────────────────
static bool testMCPTools()
{
    auto path = findMCPServer();
    if (path.empty()) { check("mcp.server_found", false, "no MCPServer binary"); return false; }

    SovereignMCPBridge& bridge = SovereignMCPBridge::instance();
    bool spawned = bridge.spawnServer(path);
    check("mcp.spawn", spawned, path.c_str());
    if (!spawned) return false;

    // tools/list — should have 3 tools now
    // listTools() returns the "result" object directly (not the full envelope)
    auto toolsResp = bridge.listTools();
    bool hasTools = toolsResp.contains("tools");
    int toolCount = 0;
    bool hasReadFile = false, hasListDir = false, hasEcho = false;
    if (hasTools)
    {
        auto& arr = toolsResp["tools"];
        toolCount = static_cast<int>(arr.size());
        for (size_t i = 0; i < arr.size(); ++i)
        {
            std::string n = arr[i].value("name", "");
            if (n == "read_file") hasReadFile = true;
            if (n == "list_dir")  hasListDir  = true;
            if (n == "echo")      hasEcho     = true;
        }
    }
    check("mcp.tools_count", toolCount == 3,
          (std::to_string(toolCount) + " tools").c_str());
    check("mcp.has_read_file", hasReadFile);
    check("mcp.has_list_dir",  hasListDir);
    check("mcp.has_echo",      hasEcho);

    // callTool() also returns the "result" object directly
    // ─── read_file: read the MCP server's own ASM source ───
    {
        nlohmann::json args;
        args["path"] = "d:\\rawrxd\\src\\asm\\RawrXD_MCPServer.asm";
        auto resp = bridge.callTool("read_file", args, 5000);
        bool ok = resp.contains("content");
        std::string snippet;
        if (ok)
        {
            auto& content = resp["content"];
            if (content.size() > 0)
                snippet = content[size_t(0)].value("text", "");
        }
        bool hasAsm = snippet.find("RawrXD_MCPServer") != std::string::npos;
        check("mcp.read_file", hasAsm,
              hasAsm ? ("got " + std::to_string(snippet.size()) + " bytes").c_str()
                     : ("snippet=" + snippet.substr(0, 80)).c_str());
    }

    // ─── list_dir: list the asm source directory ───
    {
        nlohmann::json args;
        args["path"] = "d:\\rawrxd\\src\\asm";
        auto resp = bridge.callTool("list_dir", args, 5000);
        bool ok = resp.contains("content");
        std::string listing;
        if (ok)
        {
            auto& content = resp["content"];
            if (content.size() > 0)
                listing = content[size_t(0)].value("text", "");
        }
        bool hasMCP = listing.find(".asm") != std::string::npos;
        check("mcp.list_dir", hasMCP,
              hasMCP ? ("listing=" + listing.substr(0, 120)).c_str() : "no listing returned");
    }

    // ─── echo (regression) ───
    {
        nlohmann::json args;
        args["text"] = "smoke-ok";
        auto resp = bridge.callTool("echo", args, 3000);
        bool ok = resp.contains("content");
        std::string text;
        if (ok)
        {
            auto& content = resp["content"];
            if (content.size() > 0)
                text = content[size_t(0)].value("text", "");
        }
        check("mcp.echo", text == "smoke-ok");
    }

    bridge.shutdown();
    return g_fail == 0;
}

// ───────────────────── Ollama model test ─────────────────────
// Sends a tool-augmented prompt to Ollama, checks if model emits a tool_call,
// then dispatches it through MCP and shows the result.

static int g_serverPort = 11435;  // default: IDE embedded server

static std::string ollamaGenerate(const std::string& model,
                                  const std::string& prompt)
{
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Smoke/1.0",
                                     WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return {};

    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1",
                                        (INTERNET_PORT)g_serverPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return {}; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                                            L"/api/generate",
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {}; }

    // Set generous timeout for model inference (120s)
    DWORD timeout = 120000;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    // Build JSON body
    std::string body = "{\"model\":\"" + model + "\","
                       "\"prompt\":" + nlohmann::json(prompt).dump() + ","
                       "\"stream\":false}";

    BOOL sent = WinHttpSendRequest(hRequest,
                                   L"Content-Type: application/json\r\n",
                                   -1L,
                                   (LPVOID)body.c_str(),
                                   (DWORD)body.size(),
                                   (DWORD)body.size(), 0);
    if (!sent) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {}; }
    if (!WinHttpReceiveResponse(hRequest, NULL)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {}; }

    std::string result;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0)
    {
        std::vector<char> buf(bytesAvailable);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
        result.append(buf.data(), bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Extract "response" field from JSON
    try {
        auto parsed = nlohmann::json::parse(result);
        if (parsed.is_object() && parsed.contains("response"))
            return parsed["response"].get<std::string>();
    } catch (...) {}
    return result;
}

static bool isOllamaRunning()
{
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Smoke/1.0",
                                     WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", (INTERNET_PORT)g_serverPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/tags",
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    // Set a 5-second timeout so we don't block forever
    DWORD timeout = 5000;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS,
                                   0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    bool ok = false;
    if (sent && WinHttpReceiveResponse(hRequest, NULL))
    {
        DWORD statusCode = 0, size = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,
                            &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
        ok = (statusCode == 200);
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

static std::string getFirstModel()
{
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Smoke/1.0",
                                     WINHTTP_ACCESS_TYPE_NO_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return {};
    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", (INTERNET_PORT)g_serverPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return {}; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/api/tags",
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {}; }

    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    WinHttpReceiveResponse(hRequest, NULL);

    std::string result;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0)
    {
        std::vector<char> buf(bytesAvailable);
        DWORD bytesRead = 0;
        WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
        result.append(buf.data(), bytesRead);
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    auto parsed = nlohmann::json::parse(result);
    if (parsed.is_object() && parsed.contains("models"))
    {
        auto& models = parsed["models"];
        if (models.size() > 0)
            return models[size_t(0)].value("name", "");
    }
    return {};
}

static void testWithModel(const std::string& modelOverride)
{
    if (!isOllamaRunning())
    {
        printf("\n[SKIP] Inference server not running on port %d — model tests skipped\n", g_serverPort);
        return;
    }

    std::string model = modelOverride;
    if (model.empty())
        model = getFirstModel();
    if (model.empty())
    {
        printf("\n[SKIP] No Ollama models found — model tests skipped\n");
        return;
    }

    printf("\n=== Model Smoke Test (model: %s) ===\n", model.c_str());

    // Spawn MCP server
    auto path = findMCPServer();
    auto& bridge = SovereignMCPBridge::instance();
    bool spawned = bridge.spawnServer(path);
    check("model.mcp_spawn", spawned);
    if (!spawned) return;

    // Build prompt with available tools
    std::string prompt =
        "You are a coding assistant with access to these tools:\n"
        "1. read_file(path) — Read file contents at the given absolute path.\n"
        "2. list_dir(path)  — List files in the given directory.\n"
        "3. echo(text)      — Echo back text.\n\n"
        "When you need to use a tool, output EXACTLY this JSON format on its own line:\n"
        "{\"tool_call\":{\"name\":\"TOOL_NAME\",\"arguments\":{\"PARAM\":\"VALUE\"}}}\n\n"
        "Task: List the files in d:\\rawrxd\\src\\asm and tell me what assembly files are there.\n"
        "Start by calling the list_dir tool.\n";

    printf("[MODEL] Sending prompt to %s...\n", model.c_str());
    std::string response = ollamaGenerate(model, prompt);

    if (response.empty())
    {
        check("model.inference", false, "empty response from Ollama");
        bridge.shutdown();
        return;
    }

    printf("[MODEL] Response (%zu chars):\n%.500s\n", response.size(),
           response.c_str());
    check("model.inference", true,
          (std::to_string(response.size()) + " chars").c_str());

    // Try to find a tool_call in the response
    auto tcPos = response.find("\"tool_call\"");
    if (tcPos == std::string::npos)
        tcPos = response.find("\"name\":");

    if (tcPos != std::string::npos)
    {
        // Find the enclosing JSON object
        auto braceStart = response.rfind('{', tcPos);
        if (braceStart != std::string::npos)
        {
            // Find the matching close brace (simple depth counter)
            int depth = 0;
            size_t braceEnd = braceStart;
            for (size_t i = braceStart; i < response.size(); ++i)
            {
                if (response[i] == '{') ++depth;
                else if (response[i] == '}') { --depth; if (depth == 0) { braceEnd = i + 1; break; } }
            }

            std::string toolJson = response.substr(braceStart, braceEnd - braceStart);
            printf("[MODEL] Extracted tool call: %s\n", toolJson.c_str());

            auto parsed = nlohmann::json::parse(toolJson);
            std::string toolName, toolArg;

            // Handle nested {"tool_call":{"name":...}} or direct {"name":...}
            nlohmann::json tcObj;
            if (parsed.contains("tool_call"))
                tcObj = parsed["tool_call"];
            else
                tcObj = parsed;

            if (tcObj.contains("name"))
                toolName = tcObj["name"].get<std::string>();
            if (tcObj.contains("arguments"))
            {
                auto& args = tcObj["arguments"];
                if (args.contains("path"))
                    toolArg = args["path"].get<std::string>();
                else if (args.contains("text"))
                    toolArg = args["text"].get<std::string>();
            }

            check("model.tool_detected", !toolName.empty(), toolName.c_str());

            if (!toolName.empty())
            {
                printf("[MODEL] Dispatching %s via MCP...\n", toolName.c_str());

                nlohmann::json callArgs;
                if (toolName == "list_dir" || toolName == "read_file")
                    callArgs["path"] = toolArg;
                else
                    callArgs["text"] = toolArg;

                auto toolResp = bridge.callTool(toolName, callArgs, 5000);
                bool hasResult = toolResp.contains("content");
                std::string resultText;
                if (hasResult)
                {
                    auto& content = toolResp["content"];
                    if (content.size() > 0)
                        resultText = content[size_t(0)].value("text", "");
                }

                check("model.tool_executed", hasResult && !resultText.empty(),
                      (std::to_string(resultText.size()) + " bytes returned").c_str());

                if (!resultText.empty())
                    printf("[MODEL] Tool result (first 300 chars):\n%.300s\n", resultText.c_str());
            }
        }
    }
    else
    {
        printf("[MODEL] No tool_call found in response (model may not support tool format)\n");
        check("model.tool_detected", false, "model did not emit tool_call JSON");
    }

    bridge.shutdown();
}

// ───────────────────── main ─────────────────────
int main(int argc, char** argv)
{
    printf("=== RawrXD MCP + Model Smoke Test ===\n\n");

    // Parse args
    std::string modelOverride;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc)
            modelOverride = argv[++i];
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
            g_serverPort = atoi(argv[++i]);
    }
    printf("Using inference server on port %d\n", g_serverPort);

    // Phase 1: Direct MCP tool tests
    printf("--- Phase 1: Direct MCP Tool Tests ---\n");
    testMCPTools();

    // Phase 2: Model integration test (optional)
    printf("\n--- Phase 2: Model Integration Test ---\n");
    testWithModel(modelOverride);

    // Summary
    printf("\n=== SMOKE RESULT: %d passed, %d failed ===\n",
           g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
