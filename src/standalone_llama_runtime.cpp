#include "standalone_llama_runtime.hpp"

#include <windows.h>

#include <chrono>
#include <cstring>

namespace RawrXD::Standalone {

namespace {

constexpr int32_t kMaxPromptTokens = 4096;
constexpr int32_t kMaxCtxTokens = 4096;

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
    int32_t _pad0 = 0;
    const int32_t* token = nullptr;
    const float* embd = nullptr;
    const int32_t* pos = nullptr;
    const int32_t* n_seq_id = nullptr;
    const std::uintptr_t* seq_id = nullptr;
    const std::int8_t* logits = nullptr;
    std::uint8_t all_logits = 0;
    std::uint8_t _pad1[7] = {};
};
static_assert(sizeof(llama_batch_abi) == 64, "llama_batch ABI size must match llama.dll expectations");

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
    using p_backend_init = void (*)(bool numa);
    using p_backend_free = void (*)();

    using p_model_default_params_sret = void (*)(void* out_params);
    using p_ctx_default_params_sret = void (*)(void* out_params);

    using p_load_model_indirect = void* (*)(const char* path, const void* params_blob);
    using p_free_model = void (*)(void* model);
    using p_new_ctx_indirect = void* (*)(void* model, const void* ctx_params_blob);
    using p_free_ctx = void (*)(void* ctx);

    using p_tokenize = int32_t (*)(void* model,
                                   const char* text,
                                   int32_t text_len,
                                   int32_t* tokens,
                                   int32_t n_max,
                                   bool add_special,
                                   bool parse_special);

    using p_decode_indirect = int32_t (*)(void* ctx, const void* batch_blob);
    using p_get_logits = float* (*)(void* ctx);

    using p_n_vocab = int32_t (*)(void* model);
    using p_token_eos = int32_t (*)(void* model);
    using p_token_to_piece = int32_t (*)(void* model,
                                         int32_t token,
                                         char* buf,
                                         int32_t buf_len,
                                         int32_t lstrip,
                                         bool special);

    using p_kv_cache_clear = void (*)(void* ctx);

    p_backend_init backend_init = nullptr;
    p_backend_free backend_free = nullptr;
    p_model_default_params_sret model_default_params = nullptr;
    p_ctx_default_params_sret ctx_default_params = nullptr;
    p_load_model_indirect load_model_from_file = nullptr;
    p_free_model free_model = nullptr;
    p_new_ctx_indirect new_context_with_model = nullptr;
    p_free_ctx free_ctx = nullptr;
    p_tokenize tokenize = nullptr;
    p_decode_indirect decode = nullptr;
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

    std::memset(m_model_params_blob.data(), 0, m_model_params_blob.size());
    std::memset(m_ctx_params_blob.data(), 0, m_ctx_params_blob.size());
    if (m_fn->model_default_params) m_fn->model_default_params(m_model_params_blob.data());
    if (m_fn->ctx_default_params) m_fn->ctx_default_params(m_ctx_params_blob.data());
    return true;
}

