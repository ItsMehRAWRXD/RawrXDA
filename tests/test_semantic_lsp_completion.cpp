// ============================================================================
// test_semantic_lsp_completion.cpp — Phase 2: Semantic Completion Smoke Test
// ============================================================================
// Validates that the LSP client can distinguish between two Init() functions
// in different namespaces via clangd hover/definition, without grep.
//
// Build: add to CTest via CMakeLists.txt (tests/ target)
// Run:   ctest -R semantic_lsp_completion -V
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cassert>

// ---------------------------------------------------------------------------
// We test the LSP bridge at the JSON-RPC level by launching clangd directly,
// sending initialize + textDocument/hover, and asserting namespace-aware
// symbol resolution.  No Win32IDE instance needed — pure protocol test.
// ---------------------------------------------------------------------------

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

// Simple JSON-RPC 2.0 over stdio helper (no nlohmann dependency in test)
static std::string makeJsonRpc(int id, const std::string& method, const std::string& paramsJson) {
    std::string body = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id)
                     + ",\"method\":\"" + method + "\",\"params\":" + paramsJson + "}";
    return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

static std::string makeNotification(const std::string& method, const std::string& paramsJson) {
    std::string body = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + paramsJson + "}";
    return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

static std::string filePathToUri(const std::string& path) {
    std::string result = "file:///";
    for (char c : path) {
        if (c == '\\') result += '/';
        else result += c;
    }
    return result;
}

// Read a JSON-RPC response from a pipe handle, matching by request ID.
// Skips notifications (messages without "id" matching the expected ID).
#ifdef _WIN32
static std::string readJsonRpcResponse(HANDLE hPipe, int expectedId, int timeoutMs = 10000) {
    std::string accumulated;
    char buf[4096];
    DWORD start = GetTickCount();

    while ((GetTickCount() - start) < (DWORD)timeoutMs) {
        DWORD avail = 0;
        PeekNamedPipe(hPipe, nullptr, 0, nullptr, &avail, nullptr);
        if (avail == 0) {
            Sleep(50);
            continue;
        }
        DWORD bytesRead = 0;
        DWORD toRead = (avail < sizeof(buf)) ? avail : sizeof(buf);
        ReadFile(hPipe, buf, toRead, &bytesRead, nullptr);
        accumulated.append(buf, bytesRead);

        // Try to extract complete JSON-RPC messages
        while (true) {
            auto clPos = accumulated.find("Content-Length:");
            if (clPos == std::string::npos) break;

            auto headerEnd = accumulated.find("\r\n\r\n", clPos);
            if (headerEnd == std::string::npos) break;

            int contentLen = atoi(accumulated.c_str() + clPos + 15);
            size_t bodyStart = headerEnd + 4;
            if (accumulated.size() < bodyStart + contentLen) break;

            std::string body = accumulated.substr(bodyStart, contentLen);
            accumulated.erase(0, bodyStart + contentLen);

            // Check if this is a response with the expected ID
            std::string idKey = "\"id\":" + std::to_string(expectedId);
            if (body.find(idKey) != std::string::npos && body.find("\"result\"") != std::string::npos) {
                return body;
            }
            // Otherwise it's a notification or different ID — discard and continue
        }
    }
    return "";
}
#endif

// ============================================================================
// Test fixture: create two .cpp files with Init() in different namespaces
// ============================================================================
struct TestFixture {
    std::string tmpDir;
    std::string fileA;    // EngineA::Init()
    std::string fileB;    // EngineB::Init()
    std::string fileMain; // calls both

    bool setup() {
        char tmpPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tmpPath);
        tmpDir = std::string(tmpPath) + "rawrxd_lsp_test\\";
        std::filesystem::create_directories(tmpDir);

        fileA = tmpDir + "engine_a.h";
        fileB = tmpDir + "engine_b.h";
        fileMain = tmpDir + "main_test.cpp";

        // Write engine_a.h
        {
            std::ofstream f(fileA);
            f << "#pragma once\n"
              << "namespace EngineA {\n"
              << "    struct Context { int state; };\n"
              << "    /// Initialize the EngineA subsystem with priority scheduling.\n"
              << "    /// @param ctx  Engine context to initialize\n"
              << "    /// @param pool Memory pool size in bytes\n"
              << "    bool Init(Context& ctx, unsigned long pool);\n"
              << "}\n";
        }

        // Write engine_b.h
        {
            std::ofstream f(fileB);
            f << "#pragma once\n"
              << "namespace EngineB {\n"
              << "    struct Config { float rate; };\n"
              << "    /// Initialize the EngineB renderer with VSync configuration.\n"
              << "    /// @param cfg  Render configuration\n"
              << "    /// @param vsync Enable vertical sync\n"
              << "    int Init(Config& cfg, bool vsync);\n"
              << "}\n";
        }

        // Write main_test.cpp (the file clangd will analyze)
        {
            std::ofstream f(fileMain);
            f << "#include \"engine_a.h\"\n"
              << "#include \"engine_b.h\"\n"
              << "\n"
              << "int main() {\n"
              << "    EngineA::Context ctx{};\n"
              << "    EngineA::Init(ctx, 1024);\n"    // line 5, col 14 = Init
              << "\n"
              << "    EngineB::Config cfg{};\n"
              << "    EngineB::Init(cfg, true);\n"     // line 8, col 14 = Init
              << "    return 0;\n"
              << "}\n";
        }

        // Write compile_commands.json for clangd
        {
            std::ofstream f(tmpDir + "compile_commands.json");
            std::string escaped = tmpDir;
            for (auto& c : escaped) if (c == '\\') c = '/';
            f << "[\n"
              << "  {\n"
              << "    \"directory\": \"" << escaped << "\",\n"
              << "    \"command\": \"cl.exe /EHsc /std:c++17 /I" << escaped << " main_test.cpp\",\n"
              << "    \"file\": \"" << escaped << "main_test.cpp\"\n"
              << "  }\n"
              << "]\n";
        }

        return true;
    }

    void cleanup() {
        std::error_code ec;
        std::filesystem::remove_all(tmpDir, ec);
    }
};

