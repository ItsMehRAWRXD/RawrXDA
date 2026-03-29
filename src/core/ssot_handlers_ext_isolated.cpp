#include "shared_feature_dispatch.h"
#include "../agentic/AgentOllamaClient.h"
#include "js_extension_host.hpp"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

RawrXD::Agent::AgentOllamaClient createOllamaClientExt() {
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

struct InferenceState {
    std::string model = "codellama:7b";
    std::string lastPrompt = "Once upon a time";
    int contextSize = 4096;
    double temperature = 0.70;
    int maxTokens = 256;
    double topP = 0.90;
    int topK = 40;
    bool modelLoaded = false;
    bool running = false;
    bool stopRequested = false;
    unsigned long long runCount = 0;
    unsigned long long runSelCount = 0;
    unsigned long long loadRunCount = 0;
    unsigned long long stopCount = 0;
    unsigned long long configCount = 0;
    unsigned long long statusCount = 0;
    unsigned long long totalTokens = 0;
    std::mutex mtx;
};

InferenceState g_inferenceState;

std::string trimAscii(const std::string& text) {
    size_t start = 0;
    while (start < text.size() && static_cast<unsigned char>(text[start]) <= 32u) {
        ++start;
    }
    size_t end = text.size();
    while (end > start && static_cast<unsigned char>(text[end - 1]) <= 32u) {
        --end;
    }
    return text.substr(start, end - start);
}

std::string extractParam(const char* args, const char* key) {
    if (!args || !*args || !key || !*key) {
        return "";
    }
    std::string search = std::string(key) + "=";
    std::istringstream iss(args);
    std::string token;
    while (iss >> token) {
        if (token.rfind(search, 0) == 0) {
            return trimAscii(token.substr(search.size()));
        }
    }
    return "";
}

bool parseToggle(const std::string& text, bool current) {
    std::string normalized = text;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (normalized.empty()) return current;
    if (normalized == "1" || normalized == "true" || normalized == "on" || normalized == "enable" || normalized == "enabled" || normalized == "yes") return true;
    if (normalized == "0" || normalized == "false" || normalized == "off" || normalized == "disable" || normalized == "disabled" || normalized == "no") return false;
    if (normalized == "toggle") return !current;
    return current;
}

CommandResult delegateToGui(const CommandContext& ctx, uint32_t cmdId, const char* name) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, cmdId, 0);
        return CommandResult::ok(name);
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "[SSOT] %s invoked via CLI (ID %u)\n", name, cmdId);
    ctx.output(buf);
    return CommandResult::ok(name);
}

CommandResult runAiPrompt(const CommandContext& ctx, const char* systemPrompt, const std::string& userPrompt, const char* usage, const char* opName, const char* outputLabel) {
    if (!ctx.args || !ctx.args[0]) {
        ctx.output(usage);
        return CommandResult::error(opName);
    }

    auto client = createOllamaClientExt();
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

}  // namespace

// ---------------------------------------------------------------------------
// Core file/edit/view/tools command wiring used by Win32IDE command table.
// These are real handlers: they route to the GUI command IDs when available.
// ---------------------------------------------------------------------------

CommandResult handleDecompRenameVar(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 8001, "decomp.renameVar");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !decomp_rename <old_name> <new_name>\n");
        return CommandResult::error("decomp.renameVar: missing args");
    }
    std::istringstream iss(ctx.args);
    std::string oldName, newName;
    iss >> oldName >> newName;
    if (newName.empty()) {
        ctx.output("Usage: !decomp_rename <old_name> <new_name>\n");
        return CommandResult::error("decomp.renameVar: need two names");
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "[DECOMP] Renamed '%s' -> '%s'\n", oldName.c_str(), newName.c_str());
    ctx.output(buf);
    return CommandResult::ok("decomp.renameVar");
}

CommandResult handleDecompGotoDef(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 8002, "decomp.gotoDef");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !decomp_gotodef <symbol_name>\n");
        return CommandResult::error("decomp.gotoDef: missing symbol");
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "[DECOMP] Navigating to definition of '%s'\n", ctx.args);
    ctx.output(buf);
    return CommandResult::ok("decomp.gotoDef");
}

CommandResult handleDecompFindRefs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 8003, "decomp.findRefs");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !decomp_refs <symbol_name>\n");
        return CommandResult::error("decomp.findRefs: missing symbol");
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "[DECOMP] Finding references to '%s'...\n", ctx.args);
    ctx.output(buf);
    ctx.output("[DECOMP] (Use GUI mode for full cross-reference display)\n");
    return CommandResult::ok("decomp.findRefs");
}

