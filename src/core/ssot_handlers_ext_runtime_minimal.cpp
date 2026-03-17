#include "shared_feature_dispatch.h"
#include "../agentic/AgentOllamaClient.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "js_extension_host.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {

RawrXD::Agent::AgentOllamaClient createOllamaClient() {
    RawrXD::Agent::OllamaConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 11434;
    return RawrXD::Agent::AgentOllamaClient(cfg);
}

struct AIModelState {
    std::string activeModel = "codellama:7b";
    std::mutex mtx;
};

AIModelState g_aiModelState;

CommandResult delegateToGui(const CommandContext& ctx, uint32_t cmdId, const char* name) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = reinterpret_cast<HWND>(ctx.hwnd);
        if (!hwnd) hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        if (hwnd) PostMessageA(hwnd, WM_COMMAND, cmdId, 0);
        return CommandResult::ok(name);
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "[SSOT] %s invoked via CLI (ID %u)\n", name, cmdId);
    ctx.output(buf);
    return CommandResult::ok(name);
}

CommandResult runAiPrompt(const CommandContext& ctx,
                          const char* systemPrompt,
                          const std::string& userPrompt,
                          const char* usage,
                          const char* opName,
                          const char* outputLabel) {
    if (userPrompt.empty()) {
        ctx.output(usage);
        return CommandResult::error(opName);
    }

    auto client = createOllamaClient();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available at 127.0.0.1:11434\n");
        return CommandResult::error(opName);
    }

    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    std::vector<RawrXD::Agent::ChatMessage> msgs;
    msgs.push_back(RawrXD::Agent::ChatMessage{systemPrompt, ""});
    msgs.back().role = "system";
    msgs.back().content = systemPrompt;
    msgs.push_back(RawrXD::Agent::ChatMessage{"user", userPrompt});
    auto chatResult = client.ChatSync(msgs);
    const std::string reply = chatResult.success ? chatResult.response : chatResult.error_message;
    ctx.output(outputLabel);
    ctx.output(reply.c_str());
    ctx.output("\n");
    return CommandResult::ok(opName);
}

void outputf(const CommandContext& ctx, const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ctx.output(buf);
}

}  // namespace

CommandResult handleFileAutoSave(const CommandContext& ctx) {
    return delegateToGui(ctx, 105, "file.autoSave");
}

CommandResult handleAIInlineComplete(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 401, "ai.inlineComplete");
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !ai_complete <code-prefix>\n");
        return CommandResult::error("ai.inlineComplete: missing input");
    }

    auto client = createOllamaClient();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available at 127.0.0.1:11434\n");
        return CommandResult::error("ai.inlineComplete: no ollama");
    }

    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    auto fimResult = client.FIMSync(ctx.args, "");
    if (!fimResult.success || fimResult.response.empty()) {
        ctx.output("[AI] No completion generated.\n");
        return CommandResult::ok("ai.inlineComplete");
    }
    ctx.output("[AI] Completion:\n");
    ctx.output(fimResult.response.c_str());
    ctx.output("\n");
    return CommandResult::ok("ai.inlineComplete");
}

CommandResult handleAIChatMode(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 402, "ai.chatMode");
    return runAiPrompt(
        ctx,
        "You are a helpful coding assistant.",
        std::string(ctx.args ? ctx.args : ""),
        "Usage: !ai_chat <message>\n",
        "ai.chatMode",
        "[AI] Response:\n");
}

CommandResult handleAIExplainCode(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 403, "ai.explainCode");
    return runAiPrompt(
        ctx,
        "You are an expert code explainer.",
        std::string("Explain the following code in detail:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_explain <code-or-filename>\n",
        "ai.explainCode",
        "[AI] Explanation:\n");
}

CommandResult handleAIRefactor(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 404, "ai.refactor");
    return runAiPrompt(
        ctx,
        "You are an expert code refactoring assistant.",
        std::string("Refactor the following code for readability, maintainability, and performance. Return only refactored code:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_refactor <code-or-filename>\n",
        "ai.refactor",
        "[AI] Refactored:\n");
}