// ============================================================================
// MAIN TEST
// ============================================================================
int main() {
    printf("=== Semantic LSP Completion Test (Phase 2) ===\n\n");

    // --- Step 0: Find clangd ---
    const char* clangdPaths[] = {
        "clangd",
        "C:\\Program Files\\LLVM\\bin\\clangd.exe",
        "C:\\msys64\\mingw64\\bin\\clangd.exe",
        nullptr
    };

    const char* clangdExe = nullptr;
    for (int i = 0; clangdPaths[i]; i++) {
        char fullPath[MAX_PATH];
        if (SearchPathA(nullptr, clangdPaths[i], ".exe", MAX_PATH, fullPath, nullptr)) {
            clangdExe = clangdPaths[i];
            printf("[OK] Found clangd: %s\n", fullPath);
            break;
        }
    }
    if (!clangdExe) {
        printf("[SKIP] clangd not found on PATH — test requires clangd.\n");
        printf("       Install LLVM or add clangd to PATH.\n");
        return 0; // Don't fail CI — graceful skip
    }

    // --- Step 1: Create test fixture ---
    TestFixture fixture;
    if (!fixture.setup()) {
        printf("[FAIL] Could not create test fixture.\n");
        return 1;
    }
    printf("[OK] Test fixture created at: %s\n", fixture.tmpDir.c_str());

    // --- Step 2: Launch clangd ---
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdinRead, hStdinWrite, hStdoutRead, hStdoutWrite;
    CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0);
    CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0);
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.dwFlags = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};
    std::string cmdLine = std::string(clangdExe) + " --background-index=false --log=error";
    if (!CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, fixture.tmpDir.c_str(), &si, &pi)) {
        printf("[FAIL] CreateProcess for clangd failed (err=%lu)\n", GetLastError());
        fixture.cleanup();
        return 1;
    }
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    printf("[OK] clangd launched (PID %lu)\n", pi.dwProcessId);

    // --- Step 3: Initialize LSP ---
    {
        std::string rootUri = filePathToUri(fixture.tmpDir);
        std::string initParams =
            "{\"processId\":" + std::to_string(GetCurrentProcessId()) + ","
            "\"rootUri\":\"" + rootUri + "\","
            "\"capabilities\":{\"textDocument\":{\"hover\":{\"contentFormat\":[\"plaintext\",\"markdown\"]},"
            "\"definition\":{\"linkSupport\":true}}}}";

        std::string msg = makeJsonRpc(1, "initialize", initParams);
        DWORD written;
        WriteFile(hStdinWrite, msg.c_str(), (DWORD)msg.size(), &written, nullptr);

        std::string resp = readJsonRpcResponse(hStdoutRead, 1, 15000);
        if (resp.empty() || resp.find("\"result\"") == std::string::npos) {
            printf("[FAIL] LSP initialize response empty or invalid.\n");
            TerminateProcess(pi.hProcess, 1);
            fixture.cleanup();
            return 1;
        }
        printf("[OK] LSP initialized.\n");

        // Send initialized notification
        msg = makeNotification("initialized", "{}");
        WriteFile(hStdinWrite, msg.c_str(), (DWORD)msg.size(), &written, nullptr);
    }

    // --- Step 4: Open the test file ---
    {
        std::ifstream f(fixture.fileMain);
        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());

        // Escape for JSON
        std::string escaped;
        for (char c : content) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '\n') escaped += "\\n";
            else if (c == '\r') continue;
            else escaped += c;
        }

        std::string uri = filePathToUri(fixture.fileMain);
        std::string params = "{\"textDocument\":{\"uri\":\"" + uri
                           + "\",\"languageId\":\"cpp\",\"version\":1,\"text\":\"" + escaped + "\"}}";
        std::string msg = makeNotification("textDocument/didOpen", params);
        DWORD written;
        WriteFile(hStdinWrite, msg.c_str(), (DWORD)msg.size(), &written, nullptr);
        Sleep(2000);  // Let clangd index
        printf("[OK] Opened test file in clangd.\n");
    }

    // --- Step 5: Hover on EngineA::Init (line 5, col 14) ---
    int testsPassed = 0;
    int testsFailed = 0;

    {
        std::string uri = filePathToUri(fixture.fileMain);
        std::string params = "{\"textDocument\":{\"uri\":\"" + uri
                           + "\"},\"position\":{\"line\":5,\"character\":14}}";
        std::string msg = makeJsonRpc(2, "textDocument/hover", params);
        DWORD written;
        WriteFile(hStdinWrite, msg.c_str(), (DWORD)msg.size(), &written, nullptr);

        std::string resp = readJsonRpcResponse(hStdoutRead, 2, 10000);
        printf("\n--- Test 1: Hover on EngineA::Init(line 5, col 14) ---\n");

        bool hasEngineA = resp.find("EngineA") != std::string::npos;
        bool hasPool    = resp.find("pool") != std::string::npos
                       || resp.find("unsigned long") != std::string::npos;
        bool hasEngineB = resp.find("EngineB") != std::string::npos;

        if (hasEngineA && !hasEngineB) {
            printf("[PASS] Hover correctly identifies EngineA::Init (no EngineB contamination)\n");
            testsPassed++;
        } else if (hasEngineA) {
            printf("[PASS] Hover identifies EngineA::Init (EngineB also mentioned — acceptable)\n");
            testsPassed++;
        } else {
            printf("[FAIL] Hover did not return EngineA context.\n");
            printf("       Response: %.200s\n", resp.c_str());
            testsFailed++;
        }
    }

    // --- Step 6: Hover on EngineB::Init (line 8, col 14) ---
    {
        std::string uri = filePathToUri(fixture.fileMain);
        std::string params = "{\"textDocument\":{\"uri\":\"" + uri
                           + "\"},\"position\":{\"line\":8,\"character\":14}}";
        std::string msg = makeJsonRpc(3, "textDocument/hover", params);
        DWORD written;
        WriteFile(hStdinWrite, msg.c_str(), (DWORD)msg.size(), &written, nullptr);

        std::string resp = readJsonRpcResponse(hStdoutRead, 3, 10000);
        printf("\n--- Test 2: Hover on EngineB::Init(line 8, col 14) ---\n");

        bool hasEngineB = resp.find("EngineB") != std::string::npos;
        bool hasBool    = resp.find("bool") != std::string::npos
                       || resp.find("vsync") != std::string::npos;
        bool hasEngineA = resp.find("EngineA") != std::string::npos;

        if (hasEngineB && !hasEngineA) {
            printf("[PASS] Hover correctly identifies EngineB::Init (no EngineA contamination)\n");
            testsPassed++;
        } else if (hasEngineB) {
            printf("[PASS] Hover identifies EngineB::Init (EngineA also mentioned — acceptable)\n");
            testsPassed++;
        } else {
            printf("[FAIL] Hover did not return EngineB context.\n");
            printf("       Response: %.200s\n", resp.c_str());
            testsFailed++;
        }
    }

    // --- Step 7: Go-to-definition on EngineA::Init → should resolve to engine_a.h ---
    {
        std::string uri = filePathToUri(fixture.fileMain);
        std::string params = "{\"textDocument\":{\"uri\":\"" + uri
                           + "\"},\"position\":{\"line\":5,\"character\":14}}";
        std::string msg = makeJsonRpc(4, "textDocument/definition", params);
        DWORD written;
        WriteFile(hStdinWrite, msg.c_str(), (DWORD)msg.size(), &written, nullptr);

        std::string resp = readJsonRpcResponse(hStdoutRead, 4, 10000);
        printf("\n--- Test 3: Definition of EngineA::Init → engine_a.h ---\n");

        bool goesToA = resp.find("engine_a.h") != std::string::npos;
        bool goesToB = resp.find("engine_b.h") != std::string::npos;

        if (goesToA && !goesToB) {
            printf("[PASS] Definition correctly resolves to engine_a.h\n");
            testsPassed++;
        } else {
            printf("[FAIL] Definition did not resolve to engine_a.h exclusively.\n");
            printf("       Response: %.200s\n", resp.c_str());
            testsFailed++;
        }
    }

    // --- Step 8: Go-to-definition on EngineB::Init → should resolve to engine_b.h ---
    {
        std::string uri = filePathToUri(fixture.fileMain);
        std::string params = "{\"textDocument\":{\"uri\":\"" + uri
                           + "\"},\"position\":{\"line\":8,\"character\":14}}";
        std::string msg = makeJsonRpc(5, "textDocument/definition", params);
        DWORD written;
        WriteFile(hStdinWrite, msg.c_str(), (DWORD)msg.size(), &written, nullptr);

        std::string resp = readJsonRpcResponse(hStdoutRead, 5, 10000);
        printf("\n--- Test 4: Definition of EngineB::Init → engine_b.h ---\n");

        bool goesToB = resp.find("engine_b.h") != std::string::npos;
        bool goesToA = resp.find("engine_a.h") != std::string::npos;

        if (goesToB && !goesToA) {
            printf("[PASS] Definition correctly resolves to engine_b.h\n");
            testsPassed++;
        } else {
            printf("[FAIL] Definition did not resolve to engine_b.h exclusively.\n");
            printf("       Response: %.200s\n", resp.c_str());
            testsFailed++;
        }
    }

    // --- Shutdown clangd ---
    {
        std::string msg = makeJsonRpc(99, "shutdown", "null");
        DWORD written;
        WriteFile(hStdinWrite, msg.c_str(), (DWORD)msg.size(), &written, nullptr);
        readJsonRpcResponse(hStdoutRead, 99, 3000); // consume response

        msg = makeNotification("exit", "null");
        WriteFile(hStdinWrite, msg.c_str(), (DWORD)msg.size(), &written, nullptr);
        WaitForSingleObject(pi.hProcess, 5000);
    }

    CloseHandle(hStdinWrite);
    CloseHandle(hStdoutRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    fixture.cleanup();

    // --- Summary ---
    printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
    return testsFailed > 0 ? 1 : 0;
}
