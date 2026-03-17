#include "win32/agentic/model_invoker.hpp"
#include "win32/agentic/ide_agent_bridge.hpp"
// Placeholder implementation – full functionality will be added later.
// For now, the ModelInvoker provides a minimal synchronous interface that
// returns a dummy plan containing a single no‑op action. This allows the
// rest of the agentic pipeline (ActionExecutor and IDEAgentBridge) to be
// compiled and exercised without requiring an actual LLM backend.

#include "model_invoker.hpp"
#include <thread>

namespace RawrXD {

ModelInvoker::ModelInvoker()
    : m_backend(LLMBackend::OLLAMA), m_endpoint("http://localhost:11434"),
      m_apiKey(""), m_model(""), m_temperature(0.7f), m_maxTokens(1024),
      m_timeout(std::chrono::seconds(30)), m_cacheEnabled(true) {}

ModelInvoker::~ModelInvoker() {}

void ModelInvoker::setBackend(LLMBackend backend) { m_backend = backend; }
void ModelInvoker::setEndpoint(const std::string& endpoint) { m_endpoint = endpoint; }
void ModelInvoker::setApiKey(const std::string& apiKey) { m_apiKey = apiKey; }
void ModelInvoker::setModel(const std::string& model) { m_model = model; }
void ModelInvoker::setTemperature(float temperature) { m_temperature = temperature; }
void ModelInvoker::setMaxTokens(int maxTokens) { m_maxTokens = maxTokens; }
void ModelInvoker::setTimeout(std::chrono::milliseconds timeout) { m_timeout = timeout; }

LLMResponse ModelInvoker::invoke(const std::string& wish) {
    // Simple cache lookup
    if (m_cacheEnabled) {
        std::string key = generateCacheKey(wish);
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            return it->second;
        }
    }

    // In a full implementation we would build a prompt and call the LLM.
    // Here we synthesize a minimal plan with a single no‑op action.
    LLMResponse resp;
    resp.success = true;
    resp.rawResponse = "{\"actions\":[{\"type\":\"noop\",\"description\":\"No operation\"}]}";
    resp.plan.wish = wish;
    resp.plan.id = "plan_dummy";
    Action dummy(ActionType::FILE_EDIT, "No‑op action");
    resp.plan.actions.push_back(dummy);
    resp.plan.status = PlanStatus::READY;

    if (m_cacheEnabled) {
        m_cache[generateCacheKey(wish)] = resp;
    }
    return resp;
}

void ModelInvoker::invokeAsync(const std::string& wish, PlanCallback callback) {
    std::thread([this, wish, callback]() {
        LLMResponse resp = this->invoke(wish);
        if (callback) callback(resp);
    }).detach();
}

bool ModelInvoker::validatePlan(const ExecutionPlan& plan) {
    return !plan.actions.empty();
}

void ModelInvoker::clearCache() { m_cache.clear(); }
void ModelInvoker::setCacheEnabled(bool enabled) { m_cacheEnabled = enabled; }

void ModelInvoker::setPlanGenerationStartedCallback(std::function<void(const std::string&)> callback) {
    m_planGenerationStartedCallback = std::move(callback);
}
void ModelInvoker::setPlanGeneratedCallback(std::function<void(const LLMResponse&)> callback) {
    m_planGeneratedCallback = std::move(callback);
}
void ModelInvoker::setInvocationErrorCallback(std::function<void(const std::string&, bool)> callback) {
    m_invocationErrorCallback = std::move(callback);
}

std::string ModelInvoker::buildPrompt(const std::string& wish) {
    // Minimal placeholder – real implementation would embed tool list, system prompt, etc.
    return "Generate a JSON plan for: " + wish;
}

LLMResponse ModelInvoker::invokeOllama(const std::string& wish) {
    // Placeholder – return dummy response identical to invoke()
    return invoke(wish);
}

LLMResponse ModelInvoker::invokeClaude(const std::string& wish) { return invoke(wish); }
LLMResponse ModelInvoker::invokeOpenAI(const std::string& wish) { return invoke(wish); }
LLMResponse ModelInvoker::invokeLocalGGUF(const std::string& wish) { return invoke(wish); }

ExecutionPlan ModelInvoker::parseResponse(const std::string& response) {
    // Very simple parser – expects the dummy format used in invoke().
    ExecutionPlan plan;
    plan.wish = "";
    plan.id = "plan_parsed";
    Action act(ActionType::FILE_EDIT, "Parsed dummy action");
    plan.actions.push_back(act);
    plan.status = PlanStatus::READY;
    return plan;
}

std::string ModelInvoker::httpPost(const std::string&, const std::string&, const std::string&) {
    // Not used in placeholder implementation.
    return "";
}

std::string ModelInvoker::generateCacheKey(const std::string& wish) {
    std::hash<std::string> hasher;
    return std::to_string(hasher(wish));
}

} // namespace RawrXD