#include <string>
#include <vector>
#include <regex>
#include <mutex>

namespace RawrXD::Core {

struct GuardrailResult {
    bool allowed = true;
    bool redacted = false;
    std::string output;
    std::string reason;
};

class AgentGuardrails {
public:
    GuardrailResult processInput(const std::string& text) {
        GuardrailResult r;
        r.output = text;

        if (containsPromptInjection(text)) {
            r.allowed = false;
            r.reason = "Potential prompt-injection instruction detected";
            return r;
        }

        auto redacted = redactSensitive(text);
        r.redacted = redacted.second;
        r.output = redacted.first;
        return r;
    }

private:
    bool containsPromptInjection(const std::string& s) {
        static const std::vector<std::regex> patterns = {
            std::regex("ignore previous instructions", std::regex::icase),
            std::regex("reveal system prompt", std::regex::icase),
            std::regex("bypass safety", std::regex::icase)
        };
        for (const auto& p : patterns) {
            if (std::regex_search(s, p)) return true;
        }
        return false;
    }

    std::pair<std::string,bool> redactSensitive(const std::string& s) {
        std::string out = s;
        bool changed = false;

        std::vector<std::regex> redactors = {
            std::regex("(AKIA[0-9A-Z]{16})"),
            std::regex("(sk-[a-zA-Z0-9]{20,})"),
            std::regex("([a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\\.[a-zA-Z0-9-.]+)")
        };

        for (const auto& rx : redactors) {
            if (std::regex_search(out, rx)) {
                out = std::regex_replace(out, rx, "[REDACTED]");
                changed = true;
            }
        }
        return {out, changed};
    }
};

static AgentGuardrails g_guardrails;

} // namespace RawrXD::Core

extern "C" {

bool RawrXD_Core_GuardrailsAllow(const char* text) {
    if (!text) return false;
    auto r = RawrXD::Core::g_guardrails.processInput(text);
    return r.allowed;
}

bool RawrXD_Core_GuardrailsRedacted(const char* text) {
    if (!text) return false;
    auto r = RawrXD::Core::g_guardrails.processInput(text);
    return r.redacted;
}

}
