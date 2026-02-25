#pragma once

/**
 * @file streaming_completion_engine.hpp
 * @brief Stub: streaming completion engine (used by language_server_integration_impl).
 */

#include <string>
#include <functional>
#include <vector>
#include <cstdint>

namespace rxd {
namespace lsp {

struct CompletionContext { int triggerKind = 0; };
class CancellationToken;

struct StreamingSuggestion {
    std::string text;
    std::string source;
    std::string documentation;
    std::string insertText;
    bool useAsmOptimization = false;
};

class StreamingCompletionEngine {
public:
    static StreamingCompletionEngine* instance();
    std::vector<StreamingSuggestion> predictAsync(const std::string& uri,
        const void* pos, int triggerKind, const CancellationToken& ct);
    void requestCompletion(const std::string& uri, int line, int column,
                           std::function<void(const std::vector<void*>&)> callback);
};

}  // namespace lsp
}  // namespace rxd
