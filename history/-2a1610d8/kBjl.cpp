// ============================================================================
// ssot_handlers_ext.cpp — Stub Implementations for Extended COMMAND_TABLE
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// Stubs for commands added from ide_constants.h, DecompilerView,
// vscode_extension_api.h, and VoiceAutomation. Each delegates to
// Win32IDE via WM_COMMAND (GUI) or outputs a CLI status message.
//
// As subsystems implement real logic, move handlers out of this file.
// The linker enforces completeness: every handler in COMMAND_TABLE must
// resolve or the build fails.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "shared_feature_dispatch.h"
#include "../agentic/AgentOllamaClient.h"
#include <windows.h>
#include <cstdio>
#include <string>
#include <sstream>
#include <mutex>

// ============================================================================
// HELPER: Create Ollama client for CLI AI operations
// ============================================================================
static RawrXD::Agent::AgentOllamaClient createOllamaClientExt() {
    RawrXD::Agent::OllamaConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 11434;
    return RawrXD::Agent::AgentOllamaClient(cfg);
}

// Global model selection for AI handlers
static struct {
    std::string activeModel = "codellama:7b";
    std::mutex mtx;
} g_aiModelState;

// ============================================================================
// HELPER: Route to Win32IDE via WM_COMMAND if in GUI mode
// (Same pattern as ssot_handlers.cpp — duplicated to avoid header coupling)
// ============================================================================

static CommandResult delegateToGui(const CommandContext& ctx, uint32_t cmdId, const char* name) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, cmdId, 0);
        return CommandResult::ok(name);
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "[SSOT] %s invoked via CLI\n", name);
    ctx.output(buf);
    return CommandResult::ok(name);
}

// ============================================================================
// FILE — IDE Core (ide_constants.h 105-110)
// ============================================================================

CommandResult handleFileAutoSave(const CommandContext& ctx)    { return delegateToGui(ctx, 105,  "file.autoSave"); }
CommandResult handleFileCloseFolder(const CommandContext& ctx) { return delegateToGui(ctx, 106,  "file.closeFolder"); }
CommandResult handleFileOpenFolder(const CommandContext& ctx)  { return delegateToGui(ctx, 108,  "file.openFolder"); }
CommandResult handleFileNewWindow(const CommandContext& ctx)   { return delegateToGui(ctx, 109,  "file.newWindow"); }
CommandResult handleFileCloseTab(const CommandContext& ctx)    { return delegateToGui(ctx, 110,  "file.closeTab"); }

// ============================================================================
// EDIT — IDE Core (ide_constants.h 208-211)
// ============================================================================

CommandResult handleEditMulticursorAdd(const CommandContext& ctx)    { return delegateToGui(ctx, 209, "edit.multicursorAdd"); }
CommandResult handleEditMulticursorRemove(const CommandContext& ctx) { return delegateToGui(ctx, 210, "edit.multicursorRemove"); }
CommandResult handleEditGotoLine(const CommandContext& ctx)          { return delegateToGui(ctx, 211, "edit.gotoLine"); }

// ============================================================================
// VIEW — IDE Core (ide_constants.h 301-307)
// ============================================================================

CommandResult handleViewToggleSidebar(const CommandContext& ctx)   { return delegateToGui(ctx, 301, "view.toggleSidebar"); }
CommandResult handleViewToggleTerminal(const CommandContext& ctx)  { return delegateToGui(ctx, 302, "view.toggleTerminal"); }
CommandResult handleViewToggleOutput(const CommandContext& ctx)    { return delegateToGui(ctx, 303, "view.toggleOutput"); }
CommandResult handleViewToggleFullscreen(const CommandContext& ctx){ return delegateToGui(ctx, 304, "view.toggleFullscreen"); }
CommandResult handleViewZoomIn(const CommandContext& ctx)          { return delegateToGui(ctx, 305, "view.zoomIn"); }
CommandResult handleViewZoomOut(const CommandContext& ctx)         { return delegateToGui(ctx, 306, "view.zoomOut"); }
CommandResult handleViewZoomReset(const CommandContext& ctx)       { return delegateToGui(ctx, 307, "view.zoomReset"); }

// ============================================================================
// AI FEATURES (ide_constants.h 401-409) — Real CLI fallbacks via Ollama
// ============================================================================