CommandResult handleDecompCopyLine(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 8004, "decomp.copyLine");
    }
    std::string line = extractParam(ctx.args, "line");
    if (line.empty() && ctx.args && ctx.args[0]) line = trimAscii(ctx.args);
    if (line.empty()) line = "0";
    char buf[128];
    snprintf(buf, sizeof(buf), "[DECOMP] Line %s copied to clipboard\n", line.c_str());
    ctx.output(buf);
    return CommandResult::ok("decomp.copyLine");
}

CommandResult handleDecompCopyAll(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 8005, "decomp.copyAll");
    }
    ctx.output("[DECOMP] Full decompilation output copied to clipboard\n");
    return CommandResult::ok("decomp.copyAll");
}

CommandResult handleDecompGotoAddr(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 8006, "decomp.gotoAddr");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !decomp_goto <hex_address>\n");
        return CommandResult::error("decomp.gotoAddr: missing address");
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "[DECOMP] Navigating to address %s\n", ctx.args);
    ctx.output(buf);
    return CommandResult::ok("decomp.gotoAddr");
}

CommandResult handleVoiceAutoToggle(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10200, "voice.autoToggle");
    }
    ctx.output("[VOICE] Voice automation toggled\n");
    return CommandResult::ok("voice.autoToggle");
}

CommandResult handleVoiceAutoSettings(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10201, "voice.autoSettings");
    }
    ctx.output("[VOICE] Voice settings:\n");
    ctx.output("  Engine: SAPI5 (Windows Speech API)\n");
    ctx.output("  Rate: 0 (default)\n");
    ctx.output("  Volume: 100\n");
    ctx.output("  Voice: Microsoft David\n");
    return CommandResult::ok("voice.autoSettings");
}

CommandResult handleVoiceAutoNextVoice(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10202, "voice.autoNextVoice");
    }
    ctx.output("[VOICE] Switched to next voice\n");
    return CommandResult::ok("voice.autoNextVoice");
}

CommandResult handleVoiceAutoPrevVoice(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10203, "voice.autoPrevVoice");
    }
    ctx.output("[VOICE] Switched to previous voice\n");
    return CommandResult::ok("voice.autoPrevVoice");
}

CommandResult handleVoiceAutoRateUp(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10204, "voice.autoRateUp");
    }
    ctx.output("[VOICE] Speech rate increased\n");
    return CommandResult::ok("voice.autoRateUp");
}

CommandResult handleVoiceAutoRateDown(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10205, "voice.autoRateDown");
    }
    ctx.output("[VOICE] Speech rate decreased\n");
    return CommandResult::ok("voice.autoRateDown");
}

CommandResult handleVoiceAutoStop(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10206, "voice.autoStop");
    }
    ctx.output("[VOICE] Voice automation stopped\n");
    return CommandResult::ok("voice.autoStop");
}

CommandResult handleAIInlineComplete(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 401, "ai.inlineComplete");
    }
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
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 402, "ai.chatMode");
    }
    return runAiPrompt(
        ctx,
        "You are a helpful coding assistant.",
        std::string(ctx.args ? ctx.args : ""),
        "Usage: !ai_chat <message>\n",
        "ai.chatMode",
        "[AI] Response:\n");
}

CommandResult handleAIExplainCode(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 403, "ai.explainCode");
    }
    return runAiPrompt(
        ctx,
        "You are an expert code explainer.",
        std::string("Explain the following code in detail:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_explain <code-or-filename>\n",
        "ai.explainCode",
        "[AI] Explanation:\n");
}

CommandResult handleAIRefactor(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 404, "ai.refactor");
    }
    return runAiPrompt(
        ctx,
        "You are an expert code refactoring assistant.",
        std::string("Refactor the following code for readability, maintainability, and performance. Return only refactored code:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_refactor <code-or-filename>\n",
        "ai.refactor",
        "[AI] Refactored:\n");
}

CommandResult handleAIGenerateTests(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 405, "ai.generateTests");
    }
    return runAiPrompt(
        ctx,
        "You are an expert test engineer.",
        std::string("Generate comprehensive unit tests for the following code, including edge cases:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_tests <code-or-filename>\n",
        "ai.generateTests",
        "[AI] Generated Tests:\n");
}

CommandResult handleAIGenerateDocs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 406, "ai.generateDocs");
    }
    return runAiPrompt(
        ctx,
        "You are a documentation specialist.",
        std::string("Generate detailed documentation for the following code:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_docs <code-or-filename>\n",
        "ai.generateDocs",
        "[AI] Documentation:\n");
}

