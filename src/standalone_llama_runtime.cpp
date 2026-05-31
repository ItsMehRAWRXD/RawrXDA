#include "standalone_llama_runtime.hpp"

#include <windows.h>

#include <chrono>
#include <cstdlib>
#include <cstring>

namespace RawrXD::Standalone {

namespace {

constexpr int32_t kMaxPromptTokens = 4096;
constexpr int32_t kMaxCtxTokens = 4096;

using llama_token_abi = int32_t;
using llama_pos_abi = int32_t;
using llama_seq_id_abi = int32_t;
using ggml_rxd_backend_dev_t_abi = void*;
using ggml_rxd_backend_buffer_type_t_abi = void*;
using ggml_rxd_backend_sched_eval_callback_abi = bool (*)(void*, bool, void*);
using ggml_rxd_abort_callback_abi = bool (*)(void*);
using llama_progress_callback_abi = bool (*)(float, void*);

static std::wstring dll_dir_of_current_exe() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p(path);
    auto pos = p.find_last_of(L"\\/");
    if (pos != std::wstring::npos) p.resize(pos);
    return p;
}

static bool file_exists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static HMODULE load_side_by_side(const std::wstring& dll) {
    const auto dir = dll_dir_of_current_exe();
    std::wstring full = dir;
    if (!full.empty() && full.back() != L'\\') full += L'\\';
    full += dll;
    if (file_exists(full)) return LoadLibraryW(full.c_str());
    return LoadLibraryW(dll.c_str());
}

struct llama_batch_abi {
    int32_t n_tokens = 0;
    llama_token_abi* token = nullptr;
    float* embd = nullptr;
    llama_pos_abi* pos = nullptr;
    int32_t* n_seq_id = nullptr;
    llama_seq_id_abi** seq_id = nullptr;
    int8_t* logits = nullptr;
};
static_assert(sizeof(llama_batch_abi) == 56, "llama_batch ABI size must match llama.dll expectations");

struct llama_model_tensor_buft_override_abi {
    const char* pattern = nullptr;
    ggml_rxd_backend_buffer_type_t_abi buft = nullptr;
};

struct llama_model_kv_override_abi {
    int32_t tag = 0;
    char key[128] = {};
    union {
        int64_t val_i64;
        double val_f64;
        bool val_bool;
        char val_str[128];
    } value{};
};

struct llama_model_params_abi {
    ggml_rxd_backend_dev_t_abi* devices = nullptr;
    const llama_model_tensor_buft_override_abi* tensor_buft_overrides = nullptr;
    int32_t n_gpu_layers = 0;
    int32_t split_mode = 0;
    int32_t main_gpu = 0;
    const float* tensor_split = nullptr;
    llama_progress_callback_abi progress_callback = nullptr;
    void* progress_callback_user_data = nullptr;
    const llama_model_kv_override_abi* kv_overrides = nullptr;
    bool vocab_only = false;
    bool use_mmap = false;
    bool use_direct_io = false;
    bool use_mlock = false;
    bool check_tensors = false;
    bool use_extra_bufts = false;
    bool no_host = false;
    bool no_alloc = false;
};

struct llama_context_params_abi {
    uint32_t n_ctx = 0;
    uint32_t n_batch = 0;
    uint32_t n_ubatch = 0;
    uint32_t n_seq_max = 0;
    int32_t n_threads = 0;
    int32_t n_threads_batch = 0;
    int32_t rope_scaling_type = 0;
    int32_t pooling_type = 0;
    int32_t attention_type = 0;
    int32_t flash_attn_type = 0;
    float rope_freq_base = 0.0f;
    float rope_freq_scale = 0.0f;
    float yarn_ext_factor = 0.0f;
    float yarn_attn_factor = 0.0f;
    float yarn_beta_fast = 0.0f;
    float yarn_beta_slow = 0.0f;
    uint32_t yarn_orig_ctx = 0;
    float defrag_thold = 0.0f;
    ggml_rxd_backend_sched_eval_callback_abi cb_eval = nullptr;
    void* cb_eval_user_data = nullptr;
    int32_t type_k = 0;
    int32_t type_v = 0;
    ggml_rxd_abort_callback_abi abort_callback = nullptr;
    void* abort_callback_data = nullptr;
    bool embeddings = false;
    bool offload_kqv = false;
    bool no_perf = false;
    bool op_offload = false;
    bool swa_full = false;
    bool kv_unified = false;
    void* samplers = nullptr;
    size_t n_samplers = 0;
};

static int32_t greedy_argmax(const float* logits, int32_t n_vocab) {
    if (!logits || n_vocab <= 0) return 0;
    int32_t best = 0;
    float bestv = logits[0];
    for (int32_t i = 1; i < n_vocab; i++) {
        float v = logits[i];
        if (v > bestv) {
            bestv = v;
            best = i;
        }
    }
    return best;
}

} // namespace

