#include "AgentOllamaClient.h"
#include "BackendOrchestrator.h"
#include "hotpatch/Engine.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

using RawrXD::Agent::AgentOllamaClient;
using RawrXD::Agent::InferenceResult;
using RawrXD::Agent::OllamaHealth;

namespace {
constexpr int kMaxRetries = 3;
constexpr int kRetryBaseDelayMs = 100;

extern "C" unsigned int rawr_cpu_has_avx2();
}

AgentOllamaClient::AgentOllamaClient(const OllamaConfig& config)
    : m_config(config) {
    if (!RawrXD::BackendOrchestrator::Instance().IsInitialized()) {
        RawrXD::BackendOrchestrator::Instance().Initialize();
    }

    RawrXD::Agentic::Hotpatch::Engine::instance().setModelTemperature(m_config.temperature);

    (void)rawr_cpu_has_avx2();
}

AgentOllamaClient::~AgentOllamaClient() {
    CancelStream();
}

std::string AgentOllamaClient::BuildPromptFromMessages(const std::vector<ChatMessage>& messages,
                                                       const nlohmann::json& tools) const {
    std::string prompt;

    for (const auto& msg : messages) {
        if (msg.role == "system") {
            prompt += "System: " + msg.content + "\n\n";
            break;
        }
    }

    for (const auto& msg : messages) {
        if (msg.role == "user") {
            prompt += "User: " + msg.content + "\n";
        } else if (msg.role == "assistant") {
            prompt += "Assistant: " + msg.content + "\n";
        } else if (msg.role == "tool") {
            prompt += "Tool result: " + msg.content + "\n";
        }
    }

    if (!tools.empty() && tools.is_array()) {
        prompt += "\nAvailable tools:\n";
        for (const auto& tool : tools) {
            if (tool.contains("function")) {
                const auto& func = tool["function"];
                prompt += "- " + func.value("name", "") + ": " +
                          func.value("description", "") + "\n";
            }
        }
        prompt += "\nTo call a tool, respond with JSON: {\"tool_call\": {\"name\": \"tool_name\", \"arguments\": {...}}}\n";
    }

    prompt += "Assistant: ";
    return prompt;
}

void AgentOllamaClient::ParseToolCallsFromResponse(const std::string& response,
                                                   InferenceResult& result) const {
    size_t json_start = response.find("{");
    if (json_start == std::string::npos) {
        return;
    }

    size_t json_end = response.rfind("}");
    if (json_end == std::string::npos || json_end < json_start) {
        return;
    }

    std::string json_str = response.substr(json_start, json_end - json_start + 1);
    try {
        nlohmann::json j = nlohmann::json::parse(json_str);
        if (j.contains("tool_call") && j["tool_call"].is_object()) {
            const auto& tc = j["tool_call"];
            std::string name = tc.value("name", "");
            nlohmann::json args = tc.value("arguments", nlohmann::json::object());
            if (!name.empty()) {
                result.has_tool_calls = true;
                result.tool_calls.emplace_back(name, args);
                result.response = response.substr(0, json_start);
            }
        }
    } catch (...) {
    }
}

bool AgentOllamaClient::TestConnection() {
    return TestConnectionWithStats().ok;
}

OllamaHealth AgentOllamaClient::TestConnectionWithStats() {
    OllamaHealth h;
    auto t0 = std::chrono::steady_clock::now();

    auto& bo = RawrXD::BackendOrchestrator::Instance();
    h.ok = bo.IsInitialized() && !bo.GetLoadedModelTags().empty();
    h.model_count = static_cast<int>(bo.GetLoadedModelTags().size());

    auto t1 = std::chrono::steady_clock::now();
    h.latency_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    h.version = "RawrXD-v14.7.3";

    return h;
}

std::string AgentOllamaClient::GetVersion() {
    return "RawrXD-v14.7.3";
}

std::vector<std::string> AgentOllamaClient::ListModels() {
    return RawrXD::BackendOrchestrator::Instance().GetLoadedModelTags();
}