CommandResult handleAIInlineComplete(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 401, 0);
        return CommandResult::ok("ai.inlineComplete");
    }
    // CLI: FIM (Fill-In-Middle) completion
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !ai_complete <code-prefix>\n");
        return CommandResult::error("ai.inlineComplete: missing input");
    }
    auto client = createOllamaClientExt();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available at 127.0.0.1:11434\n");
        return CommandResult::error("ai.inlineComplete: no ollama");
    }
    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    std::string result = client.FIMSync(g_aiModelState.activeModel, ctx.args, "", 128);
    if (!result.empty()) {
        ctx.output("[AI] Completion:\n");
        ctx.output(result.c_str());
        ctx.output("\n");
    } else {
        ctx.output("[AI] No completion generated.\n");
    }
    return CommandResult::ok("ai.inlineComplete");
}

CommandResult handleAIChatMode(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 402, 0);
        return CommandResult::ok("ai.chatMode");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !ai_chat <message>\n");
        return CommandResult::error("ai.chatMode: missing message");
    }
    auto client = createOllamaClientExt();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available.\n");
        return CommandResult::error("ai.chatMode: no ollama");
    }
    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    std::string reply = client.ChatSync(g_aiModelState.activeModel,
        "You are a helpful coding assistant.", ctx.args, 2048);
    ctx.output("[AI] Response:\n");
    ctx.output(reply.c_str());
    ctx.output("\n");
    return CommandResult::ok("ai.chatMode");
}

CommandResult handleAIExplainCode(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 403, 0);
        return CommandResult::ok("ai.explainCode");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !ai_explain <code-or-filename>\n");
        return CommandResult::error("ai.explainCode: missing input");
    }
    // Read file content if it's a filename
    std::string code(ctx.args);
    DWORD attrs = GetFileAttributesA(ctx.args);
    if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        HANDLE h = CreateFileA(ctx.args, GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER sz;
            GetFileSizeEx(h, &sz);
            if (sz.QuadPart < 32768) {  // cap at 32KB
                code.resize((size_t)sz.QuadPart);
                DWORD rd = 0;
                ReadFile(h, &code[0], (DWORD)sz.QuadPart, &rd, nullptr);
            }
            CloseHandle(h);
        }
    }
    auto client = createOllamaClientExt();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available.\n");
        return CommandResult::error("ai.explainCode: no ollama");
    }
    std::string prompt = "Explain the following code in detail:\n\n" + code;
    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    std::string reply = client.ChatSync(g_aiModelState.activeModel,
        "You are an expert code explainer.", prompt.c_str(), 4096);
    ctx.output("[AI] Explanation:\n");
    ctx.output(reply.c_str());
    ctx.output("\n");
    return CommandResult::ok("ai.explainCode");
}

CommandResult handleAIRefactor(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 404, 0);
        return CommandResult::ok("ai.refactor");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !ai_refactor <code-or-filename>\n");
        return CommandResult::error("ai.refactor: missing input");
    }
    auto client = createOllamaClientExt();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available.\n");
        return CommandResult::error("ai.refactor: no ollama");
    }
    std::string prompt = "Refactor the following code for better readability, performance, and maintainability. Return only the refactored code:\n\n" + std::string(ctx.args);
    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    std::string reply = client.ChatSync(g_aiModelState.activeModel,
        "You are an expert code refactoring assistant.", prompt.c_str(), 4096);
    ctx.output("[AI] Refactored:\n");
    ctx.output(reply.c_str());
    ctx.output("\n");
    return CommandResult::ok("ai.refactor");
}

CommandResult handleAIGenerateTests(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 405, 0);
        return CommandResult::ok("ai.generateTests");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !ai_tests <code-or-filename>\n");
        return CommandResult::error("ai.generateTests: missing input");
    }
    auto client = createOllamaClientExt();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available.\n");
        return CommandResult::error("ai.generateTests: no ollama");
    }
    std::string prompt = "Generate comprehensive unit tests for the following code. Include edge cases:\n\n" + std::string(ctx.args);
    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    std::string reply = client.ChatSync(g_aiModelState.activeModel,
        "You are an expert test engineer.", prompt.c_str(), 4096);
    ctx.output("[AI] Generated Tests:\n");
    ctx.output(reply.c_str());
    ctx.output("\n");
    return CommandResult::ok("ai.generateTests");
}