struct LlamaRuntime::Fn {
    using p_ggml_backend_load_all = void (*)();
    using p_backend_init = void (*)();
    using p_backend_free = void (*)();

    using p_model_default_params = llama_model_params_abi (*)();
    using p_ctx_default_params = llama_context_params_abi (*)();

    using p_load_model = void* (*)(const char* path, llama_model_params_abi params);
    using p_free_model = void (*)(void* model);
    using p_model_get_vocab = const void* (*)(const void* model);
    using p_new_ctx = void* (*)(void* model, llama_context_params_abi params);
    using p_free_ctx = void (*)(void* ctx);

    using p_tokenize = int32_t (*)(const void* vocab,
                                   const char* text,
                                   int32_t text_len,
                                   int32_t* tokens,
                                   int32_t n_max,
                                   bool add_special,
                                   bool parse_special);

    using p_batch_get_one = llama_batch_abi (*)(llama_token_abi* tokens, int32_t n_tokens);
    using p_decode = int32_t (*)(void* ctx, llama_batch_abi batch);
    using p_get_logits = float* (*)(void* ctx);

    using p_n_vocab = int32_t (*)(const void* vocab);
    using p_token_eos = int32_t (*)(const void* vocab);
    using p_token_to_piece = int32_t (*)(const void* vocab,
                                         int32_t token,
                                         char* buf,
                                         int32_t buf_len,
                                         int32_t lstrip,
                                         bool special);

    using p_kv_cache_clear = void (*)(void* ctx);

    p_ggml_backend_load_all ggml_rxd_backend_load_all = nullptr;
    p_backend_init backend_init = nullptr;
    p_backend_free backend_free = nullptr;
    p_model_default_params model_default_params = nullptr;
    p_ctx_default_params ctx_default_params = nullptr;
    p_load_model load_model_from_file = nullptr;
    p_free_model free_model = nullptr;
    p_model_get_vocab model_get_vocab = nullptr;
    p_new_ctx new_context_with_model = nullptr;
    p_free_ctx free_ctx = nullptr;
    p_tokenize tokenize = nullptr;
    p_batch_get_one batch_get_one = nullptr;
    p_decode decode = nullptr;
    p_get_logits get_logits = nullptr;
    p_n_vocab n_vocab = nullptr;
    p_token_eos token_eos = nullptr;
    p_token_to_piece token_to_piece = nullptr;
    p_kv_cache_clear kv_cache_clear = nullptr;
};

LlamaRuntime::LlamaRuntime() {
    m_model_params_blob.resize(1024);
    m_ctx_params_blob.resize(1024);
    m_tokens.resize(kMaxPromptTokens);
    m_pos.resize(kMaxPromptTokens);
    m_n_seq_id.resize(kMaxPromptTokens);
    m_seq_id_ptrs.resize(kMaxPromptTokens);
    m_logits_flags.resize(kMaxPromptTokens);
    m_piece.resize(512);
}

LlamaRuntime::~LlamaRuntime() {
    std::lock_guard<std::mutex> lock(m_mu);
    shutdown_locked();
}

std::wstring LlamaRuntime::widen_utf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w;
    w.resize((size_t)n);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

std::string LlamaRuntime::narrow_utf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string s;
    s.resize((size_t)n);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), s.data(), n, nullptr, nullptr);
    return s;
}

bool LlamaRuntime::ensure_initialized(std::string& error) {
    std::lock_guard<std::mutex> lock(m_mu);
    if (m_fn) return true;
    if (!load_dlls(error)) return false;
    if (!bind_exports(error)) return false;
    if (!init_backend(error)) return false;
    return true;
}