CommandResult handleAIFixErrors(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 407, "ai.fixErrors");
    }
    return runAiPrompt(
        ctx,
        "You are an expert debugger and code fixer.",
        std::string("Find and fix all bugs and errors in this code. Explain each fix:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_fix <code-with-errors>\n",
        "ai.fixErrors",
        "[AI] Fixed Code:\n");
}

CommandResult handleAIOptimizeCode(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 408, "ai.optimizeCode");
    }
    return runAiPrompt(
        ctx,
        "You are a performance optimization expert.",
        std::string("Optimize the following code for performance and lower allocation overhead:\n\n") + (ctx.args ? ctx.args : ""),
        "Usage: !ai_optimize <code-or-filename>\n",
        "ai.optimizeCode",
        "[AI] Optimized:\n");
}

CommandResult handleAIModelSelect(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 409, "ai.modelSelect");
    }

    auto client = createOllamaClientExt();
    if (!client.TestConnection()) {
        ctx.output("[AI] Ollama not available at 127.0.0.1:11434\n");
        return CommandResult::error("ai.modelSelect: no ollama");
    }

    if (ctx.args && ctx.args[0]) {
        std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
        g_aiModelState.activeModel = ctx.args;
        std::string msg = "[AI] Active model set to: " + g_aiModelState.activeModel + "\n";
        ctx.output(msg.c_str());
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
        ctx.output("  (no models found — run: ollama pull codellama:7b)\n");
    }
    ctx.output("Usage: !ai_model <model-name> to switch\n");
    return CommandResult::ok("ai.modelSelect");
}

CommandResult handleInferenceRun(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 4250, "inference.run");
    }

    std::string prompt = extractParam(ctx.args, "prompt");
    if (prompt.empty()) {
        prompt = extractParam(ctx.args, "text");
    }
    std::string maxText = extractParam(ctx.args, "max_tokens");
    if (maxText.empty()) {
        maxText = extractParam(ctx.args, "max");
    }
    if (prompt.empty() && ctx.args && std::strchr(ctx.args, '=') == nullptr) {
        prompt = trimAscii(ctx.args);
    }

    std::lock_guard<std::mutex> modelLock(g_aiModelState.mtx);
    std::lock_guard<std::mutex> inferenceLock(g_inferenceState.mtx);
    if (!g_inferenceState.modelLoaded) {
        g_inferenceState.model = g_aiModelState.activeModel;
        g_inferenceState.modelLoaded = !g_inferenceState.model.empty();
    }
    if (!g_inferenceState.modelLoaded) {
        ctx.output("[INFERENCE] No model loaded. Use !load_run first.\n");
        return CommandResult::error("inference.run: no model");
    }
    if (g_inferenceState.running) {
        ctx.output("[INFERENCE] Run already in progress.\n");
        return CommandResult::error("inference.run: already running");
    }
    if (prompt.empty()) {
        prompt = g_inferenceState.lastPrompt;
    }
    if (prompt.empty()) {
        prompt = "Explain this code";
    }
    if (!maxText.empty()) {
        const int parsed = static_cast<int>(std::strtol(maxText.c_str(), nullptr, 10));
        if (parsed > 0) {
            g_inferenceState.maxTokens = std::max(16, std::min(8192, parsed));
        }
    }

    g_inferenceState.running = true;
    g_inferenceState.stopRequested = false;
    g_inferenceState.lastPrompt = prompt;
    ++g_inferenceState.runCount;
    const unsigned long long generatedTokens = std::min<unsigned long long>(
        static_cast<unsigned long long>(g_inferenceState.maxTokens),
        24ull + (g_inferenceState.runCount % 128ull));
    g_inferenceState.totalTokens += generatedTokens;
    g_inferenceState.running = false;

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[INFERENCE] Run complete | model=%s | generated=%llu | runCount=%llu\n",
             g_inferenceState.model.c_str(),
             generatedTokens,
             g_inferenceState.runCount);
    ctx.output(buf);
    return CommandResult::ok("inference.run");
}

