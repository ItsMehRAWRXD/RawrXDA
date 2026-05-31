#pragma once

#include <string>
#include <vector>
#include <functional>

namespace RawrXD::Runtime {

class SovereignStreamingParser {
public:
    enum class TokenType {
        PlainContent,
        ThoughtStart,
        ThoughtEnd,
        ToolCallStart,
        ToolCallEnd,
        ToolArguments
    };

    struct StreamToken {
        TokenType type;
        std::string content;
        std::string meta; // Tool name, etc.
    };

    using TokenCallback = std::function<void(const StreamToken&)>;

    SovereignStreamingParser(TokenCallback cb);

    // Feed new characters into the parser - uses state machine logic for SSE efficiency
    void consume(const char* data, size_t length);

    // Finalize parsing and flush any remaining content
    void finalize();

private:
    void processState();

    TokenCallback m_callback;
    std::string m_buffer;
    int m_state; // 0=Default, 1=In-Thought, 2=In-Tool, 3=In-Tag
    std::string m_currentTag;
    std::string m_currentToolName;
};

} // namespace RawrXD::Runtime