bool LlamaRuntime::load_dlls(std::string& error) {
    (void)error;
    if (m_h_llama) return true;

    // Side-by-side DLLs:
    // - ggml.dll (optional)
    // - ggml-base.dll (optional, newer split backend loader)
    // - ggml-vulkan.dll (optional)
    // - ggml-cpu.dll (optional)
    // - llama.dll (required)
    m_h_ggml = (void*)load_side_by_side(L"ggml.dll");
    if (!m_h_ggml) m_h_ggml = (void*)load_side_by_side(L"ggml-base.dll");
    if (!m_h_ggml) m_h_ggml = (void*)load_side_by_side(L"ggml-vulkan.dll");
    if (!m_h_ggml) m_h_ggml = (void*)load_side_by_side(L"ggml-cpu.dll");

    m_h_llama = (void*)load_side_by_side(L"llama.dll");
    if (!m_h_llama) {
        error = "llama.dll not found (x64 required) beside the EXE or on PATH";
        return false;
    }
    return true;
}

static FARPROC req(HMODULE h, const char* name, std::string& error) {
    auto p = GetProcAddress(h, name);
    if (!p) {
        error = std::string("Missing export: ") + name;
    }
    return p;
}

bool LlamaRuntime::bind_exports(std::string& error) {
    if (m_fn) return true;
    auto* h = (HMODULE)m_h_llama;
    auto* f = new Fn();

    // Required.
    f->load_model_from_file = (Fn::p_load_model)req(h, "llama_load_model_from_file", error);
    if (!f->load_model_from_file) goto fail;
    f->new_context_with_model = (Fn::p_new_ctx)req(h, "llama_new_context_with_model", error);
    if (!f->new_context_with_model) goto fail;
    f->free_ctx = (Fn::p_free_ctx)req(h, "llama_free", error);
    if (!f->free_ctx) goto fail;
    f->free_model = (Fn::p_free_model)req(h, "llama_free_model", error);
    if (!f->free_model) goto fail;
    f->model_get_vocab = (Fn::p_model_get_vocab)req(h, "llama_model_get_vocab", error);
    if (!f->model_get_vocab) goto fail;

    f->tokenize = (Fn::p_tokenize)req(h, "llama_tokenize", error);
    if (!f->tokenize) goto fail;
    f->batch_get_one = (Fn::p_batch_get_one)req(h, "llama_batch_get_one", error);
    if (!f->batch_get_one) goto fail;
    f->decode = (Fn::p_decode)req(h, "llama_decode", error);
    if (!f->decode) goto fail;
    f->get_logits = (Fn::p_get_logits)req(h, "llama_get_logits", error);
    if (!f->get_logits) goto fail;
    f->n_vocab = (Fn::p_n_vocab)req(h, "llama_n_vocab", error);
    if (!f->n_vocab) goto fail;
    f->token_eos = (Fn::p_token_eos)req(h, "llama_token_eos", error);
    if (!f->token_eos) goto fail;
    f->token_to_piece = (Fn::p_token_to_piece)req(h, "llama_token_to_piece", error);
    if (!f->token_to_piece) goto fail;

    // Optional.
    if (m_h_ggml) {
        f->ggml_rxd_backend_load_all = (Fn::p_ggml_backend_load_all)GetProcAddress((HMODULE)m_h_ggml, "ggml_rxd_backend_load_all");
    }
    if (!f->ggml_rxd_backend_load_all) {
        f->ggml_rxd_backend_load_all = (Fn::p_ggml_backend_load_all)GetProcAddress(h, "ggml_rxd_backend_load_all");
    }
    f->backend_init = (Fn::p_backend_init)GetProcAddress(h, "llama_backend_init");
    f->backend_free = (Fn::p_backend_free)GetProcAddress(h, "llama_backend_free");
    f->model_default_params = (Fn::p_model_default_params)GetProcAddress(h, "llama_model_default_params");
    f->ctx_default_params = (Fn::p_ctx_default_params)GetProcAddress(h, "llama_context_default_params");
    f->kv_cache_clear = (Fn::p_kv_cache_clear)GetProcAddress(h, "llama_kv_cache_clear");

    m_fn = f;
    return true;

fail:
    delete f;
    shutdown_locked();
    return false;
}