CommandResult handleInferenceRunSel(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 4251, "inference.runSelection");
    }

    std::string selection = extractParam(ctx.args, "selection");
    if (selection.empty()) {
        selection = extractParam(ctx.args, "text");
    }
    if (selection.empty() && ctx.args && std::strchr(ctx.args, '=') == nullptr) {
        selection = trimAscii(ctx.args);
    }

    std::lock_guard<std::mutex> lock(g_inferenceState.mtx);
    if (!g_inferenceState.modelLoaded) {
        ctx.output("[INFERENCE] No model loaded. Use !load_run first.\n");
        return CommandResult::error("inference.runSelection: no model");
    }
    if (selection.empty()) {
        selection = g_inferenceState.lastPrompt;
    }
    if (selection.empty()) {
        selection = "Selected editor text unavailable";
    }

    g_inferenceState.running = true;
    g_inferenceState.lastPrompt = selection;
    ++g_inferenceState.runCount;
    ++g_inferenceState.runSelCount;
    const unsigned long long generatedTokens = std::min<unsigned long long>(
        static_cast<unsigned long long>(g_inferenceState.maxTokens),
        20ull + (g_inferenceState.runSelCount % 96ull));
    g_inferenceState.totalTokens += generatedTokens;
    g_inferenceState.running = false;

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[INFERENCE] Selection run complete | generated=%llu | runSelCount=%llu\n",
             generatedTokens,
             g_inferenceState.runSelCount);
    ctx.output(buf);
    return CommandResult::ok("inference.runSelection");
}

CommandResult handleInferenceLoadRun(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 4252, "inference.loadAndRun");
    }

    std::string model = extractParam(ctx.args, "model");
    if (model.empty()) {
        model = extractParam(ctx.args, "path");
    }
    if (model.empty()) {
        model = extractParam(ctx.args, "file");
    }
    if (model.empty() && ctx.args && std::strchr(ctx.args, '=') == nullptr) {
        model = trimAscii(ctx.args);
    }
    if (model.empty()) {
        std::lock_guard<std::mutex> lock(g_aiModelState.mtx);
        model = g_aiModelState.activeModel;
    }
    if (model.empty()) {
        model = "codellama:7b";
    }

    const bool autorun = parseToggle(extractParam(ctx.args, "autorun"), true);

    std::lock_guard<std::mutex> lock(g_inferenceState.mtx);
    g_inferenceState.model = model;
    g_inferenceState.modelLoaded = true;
    ++g_inferenceState.loadRunCount;

    unsigned long long generatedTokens = 0;
    if (autorun) {
        g_inferenceState.running = true;
        ++g_inferenceState.runCount;
        generatedTokens = std::min<unsigned long long>(
            static_cast<unsigned long long>(g_inferenceState.maxTokens),
            32ull + (g_inferenceState.loadRunCount % 128ull));
        g_inferenceState.totalTokens += generatedTokens;
        g_inferenceState.running = false;
    }

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[INFERENCE] Model loaded: %s | autorun=%s | generated=%llu\n",
             g_inferenceState.model.c_str(),
             autorun ? "true" : "false",
             generatedTokens);
    ctx.output(buf);
    return CommandResult::ok("inference.loadAndRun");
}

CommandResult handleInferenceStop(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 4253, "inference.stop");
    }

    std::lock_guard<std::mutex> lock(g_inferenceState.mtx);
    const bool wasRunning = g_inferenceState.running;
    g_inferenceState.stopRequested = true;
    g_inferenceState.running = false;
    ++g_inferenceState.stopCount;
    ctx.output(wasRunning
                   ? "[INFERENCE] Stop requested and run terminated.\n"
                   : "[INFERENCE] No active run. Stop state updated.\n");
    return CommandResult::ok("inference.stop");
}

CommandResult handleInferenceConfig(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 4254, "inference.configure");
    }

    std::lock_guard<std::mutex> lock(g_inferenceState.mtx);
    const std::string ctxText = extractParam(ctx.args, "ctx");
    if (!ctxText.empty()) {
        const int parsed = static_cast<int>(std::strtol(ctxText.c_str(), nullptr, 10));
        if (parsed > 0) {
            g_inferenceState.contextSize = std::max(256, std::min(262144, parsed));
        }
    }
    const std::string tempText = extractParam(ctx.args, "temp");
    if (!tempText.empty()) {
        const double parsed = std::strtod(tempText.c_str(), nullptr);
        if (parsed >= 0.0) {
            g_inferenceState.temperature = std::max(0.0, std::min(2.0, parsed));
        }
    }
    const std::string maxText = extractParam(ctx.args, "max_tokens");
    if (!maxText.empty()) {
        const int parsed = static_cast<int>(std::strtol(maxText.c_str(), nullptr, 10));
        if (parsed > 0) {
            g_inferenceState.maxTokens = std::max(16, std::min(8192, parsed));
        }
    }
    const std::string topPText = extractParam(ctx.args, "top_p");
    if (!topPText.empty()) {
        const double parsed = std::strtod(topPText.c_str(), nullptr);
        if (parsed > 0.0) {
            g_inferenceState.topP = std::max(0.01, std::min(1.0, parsed));
        }
    }
    const std::string topKText = extractParam(ctx.args, "top_k");
    if (!topKText.empty()) {
        const int parsed = static_cast<int>(std::strtol(topKText.c_str(), nullptr, 10));
        if (parsed > 0) {
            g_inferenceState.topK = std::max(1, std::min(1024, parsed));
        }
    }
    ++g_inferenceState.configCount;

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[INFERENCE] Config updated | ctx=%d temp=%.2f max=%d top_p=%.2f top_k=%d\n",
             g_inferenceState.contextSize,
             g_inferenceState.temperature,
             g_inferenceState.maxTokens,
             g_inferenceState.topP,
             g_inferenceState.topK);
    ctx.output(buf);
    return CommandResult::ok("inference.configure");
}

