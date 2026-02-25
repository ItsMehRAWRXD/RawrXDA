#include <nlohmann/json.hpp>

#include "ai_model_caller.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <sstream>
#include <thread>

namespace {
std::atomic<bool> g_modelReady{false};
std::string g_modelName;

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}  // namespace

namespace RawrXD {

bool ModelCaller::Initialize(const std::string& modelPath) {
    if (modelPath.empty()) {
        return false;
    }
    g_modelName = modelPath;
    g_modelReady = true;
    return true;
}

void ModelCaller::Shutdown() {
    g_modelReady = false;
    g_modelName.clear();
}

bool ModelCaller::IsReady() { return g_modelReady.load(); }

std::vector<ModelCaller::Completion> ModelCaller::generateCompletion(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& fileType,
    const std::string& context,
    int numCompletions) {
    std::vector<Completion> out;
    if (numCompletions <= 0) {
        return out;
    }

    const std::string prompt = buildCompletionPrompt(prefix, suffix, fileType, context);
    const std::vector<std::string> variants = parseCompletions(callModel(prompt, {}));

    int count = 0;
    for (const auto& v : variants) {
        if (count++ >= numCompletions) {
            break;
        }
        Completion c;
        c.text = v;
        c.score = scoreCompletion(v, prefix, fileType);
        c.description = getCompletionDescription(v);
        out.push_back(std::move(c));
    }

    while (static_cast<int>(out.size()) < numCompletions) {
        Completion c;
        c.text = prefix.empty() ? "// completion" : prefix + " // completion";
        c.score = 0.5f;
        c.description = "Fallback completion";
        out.push_back(std::move(c));
    }

    return out;
}

std::string ModelCaller::generateCode(
    const std::string& instruction,
    const std::string& fileType,
    const std::string& context) {
    std::ostringstream out;
    out << "// Generated (" << fileType << ")\n";
    out << "// Model: " << (g_modelName.empty() ? "minimal" : g_modelName) << "\n";
    out << "// Instruction: " << instruction << "\n";
    if (!context.empty()) {
        out << context;
        if (context.back() != '\n') {
            out << '\n';
        }
    }
    out << "// TODO: Replace minimal generator with full model backend.\n";
    return out.str();
}

std::string ModelCaller::generateRewrite(
    const std::string& code,
    const std::string& instruction,
    const std::string& context) {
    (void)context;
    std::string rewritten = code;
    const std::string lower = toLower(instruction);

    if (lower.find("security") != std::string::npos) {
        auto pos = rewritten.find("strcpy(");
        if (pos != std::string::npos) {
            rewritten.replace(pos, 7, "strncpy(");
        }
        pos = rewritten.find("sprintf(");
        if (pos != std::string::npos) {
            rewritten.replace(pos, 8, "snprintf(");
        }
    }

    if (lower.find("optimiz") != std::string::npos && rewritten.find("std::endl") != std::string::npos) {
        size_t pos = 0;
        while ((pos = rewritten.find("std::endl", pos)) != std::string::npos) {
            rewritten.replace(pos, 9, "\"\\n\"");
            pos += 4;
        }
    }

    if (rewritten.empty()) {
        rewritten = "// Rewrite requested: " + instruction + "\n";
    }
    return rewritten;
}

std::vector<Diagnostic> ModelCaller::generateDiagnostics(
    const std::string& code,
    const std::string& language) {
    std::vector<Diagnostic> out;
    const std::string lower = toLower(code);

    if (lower.find("todo") != std::string::npos) {
        out.push_back({"TODO marker found", 1, 1, "info", "Convert TODO into a tracked task", "minimal-ai"});
    }
    if (lower.find("strcpy(") != std::string::npos) {
        out.push_back({"Potential unsafe function strcpy", 1, 1, "warning",
                       "Use strncpy or safer abstractions", "minimal-ai"});
    }
    if (toLower(language) == "cpp" && lower.find("using namespace std;") != std::string::npos) {
        out.push_back({"Avoid global using-directive in headers", 1, 1, "warning",
                       "Prefer explicit std:: qualifiers", "minimal-ai"});
    }
    return out;
}

std::string ModelCaller::callModel(const std::string& prompt, const GenerationParams& params) {
    std::ostringstream out;
    out << "{\"ok\":true,\"model\":\"" << (g_modelName.empty() ? "minimal" : g_modelName)
        << "\",\"max_tokens\":" << params.max_tokens << ",\"text\":";
    out << nlohmann::json(prompt.substr(0, std::min<size_t>(prompt.size(), 240))).dump() << "}";
    return out.str();
}

bool ModelCaller::streamModel(const std::string& prompt, const GenerationParams& params,
                              StreamCallback callback, std::chrono::milliseconds delay) {
    (void)params;
    if (!callback) {
        return false;
    }

    const std::string text = prompt.empty() ? "stream:empty" : prompt;
    constexpr size_t kChunk = 24;
    for (size_t i = 0; i < text.size(); i += kChunk) {
        const std::string chunk = text.substr(i, std::min(kChunk, text.size() - i));
        if (!callback(chunk)) {
            return false;
        }
        if (delay.count() > 0) {
            std::this_thread::sleep_for(delay);
        }
    }
    return true;
}

nlohmann::json ModelCaller::ParseStructuredResponse(const std::string& response) {
    try {
        return nlohmann::json::parse(response);
    } catch (...) {
        return nlohmann::json{
            {"ok", false},
            {"error", "Failed to parse structured response"},
            {"raw", response},
        };
    }
}

std::vector<Diagnostic> ModelCaller::ExtractDiagnostics(const std::string& response) {
    std::vector<Diagnostic> out;
    const auto parsed = ParseStructuredResponse(response);

    if (parsed.is_object() && parsed.contains("diagnostics") && parsed["diagnostics"].is_array()) {
        for (const auto& d : parsed["diagnostics"]) {
            Diagnostic diag;
            diag.message = d.value("message", "diagnostic");
            diag.line = d.value("line", 1);
            diag.column = d.value("column", 1);
            diag.severity = d.value("severity", "info");
            diag.code_action = d.value("code_action", "");
            diag.source = d.value("source", "minimal-ai");
            out.push_back(std::move(diag));
        }
        return out;
    }

    return generateDiagnostics(response, "text");
}

std::string ModelCaller::buildCompletionPrompt(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& fileType,
    const std::string& context) {
    std::ostringstream out;
    out << "[fileType=" << fileType << "]\n";
    out << "[context]\n" << context << "\n";
    out << "[prefix]\n" << prefix << "\n";
    out << "[suffix]\n" << suffix << "\n";
    return out.str();
}

std::vector<std::string> ModelCaller::parseCompletions(const std::string& response) {
    std::vector<std::string> out;
    const auto parsed = ParseStructuredResponse(response);
    if (parsed.is_object() && parsed.contains("completions") && parsed["completions"].is_array()) {
        for (const auto& c : parsed["completions"]) {
            if (c.is_string()) {
                out.push_back(c.get<std::string>());
            }
        }
    }
    if (out.empty()) {
        const std::string payload = parsed.value("text", response);
        out.push_back(trim(payload));
        out.push_back(trim(payload) + "\n// completion variant");
    }
    return out;
}

float ModelCaller::scoreCompletion(
    const std::string& completion, const std::string& prefix, const std::string& fileType) {
    float score = 0.5f;
    if (!completion.empty()) {
        score += 0.2f;
    }
    if (!prefix.empty() && completion.find(prefix) != std::string::npos) {
        score += 0.2f;
    }
    if (toLower(fileType) == "cpp" && completion.find(';') != std::string::npos) {
        score += 0.1f;
    }
    return std::min(score, 1.0f);
}

std::string ModelCaller::getCompletionDescription(const std::string& completion) {
    if (completion.find("if") != std::string::npos) {
        return "Conditional branch suggestion";
    }
    if (completion.find("for") != std::string::npos) {
        return "Loop suggestion";
    }
    return "General completion";
}

}  // namespace RawrXD