bool LlamaRuntime::init_backend(std::string& error) {
    (void)error;
    if (m_fn && m_fn->ggml_rxd_backend_load_all) {
        m_fn->ggml_rxd_backend_load_all();
    }
    if (m_fn && m_fn->backend_init) {
        m_fn->backend_init();
    }
    return true;
}

void LlamaRuntime::shutdown_locked() {
    destroy_context_locked();
    if (m_model && m_fn && m_fn->free_model) {
        m_fn->free_model(m_model);
        m_model = nullptr;
    }
    if (m_fn && m_fn->backend_free) {
        m_fn->backend_free();
    }
    if (m_fn) {
        delete m_fn;
        m_fn = nullptr;
    }
    if (m_h_llama) {
        FreeLibrary((HMODULE)m_h_llama);
        m_h_llama = nullptr;
    }
    if (m_h_ggml) {
        FreeLibrary((HMODULE)m_h_ggml);
        m_h_ggml = nullptr;
    }
    m_vocab_handle = nullptr;
    m_loaded_model_path.clear();
    m_loaded_gpu_layers = 0;
    m_vocab = 0;
    m_eos = -1;
}

bool LlamaRuntime::ensure_model_loaded(const std::wstring& gguf_path, int32_t gpu_layers, std::string& error) {
    std::lock_guard<std::mutex> lock(m_mu);
    if (!m_fn) {
        error = "Runtime not initialized";
        return false;
    }

    if (!m_loaded_model_path.empty() && gguf_path == m_loaded_model_path && gpu_layers == m_loaded_gpu_layers && m_model) {
        return true;
    }

    destroy_context_locked();
    if (m_model) {
        m_fn->free_model(m_model);
        m_model = nullptr;
    }

    llama_model_params_abi model_params{};
    if (m_fn->model_default_params) {
        model_params = m_fn->model_default_params();
    }
    int32_t requested_gpu_layers = gpu_layers;
    const char* forceLayersEnv = std::getenv("RAWRXD_FORCE_GPU_LAYERS");
    if (forceLayersEnv && forceLayersEnv[0] != '\0') {
        requested_gpu_layers = static_cast<int32_t>(std::atoi(forceLayersEnv));
    }
    // GPU inference is mandatory: clamp any non-positive request up to 999 (full offload).
    // RAWRXD_FORCE_VULKAN is honored implicitly because every load now offloads layers.
    if (requested_gpu_layers <= 0) {
        requested_gpu_layers = 999;
    }
    model_params.n_gpu_layers = requested_gpu_layers;
    if (requested_gpu_layers > 0) {
        // LLAMA_SPLIT_MODE_LAYER = 1 in llama.h; avoids default "none" in older/mixed builds.
        model_params.split_mode = 1;
    }

    std::string path_utf8 = narrow_utf8(gguf_path);
    if (path_utf8.empty()) {
        error = "Invalid model path";
        return false;
    }

    void* model = m_fn->load_model_from_file(path_utf8.c_str(), model_params);
    if (!model) {
        error = "llama_load_model_from_file failed";
        return false;
    }

    m_model = model;
    m_vocab_handle = m_fn->model_get_vocab(m_model);
    if (!m_vocab_handle) {
        error = "llama_model_get_vocab failed";
        m_fn->free_model(m_model);
        m_model = nullptr;
        return false;
    }
    m_vocab = m_fn->n_vocab(m_vocab_handle);
    m_eos = m_fn->token_eos(m_vocab_handle);
    m_loaded_model_path = gguf_path;
    m_loaded_gpu_layers = requested_gpu_layers;

    // Context is created on-demand in generate() so we can recreate/clear between requests.
    return true;
}

bool LlamaRuntime::create_context_locked(std::string& error) {
    if (m_ctx) return true;
    if (!m_model) {
        error = "Model not loaded";
        return false;
    }

    llama_context_params_abi ctx_params{};
    if (m_fn->ctx_default_params) {
        ctx_params = m_fn->ctx_default_params();
    }
    ctx_params.n_ctx = kMaxCtxTokens;
    ctx_params.n_batch = 512;
    ctx_params.n_ubatch = 512;
    ctx_params.n_threads = 16;
    ctx_params.n_threads_batch = 16;

    void* ctx = m_fn->new_context_with_model(m_model, ctx_params);
    if (!ctx) {
        error = "llama_new_context_with_model failed";
        return false;
    }
    m_ctx = ctx;
    return true;
}

