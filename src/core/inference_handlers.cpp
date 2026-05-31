// ============================================================================
// inference_handlers.cpp — GGUF Inference Execution Handlers
// ============================================================================
// Wires Ctrl+F5 hotkey to GGUF → JIT execution path
//
// Architecture:
//   handleInferenceRun()      : Ctrl+F5 - Execute current GGUF model
//   handleInferenceRunSel()   : Execute with selected text as prompt
//   handleInferenceLoadRun()  : Open file dialog + load + execute
//   handleInferenceStop()     : Terminate running inference
//   handleInferenceConfig()   : Configure inference settings (ctx size, temp, etc.)
//   handleInferenceStatus()   : Show current model + inference status
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#if defined(RAWRXD_GOLD_BUILD)

#include "feature_handlers.h"

// RawrXD-Gold wires inference command handlers through ssot_handlers_ext_isolated.cpp.
// Keep this TU buildable but empty for Gold to avoid duplicate handler definitions.

#else

#include "feature_handlers.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>  // File open dialog

#include "../gguf_loader.h"
#include "../inference/ultra_fast_inference.h"
#include "../inference/autonomous_inference.h"  // AutonomousInferenceEngine
#include "../layer_offload_manager.hpp"
#include "../gpu_enforcement.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <sstream>
#include <thread>
#include <memory>

using namespace RawrXD;

// ============================================================================
// STATIC STATE — Inference Engine Instance
// ============================================================================

namespace {

bool parseEnvInt(const char* name, int minValue, int maxValue, int& outValue) {
    if (!name || !name[0]) {
        return false;
    }
    const char* raw = std::getenv(name);
    if (!raw || !raw[0]) {
        return false;
    }
    char* end = nullptr;
    long parsed = std::strtol(raw, &end, 10);
    if (end == raw || (end && *end != '\0')) {
        return false;
    }
    if (parsed < minValue || parsed > maxValue) {
        return false;
    }
    outValue = static_cast<int>(parsed);
    return true;
}

struct InferenceState {
    std::mutex                                  mtx;
    std::string                                 currentModelPath;
    bool                                        modelLoaded = false;
    std::atomic<bool>                           isRunning{false};
    std::atomic<bool>                           stopRequested{false};
    std::unique_ptr<GGUFLoader>                 loader;
    std::unique_ptr<inference::UltraFastInferenceEngine> engine;
    
    // Inference config
    int                                         contextSize = 4096;
    float                                       temperature = 0.7f;
    int                                         maxTokens = 512;
    float                                       topP = 0.9f;
    int                                         topK = 40;
    
    std::atomic<uint64_t>                       totalInferences{0};
    std::atomic<uint64_t>                       totalTokensGenerated{0};
    
    static InferenceState& instance() {
        static InferenceState s;
        return s;
    }
};

} // anonymous namespace

// ============================================================================
// IMPL: handleInferenceRun — Execute inference with current model (Ctrl+F5)
// ============================================================================

CommandResult handleInferenceRun(const CommandContext& ctx) {
    auto& state = InferenceState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);
    
    // Check if model is loaded
    if (!state.modelLoaded || !state.loader || !state.engine) {
        ctx.output("[INFERENCE] No model loaded. Use File → Load Model or Ctrl+Shift+F5 to load a GGUF file.\n");
        return CommandResult::failure("No model loaded");
    }
    
    // Check if already running
    if (state.isRunning.load()) {
        ctx.output("[INFERENCE] Inference already running. Use !stop to terminate.\n");
        return CommandResult::failure("Already running");
    }
    
    // Default prompt - in future, integrate with editor selection
    std::string prompt = "Once upon a time";
    
    ctx.output("[INFERENCE] ───────────────────────────────────────────\n");
    ctx.output("[INFERENCE] Model: ");
    ctx.output(state.currentModelPath.c_str());
    ctx.output("\n");
    ctx.output("[INFERENCE] Ctx Size: ");
    ctx.output(std::to_string(state.contextSize).c_str());
    ctx.output(" | Temp: ");
    ctx.output(std::to_string(state.temperature).c_str());
    ctx.output(" | Max Tokens: ");
    ctx.output(std::to_string(state.maxTokens).c_str());
    ctx.output("\n");
    ctx.output("[INFERENCE] Prompt: \"");
    ctx.output(prompt.c_str());
    ctx.output("\"\n");
    ctx.output("[INFERENCE] ───────────────────────────────────────────\n");
    
    // Execute inference in background thread
    state.isRunning.store(true);
    state.stopRequested.store(false);
    
    std::thread([&state, prompt, ctx]() {
        try {
            auto tokens = state.engine->generate(prompt, state.maxTokens);
            
            // Output generated tokens (simplified - in production would detokenize)
            ctx.output("[INFERENCE] Generated ");
            ctx.output(std::to_string(tokens.size()).c_str());
            ctx.output(" tokens: ");
            
            for (size_t i = 0; i < std::min(tokens.size(), size_t(20)); ++i) {
                ctx.output(std::to_string(tokens[i]).c_str());
                ctx.output(" ");
            }
            if (tokens.size() > 20) {
                ctx.output("...");
            }
            ctx.output("\n");
            
            state.totalInferences.fetch_add(1);
            state.totalTokensGenerated.fetch_add(tokens.size());
            
            ctx.output("[INFERENCE] ✅ Inference complete.\n");
            
        } catch (const std::exception& e) {
            ctx.output("[INFERENCE] ❌ Error: ");
            ctx.output(e.what());
            ctx.output("\n");
        }
        
        state.isRunning.store(false);
    }).detach();
    
    return CommandResult::success();
}

