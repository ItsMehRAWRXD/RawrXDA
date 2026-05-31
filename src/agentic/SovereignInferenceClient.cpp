// ============================================================================
// SovereignInferenceClient.cpp — Host-less Implementation
// REAL backend: LlamaNativeBridge (llama.dll + ggml-vulkan.dll)
// Zero HTTP. Zero JSON. Direct LoadLibraryW + GetProcAddress.
// ============================================================================
#include "SovereignInferenceClient.h"
#include "../inference/RawrXD_LlamaNative.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>
#include <sstream>


#include "agentic/tool_call_parser.h"

namespace RawrXD
{
namespace Agent
{

class SovereignInferenceClient::Impl
{
  public:
    explicit Impl(const SovereignModelConfig& cfg) : cfg_(cfg) {}

    bool LoadModel(const std::string& path)
    {
        if (bridge_.IsModelLoaded())
            Unload();

        std::wstring wpath(path.begin(), path.end());
        std::wstring dllDir;
        if (!cfg_.dll_dir.empty())
        {
            dllDir = std::wstring(cfg_.dll_dir.begin(), cfg_.dll_dir.end());
        }

        if (!bridge_.Initialize(dllDir.empty() ? nullptr : dllDir.c_str()))
        {
            lastError_ = bridge_.GetLastError();
            return false;
        }

        // Apply speculative decoding config
        LlamaNativeBridge::SpeculativeConfig specCfg;
        specCfg.enabled = cfg_.enable_speculative;
        specCfg.draft_tokens = static_cast<int32_t>(cfg_.draft_tokens);
        bridge_.SetSpeculativeConfig(specCfg);

        // Apply KV cache quantization if configured
        if (cfg_.kv_quant_type_k > 0 || cfg_.kv_quant_type_v > 0)
        {
            bridge_.SetKVQuantType(static_cast<int32_t>(cfg_.kv_quant_type_k),
                                   static_cast<int32_t>(cfg_.kv_quant_type_v));
        }

        int32_t gpuLayers = (cfg_.n_gpu_layers == 99) ? -1 : static_cast<int32_t>(cfg_.n_gpu_layers);
        if (!bridge_.LoadModel(wpath.c_str(), gpuLayers, cfg_.context_size))
        {
            lastError_ = bridge_.GetLastError();
            return false;
        }
        return true;
    }

    void Unload()
    {
        bridge_.UnloadModel();
        bridge_.Shutdown();
    }

    bool IsLoaded() const { return bridge_.IsModelLoaded(); }
    void ClearKVCache() { bridge_.ClearKVCache(); }

    InferenceResult ChatSync(const std::vector<ChatMessage>& messages, const nlohmann::json& tools)
    {
        InferenceResult res{};
        if (!bridge_.IsModelLoaded())
        {
            res.error_message = lastError_.empty() ? "[ERR] No model loaded" : lastError_;
            return res;
        }

        auto t0 = std::chrono::high_resolution_clock::now();

        std::string prompt = BuildPrompt(messages);
        if (prompt.empty())
        {
            res.error_message = "[ERR] Empty prompt";
            return res;
        }

        // Inject tool descriptions into system prompt if tools provided
        if (!tools.empty() && tools.is_array())
        {
            prompt = InjectToolsPrompt(prompt, tools);
        }

        auto gen = bridge_.Generate(prompt, static_cast<int32_t>(cfg_.max_tokens), cfg_.temperature,
                                    0.95f,  // top_p
                                    40      // top_k
        );

        auto t1 = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

        if (gen.success)
        {
            res.success = true;
            res.response = gen.text;
            res.completion_tokens = static_cast<uint64_t>(gen.tokens_generated);
            res.prompt_tokens = static_cast<uint64_t>(gen.prompt_tokens);
            res.total_duration_ms = dur.count();
            res.tokens_per_sec = (dur.count() > 0) ? (gen.tokens_generated * 1000.0 / dur.count()) : 0.0;

            // Parse tool calls from response
            ParseToolCalls(res, tools);
        }
        else
        {
            res.error_message = gen.error.empty() ? "[ERR] Inference failed" : gen.error;
        }
        return res;
    }

    bool ChatStream(const std::vector<ChatMessage>& messages, const nlohmann::json& tools, TokenCallback on_token,
                    ToolCallCallback on_tool_call, DoneCallback on_done, ErrorCallback on_error)
    {
        if (!bridge_.IsModelLoaded())
        {
            on_error(lastError_.empty() ? "[ERR] No model loaded" : lastError_);
            return false;
        }

        std::string prompt = BuildPrompt(messages);
        if (prompt.empty())
        {
            on_error("[ERR] Empty prompt");
            return false;
        }

        if (!tools.empty() && tools.is_array())
        {
            prompt = InjectToolsPrompt(prompt, tools);
        }

        auto t0 = std::chrono::high_resolution_clock::now();
        std::string fullResponse;
        uint64_t promptTokens = 0;
        uint64_t completionTokens = 0;

        auto gen = bridge_.GenerateStream(
            prompt,
            [&](const std::string& piece)
            {
                on_token(piece);
                fullResponse += piece;
            },
            static_cast<int32_t>(cfg_.max_tokens), cfg_.temperature, 0.95f, 40);

        auto t1 = std::chrono::high_resolution_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);

