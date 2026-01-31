#include "language_server_integration_impl.hpp"
#include <ranges>

namespace rxd::lsp {

LanguageServerIntegrationImpl::LanguageServerIntegrationImpl(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : LanguageServerIntegration(logger, metrics) {
    
}

void LanguageServerIntegrationImpl::provideHover(const std::string& uri, const Position& pos, 
                                                 const CancellationToken& ct,
                                                 std::function<void(Hover)> cb) {
    RequestScope scope("textDocument/hover", QUuid::createUuid().toString(), 
                       ObservabilitySink::instance(), ct.raw());
    if(scope.isCancelled()) return;

    // Drive via CodebaseContextAnalyzer (local intelligence)
    auto analyzer = CodebaseContextAnalyzer::instance();
    auto symbol = analyzer->resolveSymbolAt(uri, pos.line, pos.character);
    
    Hover result;
    if(symbol) {
        result.contents.kind = "markdown";
        result.contents.value = std::string("**%1** (%2)\n\n%3")
            ;
        result.range = symbol->range;
        
        // Check hot-patcher for hallucination corrections
        auto correction = AgentHotPatcher::instance()->checkHoverCorrection(uri, pos);
        if(correction.has_value()) {
            result.contents.value += std::string("\n\n🔧 *AI Corrected:* %1");
        }
    } else {
        result.contents.kind = "plaintext";
        result.contents.value = "No symbol found";
    }
    
    cb(result);
}

void LanguageServerIntegrationImpl::provideDefinition(const std::string& uri, const Position& pos,
                                                     const CancellationToken& ct,
                                                     std::function<void(std::vector<Location>)> cb) {
    RequestScope scope("textDocument/definition", QUuid::createUuid().toString(),
                       ObservabilitySink::instance(), ct.raw());
    
    auto engine = IntelligentCodebaseEngine::instance();
    auto defs = engine->findDefinition(uri, pos.line, pos.character);
    
    std::vector<Location> locs;
    for(auto& d : defs) {
        if(scope.isCancelled()) break;
        locs.append({d.uri, d.range});
    }
    cb(locs);
}

void LanguageServerIntegrationImpl::provideReferences(const std::string& uri, const Position& pos,
                                                     bool includeDecl,
                                                     const CancellationToken& ct,
                                                     std::function<void(std::vector<Location>)> cb) {
    RequestScope scope("textDocument/references", QUuid::createUuid().toString(),
                       ObservabilitySink::instance(), ct.raw());
    
    auto refs = CodebaseContextAnalyzer::instance()->findUsages(uri, pos, includeDecl);
    
    std::vector<Location> result;
    result.reserve(refs.size());
    for(auto& r : refs) {
        if(ct.isCancelled()) break;
        result.append({r.uri, r.range});
    }
    cb(result);
}

void LanguageServerIntegrationImpl::provideDocumentSymbols(const std::string& uri,
                                                          const CancellationToken& ct,
                                                          std::function<void(std::vector<DocumentSymbol>)> cb) {
    RequestScope scope("textDocument/documentSymbol", QUuid::createUuid().toString(),
                       ObservabilitySink::instance(), ct.raw());
    
    auto outline = IntelligentCodebaseEngine::instance()->buildDocumentOutline(uri);
    cb(outline);
}

void LanguageServerIntegrationImpl::provideCompletion(const std::string& uri, const Position& pos,
                                                     const CompletionContext& ctx,
                                                     const CancellationToken& ct,
                                                     std::function<void(CompletionList)> cb) {
    RequestScope scope("textDocument/completion", QUuid::createUuid().toString(),
                       ObservabilitySink::instance(), ct.raw());
    
    // High-performance path: StreamingCompletionEngine
    auto engine = StreamingCompletionEngine::instance();
    
    CompletionList list;
    list.isIncomplete = true; // Will stream more
    
    // Trigger predictive inference
    auto suggestions = engine->predictAsync(uri, pos, ctx.triggerKind, ct);
    
    // Convert to LSP format
    for(auto& s : suggestions) {
        if(ct.isCancelled()) break;
        
        CompletionItem item;
        item.label = s.text;
        item.kind = CompletionItemKind::Text; // Map via s.type
        item.detail = s.source; // AI confidence or LSP source
        item.documentation = s.documentation;
        item.insertText = s.text;
        item.sortText = std::string("%1"), 4, 10, QChar('0'));
        
        // MASM-bridge optimization: Pre-calculate edit ranges in assembly
        if(s.useAsmOptimization) {
            item.textEdit = calculateTextEditAsm(uri, pos, s.text);
        }
        
        list.items.append(item);
    }
    
    list.isIncomplete = !ct.isCancelled();
    cb(list);
}

void LanguageServerIntegrationImpl::provideDiagnostics(const std::string& uri, 
                                                      std::function<void(std::vector<Diagnostic>)> cb) {
    RequestScope scope("textDocument/diagnostic", QUuid::createUuid().toString(),
                       ObservabilitySink::instance(), nullptr);
    
    auto aiDiags = IDEAgentBridgeHotPatchingIntegration::instance()->getAIDiagnostics(uri);
    std::vector<Diagnostic> diags;
    
    for(auto& d : aiDiags) {
        Diagnostic dx;
        dx.range = d.range;
        dx.severity = d.severity; // Error=1, Warning=2, Info=3, Hint=4
        dx.code = d.code;
        dx.source = "rawrxd-ai";
        dx.message = d.message;
        
        // Attach hot-patch candidate info for quick fixes
        if(d.hasFix) {
            CodeAction action;
            action.title = "🚀 Apply AI Fix";
            action.kind = "quickfix";
            action.diagnostics = {dx};
            action.edit = generateWorkspaceEdit(d.fixEdits);
            dx.relatedInformation.append({action});
        }
        diags.append(dx);
    }
    
    cb(diags);
}

void LanguageServerIntegrationImpl::provideFormatting(const std::string& uri, const Range& range,
                                                     const CancellationToken& ct,
                                                     std::function<void(std::vector<TextEdit>)> cb) {
    RequestScope scope("textDocument/formatting", QUuid::createUuid().toString(),
                       ObservabilitySink::instance(), ct.raw());
    
    // Delegate to StyleEngine or MASM path
    auto edits = IntelligentCodebaseEngine::instance()->formatRange(uri, range);
    cb(edits);
}

void LanguageServerIntegrationImpl::provideCodeActions(const std::string& uri, const Range& range,
                                                      const std::vector<Diagnostic>& context,
                                                      const CancellationToken& ct,
                                                      std::function<void(std::vector<CodeAction>)> cb) {
    RequestScope scope("textDocument/codeAction", QUuid::createUuid().toString(),
                       ObservabilitySink::instance(), ct.raw());
    
    std::vector<CodeAction> actions;
    
    // Hot-patch suggestions
    auto candidates = AgentHotPatcher::instance()->suggestFixes(uri, range, context);
    for(auto& c : candidates) {
        if(ct.isCancelled()) break;
        
        CodeAction a;
        a.title = c.title;
        a.kind = c.isRefactoring ? "refactor" : "quickfix";
        a.edit = c.edit;
        actions.append(a);
    }
    
    // AI-generated refactorings from AgenticPuppeteer
    auto aiActions = AgenticPuppeteer::instance()->generateRefactorings(uri, range);
    actions.append(aiActions);
    
    cb(actions);
}

// Incremental Sync
void LanguageServerIntegrationImpl::applyIncrementalSync(const std::string& uri, int64_t version,
                                                        const std::vector<TextDocumentContentChangeEvent>& changes) {
    auto sink = ObservabilitySink::instance();
    auto t0 = std::chrono::high_resolution_clock::now();
    
    // Forward to underlying text buffer (via LSPClient or native)
    for(const auto& change : changes) {
        if(change.range) {
            // Partial update: Calculate diff and apply
            m_documentBuffers[uri]->applyEdit(*change.range, change.text);
        } else {
            // Full content replacement
            m_documentBuffers[uri]->setContent(change.text);
        }
    }
    m_documentVersions[uri] = version;
    
    // Trigger async semantic analysis
    IntelligentCodebaseEngine::instance()->notifyDocumentChange(uri, version);
    
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - t0).count();
    sink->emitSyncLatency(uri, version, us);
}

// Utility: MASM-accelerated text edit calculation
TextEdit LanguageServerIntegrationImpl::calculateTextEditAsm(const std::string& uri, 
                                                           const Position& pos,
                                                           const std::string& insertText) {
    TextEdit edit;
    edit.range.start = pos;
    edit.range.end = Position{pos.line, pos.character}; // Insert, no replace
    
    // Delegate to MASM for whitespace normalization and RTL detection
    std::vector<uint8_t> utf8 = insertText.toUtf8();
    int normalizedLen = rxd_asm_normalize_completion(utf8.constData(), utf8.length());
    edit.newText = std::string::fromUtf8(utf8.constData(), normalizedLen);
    
    return edit;
}

Position LanguageServerIntegrationImpl::offsetToPosition(const std::string& text, int offset) const {
    int line = 0, col = 0;
    for(int i = 0; i < offset && i < text.size(); ++i) {
        if(text[i] == '\n') { ++line; col = 0; }
        else { ++col; }
    }
    return {line, col};
}

} // namespace rxd::lsp