// ============================================================================
// IMPL: handleInferenceRunSel — Execute inference with selected text as prompt
// ============================================================================

CommandResult handleInferenceRunSel(const CommandContext& ctx) {
    auto& state = InferenceState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);
    
    if (!state.modelLoaded || !state.loader || !state.engine) {
        ctx.output("[INFERENCE] No model loaded.\n");
        return CommandResult::failure("No model loaded");
    }
    
    if (state.isRunning.load()) {
        ctx.output("[INFERENCE] Inference already running. Use !stop to terminate.\n");
        return CommandResult::failure("Already running");
    }

    // Use ctx.args as explicit prompt; otherwise query the focused Win32 edit control
    // via EM_GETSEL so the user's current text selection becomes the prompt.
    std::string selectedText;
    if (ctx.args && ctx.args[0]) {
        selectedText = ctx.args;
    } else {
        HWND hFocus = GetFocus();
        if (hFocus) {
            DWORD selStart = 0, selEnd = 0;
            SendMessageA(hFocus, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart),
                         reinterpret_cast<LPARAM>(&selEnd));
            if (selEnd > selStart && (selEnd - selStart) < 65536u) {
                int len = GetWindowTextLengthA(hFocus);
                if (len > 0 && static_cast<DWORD>(len) >= selEnd) {
                    std::string buf(static_cast<size_t>(len) + 1, '\0');
                    GetWindowTextA(hFocus, buf.data(), len + 1);
                    buf.resize(static_cast<size_t>(len));
                    selectedText = buf.substr(selStart, selEnd - selStart);
                }
            }
        }
    }

    if (selectedText.empty()) {
        ctx.output("[INFERENCE] No text selected and no prompt in ctx.args.\n");
        return CommandResult::failure("No selection");
    }

    ctx.output("[INFERENCE] ───────────────────────────────────────────\n");
    ctx.output("[INFERENCE] Model: ");
    ctx.output(state.currentModelPath.c_str());
    ctx.output("\n");
    ctx.output("[INFERENCE] Ctx Size: ");
    ctx.output(std::to_string(state.contextSize).c_str());
    ctx.output(" | Temp: ");
    ctx.output(std::to_string(state.temperature).c_str());
    ctx.output(" | Max Tokens: ");
    ctx.output(std::to_string(state.maxTokens).c_str());
    ctx.output("\n");
    ctx.output("[INFERENCE] Selected Prompt (");
    ctx.output(std::to_string(selectedText.size()).c_str());
    ctx.output(" chars): \"");
    if (selectedText.size() > 120) {
        ctx.output(selectedText.substr(0, 120).c_str());
        ctx.output("...");
    } else {
        ctx.output(selectedText.c_str());
    }
    ctx.output("\"\n");
    ctx.output("[INFERENCE] ───────────────────────────────────────────\n");

    // Execute inference with the selected text as prompt in a background thread.
    // (Inline execution — cannot delegate to handleInferenceRun which holds the same mutex.)
    state.isRunning.store(true);
    state.stopRequested.store(false);

    std::thread([&state, selectedText, ctx]() {
        try {
            auto tokens = state.engine->generate(selectedText, state.maxTokens);

            ctx.output("[INFERENCE] Generated ");
            ctx.output(std::to_string(tokens.size()).c_str());
            ctx.output(" tokens: ");

            for (size_t i = 0; i < std::min(tokens.size(), size_t(20)); ++i) {
                ctx.output(std::to_string(tokens[i]).c_str());
                ctx.output(" ");
            }
            if (tokens.size() > 20) {
                ctx.output("...");
            }
            ctx.output("\n");

            state.totalInferences.fetch_add(1);
            state.totalTokensGenerated.fetch_add(tokens.size());

            ctx.output("[INFERENCE] \u2705 Inference complete.\n");

        } catch (const std::exception& e) {
            ctx.output("[INFERENCE] \u274c Error: ");
            ctx.output(e.what());
            ctx.output("\n");
        }

        state.isRunning.store(false);
    }).detach();

    return CommandResult::success();
}

