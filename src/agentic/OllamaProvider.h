// ============================================================================
// OllamaProvider.h — Ollama-backed Prediction Provider
// ============================================================================
// Implements PredictionProvider for local Ollama instances.
//
// Features:
//   - Synchronous and streaming predictions
//   - FIM (Fill-in-Middle) prompt format for Qwen2.5-Coder
//   - Cancellable in-flight requests
//   - Connection health check (IsAvailable)
//   - WinHTTP-based (no external HTTP dependencies)
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "PredictionProvider.h"
#include <atomic>
#include <mutex>

namespace RawrXD {
namespace Prediction {

class OllamaProvider : public PredictionProvider {
public:
    OllamaProvider();
    explicit OllamaProvider(const std::string& baseUrl);
    ~OllamaProvider() override;

    // ---- Configuration ----
    void Configure(const PredictionConfig& config) override;
    bool IsAvailable() const override;

    // ---- Synchronous prediction ----
    PredictionResult Predict(const PredictionContext& ctx) override;

    // ---- Streaming prediction ----
    void PredictStreaming(const PredictionContext& ctx,
                          StreamTokenCallback callback) override;

    // ---- Cancellation ----
    void Cancel() override;

    // ---- Ollama-specific ----
    void SetBaseUrl(const std::string& url) { m_baseUrl = url; }
    std::string GetBaseUrl() const { return m_baseUrl; }
    bool CheckConnection() const;

private:
    // ---- HTTP helpers ----
    std::string PostJson(const std::string& endpoint,
                          const std::string& body,
                          bool& success) const;

    void PostJsonStreaming(const std::string& endpoint,
                           const std::string& body,
                           std::function<bool(const std::string& chunk)> onChunk) const;

    // ---- State ----
    std::string m_baseUrl;
    PredictionConfig m_config;
    mutable std::atomic<bool> m_cancelled{false};
    mutable std::mutex m_mutex;
};

} // namespace Prediction
} // namespace RawrXD
