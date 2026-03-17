#pragma once
/**
 * @file streaming_completion_engine.hpp
 * @brief Streaming completion engine for LSP — stub for language_server_integration_impl.
 */
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>

struct Position {
    int line = 0;
    int character = 0;
};

struct StreamingSuggestion {
    std::string text;
    std::string source;
    std::string documentation;
    bool useAsmOptimization = false;
};

enum class CompletionTriggerKind { Invoked = 1, TriggerCharacter = 2 };

class StreamingCompletionEngine {
public:
    static StreamingCompletionEngine* instance() {
        static StreamingCompletionEngine s_instance;
        return &s_instance;
    }

    template<typename CancelToken>
    std::vector<StreamingSuggestion> predictAsync(
        const std::string& uri,
        const Position& pos,
        CompletionTriggerKind triggerKind,
        const CancelToken& cancelToken)
    {
        (void)uri;
        (void)pos;
        (void)triggerKind;
        (void)cancelToken;
        return {};
    }

private:
    StreamingCompletionEngine() = default;
};
