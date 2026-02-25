#include "ollama_proxy.h"

#include "backend/ollama_client.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <string>

namespace {

struct OllamaProxyCtx {
    explicit OllamaProxyCtx(std::string base_url)
        : client(std::move(base_url)) {}

    RawrXD::Backend::OllamaClient client;
    std::atomic<bool> stop_requested{false};
};

static std::string Trim(std::string s) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && is_space((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && is_space((unsigned char)s.back())) s.pop_back();
    return s;
}

static bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    }
    return true;
}

static bool ModelNameMatches(const std::string& want, const std::string& have) {
    // Exact match first, then allow implicit ":latest".
    if (EqualsIgnoreCase(want, have)) return true;
    if (want.find(':') == std::string::npos) {
        return EqualsIgnoreCase(want + ":latest", have);
    }
    return false;
}

static OllamaProxyCtx* EnsureCtx(void*& opaque_ctx, const std::string& base_url) {
    if (!opaque_ctx) {
        opaque_ctx = new OllamaProxyCtx(base_url);
    }
    return reinterpret_cast<OllamaProxyCtx*>(opaque_ctx);
}

} // namespace

OllamaProxy::~OllamaProxy() {
    auto* ctx = reinterpret_cast<OllamaProxyCtx*>(m_networkContext);
    delete ctx;
    m_networkContext = nullptr;
    m_currentRequest = nullptr;
}

void OllamaProxy::setModel(const std::string& modelName) {
    m_modelName = modelName;
}

bool OllamaProxy::isOllamaAvailable() {
    auto* ctx = EnsureCtx(m_networkContext, m_ollamaUrl);
    ctx->client.setBaseUrl(m_ollamaUrl);
    return ctx->client.isRunning();
}

bool OllamaProxy::isModelAvailable(const std::string& modelName) {
    auto* ctx = EnsureCtx(m_networkContext, m_ollamaUrl);
    ctx->client.setBaseUrl(m_ollamaUrl);

    if (!ctx->client.isRunning()) return false;

    const std::string want = Trim(modelName);
    if (want.empty()) return false;

    const auto models = ctx->client.listModels();
    for (const auto& m : models) {
        if (ModelNameMatches(want, m.name)) return true;
    }
    return false;
}

OllamaProxy::OllamaModelMeta OllamaProxy::getModelMetadata(const std::string& modelName) {
    OllamaModelMeta result;
    auto* ctx = EnsureCtx(m_networkContext, m_ollamaUrl);
    ctx->client.setBaseUrl(m_ollamaUrl);

    if (!ctx->client.isRunning()) return result;

    const std::string want = Trim(modelName);
    if (want.empty()) return result;

    const auto models = ctx->client.listModels();
    for (const auto& m : models) {
        if (ModelNameMatches(want, m.name)) {
            result.name = m.name;
            result.family = m.family;
            result.parameter_size = m.parameter_size;
            result.quantization_level = m.quantization_level;
            result.size = m.size;
            result.found = true;
            break;
        }
    }
    return result;
}

void OllamaProxy::generateResponse(const std::string& prompt, float temperature, int maxTokens) {
    auto* ctx = EnsureCtx(m_networkContext, m_ollamaUrl);
    ctx->client.setBaseUrl(m_ollamaUrl);
    ctx->stop_requested.store(false, std::memory_order_release);

    const std::string model = Trim(m_modelName);
    if (model.empty()) {
        if (m_onError) m_onError("OllamaProxy: no model selected");
        return;
    }
    if (Trim(prompt).empty()) {
        if (m_onError) m_onError("OllamaProxy: empty prompt");
        return;
    }
    if (!ctx->client.isRunning()) {
        if (m_onError) m_onError("OllamaProxy: Ollama not reachable at " + m_ollamaUrl);
        return;
    }

    RawrXD::Backend::OllamaGenerateRequest req;
    req.model = model;
    req.prompt = prompt;
    req.stream = false; // blocking call here; higher layers can add async later
    req.options["temperature"] = (double)temperature;
    req.options["num_predict"] = (double)maxTokens;

    const auto resp = ctx->client.generateSync(req);
    if (ctx->stop_requested.load(std::memory_order_acquire)) return;

    if (resp.error) {
        if (m_onError) m_onError("OllamaProxy: " + resp.error_message);
        return;
    }

    if (m_onTokenArrived && !resp.response.empty()) {
        m_onTokenArrived(resp.response);
    }
    if (m_onGenerationComplete) {
        m_onGenerationComplete();
    }
}

void OllamaProxy::stopGeneration() {
    auto* ctx = reinterpret_cast<OllamaProxyCtx*>(m_networkContext);
    if (ctx) ctx->stop_requested.store(true, std::memory_order_release);
}

void OllamaProxy::onNetworkReply() {
    // Legacy hook point (Qt version). Current implementation is synchronous via backend::OllamaClient.
}

void OllamaProxy::onNetworkError(int code) {
    if (m_onError) m_onError("OllamaProxy: network error code " + std::to_string(code));
}

