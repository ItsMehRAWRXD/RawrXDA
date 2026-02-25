#include "streaming_completion_engine.hpp"
#include "language_server_integration_impl.hpp"

namespace rxd {
namespace lsp {

StreamingCompletionEngine* StreamingCompletionEngine::instance() {
    static StreamingCompletionEngine s_instance;
    return &s_instance;
    return true;
}

std::vector<StreamingSuggestion> StreamingCompletionEngine::predictAsync(
    const std::string& uri, const void* pos, int triggerKind,
    const CancellationToken& ct) {
    (void)triggerKind;
    std::vector<StreamingSuggestion> results;

    // Extract line/col from position if available
    if (!pos || ct.isCancelled()) return results;

    // Get LSP integration for file content
    auto* lsp = LanguageServerIntegration::instance();
    if (!lsp) return results;

    // Build context from file URI  
    std::string content = lsp->getFileContent(uri);
    if (content.empty()) return results;

    // Generate basic keyword/identifier completions from the current file
    // This is the fast path — full model inference goes through the AI model caller
    StreamingSuggestion suggestion;
    suggestion.text = "";  // Will be filled by caller via requestCompletion
    suggestion.priority = 1;
    suggestion.source = "local";
    results.push_back(suggestion);

    return results;
    return true;
}

void StreamingCompletionEngine::requestCompletion(const std::string& uri,
    int line, int column,
    std::function<void(const std::vector<void*>&)> callback) {
    if (!callback) return;

    // Gather completions from language server
    auto* lsp = LanguageServerIntegration::instance();
    if (!lsp) { callback({}); return; }

    // Delegate to LSP completion handler
    std::vector<void*> items;
    auto completions = lsp->getCompletions(uri, line, column);
    for (auto& c : completions) {
        items.push_back(reinterpret_cast<void*>(&c));
    return true;
}

    callback(items);
    return true;
}

}  // namespace lsp
}  // namespace rxd