InferenceResult AgentOllamaClient::ChatSync(const std::vector<ChatMessage>& messages,
                                            const nlohmann::json& tools) {
    std::string prompt = BuildPromptFromMessages(messages, tools);

    RawrXD::InferRequest req;
    req.id = m_nextRequestId++;
    req.prompt = prompt;
    req.max_tokens = m_config.max_tokens;
    req.tenant_id = "agentic";

    std::promise<std::string> completion_promise;
    std::promise<std::string> metadata_promise;

    req.complete_cb = [&](const std::string& completion, const std::string& metadata) {
        completion_promise.set_value(completion);
        metadata_promise.set_value(metadata);
    };

    auto start_time = std::chrono::steady_clock::now();
    auto& bo = RawrXD::BackendOrchestrator::Instance();
    uint64_t req_id = bo.Enqueue(req);

    std::future<std::string> completion_future = completion_promise.get_future();
    if (completion_future.wait_for(std::chrono::seconds(120)) != std::future_status::ready) {
        bo.Cancel(req_id);
        return InferenceResult::error("Inference timeout");
    }

    InferenceResult result;
    result.success = true;
    result.has_tool_calls = false;
    result.response = completion_future.get();
    if (result.response.rfind("[BackendError]", 0) == 0) {
        return InferenceResult::error(result.response);
    }
    result.prompt_tokens = 0;
    result.completion_tokens = 0;
    result.tokens_per_sec = 0.0;

    try {
        std::string metadata = metadata_promise.get_future().get();
        nlohmann::json j = nlohmann::json::parse(metadata);
        if (j.contains("prompt_eval_count")) {
            result.prompt_tokens = j["prompt_eval_count"].get<uint64_t>();
        }
        if (j.contains("eval_count")) {
            result.completion_tokens = j["eval_count"].get<uint64_t>();
        }
        if (j.contains("eval_duration") && result.completion_tokens > 0) {
            uint64_t eval_ns = j["eval_duration"].get<uint64_t>();
            result.tokens_per_sec = static_cast<double>(result.completion_tokens) /
                                    (static_cast<double>(eval_ns) / 1e9);
        }
    } catch (...) {
    }

    auto end_time = std::chrono::steady_clock::now();
    result.total_duration_ms = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());

    ParseToolCallsFromResponse(result.response, result);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_totalDurationMs += result.total_duration_ms;
    }
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);
    m_totalTokens.fetch_add(result.completion_tokens, std::memory_order_relaxed);

    return result;
}