CommandResult handleAIGenerateTests(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 405, "ai.generateTests");
    return runAiPrompt(
        ctx,
        "You are an expert test engineer.",
        std::string("Generate comprehensive unit tests for the following code, including edge cases:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_tests <code-or-filename>\n",
        "ai.generateTests",
        "[AI] Generated Tests:\n");
}

CommandResult handleAIGenerateDocs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 406, "ai.generateDocs");
    return runAiPrompt(
        ctx,
        "You are a documentation specialist.",
        std::string("Generate detailed documentation for the following code:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_docs <code-or-filename>\n",
        "ai.generateDocs",
        "[AI] Documentation:\n");
}

CommandResult handleAIFixErrors(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 407, "ai.fixErrors");
    return runAiPrompt(
        ctx,
        "You are an expert debugger and code fixer.",
        std::string("Find and fix all bugs and errors in this code. Explain each fix:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_fix <code-with-errors>\n",
        "ai.fixErrors",
        "[AI] Fixed Code:\n");
}

CommandResult handleAIOptimizeCode(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 408, "ai.optimizeCode");
    return runAiPrompt(
        ctx,
        "You are a performance optimization expert.",
        std::string("Optimize the following code for performance and lower allocation overhead:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_optimize <code-or-filename>\n",
        "ai.optimizeCode",
        "[AI] Optimized:\n");
}

CommandResult handleAIModelSelect(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 409, "ai.modelSelect");

    auto client = createOllamaClient();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available at 127.0.0.1:11434\n");
        return CommandResult::error("ai.modelSelect: no ollama");
    }

    if (ctx.args && ctx.args[0]) {
        std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
        g_aiModelState.activeModel = ctx.args;
        outputf(ctx, "[AI] Active model set to: %s\n", g_aiModelState.activeModel.c_str());
        return CommandResult::ok("ai.modelSelect");
    }

    auto models = client.ListModels();
    ctx.output("[AI] Available models:\n");
    std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
    for (size_t i = 0; i < models.size(); ++i) {
        std::string line = "  " + std::to_string(i + 1) + ". " + models[i];
        if (models[i] == g_aiModelState.activeModel) {
            line += " [ACTIVE]";
        }
        line += "\n";
        ctx.output(line.c_str());
    }
    if (models.empty()) {
        ctx.output("  (no models found - run: ollama pull codellama:7b)\n");
    }
    ctx.output("Usage: !ai_model <model-name> to switch\n");
    return CommandResult::ok("ai.modelSelect");
}

CommandResult handleVscExtStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 10000, "vscext.status");

    ctx.output("[VscExt] Extension Host Status:\n");
    auto& host = JSExtensionHost::instance();
    outputf(ctx, "  Initialized: %s\n", host.isInitialized() ? "yes" : "no");
    if (host.isInitialized()) {
        const auto stats = host.getStats();
        outputf(ctx, "  JS extensions loaded: %llu\n", static_cast<unsigned long long>(stats.jsExtensionsLoaded));
        outputf(ctx, "  JS extensions active: %llu\n", static_cast<unsigned long long>(stats.jsExtensionsActive));
    }
    return CommandResult::ok("vscext.status");
}

CommandResult handleVscExtReload(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 10001, "vscext.reload");

    ctx.output("[VscExt] Reloading extension host...\n");
    auto& host = JSExtensionHost::instance();
    if (!host.isInitialized()) {
        const auto initResult = host.initialize();
        if (!initResult.success) {
            ctx.output("[VscExt] Initialization failed.\n");
            return CommandResult::error("vscext.reload: init failed");
        }
        ctx.output("[VscExt] Host initialized.\n");
        return CommandResult::ok("vscext.reload");
    }

    JSExtensionState states[64];
    size_t count = 0;
    host.getLoadedExtensions(states, 64, &count);
    int reloaded = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!states[i].activated) {
            continue;
        }
        host.deactivateExtension(states[i].extensionId.c_str());
        const auto act = host.activateExtension(states[i].extensionId.c_str());
        if (act.success) {
            ++reloaded;
        }
    }
    outputf(ctx, "[VscExt] Reloaded %d extensions.\n", reloaded);
    return CommandResult::ok("vscext.reload");
}