        if (!gen.success)
        {
            on_error(gen.error.empty() ? "[ERR] Inference failed" : gen.error);
            return false;
        }

        promptTokens = static_cast<uint64_t>(gen.prompt_tokens);
        completionTokens = static_cast<uint64_t>(gen.tokens_generated);

        // Parse tool calls from complete response
        InferenceResult tmpRes;
        tmpRes.response = fullResponse;
        ParseToolCalls(tmpRes, tools);
        if (tmpRes.has_tool_calls)
        {
            for (const auto& tc : tmpRes.tool_calls)
            {
                on_tool_call(tc.first, tc.second);
            }
        }

        double tps = (dur.count() > 0) ? (gen.tokens_generated * 1000.0 / dur.count()) : 0.0;
        on_done(fullResponse, promptTokens, completionTokens, tps);
        return true;
    }

    double GetTokensPerSec() const { return tokensPerSec_; }

    void UpdateTokensPerSec(double tps) { tokensPerSec_ = tps; }

    std::string InjectToolsPrompt(const std::string& prompt, const nlohmann::json& tools)
    {
        std::ostringstream oss;
        oss << "You have access to the following tools:\n";
        for (const auto& tool : tools)
        {
            if (tool.contains("function"))
            {
                const auto& fn = tool["function"];
                oss << "- " << fn.value("name", "unknown") << ": " << fn.value("description", "") << "\n";
                if (fn.contains("parameters"))
                {
                    oss << "  Parameters: " << fn["parameters"].dump() << "\n";
                }
            }
        }
        oss << "\nTo call a tool, respond with a JSON object in this format:\n"
            << "{\"tool_calls\": [{\"name\": \"...\", \"arguments\": {...}}]}\n\n"
            << prompt;
        return oss.str();
    }

    void ParseToolCalls(InferenceResult& res, const nlohmann::json& /*tools*/)
    {
        res.tool_calls.clear();
        const auto parsed = RawrXD::Agentic::ToolCallParser::parse(res.response);
        if (!parsed.hasToolCalls)
        {
            res.has_tool_calls = false;
            return;
        }
        for (const auto& tc : parsed.toolCalls)
        {
            res.tool_calls.emplace_back(tc.name, tc.arguments);
        }
        res.has_tool_calls = !res.tool_calls.empty();
    }

  private:
    SovereignModelConfig cfg_;
    LlamaNativeBridge bridge_;
    std::string lastError_;
    double tokensPerSec_ = 0.0;

    std::string BuildPrompt(const std::vector<ChatMessage>& messages)
    {
        std::ostringstream oss;
        for (const auto& m : messages)
        {
            if (m.role == "system")
            {
                oss << "<|system|>\n" << m.content << "\n";
            }
            else if (m.role == "user")
            {
                oss << "<|user|>\n" << m.content << "\n";
            }
            else if (m.role == "assistant")
            {
                oss << "<|assistant|>\n" << m.content << "\n";
            }
        }
        oss << "<|assistant|>\n";
        return oss.str();
    }
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
SovereignInferenceClient::SovereignInferenceClient(const SovereignModelConfig& cfg)
    : pImpl_(std::make_unique<Impl>(cfg)), m_config(cfg)
{
}

SovereignInferenceClient::~SovereignInferenceClient() = default;

bool SovereignInferenceClient::LoadModel(const std::string& gguf_path)
{
    return pImpl_ && pImpl_->LoadModel(gguf_path);
}

void SovereignInferenceClient::UnloadModel()
{
    if (pImpl_)
        pImpl_->Unload();
}

bool SovereignInferenceClient::IsLoaded() const
{
    return pImpl_ && pImpl_->IsLoaded();
}

void SovereignInferenceClient::ClearKVCache()
{
    if (pImpl_)
        pImpl_->ClearKVCache();
}

InferenceResult SovereignInferenceClient::ChatSync(const std::vector<ChatMessage>& messages,
                                                   const nlohmann::json& tools)
{
    if (!pImpl_)
        return InferenceResult::error("[ERR] Null impl");
    m_totalRequests++;
    auto res = pImpl_->ChatSync(messages, tools);
    if (res.success)
    {
        m_totalTokens += res.completion_tokens;
        m_totalDurationMs += res.total_duration_ms;
    }
    return res;
}

bool SovereignInferenceClient::ChatStream(const std::vector<ChatMessage>& messages, const nlohmann::json& tools,
                                          TokenCallback on_token, ToolCallCallback on_tool_call, DoneCallback on_done,
                                          ErrorCallback on_error)
{

    if (!pImpl_)
    {
        on_error("[ERR] Null impl");
        return false;
    }
    m_totalRequests++;
    bool ok = pImpl_->ChatStream(messages, tools, on_token, on_tool_call, on_done, on_error);
    return ok;
}

double SovereignInferenceClient::GetAvgTokensPerSec() const
{
    if (!pImpl_)
        return 0.0;
    uint64_t totalTok = m_totalTokens.load();
    double totalDur = m_totalDurationMs;
    if (totalDur > 0.0 && totalTok > 0)
    {
        return (totalTok * 1000.0) / totalDur;
    }
    return pImpl_->GetTokensPerSec();
}

}  // namespace Agent
}  // namespace RawrXD
