#pragma once

#include "language_server_integration.h"
#include "codebase_context_analyzer.hpp"
#include "streaming_completion_engine.hpp"
#include "intelligent_codebase_engine.hpp"
#include "agent_hot_patcher.hpp"

#include <chrono>

namespace rxd::lsp {

// RAII Request Scope for Telemetry & Cancellation
struct RequestScope {
    std::string method;
    std::string reqId;
    std::chrono::high_resolution_clock::time_point t0;
    ObservabilitySink* sink;
    std::atomic_flag* cancelFlag;
    
    RequestScope(const std::string& m, const std::string& id, ObservabilitySink* s, std::atomic_flag* cf)
        : method(m), reqId(id), t0(std::chrono::high_resolution_clock::now()), sink(s), cancelFlag(cf) {
        if(sink) sink->emitLspRequestStart(method, reqId);
    }
    
    ~RequestScope() {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - t0).count();
        if(sink) {
            bool cancelled = cancelFlag && cancelFlag->test();
            sink->emitLspRequestEnd(method, reqId, us, cancelled);
        }
    }
    
    bool isCancelled() const { return cancelFlag && cancelFlag->test(); }
};

// Disposal Token (VS Code CancellationToken analog)
class CancellationToken {
    std::shared_ptr<std::atomic_flag> m_flag;
public:
    CancellationToken() : m_flag(std::make_shared<std::atomic_flag>()) {}
    void cancel() { m_flag->test_and_set(); }
    bool isCancelled() const { return m_flag->test(); }
    std::atomic_flag* raw() const { return m_flag.get(); }
};

// Provider Implementation Core
class LanguageServerIntegrationImpl : public LanguageServerIntegration {
    std::map<std::string, QTextDocument*> m_documentBuffers;
    std::map<std::string, int64_t> m_documentVersions;
    
public:
    LanguageServerIntegrationImpl(std::shared_ptr<Logger> logger,
                                  std::shared_ptr<Metrics> metrics);
    
    void provideHover(const std::string& uri, const Position& pos, 
                     const CancellationToken& ct,
                     std::function<void(Hover)> cb) override;
    
    void provideDefinition(const std::string& uri, const Position& pos,
                          const CancellationToken& ct,
                          std::function<void(std::vector<Location>)> cb) override;
    
    void provideReferences(const std::string& uri, const Position& pos,
                          bool includeDecl,
                          const CancellationToken& ct,
                          std::function<void(std::vector<Location>)> cb) override;
    
    void provideDocumentSymbols(const std::string& uri,
                               const CancellationToken& ct,
                               std::function<void(std::vector<DocumentSymbol>)> cb) override;
    
    void provideCompletion(const std::string& uri, const Position& pos,
                          const CompletionContext& ctx,
                          const CancellationToken& ct,
                          std::function<void(CompletionList)> cb) override;
    
    void provideDiagnostics(const std::string& uri, 
                           std::function<void(std::vector<Diagnostic>)> cb) override;
    
    void provideFormatting(const std::string& uri, const Range& range,
                          const CancellationToken& ct,
                          std::function<void(std::vector<TextEdit>)> cb) override;
    
    void provideCodeActions(const std::string& uri, const Range& range,
                           const std::vector<Diagnostic>& context,
                           const CancellationToken& ct,
                           std::function<void(std::vector<CodeAction>)> cb) override;
    
    // Incremental Sync
    void applyIncrementalSync(const std::string& uri, int64_t version,
                             const std::vector<TextDocumentContentChangeEvent>& changes);
    
private:
    TextEdit calculateTextEditAsm(const std::string& uri, const Position& pos, const std::string& insertText);
    Position offsetToPosition(const std::string& text, int offset) const;
};

} // namespace rxd::lsp