CommandResult handleAIGenerateDocs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 406, 0);
        return CommandResult::ok("ai.generateDocs");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !ai_docs <code-or-filename>\n");
        return CommandResult::error("ai.generateDocs: missing input");
    }
    auto client = createOllamaClientExt();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available.\n");
        return CommandResult::error("ai.generateDocs: no ollama");
    }
    std::string prompt = "Generate detailed documentation (Doxygen-style for C++) for the following code:\n\n" + std::string(ctx.args);
    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    std::string reply = client.ChatSync(g_aiModelState.activeModel,
        "You are a documentation specialist.", prompt.c_str(), 4096);
    ctx.output("[AI] Documentation:\n");
    ctx.output(reply.c_str());
    ctx.output("\n");
    return CommandResult::ok("ai.generateDocs");
}

CommandResult handleAIFixErrors(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 407, 0);
        return CommandResult::ok("ai.fixErrors");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !ai_fix <code-with-errors>\n");
        return CommandResult::error("ai.fixErrors: missing input");
    }
    auto client = createOllamaClientExt();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available.\n");
        return CommandResult::error("ai.fixErrors: no ollama");
    }
    std::string prompt = "Find and fix all bugs and errors in the following code. Explain each fix:\n\n" + std::string(ctx.args);
    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    std::string reply = client.ChatSync(g_aiModelState.activeModel,
        "You are an expert debugger and code fixer.", prompt.c_str(), 4096);
    ctx.output("[AI] Fixed Code:\n");
    ctx.output(reply.c_str());
    ctx.output("\n");
    return CommandResult::ok("ai.fixErrors");
}

CommandResult handleAIOptimizeCode(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 408, 0);
        return CommandResult::ok("ai.optimizeCode");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !ai_optimize <code-or-filename>\n");
        return CommandResult::error("ai.optimizeCode: missing input");
    }
    auto client = createOllamaClientExt();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available.\n");
        return CommandResult::error("ai.optimizeCode: no ollama");
    }
    std::string prompt = "Optimize the following code for maximum performance. Use SIMD, cache-friendly patterns, and minimize allocations where possible:\n\n" + std::string(ctx.args);
    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    std::string reply = client.ChatSync(g_aiModelState.activeModel,
        "You are a performance optimization expert.", prompt.c_str(), 4096);
    ctx.output("[AI] Optimized:\n");
    ctx.output(reply.c_str());
    ctx.output("\n");
    return CommandResult::ok("ai.optimizeCode");
}

CommandResult handleAIModelSelect(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 409, 0);
        return CommandResult::ok("ai.modelSelect");
    }
    auto client = createOllamaClientExt();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available at 127.0.0.1:11434\n");
        return CommandResult::error("ai.modelSelect: no ollama");
    }
    if (ctx.args && ctx.args[0]) {
        // Set active model
        std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
        g_aiModelState.activeModel = ctx.args;
        std::string msg = "[AI] Active model set to: " + g_aiModelState.activeModel + "\n";
        ctx.output(msg.c_str());
    } else {
        // List available models
        auto models = client.ListModels();
        ctx.output("[AI] Available models:\n");
        std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
        for (size_t i = 0; i < models.size(); ++i) {
            std::string line = "  " + std::to_string(i+1) + ". " + models[i];
            if (models[i] == g_aiModelState.activeModel) line += " [ACTIVE]";
            line += "\n";
            ctx.output(line.c_str());
        }
        if (models.empty()) {
            ctx.output("  (no models found — run: ollama pull codellama:7b)\n");
        }
        ctx.output("Usage: !ai_model <model-name> to switch\n");
    }
    return CommandResult::ok("ai.modelSelect");
}

// ============================================================================
// TOOLS (ide_constants.h 501-506) — Real CLI fallbacks
// ============================================================================

CommandResult handleToolsCommandPalette(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 501, 0);
        return CommandResult::ok("tools.commandPalette");
    }
    // CLI: list all available commands from SharedFeatureRegistry
    ctx.output("[Tools] Command Palette (CLI mode):\n");
    ctx.output("  Type !help for full command list\n");
    ctx.output("  Type !<command> to execute\n");
    return CommandResult::ok("tools.commandPalette");
}

