// RawrXD_LlamaNative.cpp - llama.cpp Native DLL Bridge Implementation
// Zero HTTP. Zero MASM. Pure LoadLibraryW + GetProcAddress + cdecl calls.
// Target: llama.cpp b3506+ with Vulkan backend for RX 7800 XT

#include "RawrXD_LlamaNative.h"
#include <chrono>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <filesystem>

// ============================================================================
// Static singleton instance
// ============================================================================
static std::unique_ptr<LlamaNativeBridge> g_bridge;

LlamaNativeBridge& GetLlamaBridge() {
    if (!g_bridge) {
        g_bridge = std::make_unique<LlamaNativeBridge>();
    }
    return *g_bridge;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
LlamaNativeBridge::LlamaNativeBridge() {
    tokenBuf_.resize(32768);  // Max tokens per request
    pieceBuf_.resize(512);    // Max bytes per token piece
    posBuf_.resize(32768);
    seqBuf_.resize(1);
    logitsBuf_.resize(32768);
}

LlamaNativeBridge::~LlamaNativeBridge() {
    Shutdown();
}

void LlamaNativeBridge::SetError(const char* msg) {
    lastError_ = msg;
}

// ============================================================================
// Initialize - Load DLLs and bind exports
// ============================================================================
bool LlamaNativeBridge::Initialize(const wchar_t* dllDir) {
    if (hLlama_) return true;  // Already initialized

    // Build DLL path prefix
    std::wstring prefix;
    if (dllDir) {
        prefix = dllDir;
        if (!prefix.empty() && prefix.back() != L'\\' && prefix.back() != L'/') {
            prefix += L'\\';
        }
    }

    // Load ggml backends (dependencies loaded first)
    // Vulkan preferred for RX 7800 XT, CPU fallback
    std::wstring ggmlVkPath = prefix + L"ggml-vulkan.dll";
    std::wstring ggmlCpuPath = prefix + L"ggml-cpu.dll";
    std::wstring ggmlBasePath = prefix + L"ggml.dll";

    // Try Vulkan first (AMD GPU acceleration)
    hGgmlVk_ = LoadLibraryW(ggmlVkPath.c_str());
    hGgmlCpu_ = LoadLibraryW(ggmlCpuPath.c_str());
    hGgml_ = LoadLibraryW(ggmlBasePath.c_str());

    // Load llama.dll
    std::wstring llamaPath = prefix + L"llama.dll";
    hLlama_ = LoadLibraryW(llamaPath.c_str());
    if (!hLlama_) {
        DWORD err = GetLastError();
        std::stringstream ss;
        ss << "Failed to load llama.dll (error " << err << "). "
           << "Ensure llama.dll is in: " << (dllDir ? "specified directory" : "exe directory");
        SetError(ss.str().c_str());
        return false;
    }

    if (!BindExports()) {
        FreeLibrary(hLlama_);
        hLlama_ = nullptr;
        return false;
    }

    // Initialize backend
    if (fn_backend_init) {
        fn_backend_init();
    }

    return true;
}

// ============================================================================
// BindExports - GetProcAddress for all required functions
// ============================================================================
bool LlamaNativeBridge::BindExports() {
    #define BIND(name, type) \
        fn_##name = (type)GetProcAddress(hLlama_, "llama_" #name); \
        if (!fn_##name) { SetError("Missing export: llama_" #name); return false; }

    #define BIND_OPT(name, type) \
        fn_##name = (type)GetProcAddress(hLlama_, "llama_" #name)

    // Required exports
    BIND(backend_init, pfn_backend_init);
    BIND(model_default_params, pfn_model_default_params);
    BIND(load_model, pfn_load_model);  // Actually "llama_load_model_from_file"
    
    // Try alternate name for load_model
    if (!fn_load_model) {
        fn_load_model = (pfn_load_model)GetProcAddress(hLlama_, "llama_load_model_from_file");
    }
    if (!fn_load_model) {
        SetError("Missing export: llama_load_model_from_file");
        return false;
    }

    fn_free_model = (pfn_free_model)GetProcAddress(hLlama_, "llama_free_model");
    fn_model_n_vocab = (pfn_model_n_vocab)GetProcAddress(hLlama_, "llama_n_vocab");
    fn_context_default_params = (pfn_context_default_params)GetProcAddress(hLlama_, "llama_context_default_params");
    fn_new_context = (pfn_new_context)GetProcAddress(hLlama_, "llama_new_context_with_model");
    fn_free_context = (pfn_free_context)GetProcAddress(hLlama_, "llama_free");
    fn_kv_cache_clear = (pfn_kv_cache_clear)GetProcAddress(hLlama_, "llama_kv_cache_clear");
    fn_tokenize = (pfn_tokenize)GetProcAddress(hLlama_, "llama_tokenize");
    fn_token_to_piece = (pfn_token_to_piece)GetProcAddress(hLlama_, "llama_token_to_piece");
    fn_token_bos = (pfn_token_bos)GetProcAddress(hLlama_, "llama_token_bos");
    fn_token_eos = (pfn_token_eos)GetProcAddress(hLlama_, "llama_token_eos");
    fn_batch_init = (pfn_batch_init)GetProcAddress(hLlama_, "llama_batch_init");
    fn_batch_free = (pfn_batch_free)GetProcAddress(hLlama_, "llama_batch_free");
    fn_decode = (pfn_decode)GetProcAddress(hLlama_, "llama_decode");
    fn_get_logits_ith = (pfn_get_logits_ith)GetProcAddress(hLlama_, "llama_get_logits_ith");

    // Optional sampler exports (b3506+)
    fn_sampler_chain_init = (pfn_sampler_chain_init)GetProcAddress(hLlama_, "llama_sampler_chain_init");
    fn_sampler_chain_add = (pfn_sampler_chain_add)GetProcAddress(hLlama_, "llama_sampler_chain_add");
    fn_sampler_sample = (pfn_sampler_sample)GetProcAddress(hLlama_, "llama_sampler_sample");
    fn_sampler_free = (pfn_sampler_free)GetProcAddress(hLlama_, "llama_sampler_free");
    fn_sampler_init_temp = (pfn_sampler_init_temp)GetProcAddress(hLlama_, "llama_sampler_init_temp");
    fn_sampler_init_top_k = (pfn_sampler_init_top_k)GetProcAddress(hLlama_, "llama_sampler_init_top_k");
    fn_sampler_init_top_p = (pfn_sampler_init_top_p)GetProcAddress(hLlama_, "llama_sampler_init_top_p");
    fn_sampler_init_greedy = (pfn_sampler_init_greedy)GetProcAddress(hLlama_, "llama_sampler_init_greedy");

    // Backend cleanup (optional)
    fn_backend_free = (pfn_backend_free)GetProcAddress(hLlama_, "llama_backend_free");

    #undef BIND
    #undef BIND_OPT

    // Validate critical exports
    if (!fn_new_context || !fn_tokenize || !fn_decode || !fn_get_logits_ith) {
        SetError("Critical exports missing from llama.dll");
        return false;
    }

    return true;
}

// ============================================================================
// Shutdown - Free resources
// ============================================================================
void LlamaNativeBridge::Shutdown() {
    UnloadModel();

    if (fn_backend_free) {
        fn_backend_free();
    }

    if (hLlama_) { FreeLibrary(hLlama_); hLlama_ = nullptr; }
    if (hGgml_) { FreeLibrary(hGgml_); hGgml_ = nullptr; }
    if (hGgmlVk_) { FreeLibrary(hGgmlVk_); hGgmlVk_ = nullptr; }
    if (hGgmlCpu_) { FreeLibrary(hGgmlCpu_); hGgmlCpu_ = nullptr; }

    // Clear function pointers
    fn_backend_init = nullptr;
    fn_load_model = nullptr;
    // ... (all others implicitly nullptr after DLL unload)
}

// ============================================================================
// LoadModel - Load GGUF model file
// ============================================================================
bool LlamaNativeBridge::LoadModel(const wchar_t* modelPath, int32_t gpuLayers, uint32_t ctxSize) {
    if (!hLlama_) {
        SetError("DLL not initialized");
        return false;
    }

    UnloadModel();  // Unload any existing model

    // Convert wide path to UTF-8
    char pathUtf8[1024];
    int pathLen = WideCharToMultiByte(CP_UTF8, 0, modelPath, -1, pathUtf8, sizeof(pathUtf8), nullptr, nullptr);
    if (pathLen == 0) {
        SetError("Invalid model path encoding");
        return false;
    }

    // Get default params
    llama_model_params mParams = {};
    if (fn_model_default_params) {
        mParams = fn_model_default_params();
    }
    
    // Override GPU layers (-1 = max offload)
    mParams.n_gpu_layers = (gpuLayers < 0) ? 999 : gpuLayers;
    mParams.use_mmap = true;
    mParams.use_mlock = false;

    // Load model
    model_ = fn_load_model(pathUtf8, mParams);
    if (!model_) {
        SetError("Failed to load model file (invalid GGUF or missing tensors)");
        return false;
    }

    // Get context params
    llama_context_params cParams = {};
    if (fn_context_default_params) {
        cParams = fn_context_default_params();
    }
    cParams.n_ctx = ctxSize;
    cParams.n_batch = 512;
    cParams.n_ubatch = 512;
    cParams.n_threads = 8;        // 7800X3D optimal
    cParams.n_threads_batch = 8;
    cParams.flash_attn = true;    // Enable FlashAttention if available

    // Create context
    ctx_ = fn_new_context(model_, cParams);
    if (!ctx_) {
        fn_free_model(model_);
        model_ = nullptr;
        SetError("Failed to create inference context (OOM?)");
        return false;
    }

    // Initialize batch
    batch_ = fn_batch_init(512, 0, 1);

    // Store model info
    modelInfo_.loaded = true;
    modelInfo_.path = pathUtf8;
    if (fn_model_n_vocab) {
        modelInfo_.n_vocab = fn_model_n_vocab(model_);
    }
    if (fn_token_bos) {
        modelInfo_.bos = fn_token_bos(model_);
    }
    if (fn_token_eos) {
        modelInfo_.eos = fn_token_eos(model_);
    }

    return true;
}

// ============================================================================
// UnloadModel - Free model and context
// ============================================================================
void LlamaNativeBridge::UnloadModel() {
    if (sampler_ && fn_sampler_free) {
        fn_sampler_free(sampler_);
        sampler_ = nullptr;
    }

    if (batch_.token && fn_batch_free) {
        fn_batch_free(batch_);
        batch_ = {};
    }

    if (ctx_ && fn_free_context) {
        fn_free_context(ctx_);
        ctx_ = nullptr;
    }

    if (model_ && fn_free_model) {
        fn_free_model(model_);
        model_ = nullptr;
    }

    modelInfo_ = {};
}

// ============================================================================
// ClearKVCache - Reset for new conversation
// ============================================================================
void LlamaNativeBridge::ClearKVCache() {
    if (ctx_ && fn_kv_cache_clear) {
        fn_kv_cache_clear(ctx_);
    }
}

// ============================================================================
// SetupSampler - Configure sampling chain
// ============================================================================
bool LlamaNativeBridge::SetupSampler(float temp, float topP, int32_t topK) {
    // Free existing sampler
    if (sampler_ && fn_sampler_free) {
        fn_sampler_free(sampler_);
        sampler_ = nullptr;
    }

    // Use greedy if sampler chain not available
    if (!fn_sampler_chain_init || !fn_sampler_chain_add) {
        return true;  // Fall back to argmax in Generate()
    }

    // Create sampler chain
    llama_context_params sparams = {};
    sampler_ = fn_sampler_chain_init(sparams);
    if (!sampler_) return false;

    // Add samplers in order: temp -> top_k -> top_p
    if (fn_sampler_init_temp && temp > 0.0f) {
        auto tempSampler = fn_sampler_init_temp(temp);
        if (tempSampler) fn_sampler_chain_add(sampler_, tempSampler);
    }

    if (fn_sampler_init_top_k && topK > 0) {
        auto topKSampler = fn_sampler_init_top_k(topK);
        if (topKSampler) fn_sampler_chain_add(sampler_, topKSampler);
    }

    if (fn_sampler_init_top_p && topP < 1.0f) {
        auto topPSampler = fn_sampler_init_top_p(topP, 1);
        if (topPSampler) fn_sampler_chain_add(sampler_, topPSampler);
    }

    return true;
}

// ============================================================================
// Generate - Synchronous text generation
// ============================================================================
LlamaNativeBridge::GenerationResult LlamaNativeBridge::Generate(
    const std::string& prompt,
    int32_t maxTokens,
    float temperature,
    float topP,
    int32_t topK
) {
    GenerationResult result;

    if (!ctx_) {
        result.error = "Model not loaded";
        return result;
    }

    auto timeStart = std::chrono::high_resolution_clock::now();

    // Clear KV cache for fresh generation
    ClearKVCache();

    // Setup sampler chain
    SetupSampler(temperature, topP, topK);

    // ========================================================================
    // 1. Tokenize prompt
    // ========================================================================
    int32_t nPromptTokens = fn_tokenize(
        model_,
        prompt.c_str(),
        static_cast<int32_t>(prompt.length()),
        tokenBuf_.data(),
        static_cast<int32_t>(tokenBuf_.size()),
        true,   // add_special (BOS)
        false   // parse_special
    );

    if (nPromptTokens < 0) {
        result.error = "Tokenization failed (prompt too long?)";
        return result;
    }
    result.prompt_tokens = nPromptTokens;

    // ========================================================================
    // 2. Decode prompt tokens (fill KV cache)
    // ========================================================================
    for (int32_t i = 0; i < nPromptTokens; ++i) {
        // Setup batch for single token
        batch_.n_tokens = 1;
        batch_.token[0] = tokenBuf_[i];
        batch_.pos[0] = i;
        batch_.n_seq_id[0] = 1;
        batch_.seq_id[0][0] = 0;
        batch_.logits[0] = (i == nPromptTokens - 1) ? 1 : 0;  // Only last token needs logits

        int32_t decodeResult = fn_decode(ctx_, batch_);
        if (decodeResult != 0) {
            std::stringstream ss;
            ss << "Decode failed at prompt token " << i << " (code " << decodeResult << ")";
            result.error = ss.str();
            return result;
        }
    }

    auto timePromptDone = std::chrono::high_resolution_clock::now();
    result.t_prompt_ms = std::chrono::duration<double, std::milli>(timePromptDone - timeStart).count();

    // ========================================================================
    // 3. Generation loop
    // ========================================================================
    int32_t nPast = nPromptTokens;
    result.text.reserve(maxTokens * 4);  // Rough estimate

    for (int32_t i = 0; i < maxTokens; ++i) {
        // Get logits for last decoded token
        float* logits = fn_get_logits_ith(ctx_, -1);
        if (!logits) {
            result.error = "Failed to get logits";
            break;
        }

        // Sample next token
        llama_token nextToken;
        
        if (sampler_ && fn_sampler_sample) {
            // Use sampler chain
            nextToken = fn_sampler_sample(sampler_, ctx_, -1);
        } else {
            // Greedy argmax fallback
            int32_t vocabSize = modelInfo_.n_vocab > 0 ? modelInfo_.n_vocab : 32000;
            nextToken = 0;
            float maxLogit = logits[0];
            for (int32_t v = 1; v < vocabSize; ++v) {
                if (logits[v] > maxLogit) {
                    maxLogit = logits[v];
                    nextToken = v;
                }
            }
        }

        // Check for EOS
        if (nextToken == modelInfo_.eos || nextToken == 2) {
            break;
        }

        // Detokenize
        if (fn_token_to_piece) {
            int32_t nChars = fn_token_to_piece(
                model_, nextToken,
                pieceBuf_.data(), static_cast<int32_t>(pieceBuf_.size()),
                0, false
            );
            if (nChars > 0) {
                result.text.append(pieceBuf_.data(), nChars);
            }
        }

        // Decode next token
        batch_.n_tokens = 1;
        batch_.token[0] = nextToken;
        batch_.pos[0] = nPast;
        batch_.n_seq_id[0] = 1;
        batch_.seq_id[0][0] = 0;
        batch_.logits[0] = 1;

        int32_t decodeResult = fn_decode(ctx_, batch_);
        if (decodeResult != 0) {
            result.error = "Generation decode failed";
            break;
        }

        ++nPast;
        ++result.tokens_generated;
    }

    auto timeGenDone = std::chrono::high_resolution_clock::now();
    result.t_gen_ms = std::chrono::duration<double, std::milli>(timeGenDone - timePromptDone).count();
    result.success = true;

    return result;
}