bool LlamaRuntime::load_dlls(std::string& error) {
    (void)error;
    if (m_h_llama) return true;

    // Side-by-side DLLs:
    // - ggml.dll (optional)
    // - ggml-vulkan.dll (optional)
    // - ggml-cpu.dll (optional)
    // - llama.dll (required)
    m_h_ggml = (void*)load_side_by_side(L"ggml.dll");
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
    f->load_model_from_file = (Fn::p_load_model_indirect)req(h, "llama_load_model_from_file", error);
    if (!f->load_model_from_file) goto fail;
    f->new_context_with_model = (Fn::p_new_ctx_indirect)req(h, "llama_new_context_with_model", error);
    if (!f->new_context_with_model) goto fail;
    f->free_ctx = (Fn::p_free_ctx)req(h, "llama_free", error);
    if (!f->free_ctx) goto fail;
    f->free_model = (Fn::p_free_model)req(h, "llama_free_model", error);
    if (!f->free_model) goto fail;

    f->tokenize = (Fn::p_tokenize)req(h, "llama_tokenize", error);
    if (!f->tokenize) goto fail;
    f->decode = (Fn::p_decode_indirect)req(h, "llama_decode", error);
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
    f->backend_init = (Fn::p_backend_init)GetProcAddress(h, "llama_backend_init");
    f->backend_free = (Fn::p_backend_free)GetProcAddress(h, "llama_backend_free");
    f->model_default_params = (Fn::p_model_default_params_sret)GetProcAddress(h, "llama_model_default_params");
    f->ctx_default_params = (Fn::p_ctx_default_params_sret)GetProcAddress(h, "llama_context_default_params");
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
    if (m_fn && m_fn->backend_init) {
        m_fn->backend_init(false);
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

    // Refresh defaults; tolerate minor ABI drift by patching only well-known prefix fields.
    std::memset(m_model_params_blob.data(), 0, m_model_params_blob.size());
    if (m_fn->model_default_params) m_fn->model_default_params(m_model_params_blob.data());
    // Patch n_gpu_layers at struct offset 0 (llama.cpp stable prefix).
    std::memcpy(m_model_params_blob.data(), &gpu_layers, sizeof(gpu_layers));

    std::string path_utf8 = narrow_utf8(gguf_path);
    if (path_utf8.empty()) {
        error = "Invalid model path";
        return false;
    }

    void* model = m_fn->load_model_from_file(path_utf8.c_str(), m_model_params_blob.data());
    if (!model) {
        error = "llama_load_model_from_file failed";
        return false;
    }

    m_model = model;
    m_vocab = m_fn->n_vocab(m_model);
    m_eos = m_fn->token_eos(m_model);
    m_loaded_model_path = gguf_path;
    m_loaded_gpu_layers = gpu_layers;

    // Context is created on-demand in generate() so we can recreate/clear between requests.
    return true;
}

bool LlamaRuntime::create_context_locked(std::string& error) {
    if (m_ctx) return true;
    if (!m_model) {
        error = "Model not loaded";
        return false;
    }

    std::memset(m_ctx_params_blob.data(), 0, m_ctx_params_blob.size());
    if (m_fn->ctx_default_params) m_fn->ctx_default_params(m_ctx_params_blob.data());

    // Patch a stable prefix subset of llama_context_params.
    // Layout for b3506+ (prefix):
    //   uint32 seed
    //   int32  n_ctx
    //   int32  n_batch
    //   int32  n_ubatch
    //   int32  n_seq_max
    //   int32  n_threads
    //   int32  n_threads_batch
    struct CtxPrefix {
        std::uint32_t seed;
        std::int32_t n_ctx;
        std::int32_t n_batch;
        std::int32_t n_ubatch;
        std::int32_t n_seq_max;
        std::int32_t n_threads;
        std::int32_t n_threads_batch;
    };
    static_assert(sizeof(CtxPrefix) == 28, "CtxPrefix size");
    CtxPrefix p{};
    std::memcpy(&p, m_ctx_params_blob.data(), sizeof(CtxPrefix));
    p.n_ctx = kMaxCtxTokens;
    p.n_batch = 512;
    p.n_threads = 16;
    p.n_threads_batch = 16;
    std::memcpy(m_ctx_params_blob.data(), &p, sizeof(CtxPrefix));

    void* ctx = m_fn->new_context_with_model(m_model, m_ctx_params_blob.data());
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

LlamaGenerateResult LlamaRuntime::generate(const std::string& prompt, int32_t max_tokens) {
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

    int32_t n_prompt = m_fn->tokenize(m_model, prompt.c_str(), (int32_t)prompt.size(),
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

    llama_batch_abi batch{};
    batch.n_tokens = n_prompt;
    batch.token = m_tokens.data();
    batch.embd = nullptr;
    batch.pos = m_pos.data();
    batch.n_seq_id = m_n_seq_id.data();
    batch.seq_id = m_seq_id_ptrs.data();
    batch.logits = m_logits_flags.data();
    batch.all_logits = 0;

    int32_t dec = m_fn->decode(m_ctx, &batch);
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

    int32_t budget = max_tokens;
    if (budget <= 0) budget = 1;
    if (budget > 4096) budget = 4096;

    for (; gen < budget; gen++) {
        float* logits = m_fn->get_logits(m_ctx);
        if (!logits) break;

        int32_t tok = greedy_argmax(logits, m_vocab);
        if (tok == m_eos) break;

        int32_t n = m_fn->token_to_piece(m_model, tok, m_piece.data(), (int32_t)m_piece.size(), 0, false);
        if (n > 0) text.append(m_piece.data(), m_piece.data() + n);

        // next token batch
        m_tokens[0] = tok;
        m_pos[0] = cur_pos;
        m_n_seq_id[0] = 1;
        m_seq_id_ptrs[0] = (std::uintptr_t)&seq0;
        m_logits_flags[0] = 1;

        llama_batch_abi b2{};
        b2.n_tokens = 1;
        b2.token = m_tokens.data();
        b2.pos = m_pos.data();
        b2.n_seq_id = m_n_seq_id.data();
        b2.seq_id = m_seq_id_ptrs.data();
        b2.logits = m_logits_flags.data();
        b2.all_logits = 0;

        int32_t dec2 = m_fn->decode(m_ctx, &b2);
        if (dec2 < 0) break;
        cur_pos++;
    }

    auto t_done = std::chrono::high_resolution_clock::now();
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

