// Lightweight context mention parser for agentic prompts.
// Extracts unique mentions such as @workspace, @terminal, @file:path.

#include <string>
#include <vector>
#include <regex>
#include <unordered_set>

namespace RawrXD {
namespace Agentic {

class ContextMentionParser {
public:
    static ContextMentionParser& instance() {
        static ContextMentionParser inst;
        return inst;
    }

    std::vector<std::string> parseMentions(const std::string& text) {
        static const std::regex mentionPattern(R"(@[A-Za-z0-9_:-]+)");
        std::unordered_set<std::string> seen;
        std::vector<std::string> mentions;

        auto it = std::sregex_iterator(text.begin(), text.end(), mentionPattern);
        auto end = std::sregex_iterator();
        for (; it != end; ++it) {
            const std::string token = it->str();
            if (seen.insert(token).second) {
                mentions.push_back(token);
            }
        }

        return mentions;
    }
};

} // namespace Agentic
} // namespace RawrXD