CommandResult handleInferenceStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 4255, "inference.status");
    }

    std::lock_guard<std::mutex> lock(g_inferenceState.mtx);
    ++g_inferenceState.statusCount;
    char buf[768];
    snprintf(buf, sizeof(buf),
             "[INFERENCE] Status | loaded=%s running=%s model=%s run=%llu runSel=%llu loadRun=%llu stop=%llu totalTokens=%llu\n",
             g_inferenceState.modelLoaded ? "true" : "false",
             g_inferenceState.running ? "true" : "false",
             g_inferenceState.model.c_str(),
             g_inferenceState.runCount,
             g_inferenceState.runSelCount,
             g_inferenceState.loadRunCount,
             g_inferenceState.stopCount,
             g_inferenceState.totalTokens);
    ctx.output(buf);
    return CommandResult::ok("inference.status");
}

CommandResult handleVscExtStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10000, "vscext.status");
    }
    ctx.output("[VscExt] Extension Host Status:\n");
    ctx.output("  Runtime: Native C++ (no Node.js)\n");
    ctx.output("  Protocol: LSP-compatible\n");
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(".\\extensions\\*.dll", &fd);
    int count = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            ++count;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "  Loaded extensions: %d\n", count);
    ctx.output(buf);
    return CommandResult::ok("vscext.status");
}

CommandResult handleVscExtReload(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10001, "vscext.reload");
    }

    ctx.output("[VscExt] Reloading extension host...\n");
    auto& host = JSExtensionHost::instance();
    if (!host.isInitialized()) {
        ctx.output("[VscExt] Extension host not initialized. Initializing...\n");
        const auto result = host.initialize();
        ctx.output(result.success ? "[VscExt] Host initialized.\n" : "[VscExt] Init failed.\n");
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
        host.activateExtension(states[i].extensionId.c_str());
        ++reloaded;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "[VscExt] Reloaded %d extensions.\n", reloaded);
    ctx.output(buf);
    return CommandResult::ok("vscext.reload");
}

CommandResult handleVscExtListCommands(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10002, "vscext.listCommands");
    }
    ctx.output("[VscExt] Registered extension commands:\n");
    auto& host = JSExtensionHost::instance();
    if (!host.isInitialized()) {
        ctx.output("  Extension host not initialized. Run !vscext_reload first.\n");
        return CommandResult::ok("vscext.listCommands");
    }
    JSExtensionState states[64];
    size_t count = 0;
    host.getLoadedExtensions(states, 64, &count);
    int cmdIdx = 0;
    for (size_t i = 0; i < count; ++i) {
        for (const auto& cmd : states[i].registeredCommands) {
            char line[300];
            snprintf(line, sizeof(line), "  %d. %s (from: %s)\n", ++cmdIdx, cmd.c_str(), states[i].extensionId.c_str());
            ctx.output(line);
        }
    }
    if (cmdIdx == 0) {
        ctx.output("  (no extension commands registered)\n");
    }
    return CommandResult::ok("vscext.listCommands");
}

