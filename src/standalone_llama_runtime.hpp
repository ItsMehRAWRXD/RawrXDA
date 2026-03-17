#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD::Standalone {

struct LlamaGenerateResult {
    bool ok = false;
    std::string text;
    std::string error;
    int32_t prompt_tokens = 0;
    int32_t generated_tokens = 0;
    double t_prompt_ms = 0.0;
    double t_gen_ms = 0.0;
};

class LlamaRuntime {
public:
    LlamaRuntime();
    ~LlamaRuntime();

    LlamaRuntime(const LlamaRuntime&) = delete;
    LlamaRuntime& operator=(const LlamaRuntime&) = delete;

    bool ensure_initialized(std::string& error);
    bool ensure_model_loaded(const std::wstring& gguf_path, int32_t gpu_layers, std::string& error);

    LlamaGenerateResult generate(const std::string& prompt, int32_t max_tokens);

    std::string loaded_model_path_utf8() const;

private:
    struct Fn;
    bool bind_exports(std::string& error);
    bool load_dlls(std::string& error);
    bool init_backend(std::string& error);
    void shutdown_locked();

    bool create_context_locked(std::string& error);
    void destroy_context_locked();

    static std::string narrow_utf8(const std::wstring& ws);
    static std::wstring widen_utf8(const std::string& s);

    mutable std::mutex m_mu;
    void* m_h_ggml = nullptr;
    void* m_h_llama = nullptr;
    Fn* m_fn = nullptr;

    void* m_model = nullptr;
    void* m_ctx = nullptr;

    int32_t m_vocab = 0;
    int32_t m_eos = -1;

    std::vector<std::uint8_t> m_model_params_blob;
    std::vector<std::uint8_t> m_ctx_params_blob;

    std::vector<int32_t> m_tokens;
    std::vector<int32_t> m_pos;
    std::vector<int32_t> m_n_seq_id;
    std::vector<std::uintptr_t> m_seq_id_ptrs;
    std::vector<std::int8_t> m_logits_flags;
    std::vector<char> m_piece;

    std::wstring m_loaded_model_path;
    int32_t m_loaded_gpu_layers = 0;
};

} // namespace RawrXD::Standalone

