#pragma once

#include "language_server_integration.h"
#include "codebase_context_analyzer.hpp"
#include "streaming_completion_engine.hpp"
#include "intelligent_codebase_engine.hpp"
#include "agent_hot_patcher.hpp"
#include "observability_sink.hpp"
#include <QAtomicInteger>
#include <QUuid>
#include <chrono>

namespace rxd::lsp {

// RAII Request Scope for Telemetry & Cancellation
struct RequestScope {
    QString method;
    QString reqId;
    std::chrono::high_resolution_clock::time_point t0;
    ObservabilitySink* sink;
    std::atomic_flag* cancelFlag;
    
    RequestScope(const QString& m, const QString& id, ObservabilitySink* s, std::atomic_flag* cf)
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
    QMap<QString, QTextDocument*> m_documentBuffers;
    QMap<QString, qint64> m_documentVersions;
    
public:
    LanguageServerIntegrationImpl(std::shared_ptr<Logger> logger,
                                  std::shared_ptr<Metrics> metrics);
    
    void provideHover(const QString& uri, const Position& pos, 
                     const CancellationToken& ct,
                     std::function<void(Hover)> cb) override;
    
    void provideDefinition(const QString& uri, const Position& pos,
                          const CancellationToken& ct,
                          std::function<void(QVector<Location>)> cb) override;
    
    void provideReferences(const QString& uri, const Position& pos,
                          bool includeDecl,
                          const CancellationToken& ct,
                          std::function<void(QVector<Location>)> cb) override;
    
    void provideDocumentSymbols(const QString& uri,
                               const CancellationToken& ct,
                               std::function<void(QVector<DocumentSymbol>)> cb) override;
    
    void provideCompletion(const QString& uri, const Position& pos,
                          const CompletionContext& ctx,
                          const CancellationToken& ct,
                          std::function<void(CompletionList)> cb) override;
    
    void provideDiagnostics(const QString& uri, 
                           std::function<void(QVector<Diagnostic>)> cb) override;
    
    void provideFormatting(const QString& uri, const Range& range,
                          const CancellationToken& ct,
                          std::function<void(QVector<TextEdit>)> cb) override;
    
    void provideCodeActions(const QString& uri, const Range& range,
                           const QVector<Diagnostic>& context,
                           const CancellationToken& ct,
                           std::function<void(QVector<CodeAction>)> cb) override;
    
    // Incremental Sync
    void applyIncrementalSync(const QString& uri, qint64 version,
                             const QVector<TextDocumentContentChangeEvent>& changes);
    
private:
    TextEdit calculateTextEditAsm(const QString& uri, const Position& pos, const QString& insertText);
    Position offsetToPosition(const QString& text, int offset) const;
};

} // namespace rxd::lsp