void LlamaRuntime::destroy_context_locked() {
    if (m_ctx && m_fn && m_fn->free_ctx) {
        m_fn->free_ctx(m_ctx);
        m_ctx = nullptr;
    }
}

LlamaGenerateResult LlamaRuntime::generate(const std::string& prompt,
                                           int32_t max_tokens,
                                           const std::function<void(const std::string&)>& on_token) {
    LlamaGenerateResult out;
    auto t0 = std::chrono::high_resolution_clock::now();

    std::unique_lock<std::mutex> lock(m_mu);
    if (!m_fn || !m_model) {
        out.error = "Model not loaded";
        return out;
    }
    std::string err;
    if (!create_context_locked(err)) {
        out.error = err;
        return out;
    }
    if (m_fn->kv_cache_clear) m_fn->kv_cache_clear(m_ctx);

    int32_t n_prompt = m_fn->tokenize(m_vocab_handle, prompt.c_str(), (int32_t)prompt.size(),
                                      m_tokens.data(), (int32_t)m_tokens.size(),
                                      true, false);
    if (n_prompt <= 0 || n_prompt > (int32_t)m_tokens.size()) {
        out.error = "Tokenization failed";
        return out;
    }
    out.prompt_tokens = n_prompt;

    const int32_t seq0 = 0;
    for (int32_t i = 0; i < n_prompt; i++) {
        m_pos[(size_t)i] = i;
        m_n_seq_id[(size_t)i] = 1;
        m_seq_id_ptrs[(size_t)i] = (std::uintptr_t)&seq0;
        m_logits_flags[(size_t)i] = (i == (n_prompt - 1)) ? 1 : 0;
    }

    llama_batch_abi batch = m_fn->batch_get_one(m_tokens.data(), n_prompt);
    int32_t dec = m_fn->decode(m_ctx, batch);
    if (dec < 0) {
        out.error = "Decode failed on prompt";
        return out;
    }

    auto t_prompt_done = std::chrono::high_resolution_clock::now();
    out.t_prompt_ms = std::chrono::duration<double, std::milli>(t_prompt_done - t0).count();

    int32_t cur_pos = n_prompt;
    int32_t gen = 0;
    std::string text;
    text.reserve((size_t)max_tokens * 4u);
    bool first_token_seen = false;
    std::chrono::high_resolution_clock::time_point t_first_token{};

    int32_t budget = max_tokens;
    if (budget <= 0) budget = 1;
    if (budget > 4096) budget = 4096;

    for (; gen < budget; gen++) {
        float* logits = m_fn->get_logits(m_ctx);
        if (!logits) break;

        int32_t tok = greedy_argmax(logits, m_vocab);
        if (tok == m_eos) break;

        int32_t n = m_fn->token_to_piece(m_vocab_handle, tok, m_piece.data(), (int32_t)m_piece.size(), 0, false);
        if (n > 0) {
            if (!first_token_seen) {
                t_first_token = std::chrono::high_resolution_clock::now();
                first_token_seen = true;
            }
            std::string piece(m_piece.data(), m_piece.data() + n);
            text.append(piece);
            if (on_token) {
                on_token(piece);
            }
        }

        // next token batch
        m_tokens[0] = tok;
        m_pos[0] = cur_pos;
        m_n_seq_id[0] = 1;
        m_seq_id_ptrs[0] = (std::uintptr_t)&seq0;
        m_logits_flags[0] = 1;

        llama_batch_abi b2 = m_fn->batch_get_one(m_tokens.data(), 1);
        int32_t dec2 = m_fn->decode(m_ctx, b2);
        if (dec2 < 0) break;
        cur_pos++;
    }

    auto t_done = std::chrono::high_resolution_clock::now();
    if (first_token_seen) {
        out.ttft_ms = std::chrono::duration<double, std::milli>(t_first_token - t0).count();
    }
    out.t_gen_ms = std::chrono::duration<double, std::milli>(t_done - t_prompt_done).count();
    out.generated_tokens = gen;
    out.text = std::move(text);
    out.ok = true;
    return out;
}

std::string LlamaRuntime::loaded_model_path_utf8() const {
    std::lock_guard<std::mutex> lock(m_mu);
    return narrow_utf8(m_loaded_model_path);
}

} // namespace RawrXD::Standalone

