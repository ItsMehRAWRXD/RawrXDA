// Stub implementation for Context Mention Parser
// TODO: Implement full context mention parsing

#include <string>
#include <vector>

namespace RawrXD {
namespace Agentic {

class ContextMentionParser {
public:
    static ContextMentionParser& instance() {
        static ContextMentionParser inst;
        return inst;
    }

    std::vector<std::string> parseMentions(const std::string& text) {
        // Stub: return empty for now
        return {};
    }
};

} // namespace Agentic
} // namespace RawrXD