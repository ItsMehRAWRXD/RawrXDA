#pragma once

#include "../cpu_inference_engine_Clean.h"
#include "../gguf_factory/behavior_config_v2.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>


namespace tokenizer
{
class TokenizerBase;
}

namespace engine
{

struct SamplingParams
{
    int top_k = 40;
    float top_p = 0.95f;
    float temperature = 1.0f;
    float repetition_penalty = 1.0f;
    std::vector<int32_t> banned_tokens;

    void apply_behavior_config(const RawrXD::BehaviorConfigV2& cfg);
};

using StreamCallback = std::function<void(const std::string& token, bool is_final)>;

class CPUEngine
{
  public:
    CPUEngine();

    bool load_model(const std::string& gguf_path);
    bool is_loaded() const;

    void set_tokenizer(std::shared_ptr<::tokenizer::TokenizerBase> tokenizer);
    ::tokenizer::TokenizerBase* get_tokenizer() { return m_tokenizer.get(); }
    const ::tokenizer::TokenizerBase* get_tokenizer() const { return m_tokenizer.get(); }

    std::vector<int32_t> encode(const std::string& text, bool add_special = true);
    std::string decode(const std::vector<int32_t>& tokens, bool skip_special = true);

    int32_t bos_token_id() const;
    int32_t eos_token_id() const;
    int32_t unk_token_id() const;

    std::vector<int32_t> generate(const std::vector<int32_t>& prompt_tokens, int max_tokens, SamplingParams params);
    void generate_streaming(const std::vector<int32_t>& prompt_tokens, int max_tokens, SamplingParams params,
                            StreamCallback cb);
    void generate_streaming(const std::vector<int32_t>& prompt_tokens, int max_tokens, SamplingParams params,
                            StreamCallback cb, const std::atomic<bool>* cancel);

    std::string generate_text(const std::string& prompt, int max_tokens, SamplingParams params);
    void generate_text_streaming(const std::string& prompt, int max_tokens, SamplingParams params, StreamCallback cb);
    void generate_text_streaming(const std::string& prompt, int max_tokens, SamplingParams params, StreamCallback cb,
                                 const std::atomic<bool>* cancel);

    void clear_cache();

    void set_behavior_config(const RawrXD::BehaviorConfigV2& cfg) { m_behavior = cfg; }
    const RawrXD::BehaviorConfigV2& behavior_config() const { return m_behavior; }

    const std::string& last_error() const { return m_last_error; }

  private:
    int32_t sample_token_(std::vector<float>& logits, const SamplingParams& params, const std::vector<int32_t>& recent);
    void apply_repetition_penalty_(std::vector<float>& logits, const std::vector<int32_t>& recent, float penalty);
    void apply_banned_tokens_(std::vector<float>& logits, const std::vector<int32_t>& banned);
    int32_t sample_top_k_top_p_(const std::vector<float>& probs, int top_k, float top_p);

    CPUInference::CPUInferenceEngine m_engine;
    std::shared_ptr<::tokenizer::TokenizerBase> m_tokenizer;
    RawrXD::BehaviorConfigV2 m_behavior;
    std::string m_last_error;
};

}  // namespace engine