CommandResult handleVscExtListCommands(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 10002, "vscext.listCommands");
    auto& host = JSExtensionHost::instance();
    if (!host.isInitialized()) {
        ctx.output("[VscExt] Extension host not initialized. Run !vscext reload first.\n");
        return CommandResult::ok("vscext.listCommands");
    }

    ctx.output("[VscExt] Registered extension commands:\n");
    JSExtensionState states[64];
    size_t count = 0;
    host.getLoadedExtensions(states, 64, &count);
    int cmdIdx = 0;
    for (size_t i = 0; i < count; ++i) {
        for (const auto& cmd : states[i].registeredCommands) {
            outputf(ctx, "  %d. %s (from: %s)\n", ++cmdIdx, cmd.c_str(), states[i].extensionId.c_str());
        }
    }
    if (cmdIdx == 0) {
        ctx.output("  (no extension commands registered)\n");
    }
    return CommandResult::ok("vscext.listCommands");
}

CommandResult handleVscExtListProviders(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 10003, "vscext.listProviders");
    auto& host = JSExtensionHost::instance();

    ctx.output("[VscExt] Registered providers:\n");
    if (host.isInitialized()) {
        JSExtensionState states[64];
        size_t count = 0;
        host.getLoadedExtensions(states, 64, &count);
        int provIdx = 0;
        for (size_t i = 0; i < count; ++i) {
            for (const auto& prov : states[i].registeredProviders) {
                outputf(ctx, "  %d. %s (from: %s)\n", ++provIdx, prov.c_str(), states[i].extensionId.c_str());
            }
        }
        if (provIdx == 0) {
            ctx.output("  (no JS extension providers)\n");
        }
    }
    ctx.output("  Built-in native providers:\n");
    ctx.output("    CompletionProvider (Ollama)\n");
    return CommandResult::ok("vscext.listProviders");
}

CommandResult handleVscExtDiagnostics(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 10004, "vscext.diagnostics");
    ctx.output("[VscExt] Extension diagnostics:\n");
    MEMORYSTATUSEX memInfo = {sizeof(memInfo)};
    GlobalMemoryStatusEx(&memInfo);
    outputf(ctx, "  Memory: %llu MB used / %llu MB total\n",
            static_cast<unsigned long long>((memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024)),
            static_cast<unsigned long long>(memInfo.ullTotalPhys / (1024 * 1024)));
    outputf(ctx, "  Process ID: %lu\n", GetCurrentProcessId());
    return CommandResult::ok("vscext.diagnostics");
}

CommandResult handleVscExtExtensions(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 10005, "vscext.extensions");
    ctx.output("[VscExt] Loaded JS extensions:\n");
    auto& host = JSExtensionHost::instance();
    if (!host.isInitialized()) {
        ctx.output("  Extension host not initialized.\n");
        return CommandResult::ok("vscext.extensions");
    }
    JSExtensionState states[64];
    size_t count = 0;
    host.getLoadedExtensions(states, 64, &count);
    if (count == 0) {
        ctx.output("  (none)\n");
        return CommandResult::ok("vscext.extensions");
    }
    for (size_t i = 0; i < count; ++i) {
        outputf(ctx, "  %zu. %s [%s]\n", i + 1, states[i].extensionId.c_str(), states[i].activated ? "active" : "inactive");
    }
    return CommandResult::ok("vscext.extensions");
}