bool AgentOllamaClient::ChatStream(const std::vector<ChatMessage>& messages,
                                   const nlohmann::json& tools,
                                   TokenCallback on_token,
                                   ToolCallCallback on_tool_call,
                                   DoneCallback on_done,
                                   ErrorCallback on_error) {
    (void)on_error;

    std::string prompt = BuildPromptFromMessages(messages, tools);

    RawrXD::InferRequest req;
    req.id = m_nextRequestId++;
    req.prompt = prompt;
    req.max_tokens = m_config.max_tokens;
    req.tenant_id = "agentic";

    m_streaming.store(true);
    m_cancelRequested.store(false);
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    auto start_time = std::chrono::steady_clock::now();

    auto full_response = std::make_shared<std::string>();
    auto prompt_tokens = std::make_shared<uint64_t>(0);
    auto completion_tokens = std::make_shared<uint64_t>(0);
    auto tps = std::make_shared<double>(0.0);

    req.stream_cb = [this, on_token, full_response](const std::string& token) {
        if (m_cancelRequested.load()) {
            return;
        }
        full_response->append(token);
        if (on_token) {
            on_token(token);
        }
    };

    req.complete_cb = [=](const std::string& completion, const std::string& metadata) {
        if (completion.rfind("[BackendError]", 0) == 0) {
            m_streaming.store(false);
            if (on_error) {
                on_error(completion);
            }
            return;
        }

        *full_response = completion;

        try {
            nlohmann::json j = nlohmann::json::parse(metadata);
            if (j.contains("prompt_eval_count")) {
                *prompt_tokens = j["prompt_eval_count"].get<uint64_t>();
            }
            if (j.contains("eval_count")) {
                *completion_tokens = j["eval_count"].get<uint64_t>();
            }
            if (j.contains("eval_duration") && *completion_tokens > 0) {
                uint64_t eval_ns = j["eval_duration"].get<uint64_t>();
                *tps = static_cast<double>(*completion_tokens) /
                       (static_cast<double>(eval_ns) / 1e9);
            }
        } catch (...) {
        }

        InferenceResult parsed;
        parsed.success = true;
        parsed.has_tool_calls = false;
        parsed.response = completion;
        ParseToolCallsFromResponse(completion, parsed);

        if (on_tool_call) {
            for (const auto& tool_call : parsed.tool_calls) {
                on_tool_call(tool_call.first, tool_call.second);
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        double elapsed_ms = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_totalDurationMs += elapsed_ms;
        }
        m_totalTokens.fetch_add(*completion_tokens, std::memory_order_relaxed);
        m_streaming.store(false);

        if (on_done) {
            on_done(completion, *prompt_tokens, *completion_tokens, *tps);
        }
    };

    RawrXD::BackendOrchestrator::Instance().Enqueue(req);
    return true;
}

InferenceResult AgentOllamaClient::FIMSync(const std::string& prefix,
                                           const std::string& suffix,
                                           const std::string& filename) {
    (void)filename;

    std::string prompt = prefix + "<FILL>" + suffix;

    RawrXD::InferRequest req;
    req.id = m_nextRequestId++;
    req.prompt = prompt;
    req.max_tokens = m_config.fim_max_tokens;
    req.tenant_id = "agentic";

    auto completion_promise = std::make_shared<std::promise<std::string>>();

    req.complete_cb = [completion_promise](const std::string& completion, const std::string& metadata) {
        (void)metadata;
        completion_promise->set_value(completion);
    };

    auto& bo = RawrXD::BackendOrchestrator::Instance();
    uint64_t req_id = bo.Enqueue(req);

    std::future<std::string> completion_future = completion_promise->get_future();
    if (completion_future.wait_for(std::chrono::seconds(60)) != std::future_status::ready) {
        bo.Cancel(req_id);
        return InferenceResult::error("FIM timeout");
    }

    std::string response = completion_future.get();
    if (response.rfind("[BackendError]", 0) == 0) {
        return InferenceResult::error(response);
    }

    size_t fill_pos = response.find("<FILL>");
    if (fill_pos != std::string::npos) {
        std::string fill = response.substr(fill_pos + 6);
        if (!suffix.empty() && fill.size() >= suffix.size() &&
            fill.compare(fill.size() - suffix.size(), suffix.size(), suffix) == 0) {
            fill.resize(fill.size() - suffix.size());
        }
        return InferenceResult::ok(fill);
    }

    return InferenceResult::ok(response);
}

bool AgentOllamaClient::FIMStream(const std::string& prefix,
                                  const std::string& suffix,
                                  const std::string& filename,
                                  TokenCallback on_token,
                                  DoneCallback on_done,
                                  ErrorCallback on_error) {
    (void)filename;
    (void)on_error;

    std::string prompt = prefix + "<FILL>" + suffix;

    RawrXD::InferRequest req;
    req.id = m_nextRequestId++;
    req.prompt = prompt;
    req.max_tokens = m_config.fim_max_tokens;
    req.tenant_id = "agentic";

    m_streaming.store(true);
    m_cancelRequested.store(false);
    m_totalRequests.fetch_add(1, std::memory_order_relaxed);

    auto start_time = std::chrono::steady_clock::now();
    auto completion_tokens = std::make_shared<uint64_t>(0);
    auto tps = std::make_shared<double>(0.0);

    req.stream_cb = [this, on_token](const std::string& token) {
        if (m_cancelRequested.load()) {
            return;
        }
        if (on_token) {
            on_token(token);
        }
    };

    req.complete_cb = [=](const std::string& completion, const std::string& metadata) {
        if (completion.rfind("[BackendError]", 0) == 0) {
            m_streaming.store(false);
            if (on_error) {
                on_error(completion);
            }
            return;
        }

        try {
            nlohmann::json j = nlohmann::json::parse(metadata);
            if (j.contains("eval_count")) {
                *completion_tokens = j["eval_count"].get<uint64_t>();
            }
            if (j.contains("eval_duration") && *completion_tokens > 0) {
                uint64_t eval_ns = j["eval_duration"].get<uint64_t>();
                *tps = static_cast<double>(*completion_tokens) /
                       (static_cast<double>(eval_ns) / 1e9);
            }
        } catch (...) {
        }

        auto end_time = std::chrono::steady_clock::now();
        double elapsed_ms = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count());

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_totalDurationMs += elapsed_ms;
        }
        m_totalTokens.fetch_add(*completion_tokens, std::memory_order_relaxed);
        m_streaming.store(false);

        if (on_done) {
            on_done(completion, 0, *completion_tokens, *tps);
        }
    };

    RawrXD::BackendOrchestrator::Instance().Enqueue(req);
    return true;
}