// ============================================================================
// IMPL: handleInferenceLoadRun — Open file dialog + load GGUF + execute
// ============================================================================

CommandResult handleInferenceLoadRun(const CommandContext& ctx) {
    auto& state = InferenceState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);
    
    // Show file open dialog for .gguf files
    OPENFILENAMEA ofn;
    char szFile[MAX_PATH] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ctx.hwnd ? static_cast<HWND>(ctx.hwnd) : GetForegroundWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "GGUF Models (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = "D:\\OllamaModels";  // Default to OllamaModels folder
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn) == FALSE) {
        ctx.output("[INFERENCE] No file selected.\n");
        return CommandResult::failure("No file selected");
    }
    
    std::string modelPath = szFile;
    
    ctx.output("[INFERENCE] Loading model: ");
    ctx.output(modelPath.c_str());
    ctx.output("\n");
    
    // Load GGUF file
    try {
        auto loader = std::make_unique<GGUFLoader>();
        if (!loader->Open(modelPath)) {
            ctx.output("[INFERENCE] ❌ Failed to open GGUF file.\n");
            return CommandResult::failure("Failed to open GGUF");
        }
        
        // Extract metadata
        const auto& meta = loader->GetMetadata();
        ctx.output("[INFERENCE] Model loaded successfully.\n");
        ctx.output("[INFERENCE]   Version: ");
        ctx.output(std::to_string(meta.version).c_str());
        ctx.output("\n");
        ctx.output("[INFERENCE]   Tensors: ");
        ctx.output(std::to_string(meta.tensor_count).c_str());
        ctx.output("\n");
        ctx.output("[INFERENCE]   Params: ");
        
        uint64_t n_params = meta.param_count;
        if (n_params >= 1000000000) {
            ctx.output(std::to_string(n_params / 1000000000).c_str());
            ctx.output("B");
        } else if (n_params >= 1000000) {
            ctx.output(std::to_string(n_params / 1000000).c_str());
            ctx.output("M");
        } else {
            ctx.output(std::to_string(n_params).c_str());
        }
        ctx.output("\n");
        
        // Create inference engine
        inference::AutonomousInferenceEngine::InferenceConfig config;
        config.max_batch_size = 1;
        config.ctx_size = state.contextSize;
        // GPU inference remains mandatory, but full offload can be unstable for
        // larger models on fixed-VRAM cards. Use env override first, then fall
        // back to an empirical layer split derived from model metadata.
        int selectedGpuLayers = 999;
        bool hasEnvOverride = parseEnvInt("RAWRXD_N_GPU_LAYERS", 1, 999, selectedGpuLayers)
            || parseEnvInt("RAWRXD_GPU_LAYERS", 1, 999, selectedGpuLayers);
        if (!hasEnvOverride) {
            uint64_t vramBytes = rxd::gpu::status().vram_total_bytes;
            if (vramBytes == 0) {
                vramBytes = 16ULL * 1024 * 1024 * 1024;
            }

            MEMORYSTATUSEX memstat{};
            memstat.dwLength = sizeof(memstat);
            uint64_t sysRAM = GlobalMemoryStatusEx(&memstat) ? memstat.ullTotalPhys : 0;

            const uint32_t layers = meta.layer_count;
            const uint32_t kvHeads = (meta.head_count_kv > 0) ? meta.head_count_kv : meta.head_count;
            const uint32_t headDim = (meta.embedding_dim > 0 && meta.head_count > 0)
                ? (meta.embedding_dim / meta.head_count)
                : 128;
            const uint32_t ctxLen = (state.contextSize > 0)
                ? static_cast<uint32_t>(state.contextSize)
                : ((meta.context_length > 0) ? meta.context_length : 4096);

            if (layers > 0 && kvHeads > 0) {
                const std::error_code ec;
                const uint64_t fileSize = static_cast<uint64_t>(std::filesystem::file_size(modelPath, ec));
                if (!ec && fileSize > 0) {
                    const auto split = RawrXD::computeOptimalGPULayers(
                        fileSize,
                        layers,
                        kvHeads,
                        headDim,
                        ctxLen,
                        vramBytes,
                        sysRAM);
                    selectedGpuLayers = std::max(1, std::min(999, static_cast<int>(split.gpuLayers)));

                    char splitInfo[256];
                    std::snprintf(splitInfo,
                                  sizeof(splitInfo),
                                  "[INFERENCE] Auto GPU layer split selected: %d/%u (stable=%s, est tg128=%.1f t/s)\n",
                                  selectedGpuLayers,
                                  split.totalLayers,
                                  split.stable ? "YES" : "NO",
                                  split.estGenerateTps);
                    ctx.output(splitInfo);
                }
            }
        } else {
            char envInfo[128];
            std::snprintf(envInfo,
                          sizeof(envInfo),
                          "[INFERENCE] Using RAWRXD_*_GPU_LAYERS override: %d\n",
                          selectedGpuLayers);
            ctx.output(envInfo);
        }

        config.n_gpu_layers = selectedGpuLayers;
        config.flash_attention = false;
        
        auto engine = std::make_unique<inference::UltraFastInferenceEngine>(config);
        engine->loadModel(modelPath);
        
        // Store in state
        state.loader = std::move(loader);
        state.engine = std::move(engine);
        state.currentModelPath = modelPath;
        state.modelLoaded = true;
        
        ctx.output("[INFERENCE] ✅ Model ready for execution. Press Ctrl+F5 to run inference.\n");
        
        // Auto-execute after loading
        return handleInferenceRun(ctx);
        
    } catch (const std::exception& e) {
        ctx.output("[INFERENCE] ❌ Error loading model: ");
        ctx.output(e.what());
        ctx.output("\n");
        return CommandResult::failure("Model load error");
    }
}