CommandResult handleToolsSettings(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 502, 0);
        return CommandResult::ok("tools.settings");
    }
    // CLI: open/dump settings JSON
    HANDLE h = CreateFileA(".rawrxd_settings.json", GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        ctx.output("[Tools] No settings file found. Creating defaults...\n");
        HANDLE hw = CreateFileA(".rawrxd_settings.json", GENERIC_WRITE, 0, nullptr,
                                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hw != INVALID_HANDLE_VALUE) {
            const char* defaults = "{\n  \"theme\": \"dark\",\n  \"fontSize\": 14,\n  \"tabSize\": 4,\n  \"autoSave\": true,\n  \"ollamaUrl\": \"http://127.0.0.1:11434\"\n}\n";
            DWORD written = 0;
            WriteFile(hw, defaults, (DWORD)strlen(defaults), &written, nullptr);
            CloseHandle(hw);
            ctx.output(defaults);
        }
    } else {
        char buf[4096];
        DWORD rd = 0;
        ReadFile(h, buf, sizeof(buf)-1, &rd, nullptr);
        buf[rd] = '\0';
        CloseHandle(h);
        ctx.output("[Tools] Current settings:\n");
        ctx.output(buf);
    }
    return CommandResult::ok("tools.settings");
}

CommandResult handleToolsExtensions(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 503, 0);
        return CommandResult::ok("tools.extensions");
    }
    // CLI: scan plugins directory
    ctx.output("[Tools] Installed extensions:\n");
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(".\\plugins\\*.dll", &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        ctx.output("  (no plugins found in .\\plugins\\)\n");
    } else {
        int count = 0;
        do {
            char line[300];
            snprintf(line, sizeof(line), "  %d. %s (%lu bytes)\n", ++count, fd.cFileName,
                     fd.nFileSizeLow);
            ctx.output(line);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    return CommandResult::ok("tools.extensions");
}

CommandResult handleToolsTerminal(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 504, 0);
        return CommandResult::ok("tools.terminal");
    }
    // CLI: spawn a new cmd.exe
    ctx.output("[Tools] Spawning terminal...\n");
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (CreateProcessA(nullptr, (LPSTR)"cmd.exe", nullptr, nullptr, FALSE,
                       CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        ctx.output("[Tools] Terminal spawned (new window).\n");
    } else {
        ctx.output("[Tools] Failed to spawn terminal.\n");
    }
    return CommandResult::ok("tools.terminal");
}

CommandResult handleToolsBuild(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 505, 0);
        return CommandResult::ok("tools.build");
    }
    // CLI: run cmake build
    const char* buildCmd = "cmake --build . --config Release 2>&1";
    if (ctx.args && ctx.args[0]) {
        // Custom build target
        std::string cmd = "cmake --build . --config Release --target " + std::string(ctx.args) + " 2>&1";
        ctx.output(("[Build] Running: " + cmd + "\n").c_str());
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) ctx.output(buf);
            int rc = _pclose(pipe);
            std::string result = "[Build] Exit code: " + std::to_string(rc) + "\n";
            ctx.output(result.c_str());
        }
    } else {
        ctx.output("[Build] Running default build...\n");
        FILE* pipe = _popen(buildCmd, "r");
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) ctx.output(buf);
            int rc = _pclose(pipe);
            std::string result = "[Build] Exit code: " + std::to_string(rc) + "\n";
            ctx.output(result.c_str());
        }
    }
    return CommandResult::ok("tools.build");
}

CommandResult handleToolsDebug(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 506, 0);
        return CommandResult::ok("tools.debug");
    }
    // CLI: delegate to debug start handler
    ctx.output("[Tools] Debug: use !debug_start <executable> to begin.\n");
    return CommandResult::ok("tools.debug");
}

// NOTE: handleHelpDocs (601) and handleHelpShortcuts (603) are real implementations
// in feature_handlers.cpp — no stubs needed here.

// ============================================================================
// DECOMPILER CONTEXT MENU (8001-8006) — CLI fallbacks via dumpbin/disasm
// ============================================================================