void AgentOllamaClient::CancelStream() {
    m_cancelRequested.store(true);
}

bool AgentOllamaClient::WarmupConnection() {
    auto t0 = std::chrono::steady_clock::now();
    for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
        if (TestConnection()) {
            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            std::cerr << "[AgentOllamaClient] Native warmup succeeded in " << ms
                      << "ms (attempt " << (attempt + 1) << ")\n";
            return true;
        }
        if (attempt < kMaxRetries - 1) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(kRetryBaseDelayMs * (1 << attempt)));
        }
    }
    std::cerr << "[AgentOllamaClient] Native warmup failed after " << kMaxRetries
              << " attempts\n";
    return false;
}

bool AgentOllamaClient::CheckModelHealth(const std::string& modelName) {
    auto models = ListModels();
    for (const auto& m : models) {
        if (m == modelName || m.find(modelName) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool AgentOllamaClient::ShouldEmitError(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_mutex);

    while (m_recentErrors.size() > 10) {
        m_recentErrors.pop_front();
    }

    for (const auto& recent : m_recentErrors) {
        if (recent == msg) {
            return false;
        }
    }

    m_recentErrors.push_back(msg);
    m_consecutiveErrors++;
    return m_consecutiveErrors <= 10;
}

void AgentOllamaClient::SetConfig(const OllamaConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    RawrXD::Agentic::Hotpatch::Engine::instance().setModelTemperature(m_config.temperature);
}

double AgentOllamaClient::GetAvgTokensPerSec() const {
    uint64_t tokens = m_totalTokens.load(std::memory_order_relaxed);
    if (tokens == 0 || m_totalDurationMs <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(tokens) / (m_totalDurationMs / 1000.0);
}

AgentOllamaClient::MetricsSnapshot AgentOllamaClient::GetMetricsSnapshot() const {
    MetricsSnapshot snap;
    snap.totalRequests = m_totalRequests.load(std::memory_order_relaxed);
    snap.totalTokens = m_totalTokens.load(std::memory_order_relaxed);
    snap.avgTokensPerSec = GetAvgTokensPerSec();
    snap.isStreaming = m_streaming.load(std::memory_order_relaxed);
    snap.consecutiveErrors = m_consecutiveErrors;
    snap.chatModel = m_config.chat_model;
    snap.fimModel = m_config.fim_model;
    snap.host = m_config.host;
    snap.port = m_config.port;
    return snap;
}

InferenceResult AgentOllamaClient::ChatSyncWithRetry(const std::vector<ChatMessage>& messages,
                                                     const nlohmann::json& tools,
                                                     int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        InferenceResult result = ChatSync(messages, tools);
        if (result.success) {
            m_consecutiveErrors = 0;
            return result;
        }

        if (result.error_message.find("model not found") != std::string::npos ||
            result.error_message.find("invalid") != std::string::npos) {
            return result;
        }

        if (attempt < maxRetries - 1) {
            int delayMs = kRetryBaseDelayMs * (1 << attempt);
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }
    }

    return InferenceResult::error("ChatSync failed after retries");
}