CommandResult handleVscExtStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 10006, "vscext.stats");
    auto& host = JSExtensionHost::instance();
    if (!host.isInitialized()) {
        ctx.output("[VscExt] Extension host not initialized.\n");
        return CommandResult::ok("vscext.stats");
    }
    const auto stats = host.getStats();
    ctx.output("[VscExt] Extension runtime stats:\n");
    outputf(ctx, "  loaded: %llu\n", static_cast<unsigned long long>(stats.jsExtensionsLoaded));
    outputf(ctx, "  active: %llu\n", static_cast<unsigned long long>(stats.jsExtensionsActive));
    outputf(ctx, "  api calls: %llu\n", static_cast<unsigned long long>(stats.totalJSApiCalls));
    outputf(ctx, "  scripts: %llu\n", static_cast<unsigned long long>(stats.totalScriptExecutions));
    outputf(ctx, "  polyfills used: %llu\n", static_cast<unsigned long long>(stats.polyfillsUsed));
    outputf(ctx, "  require calls: %llu\n", static_cast<unsigned long long>(stats.requireCalls));
    return CommandResult::ok("vscext.stats");
}

CommandResult handleVscExtLoadNative(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 10007, "vscext.loadNative");
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !vscext native <path-to-dll>\n");
        return CommandResult::error("vscext.loadNative: missing path");
    }

    HMODULE hMod = LoadLibraryA(ctx.args);
    if (!hMod) {
        outputf(ctx, "[VscExt] Failed to load: %s (err %lu)\n", ctx.args, GetLastError());
        return CommandResult::error("vscext.loadNative: load failed");
    }

    typedef int (*ExtEntryFn)(void);
    auto entry = reinterpret_cast<ExtEntryFn>(GetProcAddress(hMod, "rawrxd_ext_init"));
    if (entry) {
        const int rc = entry();
        outputf(ctx, "[VscExt] Extension loaded and initialized (rc=%d).\n", rc);
    } else {
        ctx.output("[VscExt] Extension loaded but no rawrxd_ext_init() found.\n");
    }
    return CommandResult::ok("vscext.loadNative");
}

CommandResult handleVscExtDeactivateAll(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 10008, "vscext.deactivateAll");
    auto& host = JSExtensionHost::instance();
    if (!host.isInitialized()) {
        ctx.output("[VscExt] Extension host not initialized - nothing to deactivate.\n");
        return CommandResult::ok("vscext.deactivateAll");
    }

    JSExtensionState states[64];
    size_t count = 0;
    host.getLoadedExtensions(states, 64, &count);
    int deactivated = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!states[i].activated) {
            continue;
        }
        const auto result = host.deactivateExtension(states[i].extensionId.c_str());
        if (result.success) {
            ++deactivated;
        }
    }
    outputf(ctx, "[VscExt] Deactivated %d of %zu extensions.\n", deactivated, count);
    return CommandResult::ok("vscext.deactivateAll");
}

CommandResult handleVscExtExportConfig(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 10009, "vscext.exportConfig");
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "rawrxd_ext_config.json";
    auto& host = JSExtensionHost::instance();

    HANDLE h = CreateFileA(outFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        ctx.output("[VscExt] Cannot create config export file.\n");
        return CommandResult::error("vscext.exportConfig: write failed");
    }

    std::string json = "{\n  \"extensions\": [\n";
    if (host.isInitialized()) {
        JSExtensionState states[64];
        size_t count = 0;
        host.getLoadedExtensions(states, 64, &count);
        for (size_t i = 0; i < count; ++i) {
            json += "    {\"id\":\"" + states[i].extensionId + "\",\"active\":";
            json += states[i].activated ? "true" : "false";
            json += "}";
            if (i + 1 < count) {
                json += ",";
            }
            json += "\n";
        }
    }
    json += "  ],\n  \"version\": \"1.0\"\n}\n";

    DWORD written = 0;
    WriteFile(h, json.data(), static_cast<DWORD>(json.size()), &written, nullptr);
    CloseHandle(h);
    outputf(ctx, "[VscExt] Config exported to: %s\n", outFile);
    return CommandResult::ok("vscext.exportConfig");
}