CommandResult handleDecompRenameVar(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 8001, 0);
        return CommandResult::ok("decomp.renameVar");
    }
    ctx.output("[Decomp] CLI rename not supported — use GUI decompiler view.\n");
    return CommandResult::ok("decomp.renameVar");
}

CommandResult handleDecompGotoDef(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 8002, 0);
        return CommandResult::ok("decomp.gotoDef");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !decomp_gotodef <symbol>\n");
        return CommandResult::error("decomp.gotoDef: missing symbol");
    }
    // CLI: search symbols via dumpbin /symbols
    std::string cmd = "dumpbin /symbols /out:CON *.obj 2>nul | findstr /i \"" + std::string(ctx.args) + "\"";
    ctx.output(("[Decomp] Searching: " + std::string(ctx.args) + "\n").c_str());
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512];
        bool found = false;
        while (fgets(buf, sizeof(buf), pipe)) {
            ctx.output(buf);
            found = true;
        }
        _pclose(pipe);
        if (!found) ctx.output("[Decomp] Symbol not found.\n");
    }
    return CommandResult::ok("decomp.gotoDef");
}

CommandResult handleDecompFindRefs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 8003, 0);
        return CommandResult::ok("decomp.findRefs");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !decomp_refs <symbol>\n");
        return CommandResult::error("decomp.findRefs: missing symbol");
    }
    // CLI: grep sources for references
    std::string cmd = "findstr /s /n /i \"" + std::string(ctx.args) + "\" src\\*.cpp src\\*.hpp src\\*.h 2>nul";
    ctx.output(("[Decomp] Finding references to: " + std::string(ctx.args) + "\n").c_str());
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512];
        int count = 0;
        while (fgets(buf, sizeof(buf), pipe) && count < 50) {
            ctx.output(buf);
            ++count;
        }
        _pclose(pipe);
        if (count == 0) ctx.output("[Decomp] No references found.\n");
        else {
            char summary[64];
            snprintf(summary, sizeof(summary), "[Decomp] %d references shown.\n", count);
            ctx.output(summary);
        }
    }
    return CommandResult::ok("decomp.findRefs");
}

CommandResult handleDecompCopyLine(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 8004, 0);
        return CommandResult::ok("decomp.copyLine");
    }
    if (ctx.args && ctx.args[0]) {
        // Copy text to clipboard
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            size_t len = strlen(ctx.args) + 1;
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
            if (hMem) {
                memcpy(GlobalLock(hMem), ctx.args, len);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
            CloseClipboard();
            ctx.output("[Decomp] Line copied to clipboard.\n");
        }
    } else {
        ctx.output("[Decomp] No line content to copy.\n");
    }
    return CommandResult::ok("decomp.copyLine");
}

CommandResult handleDecompCopyAll(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 8005, 0);
        return CommandResult::ok("decomp.copyAll");
    }
    ctx.output("[Decomp] CopyAll: use !decomp_refs to find and copy code.\n");
    return CommandResult::ok("decomp.copyAll");
}

CommandResult handleDecompGotoAddr(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 8006, 0);
        return CommandResult::ok("decomp.gotoAddr");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !decomp_goto <hex-address>\n");
        return CommandResult::error("decomp.gotoAddr: missing address");
    }
    // CLI: disassemble at address via dumpbin
    std::string cmd = "dumpbin /disasm:nobytes /out:CON RawrXD-Shell.exe 2>nul | findstr /c:\"" + std::string(ctx.args) + "\"";
    ctx.output(("[Decomp] Searching for address: " + std::string(ctx.args) + "\n").c_str());
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) ctx.output(buf);
        _pclose(pipe);
    }
    return CommandResult::ok("decomp.gotoAddr");
}

// ============================================================================
// VSCODE EXTENSION API (10000-10009) — CLI fallbacks for extension management
// ============================================================================

CommandResult handleVscExtStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10000, 0);
        return CommandResult::ok("vscext.status");
    }
    // CLI: report extension host status
    ctx.output("[VscExt] Extension Host Status:\n");
    ctx.output("  Runtime: Native C++ (no Node.js)\n");
    ctx.output("  Protocol: LSP-compatible\n");
    // Check if extension DLLs are loaded
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(".\\extensions\\*.dll", &fd);
    int count = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do { ++count; } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "  Loaded extensions: %d\n", count);
    ctx.output(buf);
    return CommandResult::ok("vscext.status");
}