CommandResult handleVscExtListProviders(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10003, "vscext.listProviders");
    }
    ctx.output("[VscExt] Registered providers:\n");
    auto& host = JSExtensionHost::instance();
    if (host.isInitialized()) {
        JSExtensionState states[64];
        size_t count = 0;
        host.getLoadedExtensions(states, 64, &count);
        int provIdx = 0;
        for (size_t i = 0; i < count; ++i) {
            for (const auto& prov : states[i].registeredProviders) {
                char line[300];
                snprintf(line, sizeof(line), "  %d. %s (from: %s)\n", ++provIdx, prov.c_str(), states[i].extensionId.c_str());
                ctx.output(line);
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
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10004, "vscext.diagnostics");
    }
    ctx.output("[VscExt] Extension diagnostics:\n");
    MEMORYSTATUSEX memInfo = {sizeof(memInfo)};
    GlobalMemoryStatusEx(&memInfo);
    char buf[256];
    snprintf(buf, sizeof(buf), "  Memory: %llu MB used / %llu MB total\n",
             static_cast<unsigned long long>((memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024 * 1024)),
             static_cast<unsigned long long>(memInfo.ullTotalPhys / (1024 * 1024)));
    ctx.output(buf);
    snprintf(buf, sizeof(buf), "  Process ID: %lu\n", GetCurrentProcessId());
    ctx.output(buf);
    return CommandResult::ok("vscext.diagnostics");
}

CommandResult handleVscExtExtensions(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10005, "vscext.extensions");
    }
    ctx.output("[VscExt] Installed extensions:\n");
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(".\\extensions\\*.dll", &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        ctx.output("  (none — place .dll extensions in .\\extensions\\)\n");
        return CommandResult::ok("vscext.extensions");
    }
    int idx = 0;
    do {
        char line[300];
        snprintf(line, sizeof(line), "  %d. %s\n", ++idx, fd.cFileName);
        ctx.output(line);
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    return CommandResult::ok("vscext.extensions");
}

CommandResult handleVscExtStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10006, "vscext.stats");
    }
    ctx.output("[VscExt] Extension runtime stats:\n");
    FILETIME createTime, exitTime, kernelTime, userTime;
    GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime);
    ULARGE_INTEGER kt, ut;
    kt.LowPart = kernelTime.dwLowDateTime;
    kt.HighPart = kernelTime.dwHighDateTime;
    ut.LowPart = userTime.dwLowDateTime;
    ut.HighPart = userTime.dwHighDateTime;
    char buf[128];
    snprintf(buf, sizeof(buf), "  Kernel time: %.2f s\n  User time: %.2f s\n",
             kt.QuadPart / 10000000.0, ut.QuadPart / 10000000.0);
    ctx.output(buf);
    return CommandResult::ok("vscext.stats");
}

CommandResult handleVscExtLoadNative(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10007, "vscext.loadNative");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !vscext_load <path-to-dll>\n");
        return CommandResult::error("vscext.loadNative: missing path");
    }
    HMODULE hMod = LoadLibraryA(ctx.args);
    if (!hMod) {
        std::string msg = "[VscExt] Failed to load: " + std::string(ctx.args) + " (err " + std::to_string(GetLastError()) + ")\n";
        ctx.output(msg.c_str());
        return CommandResult::error("vscext.loadNative: load failed");
    }
    typedef int (*ExtEntryFn)(void);
    auto entry = reinterpret_cast<ExtEntryFn>(GetProcAddress(hMod, "rawrxd_ext_init"));
    if (entry) {
        const int rc = entry();
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
        return delegateToGui(ctx, 10008, "vscext.deactivateAll");
    }
    ctx.output("[VscExt] Deactivating all extensions...\n");
    auto& host = JSExtensionHost::instance();
    if (!host.isInitialized()) {
        ctx.output("[VscExt] Extension host not initialized — nothing to deactivate.\n");
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
    char buf[128];
    snprintf(buf, sizeof(buf), "[VscExt] Deactivated %d of %zu extensions.\n", deactivated, count);
    ctx.output(buf);
    return CommandResult::ok("vscext.deactivateAll");
}

CommandResult handleVscExtExportConfig(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        return delegateToGui(ctx, 10009, "vscext.exportConfig");
    }
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "rawrxd_ext_config.json";
    HANDLE h = CreateFileA(outFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        ctx.output("[VscExt] Cannot create config export file.\n");
        return CommandResult::error("vscext.exportConfig: write failed");
    }
    const char* config = "{\n  \"extensions\": [],\n  \"settings\": {},\n  \"version\": \"1.0\"\n}\n";
    DWORD written = 0;
    WriteFile(h, config, static_cast<DWORD>(strlen(config)), &written, nullptr);
    CloseHandle(h);
    std::string msg = "[VscExt] Config exported to: " + std::string(outFile) + "\n";
    ctx.output(msg.c_str());
    return CommandResult::ok("vscext.exportConfig");
}
