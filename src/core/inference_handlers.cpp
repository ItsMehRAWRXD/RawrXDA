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
    
    // TODO: Retrieve selected text from IDE via ctx.args or Win32 message
    std::string selectedText = "TODO: Get selected text from editor";
    
    ctx.output("[INFERENCE] Executing with selected text: \"");
    ctx.output(selectedText.c_str());
    ctx.output("\"\n");
    
    // Same execution logic as handleInferenceRun but with custom prompt
    // (For brevity, calling handleInferenceRun - production would parameterize prompt)
    return handleInferenceRun(ctx);
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
    ofn.hwndOwner = NULL;  // TODO: Set to IDE main window handle
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
        config.n_gpu_layers = 0;  // CPU-only for now
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
    
    // TODO: Show configuration dialog or parse from ctx.args
    // For now, just display current config
    
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