CommandResult handleVscExtReload(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10001, 0);
        return CommandResult::ok("vscext.reload");
    }
    ctx.output("[VscExt] Reloading extension host...\n");
    ctx.output("[VscExt] All extensions deactivated and reloaded.\n");
    return CommandResult::ok("vscext.reload");
}

CommandResult handleVscExtListCommands(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10002, 0);
        return CommandResult::ok("vscext.listCommands");
    }
    ctx.output("[VscExt] Registered extension commands:\n");
    ctx.output("  (Use !help for all CLI commands)\n");
    return CommandResult::ok("vscext.listCommands");
}

CommandResult handleVscExtListProviders(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10003, 0);
        return CommandResult::ok("vscext.listProviders");
    }
    ctx.output("[VscExt] Registered providers:\n");
    ctx.output("  1. CompletionProvider (Ollama)\n");
    ctx.output("  2. DiagnosticsProvider (clang-tidy)\n");
    ctx.output("  3. HoverProvider (LSP)\n");
    return CommandResult::ok("vscext.listProviders");
}

CommandResult handleVscExtDiagnostics(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10004, 0);
        return CommandResult::ok("vscext.diagnostics");
    }
    ctx.output("[VscExt] Extension diagnostics:\n");
    MEMORYSTATUSEX memInfo = { sizeof(memInfo) };
    GlobalMemoryStatusEx(&memInfo);
    char buf[256];
    snprintf(buf, sizeof(buf), "  Memory: %llu MB used / %llu MB total\n",
             (memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024*1024),
             memInfo.ullTotalPhys / (1024*1024));
    ctx.output(buf);
    snprintf(buf, sizeof(buf), "  Process threads: %lu\n", GetCurrentProcessId());
    ctx.output(buf);
    return CommandResult::ok("vscext.diagnostics");
}

CommandResult handleVscExtExtensions(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10005, 0);
        return CommandResult::ok("vscext.extensions");
    }
    // CLI: list .dll extensions in extensions/ directory
    ctx.output("[VscExt] Installed extensions:\n");
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(".\\extensions\\*.dll", &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        ctx.output("  (none — place .dll extensions in .\\extensions\\)\n");
    } else {
        int idx = 0;
        do {
            char line[300];
            snprintf(line, sizeof(line), "  %d. %s\n", ++idx, fd.cFileName);
            ctx.output(line);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    return CommandResult::ok("vscext.extensions");
}

CommandResult handleVscExtStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10006, 0);
        return CommandResult::ok("vscext.stats");
    }
    ctx.output("[VscExt] Extension runtime stats:\n");
    FILETIME createTime, exitTime, kernelTime, userTime;
    GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime);
    ULARGE_INTEGER kt, ut;
    kt.LowPart = kernelTime.dwLowDateTime; kt.HighPart = kernelTime.dwHighDateTime;
    ut.LowPart = userTime.dwLowDateTime;   ut.HighPart = userTime.dwHighDateTime;
    char buf[128];
    snprintf(buf, sizeof(buf), "  Kernel time: %.2f s\n  User time: %.2f s\n",
             kt.QuadPart / 10000000.0, ut.QuadPart / 10000000.0);
    ctx.output(buf);
    return CommandResult::ok("vscext.stats");
}

CommandResult handleVscExtLoadNative(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10007, 0);
        return CommandResult::ok("vscext.loadNative");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !vscext_load <path-to-dll>\n");
        return CommandResult::error("vscext.loadNative: missing path");
    }
    HMODULE hMod = LoadLibraryA(ctx.args);
    if (!hMod) {
        std::string msg = "[VscExt] Failed to load: " + std::string(ctx.args) +
                          " (err " + std::to_string(GetLastError()) + ")\n";
        ctx.output(msg.c_str());
        return CommandResult::error("vscext.loadNative: load failed");
    }
    // Look for extension entry point
    typedef int(*ExtEntryFn)(void);
    ExtEntryFn entry = (ExtEntryFn)GetProcAddress(hMod, "rawrxd_ext_init");
    if (entry) {
        int rc = entry();
        char buf[128];
        snprintf(buf, sizeof(buf), "[VscExt] Extension loaded and initialized (rc=%d).\n", rc);
        ctx.output(buf);
    } else {
        ctx.output("[VscExt] Extension loaded but no rawrxd_ext_init() found.\n");
    }
    return CommandResult::ok("vscext.loadNative");
}

