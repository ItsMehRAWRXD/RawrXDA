#include "cpu_engine.hpp"

#include "../tokenizer/tokenizer_base.hpp"
#include "global_runtime_orchestrator.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <chrono>

namespace engine
{

void SamplingParams::apply_behavior_config(const RawrXD::BehaviorConfigV2& cfg)
{
    // BehaviorConfigV2 is about higher-level behaviors; we map a few knobs conservatively.
    // - More "adversarial" or "self-modeling" tends to reduce randomness for determinism.
    // - Speculative/predictive modes can increase top-k to keep options open.
    if (cfg.adversarial.enabled || cfg.self_model.meta_reasoning_depth > 0)
    {
        if (temperature > 0.7f)
            temperature = 0.7f;
        if (top_p > 0.9f)
            top_p = 0.9f;
    }
    if (cfg.speculative.enabled || cfg.predictive.enabled)
    {
        if (top_k < 60)
            top_k = 60;
    }
}

CPUEngine::CPUEngine() = default;

bool CPUEngine::load_model(const std::string& gguf_path)
{
    if (!m_engine.LoadModel(gguf_path))
    {
        m_last_error = "LoadModel failed";
        return false;
    }
    m_last_error.clear();
    return true;
}

bool CPUEngine::is_loaded() const
{
    return m_engine.IsModelLoaded();
}

void CPUEngine::set_tokenizer(std::shared_ptr<::tokenizer::TokenizerBase> tokenizer)
{
    m_tokenizer = std::move(tokenizer);
}

std::vector<int32_t> CPUEngine::encode(const std::string& text, bool add_special)
{
    if (m_tokenizer)
    {
        const auto enc = m_tokenizer->encode(text, add_special);
        return enc.token_ids;
    }
    return m_engine.Tokenize(text);
}

std::string CPUEngine::decode(const std::vector<int32_t>& tokens, bool skip_special)
{
    if (m_tokenizer)
    {
        ::tokenizer::DecodeOptions opt;
        opt.skip_special_tokens = skip_special;
        return m_tokenizer->decode(tokens, opt);
    }
    return m_engine.Detokenize(tokens);
}

int32_t CPUEngine::bos_token_id() const
{
    if (m_tokenizer)
        return m_tokenizer->bos_token_id();
    return m_engine.BosTokenId();
}

int32_t CPUEngine::eos_token_id() const
{
    if (m_tokenizer)
        return m_tokenizer->eos_token_id();
    return m_engine.EosTokenId();
}

int32_t CPUEngine::unk_token_id() const
{
    if (m_tokenizer)
        return m_tokenizer->unk_token_id();
    return m_engine.UnkTokenId();
}

void CPUEngine::clear_cache()
{
    m_engine.ClearCache();
}

void CPUEngine::apply_repetition_penalty_(std::vector<float>& logits, const std::vector<int32_t>& recent, float penalty)
{
    if (penalty == 1.0f)
        return;
    for (int32_t id : recent)
    {
        if (id < 0 || (size_t)id >= logits.size())
            continue;
        float& v = logits[(size_t)id];
        v = (v > 0.0f) ? (v / penalty) : (v * penalty);
    }
}

void CPUEngine::apply_banned_tokens_(std::vector<float>& logits, const std::vector<int32_t>& banned)
{
    for (int32_t id : banned)
    {
        if (id < 0 || (size_t)id >= logits.size())
            continue;
        logits[(size_t)id] = -INFINITY;
    }
}

int32_t CPUEngine::sample_top_k_top_p_(const std::vector<float>& probs, int top_k, float top_p)
{
    const int n = (int)probs.size();
    std::vector<int> idx(n);
    std::iota(idx.begin(), idx.end(), 0);

    // Sort indices by probability desc.
    std::partial_sort(idx.begin(), idx.begin() + std::min(std::max(top_k, 1), n), idx.end(),
                      [&](int a, int b) { return probs[(size_t)a] > probs[(size_t)b]; });

    std::vector<std::pair<int, float>> kept;
    kept.reserve((size_t)std::min(top_k, n));

    float cumsum = 0.0f;
    const int k_cap = (top_k <= 0) ? n : std::min(top_k, n);
    for (int i = 0; i < k_cap; ++i)
    {
        const int id = idx[(size_t)i];
        const float p = probs[(size_t)id];
        if (!(p > 0.0f))
            continue;
        kept.push_back({id, p});
        cumsum += p;
        if (top_p < 1.0f && cumsum >= top_p)
            break;
    }

    if (kept.empty())
        return 0;

    // Renormalize for sampling.
    float sum = 0.0f;
    for (auto& kv : kept)
        sum += kv.second;
    if (!(sum > 0.0f))
        return kept[0].first;
    for (auto& kv : kept)
        kv.second /= sum;

    // Sample from categorical distribution.
    thread_local std::mt19937 rng{std::random_device{}()};
    std::vector<float> w;
    w.reserve(kept.size());
    for (auto& kv : kept)
        w.push_back(kv.second);
    std::discrete_distribution<int> dist(w.begin(), w.end());
    const int pick = dist(rng);
    return kept[(size_t)pick].first;
}

int32_t CPUEngine::sample_token_(std::vector<float>& logits, const SamplingParams& params,
                                 const std::vector<int32_t>& recent)
{
    if (logits.empty())
        return 0;

    apply_banned_tokens_(logits, params.banned_tokens);
    apply_repetition_penalty_(logits, recent, params.repetition_penalty);

    const float temp = (params.temperature <= 0.0f) ? 1.0f : params.temperature;

    // Softmax(logits / temp)
    float max_logit = -INFINITY;
    for (float v : logits)
        max_logit = std::max(max_logit, v);

    std::vector<float> probs(logits.size(), 0.0f);
    float sum = 0.0f;
    for (size_t i = 0; i < logits.size(); ++i)
    {
        const float v = (logits[i] - max_logit) / temp;
        const float e = std::exp(v);
        probs[i] = e;
        sum += e;
    }
    if (!(sum > 0.0f))
        return 0;
    for (float& p : probs)
        p /= sum;

    return sample_top_k_top_p_(probs, params.top_k, params.top_p);
}

std::vector<int32_t> CPUEngine::generate(const std::vector<int32_t>& prompt_tokens, int max_tokens,
                                         SamplingParams params)
{
    if (!is_loaded() || prompt_tokens.empty())
        return {};

    params.apply_behavior_config(m_behavior);

    clear_cache();

    // Prefill prompt
    for (size_t i = 0; i < prompt_tokens.size(); ++i)
    {
        m_engine.Eval({prompt_tokens[i]});
    }

    std::vector<int32_t> out;
    out.reserve((size_t)max_tokens);

    std::vector<int32_t> all = prompt_tokens;
    int32_t cur = prompt_tokens.back();

    for (int step = 0; step < max_tokens; ++step)
    {
        std::vector<float> logits = m_engine.Eval({cur});
        const int32_t next = sample_token_(logits, params, all);
        out.push_back(next);
        all.push_back(next);
        cur = next;

        if (next == eos_token_id())
            break;
    }

    return out;
}

void CPUEngine::generate_streaming(const std::vector<int32_t>& prompt_tokens, int max_tokens, SamplingParams params,
                                   StreamCallback cb)
{
    generate_streaming(prompt_tokens, max_tokens, params, std::move(cb), /*cancel=*/nullptr);
}

void CPUEngine::generate_streaming(const std::vector<int32_t>& prompt_tokens, int max_tokens, SamplingParams params,
                                   StreamCallback cb, const std::atomic<bool>* cancel)
{
    if (!cb)
        return;
    if (!is_loaded() || prompt_tokens.empty())
    {
        cb("", true);
        return;
    }

    params.apply_behavior_config(m_behavior);

    clear_cache();

    for (size_t i = 0; i < prompt_tokens.size(); ++i)
    {
        if (cancel && cancel->load(std::memory_order_acquire))
        {
            cb("", true);
            return;
        }
        m_engine.Eval({prompt_tokens[i]});
    }

    std::vector<int32_t> all = prompt_tokens;
    int32_t cur = prompt_tokens.back();

    auto start_time = std::chrono::steady_clock::now();
    int tokens_generated = 0;

    for (int step = 0; step < max_tokens; ++step)
    {
        if (cancel && cancel->load(std::memory_order_acquire))
        {
            cb("", true);
            return;
        }

        // Mock acceptance rate for now (0.7-0.9) until draft model is fully wired
        float acceptance_rate = 0.85f;
        
        std::vector<float> logits = m_engine.Eval({cur});
        const int32_t next = sample_token_(logits, params, all);
        all.push_back(next);
        cur = next;
        tokens_generated++;

        // Update Orchestrator periodically
        if (tokens_generated % 5 == 0) {
            auto now = std::chrono::steady_clock::now();
            float seconds = std::chrono::duration<float>(now - start_time).count();
            float tps = (seconds > 0) ? (tokens_generated / seconds) : 0.0f;
            RawrXD::GlobalRuntimeOrchestrator::Get().UpdateInferenceMetrics(acceptance_rate, tps);
            
            // Mock memory pressure and cache reuse for HUD demonstration
            RawrXD::GlobalRuntimeOrchestrator::Get().UpdateMemoryMetrics(0.45f + (step * 0.001f));
            RawrXD::GlobalRuntimeOrchestrator::Get().UpdateCacheMetrics(0.92f);
        }

        if (next == eos_token_id())
        {
            cb("", true);
            return;
        }

        cb(decode({next}, /*skip_special=*/false), false);
    }

    cb("", true);
}

std::string CPUEngine::generate_text(const std::string& prompt, int max_tokens, SamplingParams params)
{
    auto prompt_tokens = encode(prompt, /*add_special=*/true);
    auto out_tokens = generate(prompt_tokens, max_tokens, params);
    return decode(out_tokens, /*skip_special=*/true);
}

void CPUEngine::generate_text_streaming(const std::string& prompt, int max_tokens, SamplingParams params,
                                        StreamCallback cb)
{
    auto prompt_tokens = encode(prompt, /*add_special=*/true);
    generate_streaming(prompt_tokens, max_tokens, params, std::move(cb), /*cancel=*/nullptr);
}

void CPUEngine::generate_text_streaming(const std::string& prompt, int max_tokens, SamplingParams params,
                                        StreamCallback cb, const std::atomic<bool>* cancel)
{
    auto prompt_tokens = encode(prompt, /*add_special=*/true);
    generate_streaming(prompt_tokens, max_tokens, params, std::move(cb), cancel);
}

}  // namespace engine