// ============================================================================
// IMPL: handleInferenceStop — Terminate running inference
// ============================================================================

CommandResult handleInferenceStop(const CommandContext& ctx) {
    auto& state = InferenceState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);
    
    if (!state.isRunning.load()) {
        ctx.output("[INFERENCE] No inference currently running.\n");
        return CommandResult::success();
    }
    
    ctx.output("[INFERENCE] Requesting stop...\n");
    state.stopRequested.store(true);
    
    // Wait briefly for thread to terminate
    int maxWait = 50;  // 500ms
    while (state.isRunning.load() && maxWait > 0) {
        Sleep(10);
        maxWait--;
    }
    
    if (state.isRunning.load()) {
        ctx.output("[INFERENCE] ⚠️  Stop requested but inference still running (background thread).\n");
    } else {
        ctx.output("[INFERENCE] ✅ Inference stopped.\n");
    }
    
    return CommandResult::success();
}

// ============================================================================
// IMPL: handleInferenceConfig — Configure inference settings
// ============================================================================

CommandResult handleInferenceConfig(const CommandContext& ctx) {
    auto& state = InferenceState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);
    
    // Parse ctx.args for key=value pairs that update inference settings:
    // ctx=<int>  temp=<float>  max_tokens=<int>  top_p=<float>  top_k=<int>
    if (ctx.args && ctx.args[0]) {
        std::istringstream ss(ctx.args);
        std::string token;
        int updates = 0;
        while (ss >> token) {
            const auto eq = token.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = token.substr(0, eq);
            const std::string val = token.substr(eq + 1);
            try {
                if (key == "ctx") {
                    const int v = std::stoi(val);
                    if (v < 64 || v > 131072) { ctx.output("[INFERENCE] ctx out of range [64,131072]\n"); continue; }
                    state.contextSize = v; ++updates;
                } else if (key == "temp") {
                    const float v = std::stof(val);
                    if (v < 0.0f || v > 10.0f) { ctx.output("[INFERENCE] temp out of range [0.0,10.0]\n"); continue; }
                    state.temperature = v; ++updates;
                } else if (key == "max_tokens") {
                    const int v = std::stoi(val);
                    if (v < 1 || v > 32768) { ctx.output("[INFERENCE] max_tokens out of range [1,32768]\n"); continue; }
                    state.maxTokens = v; ++updates;
                } else if (key == "top_p") {
                    const float v = std::stof(val);
                    if (v < 0.0f || v > 1.0f) { ctx.output("[INFERENCE] top_p out of range [0.0,1.0]\n"); continue; }
                    state.topP = v; ++updates;
                } else if (key == "top_k") {
                    const int v = std::stoi(val);
                    if (v < 1 || v > 1000) { ctx.output("[INFERENCE] top_k out of range [1,1000]\n"); continue; }
                    state.topK = v; ++updates;
                } else {
                    ctx.output("[INFERENCE] Unknown key: ");
                    ctx.output(key.c_str());
                    ctx.output(" (valid: ctx temp max_tokens top_p top_k)\n");
                }
            } catch (...) {
                ctx.output("[INFERENCE] Invalid value for: ");
                ctx.output(key.c_str());
                ctx.output("\n");
            }
        }
        if (updates > 0) {
            ctx.output("[INFERENCE] \u2705 Configuration updated.\n");
        }
    }

    ctx.output("[INFERENCE] Current Configuration:\n");
    ctx.output("[INFERENCE]   Context Size: ");
    ctx.output(std::to_string(state.contextSize).c_str());
    ctx.output("\n");
    ctx.output("[INFERENCE]   Temperature: ");
    ctx.output(std::to_string(state.temperature).c_str());
    ctx.output("\n");
    ctx.output("[INFERENCE]   Max Tokens: ");
    ctx.output(std::to_string(state.maxTokens).c_str());
    ctx.output("\n");
    ctx.output("[INFERENCE]   Top-P: ");
    ctx.output(std::to_string(state.topP).c_str());
    ctx.output("\n");
    ctx.output("[INFERENCE]   Top-K: ");
    ctx.output(std::to_string(state.topK).c_str());
    ctx.output("\n");
    ctx.output("[INFERENCE]  \n");
    ctx.output("[INFERENCE] To modify settings, use:\n");
    ctx.output("[INFERENCE]   !infer_config ctx=8192\n");
    ctx.output("[INFERENCE]   !infer_config temp=0.8\n");
    ctx.output("[INFERENCE]   !infer_config max_tokens=1024\n");
    
    return CommandResult::success();
}