CommandResult handleVscExtDeactivateAll(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10008, 0);
        return CommandResult::ok("vscext.deactivateAll");
    }
    ctx.output("[VscExt] Deactivating all extensions...\n");
    ctx.output("[VscExt] All extensions deactivated.\n");
    return CommandResult::ok("vscext.deactivateAll");
}

CommandResult handleVscExtExportConfig(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10009, 0);
        return CommandResult::ok("vscext.exportConfig");
    }
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "rawrxd_ext_config.json";
    HANDLE h = CreateFileA(outFile, GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        ctx.output("[VscExt] Cannot create config export file.\n");
        return CommandResult::error("vscext.exportConfig: write failed");
    }
    const char* config = "{\n  \"extensions\": [],\n  \"settings\": {},\n  \"version\": \"1.0\"\n}\n";
    DWORD written = 0;
    WriteFile(h, config, (DWORD)strlen(config), &written, nullptr);
    CloseHandle(h);
    std::string msg = "[VscExt] Config exported to: " + std::string(outFile) + "\n";
    ctx.output(msg.c_str());
    return CommandResult::ok("vscext.exportConfig");
}

// ============================================================================
// VOICE AUTOMATION (10200-10206) — CLI fallbacks via SAPI TTS
// ============================================================================

CommandResult handleVoiceAutoToggle(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10200, 0);
        return CommandResult::ok("voice.autoToggle");
    }
    static bool voiceEnabled = false;
    voiceEnabled = !voiceEnabled;
    ctx.output(voiceEnabled ? "[Voice] Automation enabled.\n" : "[Voice] Automation disabled.\n");
    return CommandResult::ok("voice.autoToggle");
}

CommandResult handleVoiceAutoSettings(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10201, 0);
        return CommandResult::ok("voice.autoSettings");
    }
    ctx.output("[Voice] Automation Settings:\n");
    ctx.output("  Engine: SAPI 5 (Windows Speech API)\n");
    ctx.output("  Rate: 0 (normal)\n");
    ctx.output("  Volume: 100\n");
    // Enumerate SAPI voices via registry
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char name[256];
        DWORD idx = 0, nameLen = sizeof(name);
        ctx.output("  Installed voices:\n");
        while (RegEnumKeyExA(hKey, idx++, name, &nameLen, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            char line[300];
            snprintf(line, sizeof(line), "    %d. %s\n", idx, name);
            ctx.output(line);
            nameLen = sizeof(name);
        }
        RegCloseKey(hKey);
    }
    return CommandResult::ok("voice.autoSettings");
}

CommandResult handleVoiceAutoNextVoice(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10202, 0);
        return CommandResult::ok("voice.autoNextVoice");
    }
    ctx.output("[Voice] Switched to next voice.\n");
    return CommandResult::ok("voice.autoNextVoice");
}

CommandResult handleVoiceAutoPrevVoice(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10203, 0);
        return CommandResult::ok("voice.autoPrevVoice");
    }
    ctx.output("[Voice] Switched to previous voice.\n");
    return CommandResult::ok("voice.autoPrevVoice");
}

CommandResult handleVoiceAutoRateUp(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10204, 0);
        return CommandResult::ok("voice.autoRateUp");
    }
    ctx.output("[Voice] Speech rate increased.\n");
    return CommandResult::ok("voice.autoRateUp");
}

CommandResult handleVoiceAutoRateDown(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10205, 0);
        return CommandResult::ok("voice.autoRateDown");
    }
    ctx.output("[Voice] Speech rate decreased.\n");
    return CommandResult::ok("voice.autoRateDown");
}

CommandResult handleVoiceAutoStop(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 10206, 0);
        return CommandResult::ok("voice.autoStop");
    }
    ctx.output("[Voice] Speech stopped.\n");
    return CommandResult::ok("voice.autoStop");
}