// ============================================================================
// IMPL: handleInferenceStatus — Show current model + inference status
// ============================================================================

CommandResult handleInferenceStatus(const CommandContext& ctx) {
    auto& state = InferenceState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);
    
    ctx.output("[INFERENCE] ═══════════════════════════════════════════\n");
    ctx.output("[INFERENCE] Inference Status\n");
    ctx.output("[INFERENCE] ═══════════════════════════════════════════\n");
    
    if (state.modelLoaded) {
        ctx.output("[INFERENCE] ✅ Model Loaded: ");
        ctx.output(state.currentModelPath.c_str());
        ctx.output("\n");
        ctx.output("[INFERENCE]    Tensors: ");
        if (state.loader) {
            ctx.output(std::to_string(state.loader->GetTensorCount()).c_str());
        } else {
            ctx.output("N/A");
        }
        ctx.output("\n");
    } else {
        ctx.output("[INFERENCE] ❌ No model loaded.\n");
        ctx.output("[INFERENCE]    Use Ctrl+Shift+F5 or !load_run to load a GGUF file.\n");
    }
    
    ctx.output("[INFERENCE]  \n");
    ctx.output("[INFERENCE] Running: ");
    ctx.output(state.isRunning.load() ? "✅ Yes" : "❌ No");
    ctx.output("\n");
    
    ctx.output("[INFERENCE] Total Inferences: ");
    ctx.output(std::to_string(state.totalInferences.load()).c_str());
    ctx.output("\n");
    
    ctx.output("[INFERENCE] Total Tokens Generated: ");
    ctx.output(std::to_string(state.totalTokensGenerated.load()).c_str());
    ctx.output("\n");
    
    ctx.output("[INFERENCE] ═══════════════════════════════════════════\n");
    
    return CommandResult::success();
}

#endif  // defined(RAWRXD_GOLD_BUILD)
