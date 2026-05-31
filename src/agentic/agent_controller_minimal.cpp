// ============================================================================
// agent_controller_minimal.cpp
// Implementation of Minimal Agent Controller
// ============================================================================

#include "agent_controller_minimal.h"
#include "../cpu_inference_engine.h"
#include "../logging/Logger.h"
#include "AgentToolHandlers.h"
#include "parrot_detector.h"
#include "response_validator.h"
#include "small_model_agent_prompt.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <unordered_set>


using json = nlohmann::json;

#define SAFE_LOOKUP(m, k)                                                                                              \
    (                                                                                                                  \
        [&]() -> decltype(auto)                                                                                        \
        {                                                                                                              \
            auto it = (m).find(k);                                                                                     \
            if (it == (m).end())                                                                                       \
            {                                                                                                          \
                agentLogError("SAFE_LOOKUP failed for key '" + std::string(k) + "'");                                  \
                throw std::runtime_error("SAFE_LOOKUP: key not found");                                                \
            }                                                                                                          \
            return it->second;                                                                                         \
        })()

namespace
{

constexpr size_t kMaxSessionHistory = 24;
constexpr int kMaxAutomaticBuildReplans = 2;
constexpr const char* kMinimalAgentLogComponent = "MinimalAgentController";

inline void agentLogInfo(const std::string& msg)
{
    RawrXD::Logging::Logger::instance().info(msg, kMinimalAgentLogComponent);
}
inline void agentLogWarn(const std::string& msg)
{
    RawrXD::Logging::Logger::instance().warning(msg, kMinimalAgentLogComponent);
}
inline void agentLogError(const std::string& msg)
{
    RawrXD::Logging::Logger::instance().error(msg, kMinimalAgentLogComponent);
}

enum class PolicyAction
{
    CONTINUE,
    RETRY_COLD,
    FALLBACK_SYNTHETIC,
    ABORT,
};

struct AgenticRunResult
{
    std::string prompt;
    std::string output;
    bool isValid = false;
    bool isParrot = false;
    bool isDegenerate = false;
    bool hasPromptLeakage = false;
    long long latency_ms = 0;
};

struct VerificationDecision
{
    bool attempted = false;
    bool passed = false;
    std::string verifierKind;
    std::string summary;
};

size_t FindCaseInsensitive(const std::string& haystack, const std::string& needle, size_t startPos = 0);
bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle);
bool ContainsAnyMarker(const std::string& text, const std::vector<std::string>& markers);
std::string FirstMarkerMatch(const std::string& text, const std::vector<std::string>& markers);
std::string TrimCopy(std::string value);

void PushSessionMessage(rawrxd::SessionContext& session, const std::string& role, const std::string& content)
{
    session.history.push_back({role, content});
    if (session.history.size() > kMaxSessionHistory)
    {
        session.history.erase(session.history.begin());
    }

    const size_t approximateTokens = std::max<size_t>(1, (content.size() + 3) / 4);
    session.total_tokens += approximateTokens;
}

void StampMinimalAgentTranscript(rawrxd::MinimalAgenticResponse& out, const rawrxd::SessionContext& session,
                                 size_t startIndex)
{
    if (startIndex >= session.history.size())
    {
        return;
    }
    out.transcript_delta.assign(session.history.begin() + static_cast<std::ptrdiff_t>(startIndex),
                                session.history.end());
}

std::string MergePromptContinuity(const std::string& task, const std::string& continuity)
{
    if (continuity.empty())
    {
        return task;
    }
    return continuity + "\nCurrent task:\n" + task;
}

std::string TruncateVerificationEvidence(const std::string& text, size_t maxChars = 600)
{
    if (text.size() <= maxChars)
    {
        return text;
    }
    return text.substr(0, maxChars) + "...";
}

std::string EnsureCommandCapturesStderr(const std::string& command)
{
    if (command.empty())
    {
        return command;
    }
    if (command.find("2>") != std::string::npos || command.find("2>&1") != std::string::npos)
    {
        return command;
    }
    return command + " 2>&1";
}

VerificationDecision VerifyExecutionOutcome(const rawrxd::ExecutionPlan& plan, bool success,
                                            const std::string& outputPreview, const std::string& rawEvidence)
{
    static const std::vector<std::string> failureMarkers = {"error C",
                                                            "fatal error",
                                                            "failed",
                                                            "exception",
                                                            "traceback",
                                                            "segmentation fault",
                                                            "not recognized as an internal or external command",
                                                            "undefined reference",
                                                            "LNK",
                                                            "no such file",
                                                            "cannot open",
                                                            "permission denied",
                                                            "timed out"};
    static const std::vector<std::string> successMarkers = {
        "build succeeded", "0 failed", "0 error", "no errors found", "completed successfully", "success", "passed"};

    VerificationDecision decision;
    decision.attempted = true;
    decision.verifierKind = plan.intent == rawrxd::TaskIntent::Terminal || plan.intent == rawrxd::TaskIntent::Hybrid
                                ? "runtime"
                                : "execution";

    const std::string& signalText = rawEvidence.empty() ? outputPreview : rawEvidence;

    const bool hasFailureMarker = ContainsAnyMarker(signalText, failureMarkers);
    const bool hasSuccessMarker = ContainsAnyMarker(signalText, successMarkers);
    const std::string failureMarker = FirstMarkerMatch(signalText, failureMarkers);
    const std::string evidencePreview = rawEvidence.empty() ? std::string() : TruncateVerificationEvidence(rawEvidence);

    if (!success || hasFailureMarker)
    {
        decision.passed = false;
        decision.summary = failureMarker.empty() ? "Execution finished in a failed state."
                                                 : "Verifier detected failure marker: " + failureMarker;
        if (!evidencePreview.empty())
        {
            decision.summary += " | raw: " + evidencePreview;
        }
        return decision;
    }

    decision.passed = true;
    if (hasSuccessMarker)
    {
        decision.summary = "Execution completed with positive verification markers in output.";
    }
    else if (!signalText.empty())
    {
        decision.summary = "Execution completed without detected failure markers.";
    }
    else
    {
        decision.summary = "Execution completed, but produced no verifier-visible output.";
    }
    if (!evidencePreview.empty())
    {
        decision.summary += " | raw: " + evidencePreview;
    }
    return decision;
}

VerificationDecision VerifyIdeOutcome(rawrxd::IdeRequestKind kind, bool success, rawrxd::TrustScore trust,
                                      const std::string& outputPreview)
{
    VerificationDecision decision;
    decision.attempted = true;

    if (kind == rawrxd::IdeRequestKind::CodeEdit)
    {
        decision.verifierKind = "proposal";
        decision.passed = success;
        decision.summary = success ? "Diff plan generated; runtime verification is pending approval and execution."
                                   : "Diff plan generation failed before execution review.";
        return decision;
    }

    decision.verifierKind = kind == rawrxd::IdeRequestKind::InlineCompletion ? "completion_quality" : "rewrite_quality";
    if (!success)
    {
        decision.passed = false;
        decision.summary = outputPreview.empty() ? "Model response was rejected before producing a usable result."
                                                 : "Model response failed controller validation or execution checks.";
        return decision;
    }

    if (trust == rawrxd::TrustScore::ModelInvalid)
    {
        decision.passed = false;
        decision.summary = "Output was produced, but verifier classified the model response as invalid.";
        return decision;
    }

    decision.passed = true;
    decision.summary = kind == rawrxd::IdeRequestKind::InlineCompletion
                           ? "Inline completion passed controller trust checks."
                           : "Rewrite output passed controller trust checks.";
    return decision;
}

bool IsWeakEchoSignal(const AgenticRunResult& res)
{
    if (res.latency_ms >= 20)
    {
        return false;
    }

    if (res.isParrot || res.hasPromptLeakage || res.isDegenerate)
    {
        return true;
    }

    if (res.output.empty())
    {
        return true;
    }

    const std::string normalizedOutput = rawrxd::ParrotDetector::Normalize(res.output);
    const std::string normalizedPrompt = rawrxd::ParrotDetector::Normalize(res.prompt);
    if (normalizedOutput.empty() || normalizedPrompt.empty())
    {
        return true;
    }

    if (normalizedPrompt.find(normalizedOutput) != std::string::npos)
    {
        return true;
    }

    return rawrxd::ParrotDetector::Similarity(normalizedOutput, normalizedPrompt) > 0.70;
}

PolicyAction EvaluateResponse(const AgenticRunResult& res, const rawrxd::SessionContext& session,
                              rawrxd::ExecutionLane lane)
{
    if (res.isParrot || res.isDegenerate || res.hasPromptLeakage || IsWeakEchoSignal(res))
    {
        if (session.retry_count >= 1 || lane == rawrxd::ExecutionLane::SyntheticFirst ||
            lane == rawrxd::ExecutionLane::SyntheticOnly)
        {
            return PolicyAction::FALLBACK_SYNTHETIC;
        }
        return PolicyAction::RETRY_COLD;
    }

    if (res.isValid)
    {
        return PolicyAction::CONTINUE;
    }

    if (session.retry_count >= 1)
    {
        return PolicyAction::FALLBACK_SYNTHETIC;
    }

    return PolicyAction::RETRY_COLD;
}

std::string EscapeXml(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (const char ch : value)
    {
        switch (ch)
        {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&apos;";
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

std::string ScrubModelResponse(std::string text)
{
    text = TrimCopy(std::move(text));

    // Remove markdown code fences if they wrap the entire response or tool block
    const std::vector<std::string> fences = {"```json", "```", "'''json", "'''"};
    for (const auto& fence : fences)
    {
        if (text.find(fence) == 0)
        {
            text.erase(0, fence.size());
            size_t endFence = text.rfind("```");
            if (endFence == std::string::npos)
                endFence = text.rfind("'''");
            if (endFence != std::string::npos)
            {
                text.erase(endFence);
            }
            text = TrimCopy(std::move(text));
        }
    }

    // Remove common "unleashed" LLM commentary prefixes
    const std::vector<std::string> noise = {"I will help you with that.",
                                            "Sure, here is the tool call:", "Understood."};
    for (const auto& n : noise)
    {
        if (text.find(n) == 0)
        {
            text.erase(0, n.size());
            text = TrimCopy(std::move(text));
        }
    }

    return text;
}

std::string ExtractTagBody(const std::string& text, const std::string& tag)
{
    const std::string open = "<" + tag + ">";
    const std::string close = "</" + tag + ">";
    const size_t start = text.find(open);
    if (start == std::string::npos)
    {
        return {};
    }
    const size_t bodyStart = start + open.size();
    const size_t end = text.find(close, bodyStart);
    if (end == std::string::npos)
    {
        return {};
    }
    return text.substr(bodyStart, end - bodyStart);
}

std::string TrimCopy(std::string value)
{
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string ToLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string EscapeJsonString(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (const char ch : value)
    {
        switch (ch)
        {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

size_t FindCaseInsensitive(const std::string& haystack, const std::string& needle, size_t startPos)
{
    if (needle.empty() || haystack.size() < needle.size() || startPos >= haystack.size())
    {
        return std::string::npos;
    }

    const std::string lowerHaystack = ToLowerCopy(haystack);
    const std::string lowerNeedle = ToLowerCopy(needle);
    return lowerHaystack.find(lowerNeedle, startPos);
}

bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle)
{
    return FindCaseInsensitive(haystack, needle) != std::string::npos;
}

bool ContainsAnyMarker(const std::string& text, const std::vector<std::string>& markers)
{
    for (const auto& marker : markers)
    {
        if (ContainsCaseInsensitive(text, marker))
        {
            return true;
        }
    }
    return false;
}

std::string FirstMarkerMatch(const std::string& text, const std::vector<std::string>& markers)
{
    for (const auto& marker : markers)
    {
        if (ContainsCaseInsensitive(text, marker))
        {
            return marker;
        }
    }
    return {};
}

size_t FindMatchingDelimited(const std::string& text, size_t openPos, char openChar, char closeChar)
{
    if (openPos == std::string::npos || openPos >= text.size() || text[openPos] != openChar)
    {
        return std::string::npos;
    }

    int depth = 0;
    bool inString = false;
    bool escaping = false;
    for (size_t i = openPos; i < text.size(); ++i)
    {
        const char ch = text[i];
        if (escaping)
        {
            escaping = false;
            continue;
        }
        if (ch == '\\' && inString)
        {
            escaping = true;
            continue;
        }
        if (ch == '"')
        {
            inString = !inString;
            continue;
        }
        if (inString)
        {
            continue;
        }
        if (ch == openChar)
        {
            ++depth;
        }
        else if (ch == closeChar)
        {
            --depth;
            if (depth == 0)
            {
                return i;
            }
        }
    }

    return std::string::npos;
}

std::string ExtractJsonStringField(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos)
    {
        return {};
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos)
    {
        return {};
    }

    const size_t valueStart = json.find('"', colonPos + 1);
    if (valueStart == std::string::npos)
    {
        return {};
    }

    bool escaping = false;
    std::string value;
    for (size_t i = valueStart + 1; i < json.size(); ++i)
    {
        const char ch = json[i];
        if (escaping)
        {
            value.push_back(ch);
            escaping = false;
            continue;
        }
        if (ch == '\\')
        {
            escaping = true;
            value.push_back(ch);
            continue;
        }
        if (ch == '"')
        {
            return value;
        }
        value.push_back(ch);
    }

    return {};
}

int ExtractJsonIntField(const std::string& json, const std::string& key, int fallback = 0)
{
    const std::string needle = "\"" + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos)
    {
        return fallback;
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos)
    {
        return fallback;
    }

    size_t valueStart = json.find_first_of("-0123456789", colonPos + 1);
    if (valueStart == std::string::npos)
    {
        return fallback;
    }
    size_t valueEnd = json.find_first_not_of("-0123456789", valueStart);
    const std::string value =
        json.substr(valueStart, valueEnd == std::string::npos ? std::string::npos : valueEnd - valueStart);
    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        return fallback;
    }
}

std::string ExtractDiagnosticExcerpt(const std::string& text)
{
    static const std::vector<std::string> diagnosticMarkers = {"error C",
                                                               "fatal error",
                                                               "warning C",
                                                               "note:",
                                                               "LNK",
                                                               "undefined reference",
                                                               "not recognized as an internal or external command",
                                                               "MSB",
                                                               "FAILED",
                                                               "Traceback",
                                                               "Exception"};

    if (text.empty())
    {
        return {};
    }

    std::istringstream input(text);
    std::string line;
    std::vector<std::string> matchingLines;
    std::vector<std::string> allLines;
    while (std::getline(input, line))
    {
        if (!line.empty())
        {
            allLines.push_back(line);
        }
        if (ContainsAnyMarker(line, diagnosticMarkers))
        {
            matchingLines.push_back(line);
            if (matchingLines.size() >= 8)
            {
                break;
            }
        }
    }

    std::ostringstream excerpt;
    const auto& source = matchingLines.empty() ? allLines : matchingLines;
    const size_t startIndex = source.size() > 8 ? source.size() - 8 : 0;
    for (size_t i = startIndex; i < source.size(); ++i)
    {
        if (!excerpt.str().empty())
        {
            excerpt << " || ";
        }
        excerpt << TrimCopy(source[i]);
    }
    return TruncateVerificationEvidence(excerpt.str(), 700);
}

std::string ExtractTerminalVerificationEvidence(const std::string& toolResultJson)
{
    const std::string error = ExtractJsonStringField(toolResultJson, "error");
    const std::string stdoutText = ExtractJsonStringField(toolResultJson, "stdout");
    const std::string command = ExtractJsonStringField(toolResultJson, "command");
    const int exitCode = ExtractJsonIntField(toolResultJson, "exit_code", 0);

    std::ostringstream evidence;
    if (!command.empty())
    {
        evidence << "command=" << command;
    }
    if (exitCode != 0)
    {
        if (!evidence.str().empty())
        {
            evidence << " || ";
        }
        evidence << "exit_code=" << exitCode;
    }

    const std::string excerpt = !error.empty() ? error : ExtractDiagnosticExcerpt(stdoutText);
    if (!excerpt.empty())
    {
        if (!evidence.str().empty())
        {
            evidence << " || ";
        }
        evidence << excerpt;
    }

    return evidence.str();
}

bool IsBuildLikeCommand(const std::string& command)
{
    if (command.empty())
    {
        return false;
    }

    static const std::vector<std::string> buildMarkers = {"cmake --build", "msbuild",   "ninja", "nmake", "devenv",
                                                          "cl.exe",        " link.exe", "\cl ",  "make ", "jom "};
    return ContainsAnyMarker(command, buildMarkers);
}

std::string BuildAutomaticCorrectionContext(const std::string& toolResultJson, int attempt, int maxAttempts)
{
    const std::string command = ExtractJsonStringField(toolResultJson, "command");
    const int exitCode = ExtractJsonIntField(toolResultJson, "exit_code", 0);
    if (exitCode == 0 || !IsBuildLikeCommand(command))
    {
        return {};
    }

    const std::string evidence = ExtractTerminalVerificationEvidence(toolResultJson);
    if (evidence.empty())
    {
        return {};
    }

    std::ostringstream context;
    context << "Automatic correction context:\n";
    context << "The previous build-related tool execution failed verification with a non-zero exit code.\n";
    context << "Re-plan from the compiler evidence below, fix the root cause, and do not claim success until a later "
               "tool result confirms the build passes.\n";
    context << "Replan budget: attempt " << attempt << " of " << maxAttempts << "\n";
    context << "Failed command: " << command << "\n";
    context << "Compiler evidence: " << TruncateVerificationEvidence(evidence, 1200) << "\n";
    context
        << "Required behavior: produce the next corrective tool call or a concise fix plan grounded in this evidence.";
    return context.str();
}

std::string ExtractJsonObjectField(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos)
    {
        return {};
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos)
    {
        return {};
    }

    const size_t objectStart = json.find('{', colonPos + 1);
    if (objectStart == std::string::npos)
    {
        return {};
    }

    const size_t objectEnd = FindMatchingDelimited(json, objectStart, '{', '}');
    if (objectEnd == std::string::npos)
    {
        return {};
    }

    return json.substr(objectStart, objectEnd - objectStart + 1);
}

std::string ExtractJsonArrayField(const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\"";
    const size_t keyPos = json.find(needle);
    if (keyPos == std::string::npos)
    {
        return {};
    }

    const size_t colonPos = json.find(':', keyPos + needle.size());
    if (colonPos == std::string::npos)
    {
        return {};
    }

    const size_t arrayStart = json.find('[', colonPos + 1);
    if (arrayStart == std::string::npos)
    {
        return {};
    }

    const size_t arrayEnd = FindMatchingDelimited(json, arrayStart, '[', ']');
    if (arrayEnd == std::string::npos)
    {
        return {};
    }

    return json.substr(arrayStart, arrayEnd - arrayStart + 1);
}

bool TryNormalizeToolCallEnvelope(const std::string& envelope, std::pair<std::string, std::string>& outCall)
{
    std::string name = ExtractJsonStringField(envelope, "name");
    if (name.empty())
    {
        name = ExtractJsonStringField(envelope, "tool");
    }

    std::string arguments = ExtractJsonObjectField(envelope, "arguments");
    if (arguments.empty())
    {
        arguments = ExtractJsonObjectField(envelope, "params");
    }

    const std::string functionObject = ExtractJsonObjectField(envelope, "function");
    if (!functionObject.empty())
    {
        if (name.empty())
        {
            name = ExtractJsonStringField(functionObject, "name");
        }
        if (arguments.empty())
        {
            arguments = ExtractJsonObjectField(functionObject, "arguments");
        }
        if (arguments.empty())
        {
            const std::string escapedArgs = ExtractJsonStringField(functionObject, "arguments");
            const std::string trimmed = TrimCopy(escapedArgs);
            if (!trimmed.empty() && trimmed.front() == '{')
            {
                const size_t end = FindMatchingDelimited(trimmed, 0, '{', '}');
                if (end != std::string::npos && end == trimmed.size() - 1)
                {
                    arguments = trimmed;
                }
            }
        }
    }

    name = TrimCopy(std::move(name));
    arguments = TrimCopy(std::move(arguments));

    if (name.empty() || arguments.empty())
    {
        return false;
    }
    if (arguments.front() != '{')
    {
        return false;
    }
    const size_t argsEnd = FindMatchingDelimited(arguments, 0, '{', '}');
    if (argsEnd == std::string::npos || argsEnd != arguments.size() - 1)
    {
        return false;
    }

    outCall = {name, arguments};
    return true;
}

void CollectToolCallsFromArray(const std::string& arrayJson, std::vector<std::pair<std::string, std::string>>& calls,
                               std::unordered_set<std::string>& dedupe)
{
    if (arrayJson.empty() || arrayJson.front() != '[')
    {
        return;
    }

    size_t cursor = 1;
    while (cursor < arrayJson.size())
    {
        const size_t objectStart = arrayJson.find('{', cursor);
        if (objectStart == std::string::npos)
        {
            break;
        }
        const size_t objectEnd = FindMatchingDelimited(arrayJson, objectStart, '{', '}');
        if (objectEnd == std::string::npos)
        {
            break;
        }

        std::pair<std::string, std::string> call;
        const std::string entry = arrayJson.substr(objectStart, objectEnd - objectStart + 1);
        if (TryNormalizeToolCallEnvelope(entry, call))
        {
            const std::string key = call.first + "\n" + call.second;
            if (dedupe.insert(key).second)
            {
                calls.push_back(std::move(call));
            }
        }

        cursor = objectEnd + 1;
    }
}

void CollectToolCallsFromObject(const std::string& objectJson, std::vector<std::pair<std::string, std::string>>& calls,
                                std::unordered_set<std::string>& dedupe)
{
    if (objectJson.empty() || objectJson.front() != '{')
    {
        return;
    }

    const std::string singleToolCall = ExtractJsonObjectField(objectJson, "tool_call");
    if (!singleToolCall.empty())
    {
        std::pair<std::string, std::string> call;
        if (TryNormalizeToolCallEnvelope(singleToolCall, call))
        {
            const std::string key = call.first + "\n" + call.second;
            if (dedupe.insert(key).second)
            {
                calls.push_back(std::move(call));
            }
        }
    }

    const std::string multiToolCalls = ExtractJsonArrayField(objectJson, "tool_calls");
    if (!multiToolCalls.empty())
    {
        CollectToolCallsFromArray(multiToolCalls, calls, dedupe);
    }

    std::pair<std::string, std::string> directCall;
    if (TryNormalizeToolCallEnvelope(objectJson, directCall))
    {
        const std::string key = directCall.first + "\n" + directCall.second;
        if (dedupe.insert(key).second)
        {
            calls.push_back(std::move(directCall));
        }
    }
}

std::string BuildMinimalToolSpec(const std::vector<rawrxd::MinimalTool>& tools)
{
    std::ostringstream spec;
    bool firstTool = true;
    for (const auto& tool : tools)
    {
        if (!firstTool)
        {
            spec << "; ";
        }
        firstTool = false;
        spec << tool.name << '(';
        bool firstParam = true;
        for (const auto& [key, unusedDesc] : tool.parameters)
        {
            (void)unusedDesc;
            if (!firstParam)
            {
                spec << ',';
            }
            firstParam = false;
            spec << key << '*';
        }
        spec << ')';
    }
    if (!firstTool)
    {
        spec << ';';
    }
    return spec.str();
}

std::string NormalizeForcedPrefixResponse(std::string response, const std::string& forcedPrefix)
{
    response = TrimCopy(std::move(response));
    if (forcedPrefix.empty())
    {
        return response;
    }
    if (response.find("<tool_call>") != std::string::npos || response.find("<final_answer>") != std::string::npos)
    {
        return response;
    }
    if (response.empty() || rawrxd::ResponseValidator::ContainsPromptLeakage(response))
    {
        return response;
    }
    return forcedPrefix + response;
}

std::string NormalizeToolResultJsonValue(const std::string& value)
{
    const std::string trimmed = TrimCopy(value);
    if (trimmed.empty())
    {
        return R"({"error":"empty_tool_result"})";
    }

    const char first = trimmed.front();
    const bool looksLikeJsonValue = first == '{' || first == '[' || first == '"' || first == '-' ||
                                    std::isdigit(static_cast<unsigned char>(first)) || first == 't' || first == 'f' ||
                                    first == 'n';

    if (looksLikeJsonValue)
    {
        return trimmed;
    }

    return std::string("{\"text\":\"") + EscapeJsonString(trimmed) + "\"}";
}

std::string ExtractLikelyPath(const std::string& message)
{
    std::istringstream stream(message);
    std::string token;
    while (stream >> token)
    {
        token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char ch)
                                   { return ch == '"' || ch == '\'' || ch == ',' || ch == ')' || ch == '('; }),
                    token.end());
        if (token.find("..") != std::string::npos)
        {
            continue;
        }
        if (token.find('/') != std::string::npos || token.find('\\') != std::string::npos ||
            token.find(".txt") != std::string::npos || token.find(".md") != std::string::npos ||
            token.find(".cpp") != std::string::npos || token.find(".h") != std::string::npos)
        {
            return token;
        }
    }
    return {};
}

std::string ExtractLikelyCommand(const std::string& message)
{
    const size_t cmdPos = FindCaseInsensitive(message, "cmd /c");
    if (cmdPos != std::string::npos)
    {
        return TrimCopy(message.substr(cmdPos));
    }

    const size_t commandPos = FindCaseInsensitive(message, "command ");
    if (commandPos != std::string::npos)
    {
        std::string candidate = TrimCopy(message.substr(commandPos + 8));
        const size_t andPos = FindCaseInsensitive(candidate, " and ");
        if (andPos != std::string::npos)
        {
            candidate = TrimCopy(candidate.substr(0, andPos));
        }
        return candidate;
    }

    return {};
}

std::string SummarizeQueryResult(const rawrxd::QueryResult& query)
{
    std::ostringstream summary;
    if (!query.gitState.branch.empty())
    {
        summary << "branch=" << query.gitState.branch << "\n";
    }
    if (!query.runtimeState.cwd.empty())
    {
        summary << "cwd=" << query.runtimeState.cwd << "\n";
    }
    if (!query.matchedFiles.empty())
    {
        summary << "files:";
        for (const auto& file : query.matchedFiles)
        {
            summary << " " << file.path;
        }
        summary << "\n";
    }
    if (!query.matchedSymbols.empty())
    {
        summary << "symbols:";
        for (const auto& symbol : query.matchedSymbols)
        {
            summary << " " << symbol.name;
        }
        summary << "\n";
    }
    return summary.str();
}

std::string NormalizeAssistantText(std::string response)
{
    response = rawrxd::ResponseValidator::StripFluff(response);
    if (response.rfind("FINAL:", 0) == 0)
    {
        response = response.substr(6);
    }
    if (response.find("<final_answer>") != std::string::npos)
    {
        const std::string finalText = ExtractTagBody(response, "final_answer");
        if (!finalText.empty())
        {
            response = finalText;
        }
    }
    return rawrxd::ResponseValidator::Trim(response);
}

std::string AliasFromToolName(const std::string& toolName)
{
    if (toolName == "file_read")
    {
        return "file_content";
    }
    if (toolName == "terminal_execute")
    {
        return "terminal_result";
    }
    return toolName.empty() ? "step_result" : toolName + "_result";
}

std::string EffectiveIdeSessionId(const rawrxd::IdeRequest& req)
{
    if (!req.sessionId.empty())
    {
        return req.sessionId;
    }

    switch (req.kind)
    {
        case rawrxd::IdeRequestKind::InlineCompletion:
            return "ide-inline";
        case rawrxd::IdeRequestKind::SelectionRewrite:
            return "ide-rewrite";
        case rawrxd::IdeRequestKind::CodeEdit:
            return "ide-edit";
        case rawrxd::IdeRequestKind::AgenticTask:
            return req.agentRequest.session_id.empty() ? "ide-agentic" : req.agentRequest.session_id;
    }

    return "ide-default";
}

std::optional<std::string> ApplyDeterministicRewrite(const std::string& selectedText, const std::string& instruction)
{
    const std::string lowerInstruction = ToLowerCopy(instruction);
    if (lowerInstruction.find("uppercase") != std::string::npos)
    {
        std::string rewritten = selectedText;
        std::transform(rewritten.begin(), rewritten.end(), rewritten.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        return rewritten;
    }
    if (lowerInstruction.find("lowercase") != std::string::npos)
    {
        std::string rewritten = selectedText;
        std::transform(rewritten.begin(), rewritten.end(), rewritten.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return rewritten;
    }
    if (lowerInstruction.find("trim trailing whitespace") != std::string::npos)
    {
        std::ostringstream output;
        std::istringstream input(selectedText);
        std::string line;
        bool firstLine = true;
        while (std::getline(input, line))
        {
            while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
            {
                line.pop_back();
            }
            if (!firstLine)
            {
                output << '\n';
            }
            firstLine = false;
            output << line;
        }
        return output.str();
    }
    return std::nullopt;
}

}  // namespace

namespace rawrxd
{

// Provided by agentic_controller_wiring.cpp (same namespace).
std::string getLLMResponse(const std::string& system_prompt, const std::string& user_message,
                           const std::string& model_path);

MinimalAgentController& MinimalAgentController::instance()
{
    static MinimalAgentController instance;
    return instance;
}

void MinimalAgentController::initialize()
{
    if (initialized_)
        return;

    // Core registry init handles all 44+ tools via X-Macros
    // We bind to the central AgentToolRegistry for consolidated autonomy

    workspace_context_.Initialize(std::filesystem::current_path().string());
    session_state_.BindWorkspaceSnapshot(workspace_context_.RefreshSnapshot());
    initialized_ = true;

    agentLogInfo("[AgentController] Initialized via AgentToolRegistry");
}

void MinimalAgentController::registerDefaultTools()
{
    // Deprecated: Tools are now managed by AgentToolRegistry
}

std::string MinimalAgentController::buildSystemPrompt() const
{
    return BuildSmallModelPrompt("{{USER_QUERY}}", BuildMinimalToolSpec(tools_));
}

std::vector<std::pair<std::string, std::string>> MinimalAgentController::parseToolCalls(const std::string& response)
{
    std::vector<std::pair<std::string, std::string>> calls;
    std::unordered_set<std::string> dedupe;

    const std::string xmlName = TrimCopy(ExtractTagBody(response, "name"));
    const std::string xmlParams = TrimCopy(ExtractTagBody(response, "parameters"));
    if (!xmlName.empty() && response.find("<tool_call>") != std::string::npos)
    {
        calls.push_back({xmlName, xmlParams});
        return calls;
    }

    // Preferred protocol: TOOL_CALL: {"name":"...","arguments":{...}}
    size_t scanPos = 0;
    while (true)
    {
        const size_t toolCallPos = FindCaseInsensitive(response, "TOOL_CALL:", scanPos);
        if (toolCallPos == std::string::npos)
        {
            break;
        }
        const size_t jsonStart = response.find('{', toolCallPos);
        const size_t jsonEnd =
            jsonStart == std::string::npos ? std::string::npos : FindMatchingDelimited(response, jsonStart, '{', '}');
        if (jsonStart != std::string::npos && jsonEnd != std::string::npos)
        {
            const std::string toolCallEnvelope = response.substr(jsonStart, jsonEnd - jsonStart + 1);
            CollectToolCallsFromObject(toolCallEnvelope, calls, dedupe);
            scanPos = jsonEnd + 1;
        }
        else
        {
            scanPos = toolCallPos + std::strlen("TOOL_CALL:");
        }
    }

    if (!calls.empty())
    {
        return calls;
    }

    // Fallback: scan each balanced JSON object in the response and attempt
    // to normalize either direct tool envelopes or OpenAI-style wrappers.
    size_t objectCursor = 0;
    while (objectCursor < response.size())
    {
        const size_t objectStart = response.find('{', objectCursor);
        if (objectStart == std::string::npos)
        {
            break;
        }
        const size_t objectEnd = FindMatchingDelimited(response, objectStart, '{', '}');
        if (objectEnd == std::string::npos)
        {
            break;
        }
        const std::string objectJson = response.substr(objectStart, objectEnd - objectStart + 1);
        CollectToolCallsFromObject(objectJson, calls, dedupe);
        objectCursor = objectEnd + 1;
    }

    return calls;
}

std::string MinimalAgentController::executeTool(const std::string& name, const std::string& args_json)
{
    auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
    try
    {
        json params = json::parse(args_json);
        auto result = handlers.Execute(name, params);
        if (result.isSuccess())
        {
            return result.output.empty() ? result.toJson().dump() : result.output;
        }
        const std::string err = result.error.empty() ? "tool execution failed" : result.error;
        return json{{"error", err}}.dump();
    }
    catch (const std::exception& e)
    {
        return json{{"error", std::string("JSON parse/dispatch failed: ") + e.what()}}.dump();
    }
}

std::string MinimalAgentController::callLLM(const std::string& system_prompt, const std::string& user_message,
                                            const std::string& model_path)
{
    try
    {
        // First prefer the wiring lane so agentic and bridge-backed inference stay aligned.
        std::string effectiveSystemPrompt = system_prompt;
        std::string effectiveUserMessage = user_message;
        if (effectiveSystemPrompt.find("{{USER_QUERY}}") != std::string::npos)
        {
            const size_t markerPos = effectiveSystemPrompt.find("{{USER_QUERY}}");
            effectiveSystemPrompt.replace(markerPos, std::strlen("{{USER_QUERY}}"), user_message);
            effectiveUserMessage.clear();
        }

        const std::string wired = getLLMResponse(effectiveSystemPrompt, effectiveUserMessage, model_path);
        if (!wired.empty() && wired.rfind("Error:", 0) != 0)
        {
            return wired;
        }

        if (!inference_engine_ || !inference_engine_->IsModelLoaded())
        {
            return wired.empty() ? "Error: Inference backend unavailable" : wired;
        }

        const std::string full_prompt =
            effectiveUserMessage.empty()
                ? effectiveSystemPrompt
                : (effectiveSystemPrompt + "\n\nUser: " + effectiveUserMessage + "\n\nAssistant:");
        auto input_tokens = inference_engine_->Tokenize(full_prompt);
        if (input_tokens.empty())
        {
            return "Error: Failed to tokenize prompt";
        }

        std::string text;
        auto start = std::chrono::steady_clock::now();
        inference_engine_->GenerateStreaming(
            input_tokens, 1024, [&](const std::string& token_text) { text += token_text; }, []() {});
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

        if (text.empty())
        {
            return "Error: Empty completion from CPUInferenceEngine";
        }

        agentLogInfo(std::string("[AgentController] LLM response ") + std::to_string(elapsed.count()) + "ms latency");
        return text;
    }
    catch (const std::exception& e)
    {
        agentLogError(std::string("[AgentController] Exception in LLM call: ") + e.what());
        return "Error: " + std::string(e.what());
    }
}

std::string MinimalAgentController::getJsonValue(const std::string& json_str, const std::string& key)
{
    auto pos = json_str.find("\"" + key + "\"");
    if (pos == std::string::npos)
    {
        return "";
    }

    auto start = json_str.find("\"", pos + key.length() + 3) + 1;
    auto end = json_str.find("\"", start);

    if (start == std::string::npos || end == std::string::npos)
    {
        return "";
    }

    return json_str.substr(start, end - start);
}

TaskIntent MinimalAgentController::classifyIntent(const std::string& message) const
{
    const std::string lower = ToLowerCopy(message);
    const bool mentionsFile = lower.find("file") != std::string::npos || lower.find("read ") != std::string::npos ||
                              lower.find("write ") != std::string::npos;
    const bool mentionsTerminal = lower.find("terminal") != std::string::npos ||
                                  lower.find("cmd /c") != std::string::npos ||
                                  lower.find("command") != std::string::npos || lower.find("run ") != std::string::npos;

    if (mentionsFile && mentionsTerminal)
    {
        return TaskIntent::Hybrid;
    }
    if (mentionsFile)
    {
        return TaskIntent::FileIO;
    }
    if (mentionsTerminal)
    {
        return TaskIntent::Terminal;
    }
    if (lower.find("why") != std::string::npos || lower.find("explain") != std::string::npos ||
        lower.find("summarize") != std::string::npos || lower.find("analyze") != std::string::npos)
    {
        return TaskIntent::Reasoning;
    }
    return TaskIntent::Unknown;
}

ExecutionLane MinimalAgentController::selectLane(TaskIntent intent) const
{
    switch (intent)
    {
        case TaskIntent::FileIO:
        case TaskIntent::Terminal:
            return ExecutionLane::SyntheticFirst;
        case TaskIntent::Hybrid:
            return ExecutionLane::ModelAssisted;
        case TaskIntent::Reasoning:
            return ExecutionLane::ModelFirst;
        case TaskIntent::Unknown:
            return ExecutionLane::ModelAssisted;
    }
    return ExecutionLane::ModelAssisted;
}

ExecutionPlan MinimalAgentController::buildExecutionPlan(const std::string& message) const
{
    ExecutionPlan plan;
    plan.intent = classifyIntent(message);
    plan.lane = selectLane(plan.intent);
    plan.trust = plan.lane == ExecutionLane::SyntheticOnly ? TrustScore::DeterministicOnly : TrustScore::ModelSuspect;

    switch (plan.intent)
    {
        case TaskIntent::FileIO:
        {
            plan.rationale = "File-oriented request routed toward deterministic file tooling.";
            const std::string path = ExtractLikelyPath(message);
            if (!path.empty())
            {
                plan.steps.push_back({"file_read", std::string("{\"path\":\"") + path + "\"}", true});
            }
            break;
        }
        case TaskIntent::Terminal:
        {
            plan.rationale = "Terminal-oriented request routed toward deterministic command execution.";
            const std::string command = ExtractLikelyCommand(message);
            if (!command.empty())
            {
                plan.steps.push_back({"terminal_execute", std::string("{\"command\":\"") + command + "\"}", true});
            }
            break;
        }
        case TaskIntent::Hybrid:
            plan.rationale = "Hybrid request may need deterministic tools plus model-assisted binding.";
            break;
        case TaskIntent::Reasoning:
            plan.rationale = "Reasoning-dominant request can start in the model lane.";
            plan.trust = TrustScore::ModelSuspect;
            break;
        case TaskIntent::Unknown:
            plan.rationale = "Unknown request defaults to model-assisted planning with deterministic fallback.";
            break;
    }

    return plan;
}

TrustScore MinimalAgentController::evaluateTrust(const std::string& response, const std::string& prompt) const
{
    if (ParrotDetector::IsParroting(response, prompt) || ParrotDetector::IsDegenerate(response))
    {
        return TrustScore::ModelInvalid;
    }
    if (ResponseValidator::ContainsPromptLeakage(response))
    {
        return TrustScore::ModelInvalid;
    }
    const auto validation = ResponseValidator::Validate(response, {});
    if (validation.isValid)
    {
        return TrustScore::ModelOk;
    }
    return TrustScore::ModelSuspect;
}

MinimalAgenticResponse MinimalAgentController::process(const MinimalAgenticRequest& request)
{
    if (!initialized_)
    {
        return {"", false, "AgentController not initialized", 0, {}, {}};
    }

    try
    {
        const int max_iterations = std::max(1, request.max_iterations);
        std::string toolsSpec = RawrXD::Agent::AgentToolHandlers::BuildCompactToolCatalogForPrompt();
        constexpr size_t kMaxToolCatalogChars = 14000;
        if (toolsSpec.size() > kMaxToolCatalogChars)
        {
            toolsSpec.resize(kMaxToolCatalogChars);
            toolsSpec += "\n...(tool catalog truncated for prompt size)\n";
        }
        ExecutionPlan plan = buildExecutionPlan(request.message);
        const std::string effectiveSessionId = request.session_id.empty() ? "default-session" : request.session_id;
        const ExecutionPlanIR planIr = ConvertExecutionPlanToIR(plan, request.message);
        SessionContext session = loadSessionContext(request.session_id);
        const size_t transcript_start_index = session.history.size();
        const WorkspaceSnapshot workspaceSnapshot = workspace_context_.RefreshSnapshot();
        session_state_.BindWorkspaceSnapshot(workspaceSnapshot);
        session_state_.AppendPlanIR(effectiveSessionId, planIr, "agentic_task", DescribePlanIR(planIr),
                                    request.message);
        const QueryResult workspaceQuery = workspace_context_.Query(request.message);
        session_state_.RecordInteraction({"agentic_task", request.message, "", "", request.session_id, ""});
        PushSessionMessage(session, "user", request.message);
        const std::string continuityContext = session_state_.BuildPlanContinuityContext(effectiveSessionId, 2);
        int iterations = 0;
        int tool_calls_total = 0;
        std::vector<MinimalAgenticToolStep> tool_step_trace;
        int automaticBuildReplans = 0;
        std::string verificationEvidence;
        const auto isRegisteredTool = [](const std::string& toolName)
        { return RawrXD::Agent::AgentToolHandlers::Instance().HasTool(toolName); };
        std::string current_message = MergePromptContinuity(request.message, continuityContext);
        const auto finalizePlanGraph = [&](bool success, const std::string& outputPreview)
        {
            session_state_.UpdatePlanOutcome(effectiveSessionId, planIr.planId, success, outputPreview);
            const VerificationDecision verification =
                VerifyExecutionOutcome(plan, success, outputPreview, verificationEvidence);
            if (verification.attempted)
            {
                session_state_.AppendVerificationOutcome(effectiveSessionId, planIr.planId, verification.verifierKind,
                                                         verification.passed, verification.summary);
            }
        };
        const std::vector<std::string> forbiddenPhrases = {"CRITICAL CONSTRAINTS", "OUTPUT FORMAT SPECIFICATION",
                                                           "FEW-SHOT EXAMPLES",    "DECISION PROTOCOL",
                                                           "Wrong (echo):",        "Correct:"};

        {
            std::ostringstream procStart;
            procStart << "[AgentController] process start session=" << request.session_id
                      << " model=" << request.model_path << " max_iterations=" << max_iterations
                      << " tools=" << (request.enable_tools ? "on" : "off") << " intent=" << ToString(plan.intent)
                      << " lane=" << ToString(plan.lane) << " trust=" << ToString(plan.trust);
            agentLogInfo(procStart.str());
        }
        if (!workspaceQuery.matchedFiles.empty() || !workspaceQuery.matchedSymbols.empty())
        {
            std::ostringstream wk;
            wk << "[AgentController] workspace hits files=" << workspaceQuery.matchedFiles.size()
               << " symbols=" << workspaceQuery.matchedSymbols.size();
            agentLogInfo(wk.str());
        }
        if (!plan.rationale.empty())
        {
            agentLogInfo(std::string("[AgentController] plan rationale=") + plan.rationale);
        }
        for (const auto& step : plan.steps)
        {
            std::ostringstream ps;
            ps << "[AgentController] plan step tool=" << step.toolName
               << " deterministic=" << (step.deterministic ? "yes" : "no") << " args=" << step.argumentsJson;
            agentLogInfo(ps.str());
        }

        while (iterations < max_iterations)
        {
            iterations++;

            {
                std::ostringstream it;
                it << "[AgentController] iter=" << iterations << " message_size=" << current_message.size();
                agentLogInfo(it.str());
            }

            std::string llm_response;
            ResponseValidator::ValidationResult validation;
            bool accepted = false;
            bool fallbackToSynthetic = false;
            std::string attempt_message = current_message;
            for (int attempt = 0; attempt < 4; ++attempt)
            {
                const ConstraintLevel level = static_cast<ConstraintLevel>(attempt < 3 ? attempt : 3);
                std::string forcedPrefix;
                const std::string prompt = BuildPromptForLevel(level, attempt_message, toolsSpec, &forcedPrefix);
                const auto llmStart = std::chrono::steady_clock::now();
                llm_response = callLLM(prompt, "", request.model_path);
                const auto llmLatency =
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - llmStart);

                if (llm_response.rfind("Error:", 0) == 0)
                {
                    break;
                }

                const std::string candidateResponse = NormalizeForcedPrefixResponse(llm_response, forcedPrefix);

                // P0: JSON Hardening — scrub the response before trust evaluation or validation
                const std::string scrubbed = ScrubModelResponse(candidateResponse);
                llm_response = scrubbed;

                plan.trust = evaluateTrust(scrubbed, prompt);

                if (ResponseValidator::ContainsPromptLeakage(scrubbed))
                {
                    validation.rejectionReason = "Prompt leakage detected";
                }
                else
                {
                    validation = ResponseValidator::Validate(scrubbed, forbiddenPhrases);
                }

                const std::string finalResponse = validation.isValid ? validation.cleanedResponse : scrubbed;
                // Ensure cleanedResponse is also scrubbed if valid
                if (validation.isValid)
                {
                    validation.cleanedResponse = ScrubModelResponse(validation.cleanedResponse);
                }

                AgenticRunResult runResult;
                runResult.prompt = prompt;
                runResult.output = candidateResponse;
                runResult.isValid = validation.isValid;
                runResult.isParrot = ParrotDetector::IsParroting(candidateResponse, prompt);
                runResult.isDegenerate = ParrotDetector::IsDegenerate(candidateResponse);
                runResult.hasPromptLeakage = ResponseValidator::ContainsPromptLeakage(candidateResponse);
                runResult.latency_ms = llmLatency.count();

                const PolicyAction action = EvaluateResponse(runResult, session, plan.lane);
                if (action == PolicyAction::RETRY_COLD)
                {
                    ++session.retry_count;
                    validation.rejectionReason = validation.rejectionReason.empty()
                                                     ? "Cold retry requested by response policy"
                                                     : validation.rejectionReason;
                    attempt_message =
                        request.message +
                        "\nPrevious attempt was invalid or echoed the prompt. Respond only with TOOL_CALL or FINAL.";
                    {
                        std::ostringstream r;
                        r << "[AgentController] retry level=" << ConstraintLevelName(level)
                          << " validation_failure=" << validation.rejectionReason << " trust=" << ToString(plan.trust)
                          << " latency_ms=" << runResult.latency_ms << " preview=" << candidateResponse.substr(0, 64);
                        agentLogInfo(r.str());
                    }
                    continue;
                }

                if (action == PolicyAction::FALLBACK_SYNTHETIC)
                {
                    session.fallback_active = true;
                    fallbackToSynthetic = true;
                    validation.rejectionReason = validation.rejectionReason.empty()
                                                     ? "Synthetic fallback activated by response policy"
                                                     : validation.rejectionReason;
                    break;
                }

                if (action == PolicyAction::ABORT)
                {
                    validation.rejectionReason = validation.rejectionReason.empty() ? "Response policy requested abort"
                                                                                    : validation.rejectionReason;
                    break;
                }

                if (validation.isValid)
                {
                    accepted = true;
                    session.retry_count = 0;
                    session.fallback_active = false;
                    {
                        std::ostringstream r;
                        r << "[AgentController] retry success level=" << ConstraintLevelName(level)
                          << " trust=" << ToString(plan.trust) << " preview=" << candidateResponse.substr(0, 64);
                        agentLogInfo(r.str());
                    }
                    break;
                }

                {
                    std::ostringstream r;
                    r << "[AgentController] retry level=" << ConstraintLevelName(level)
                      << " validation_failure=" << validation.rejectionReason << " trust=" << ToString(plan.trust)
                      << " preview=" << candidateResponse.substr(0, 64);
                    agentLogInfo(r.str());
                }
            }

            if (fallbackToSynthetic)
            {
                auto response =
                    executeSyntheticFallback(request, plan, session, tool_calls_total, transcript_start_index);
                storeSessionContext(request.session_id, session);
                finalizePlanGraph(response.success, response.success ? response.final_message : response.error);
                session_state_.AppendExecution({plan,
                                                {response.success, response.final_message, response.error},
                                                TrustScore::DeterministicOnly,
                                                iterations,
                                                request.session_id,
                                                "",
                                                planIr.planId});
                return response;
            }

            if (llm_response.rfind("Error:", 0) == 0)
            {
                agentLogInfo(std::string("[AgentController] stop=error llm=") + llm_response);
                PushSessionMessage(session, "assistant", llm_response);
                MinimalAgenticResponse response{"", false, llm_response, tool_calls_total, tool_step_trace, {}};
                storeSessionContext(request.session_id, session);
                finalizePlanGraph(false, llm_response);
                session_state_.AppendExecution(
                    {plan, {false, "", llm_response}, plan.trust, iterations, request.session_id, "", planIr.planId});
                StampMinimalAgentTranscript(response, session, transcript_start_index);
                return response;
            }

            if (!accepted)
            {
                const std::string error = validation.rejectionReason.empty() ? "Model response failed validation"
                                                                             : validation.rejectionReason;
                PushSessionMessage(session, "assistant", error);
                MinimalAgenticResponse response{"", false, error, tool_calls_total, tool_step_trace, {}};
                storeSessionContext(request.session_id, session);
                finalizePlanGraph(false, error);
                session_state_.AppendExecution(
                    {plan, {false, "", error}, plan.trust, iterations, request.session_id, "", planIr.planId});
                StampMinimalAgentTranscript(response, session, transcript_start_index);
                return response;
            }

            if (!request.enable_tools)
            {
                agentLogInfo("[AgentController] stop=tools_disabled");
                PushSessionMessage(session, "assistant", validation.cleanedResponse);
                MinimalAgenticResponse response{
                    validation.cleanedResponse, true, "", tool_calls_total, tool_step_trace, {},
                };
                storeSessionContext(request.session_id, session);
                finalizePlanGraph(true, validation.cleanedResponse);
                session_state_.AppendExecution({plan,
                                                {true, validation.cleanedResponse, ""},
                                                plan.trust,
                                                iterations,
                                                request.session_id,
                                                "",
                                                planIr.planId});
                StampMinimalAgentTranscript(response, session, transcript_start_index);
                return response;
            }

            // Check for tool calls
            std::vector<std::pair<std::string, std::string>> tool_calls;
            try
            {
                // Scrub the response once before trying all parsing logic
                validation.cleanedResponse = ScrubModelResponse(validation.cleanedResponse);
                tool_calls = parseToolCalls(validation.cleanedResponse);
            }
            catch (const std::exception& ex)
            {
                const std::string error = std::string("Tool call parse failure: ") + ex.what();
                PushSessionMessage(session, "assistant", error);
                MinimalAgenticResponse response{"", false, error, tool_calls_total, tool_step_trace, {}};
                storeSessionContext(request.session_id, session);
                finalizePlanGraph(false, error);
                session_state_.AppendExecution({plan,
                                                {false, "", error},
                                                TrustScore::ModelInvalid,
                                                iterations,
                                                request.session_id,
                                                "",
                                                planIr.planId});
                StampMinimalAgentTranscript(response, session, transcript_start_index);
                return response;
            }
            catch (...)
            {
                const std::string error = "Tool call parse failure: unknown exception";
                PushSessionMessage(session, "assistant", error);
                MinimalAgenticResponse response{"", false, error, tool_calls_total, tool_step_trace, {}};
                storeSessionContext(request.session_id, session);
                finalizePlanGraph(false, error);
                session_state_.AppendExecution({plan,
                                                {false, "", error},
                                                TrustScore::ModelInvalid,
                                                iterations,
                                                request.session_id,
                                                "",
                                                planIr.planId});
                StampMinimalAgentTranscript(response, session, transcript_start_index);
                return response;
            }

            if (tool_calls.empty())
            {
                const bool looksLikeToolPayload =
                    FindCaseInsensitive(validation.cleanedResponse, "tool_call", 0) != std::string::npos ||
                    FindCaseInsensitive(validation.cleanedResponse, "\"action\"", 0) != std::string::npos ||
                    FindCaseInsensitive(validation.cleanedResponse, "\"tool\"", 0) != std::string::npos ||
                    FindCaseInsensitive(validation.cleanedResponse, "<tool_call>", 0) != std::string::npos;
                if (looksLikeToolPayload)
                {
                    const std::string responseSnippet = validation.cleanedResponse.size() > 512
                                                            ? (validation.cleanedResponse.substr(0, 512) + "...")
                                                            : validation.cleanedResponse;
                    agentLogWarn(std::string("[AgentController] tool-call parse miss response=") + responseSnippet);
                }

                if (validation.isFinalAnswer && validation.cleanedResponse.find("<final_answer>") != std::string::npos)
                {
                    const std::string finalText = TrimCopy(ExtractTagBody(validation.cleanedResponse, "final_answer"));
                    agentLogInfo("[AgentController] stop=final_xml");
                    const std::string output = finalText.empty() ? validation.cleanedResponse : finalText;
                    PushSessionMessage(session, "assistant", output);
                    MinimalAgenticResponse response{output, true, "", tool_calls_total, tool_step_trace, {}};
                    storeSessionContext(request.session_id, session);
                    finalizePlanGraph(true, output);
                    session_state_.AppendExecution(
                        {plan, {true, output, ""}, plan.trust, iterations, request.session_id, "", planIr.planId});
                    StampMinimalAgentTranscript(response, session, transcript_start_index);
                    return response;
                }

                // Preferred final-answer envelope.
                if (validation.cleanedResponse.rfind("FINAL:", 0) == 0)
                {
                    const std::string final_text = validation.cleanedResponse.substr(6);
                    agentLogInfo("[AgentController] stop=final_prefix");
                    PushSessionMessage(session, "assistant", final_text);
                    MinimalAgenticResponse response{
                        final_text, true, "", tool_calls_total, tool_step_trace, {},
                    };
                    storeSessionContext(request.session_id, session);
                    finalizePlanGraph(true, final_text);
                    session_state_.AppendExecution(
                        {plan, {true, final_text, ""}, plan.trust, iterations, request.session_id, "", planIr.planId});
                    StampMinimalAgentTranscript(response, session, transcript_start_index);
                    return response;
                }

                // No tool calls - return final response
                agentLogInfo("[AgentController] stop=final_plain");
                PushSessionMessage(session, "assistant", validation.cleanedResponse);
                MinimalAgenticResponse response{
                    validation.cleanedResponse, true, "", tool_calls_total, tool_step_trace, {},
                };
                storeSessionContext(request.session_id, session);
                finalizePlanGraph(true, validation.cleanedResponse);
                session_state_.AppendExecution({plan,
                                                {true, validation.cleanedResponse, ""},
                                                plan.trust,
                                                iterations,
                                                request.session_id,
                                                "",
                                                planIr.planId});
                StampMinimalAgentTranscript(response, session, transcript_start_index);
                return response;
            }

            // Execute tool calls
            tool_calls_total += tool_calls.size();

            // Build results message
            std::string tool_results = "{\"tool_results\":[";
            std::string automaticCorrectionContext;
            for (int i = 0; i < static_cast<int>(tool_calls.size()); ++i)
            {
                if (i > 0)
                    tool_results += ",";

                const auto& [tool_name, args] = tool_calls[i];
                agentLogInfo(std::string("[AgentController] tool_call name=") + tool_name);
                if (tool_name == "terminal_execute")
                {
                    const std::string command = ExtractJsonStringField(args, "command");
                    if (!command.empty())
                    {
                        workspace_context_.RecordCommand(command);
                    }
                }
                std::string result;
                if (!isRegisteredTool(tool_name))
                {
                    result = std::string("{\"error\":\"tool_not_registered:") + EscapeJsonString(tool_name) + "\"}";
                }
                else
                {
                    result = executeTool(tool_name, args);
                }
                if (tool_name == "terminal_execute")
                {
                    const std::string evidence = ExtractTerminalVerificationEvidence(result);
                    if (!evidence.empty())
                    {
                        if (!verificationEvidence.empty())
                        {
                            verificationEvidence += "\n---\n";
                        }
                        verificationEvidence += evidence;
                        verificationEvidence = TruncateVerificationEvidence(verificationEvidence, 2000);
                    }
                    if (automaticBuildReplans < kMaxAutomaticBuildReplans && automaticCorrectionContext.empty())
                    {
                        automaticCorrectionContext = BuildAutomaticCorrectionContext(result, automaticBuildReplans + 1,
                                                                                     kMaxAutomaticBuildReplans);
                    }
                }
                tool_step_trace.push_back(MinimalAgenticToolStep{tool_name, args, result});
                PushSessionMessage(session, "tool", tool_name + ": " + result);

                const std::string safeResult = NormalizeToolResultJsonValue(result);
                tool_results += "{\"tool\":\"" + EscapeJsonString(tool_name) + "\",\"result\":" + safeResult + "}";
            }
            tool_results += "]}";

            // Add to conversation and continue
            if (!automaticCorrectionContext.empty())
            {
                ++automaticBuildReplans;
                current_message = MergePromptContinuity(request.message, continuityContext) + "\n\n" +
                                  automaticCorrectionContext +
                                  "\n\nLatest tool results:\nTOOL_RESULT: " + tool_results +
                                  "\nRespond with TOOL_CALL: {...} to correct the failure, or FINAL: ... only if the "
                                  "build issue is resolved.";
                {
                    std::ostringstream ar;
                    ar << "[AgentController] auto_replan trigger=build_failure attempt=" << automaticBuildReplans
                       << " evidence_preview=" << automaticCorrectionContext.substr(0, 120);
                    agentLogInfo(ar.str());
                }
            }
            else
            {
                current_message =
                    "TOOL_RESULT: " + tool_results + "\nRespond with either TOOL_CALL: {...} or FINAL: ...";
            }
        }

        agentLogInfo(std::string("[AgentController] stop=max_iterations iterations=") + std::to_string(iterations));

        const std::string maxIterErr = "Max iterations exceeded";
        PushSessionMessage(session, "assistant", maxIterErr);
        MinimalAgenticResponse response{"", false, maxIterErr, tool_calls_total, tool_step_trace, {}};
        storeSessionContext(request.session_id, session);
        finalizePlanGraph(false, maxIterErr);
        session_state_.AppendExecution({plan,
                                        {false, "", "Max iterations exceeded"},
                                        plan.trust,
                                        iterations,
                                        request.session_id,
                                        "",
                                        planIr.planId});
        StampMinimalAgentTranscript(response, session, transcript_start_index);
        return response;
    }
    catch (const std::exception& e)
    {
        const std::string error = std::string("Exception: ") + e.what();
        session_state_.AppendExecution({{}, {false, "", error}, TrustScore::ModelInvalid, 0, request.session_id, ""});
        return {"", false, error, 0, {}, {}};
    }
}

SessionContext MinimalAgentController::loadSessionContext(const std::string& sessionId) const
{
    const std::string effectiveSessionId = sessionId.empty() ? "default-session" : sessionId;
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    const auto it = sessions_.find(effectiveSessionId);
    return it == sessions_.end() ? SessionContext{} : it->second;
}

void MinimalAgentController::storeSessionContext(const std::string& sessionId, const SessionContext& session)
{
    const std::string effectiveSessionId = sessionId.empty() ? "default-session" : sessionId;
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_[effectiveSessionId] = session;
}

MinimalAgenticResponse MinimalAgentController::executeSyntheticFallback(const MinimalAgenticRequest& request,
                                                                        const ExecutionPlan& plan,
                                                                        SessionContext& session, int toolCallsTotal,
                                                                        size_t transcriptStartIndex)
{
    (void)request;
    if (plan.steps.empty())
    {
        const std::string error = "Synthetic fallback requested, but no deterministic plan steps are available";
        PushSessionMessage(session, "assistant", error);
        MinimalAgenticResponse response{"", false, error, toolCallsTotal, {}, {}};
        StampMinimalAgentTranscript(response, session, transcriptStartIndex);
        return response;
    }

    std::ostringstream combined;
    bool first = true;
    int executedSteps = toolCallsTotal;
    std::vector<MinimalAgenticToolStep> syn_trace;
    for (const auto& step : plan.steps)
    {
        if (!step.deterministic)
        {
            continue;
        }

        const std::string result = executeTool(step.toolName, step.argumentsJson);
        syn_trace.push_back(MinimalAgenticToolStep{step.toolName, step.argumentsJson, result});
        if (!first)
        {
            combined << "\n";
        }
        first = false;
        combined << step.toolName << ": " << result;
        PushSessionMessage(session, "tool", step.toolName + ": " + result);
        ++executedSteps;
    }

    if (first)
    {
        const std::string error =
            "Synthetic fallback activated, but no deterministic steps were eligible for execution";
        PushSessionMessage(session, "assistant", error);
        MinimalAgenticResponse response{"", false, error, toolCallsTotal, {}, {}};
        StampMinimalAgentTranscript(response, session, transcriptStartIndex);
        return response;
    }

    session.retry_count = 0;
    session.fallback_active = false;
    const std::string output = combined.str();
    PushSessionMessage(session, "assistant", output);
    MinimalAgenticResponse response{output, true, "", executedSteps, std::move(syn_trace), {}};
    StampMinimalAgentTranscript(response, session, transcriptStartIndex);
    return response;
}

void MinimalAgentController::setWorkspaceRoot(const std::string& workspaceRoot)
{
    workspace_context_.Initialize(workspaceRoot);
    session_state_.BindWorkspaceSnapshot(workspace_context_.RefreshSnapshot());
}

CompletionResult MinimalAgentController::OnInlineCompletionRequest(const std::string& prefix, const EditorContext& ctx)
{
    IdeRequest req;
    req.kind = IdeRequestKind::InlineCompletion;
    req.primaryText = prefix;
    req.filePath = ctx.filePath;
    req.editorContext = ctx;
    req.sessionId = "ide-inline";
    const AgentResponse response = HandleIDERequest(req);
    return response.completion;
}

std::string MinimalAgentController::DescribeSessionPlanGraph(const std::string& sessionId, size_t maxNodes) const
{
    const PlanGraph graph = session_state_.planGraph(sessionId);
    std::ostringstream stream;
    stream << "Session PlanGraph: " << (graph.sessionId.empty() ? sessionId : graph.sessionId) << "\r\n";
    if (!graph.rootGoal.empty())
    {
        stream << "Root Goal: " << graph.rootGoal << "\r\n";
    }
    stream << "Nodes: " << graph.nodes.size() << "  Edges: " << graph.edges.size() << "\r\n";

    if (graph.nodes.empty())
    {
        stream << "No plan lineage recorded.\r\n";
        return stream.str();
    }

    const size_t startIndex = graph.nodes.size() > maxNodes ? graph.nodes.size() - maxNodes : 0;
    for (size_t index = startIndex; index < graph.nodes.size(); ++index)
    {
        const PlanGraphNode& node = graph.nodes[index];
        stream << "\r\n[" << node.sequence << "] " << node.sourceKind << " plan=" << node.planId << " v" << node.version
               << "\r\n";
        stream << "  completed=" << (node.completed ? "yes" : "no") << " success=" << (node.success ? "yes" : "no")
               << " executable=" << (node.executable ? "yes" : "no") << "\r\n";
        if (!node.summary.empty())
        {
            stream << "  summary: " << node.summary << "\r\n";
        }
        if (!node.outputPreview.empty())
        {
            stream << "  output: " << node.outputPreview << "\r\n";
        }
        if (node.verification.attempted)
        {
            stream << "  verification: " << node.verification.verifierKind << " -> "
                   << (node.verification.passed ? "pass" : "fail") << "\r\n";
            if (!node.verification.summary.empty())
            {
                stream << "  verifier summary: " << node.verification.summary << "\r\n";
            }
        }
    }

    return stream.str();
}

void MinimalAgentController::RecordInlineCompletionFeedback(const std::string& sessionId, const std::string& planId,
                                                            bool accepted, const std::string& detail)
{
    if (planId.empty())
    {
        return;
    }

    const std::string effectiveSessionId = sessionId.empty() ? "ide-inline" : sessionId;
    session_state_.AppendVerificationOutcome(
        effectiveSessionId, planId, "ui_feedback", accepted,
        detail.empty() ? (accepted ? "Ghost text accepted by user." : "Ghost text dismissed by user.") : detail);
    session_state_.RecordInteraction(
        {accepted ? "ghost_text_accepted" : "ghost_text_dismissed", detail, planId, "", effectiveSessionId, ""});
}

RewriteResult MinimalAgentController::OnSelectionRewrite(const std::string& selectedText,
                                                         const std::string& instruction, const EditorContext& ctx)
{
    IdeRequest req;
    req.kind = IdeRequestKind::SelectionRewrite;
    req.primaryText = selectedText;
    req.secondaryText = instruction;
    req.filePath = ctx.filePath;
    req.editorContext = ctx;
    req.sessionId = "ide-rewrite";
    const AgentResponse response = HandleIDERequest(req);
    return response.rewrite;
}

DiffPlan MinimalAgentController::OnCodeEditRequest(const std::string& filePath, const std::string& proposedChange,
                                                   const EditorContext& ctx)
{
    IdeRequest req;
    req.kind = IdeRequestKind::CodeEdit;
    req.primaryText = proposedChange;
    req.filePath = filePath;
    req.editorContext = ctx;
    req.sessionId = "ide-edit";
    const AgentResponse response = HandleIDERequest(req);
    return response.diff;
}

AgentResponse MinimalAgentController::EvaluateIDEAction(const IdeRequest& req)
{
    AgentResponse response;
    if (!initialized_)
    {
        response.success = false;
        response.message = "Controller not initialized";
        return response;
    }

    const std::string workspaceRoot =
        !req.editorContext.workspaceRoot.empty() ? req.editorContext.workspaceRoot : workspace_context_.workspaceRoot();
    if (!workspaceRoot.empty())
    {
        workspace_context_.Initialize(workspaceRoot);
    }

    const WorkspaceSnapshot snapshot = workspace_context_.RefreshSnapshot();
    session_state_.BindWorkspaceSnapshot(snapshot);
    const std::string sessionId = EffectiveIdeSessionId(req);
    SessionContext session = loadSessionContext(sessionId);
    const ExecutionPlanIR planIr = EmitPlanIR(req);
    response.planIrId = planIr.planId;
    response.planIrPreview = DescribePlanIR(planIr);
    response.hasExecutablePlan = planIr.Validate();
    const auto finalizePlanGraph = [&](bool success, const std::string& outputPreview, TrustScore trust)
    {
        session_state_.UpdatePlanOutcome(sessionId, planIr.planId, success, outputPreview);
        const VerificationDecision verification = VerifyIdeOutcome(req.kind, success, trust, outputPreview);
        if (verification.attempted)
        {
            session_state_.AppendVerificationOutcome(sessionId, planIr.planId, verification.verifierKind,
                                                     verification.passed, verification.summary);
        }
    };
    const std::string continuityContext = session_state_.BuildPlanContinuityContext(sessionId, 2);

    if (req.kind != IdeRequestKind::AgenticTask)
    {
        session_state_.AppendPlanIR(sessionId, planIr,
                                    req.kind == IdeRequestKind::InlineCompletion   ? "inline_completion"
                                    : req.kind == IdeRequestKind::SelectionRewrite ? "selection_rewrite"
                                                                                   : "code_edit",
                                    response.planIrPreview,
                                    req.primaryText.empty() ? req.secondaryText : req.primaryText);
    }

    switch (req.kind)
    {
        case IdeRequestKind::InlineCompletion:
        {
            session_state_.RecordInteraction(
                {"inline_completion", req.primaryText, req.payload, req.filePath, sessionId, ""});
            PushSessionMessage(session, "user", req.primaryText);
            const QueryResult query = workspace_context_.Query(req.primaryText + " " + req.filePath);

            ExecutionPlan plan;
            plan.intent = TaskIntent::Reasoning;
            plan.lane = ExecutionLane::ModelAssisted;
            plan.trust = TrustScore::ModelSuspect;
            plan.rationale = "Inline completion stays in the model-assisted lane with empty fallback.";

            const std::string prompt =
                MergePromptContinuity("Return only completion text. Do not explain.\n"
                                      "file=" +
                                          req.filePath +
                                          "\n"
                                          "language=" +
                                          req.editorContext.language +
                                          "\n"
                                          "cursor=" +
                                          std::to_string(req.editorContext.cursorLine) + ":" +
                                          std::to_string(req.editorContext.cursorColumn) + "\n" +
                                          SummarizeQueryResult(query) + "prefix:\n" + req.primaryText,
                                      continuityContext);

            const std::string raw = callLLM(prompt, "", req.editorContext.modelPath);
            if (raw.rfind("Error:", 0) == 0)
            {
                response.completion = {"", true, TrustScore::ModelInvalid,
                                       "Model unavailable, returning empty completion."};
                response.completion.planId = response.planIrId;
                response.completion.sessionId = sessionId;
                response.success = response.completion.success;
                response.message = response.completion.suggestion;
                PushSessionMessage(session, "assistant", raw);
                finalizePlanGraph(true, raw, TrustScore::ModelInvalid);
                session_state_.AppendExecution(
                    {plan, {true, "", raw}, TrustScore::ModelInvalid, 1, sessionId, "", planIr.planId});
                storeSessionContext(sessionId, session);
                break;
            }

            const TrustScore trust = evaluateTrust(raw, prompt);
            if (trust == TrustScore::ModelInvalid || ParrotDetector::IsParroting(raw, prompt) ||
                raw.find("<tool_call>") != std::string::npos || raw.find("TOOL_CALL:") != std::string::npos)
            {
                response.completion = {"", true, trust,
                                       "Trust gate rejected inline suggestion; empty fallback returned."};
                response.completion.planId = response.planIrId;
                response.completion.sessionId = sessionId;
                response.success = response.completion.success;
                response.message = response.completion.suggestion;
                finalizePlanGraph(true, response.completion.rationale, trust);
                session_state_.AppendExecution({plan, {true, "", ""}, trust, 1, sessionId, "", planIr.planId});
                storeSessionContext(sessionId, session);
                break;
            }

            const std::string suggestion = NormalizeAssistantText(raw);
            response.completion = {suggestion, true, trust, "Inline suggestion generated from model-assisted lane."};
            response.completion.planId = response.planIrId;
            response.completion.sessionId = sessionId;
            response.success = response.completion.success;
            response.message = response.completion.suggestion;
            PushSessionMessage(session, "assistant", suggestion);
            finalizePlanGraph(true, suggestion, trust);
            session_state_.AppendExecution({plan, {true, suggestion, ""}, trust, 1, sessionId, "", planIr.planId});
            storeSessionContext(sessionId, session);
            break;
        }
        case IdeRequestKind::SelectionRewrite:
        {
            session_state_.RecordInteraction(
                {"selection_rewrite", req.secondaryText, req.primaryText, req.filePath, sessionId, ""});
            PushSessionMessage(session, "user", req.secondaryText + "\n" + req.primaryText);

            ExecutionPlan plan;
            plan.intent = TaskIntent::Reasoning;
            plan.lane = ExecutionLane::SyntheticFirst;
            plan.trust = TrustScore::DeterministicOnly;
            plan.rationale = "Selection rewrite may only return rewritten text or no-op.";

            if (const auto deterministic = ApplyDeterministicRewrite(req.primaryText, req.secondaryText);
                deterministic.has_value())
            {
                response.rewrite = {
                    *deterministic, true, false, TrustScore::DeterministicOnly, "Deterministic rewrite applied.", ""};
                response.success = response.rewrite.success;
                response.message = response.rewrite.rewrittenText;
                PushSessionMessage(session, "assistant", *deterministic);
                finalizePlanGraph(true, *deterministic, TrustScore::DeterministicOnly);
                session_state_.AppendExecution(
                    {plan, {true, *deterministic, ""}, TrustScore::DeterministicOnly, 1, sessionId, "", planIr.planId});
                storeSessionContext(sessionId, session);
                break;
            }

            const QueryResult query = workspace_context_.Query(req.secondaryText + " " + req.filePath);
            const std::string prompt = MergePromptContinuity(
                "Rewrite the selected text according to the instruction. Return only rewritten text.\n"
                "file=" +
                    req.filePath +
                    "\n"
                    "instruction=" +
                    req.secondaryText + "\n" + SummarizeQueryResult(query) + "selected:\n" + req.primaryText,
                continuityContext);

            const std::string raw = callLLM(prompt, "", req.editorContext.modelPath);
            if (raw.rfind("Error:", 0) == 0)
            {
                response.rewrite = {req.primaryText,         false, true, TrustScore::ModelInvalid,
                                    "Model rewrite failed.", raw};
                response.success = response.rewrite.success;
                response.message = response.rewrite.error;
                PushSessionMessage(session, "assistant", raw);
                finalizePlanGraph(false, raw, TrustScore::ModelInvalid);
                session_state_.AppendExecution(
                    {plan, {false, "", raw}, TrustScore::ModelInvalid, 1, sessionId, "", planIr.planId});
                storeSessionContext(sessionId, session);
                break;
            }

            const TrustScore trust = evaluateTrust(raw, prompt);
            if (trust == TrustScore::ModelInvalid)
            {
                const std::string error = "Rewrite response failed trust gate.";
                response.rewrite = {req.primaryText, false, true, trust, error, error};
                response.success = response.rewrite.success;
                response.message = response.rewrite.error;
                finalizePlanGraph(false, error, trust);
                session_state_.AppendExecution({plan, {false, "", error}, trust, 1, sessionId, "", planIr.planId});
                storeSessionContext(sessionId, session);
                break;
            }

            const std::string rewritten = NormalizeAssistantText(raw);
            response.rewrite = {rewritten, true, true, trust, "Rewrite generated.", ""};
            response.success = response.rewrite.success;
            response.message = response.rewrite.rewrittenText;
            PushSessionMessage(session, "assistant", rewritten);
            finalizePlanGraph(true, rewritten, trust);
            session_state_.AppendExecution({plan, {true, rewritten, ""}, trust, 1, sessionId, "", planIr.planId});
            storeSessionContext(sessionId, session);
            break;
        }
        case IdeRequestKind::CodeEdit:
        {
            session_state_.RecordInteraction({"code_edit", req.primaryText, req.payload, req.filePath, sessionId, ""});
            PushSessionMessage(session, "user", req.filePath + "\n" + req.primaryText);
            const QueryResult query = workspace_context_.Query(req.filePath + " " + req.primaryText);

            ExecutionPlan plan = buildExecutionPlan(req.filePath + " " + req.primaryText);
            plan.lane = ExecutionLane::SyntheticFirst;
            plan.trust = TrustScore::DeterministicOnly;
            if (plan.steps.empty() && !req.filePath.empty())
            {
                plan.steps.push_back(
                    {"file_read", std::string("{\"path\":\"") + EscapeJsonString(req.filePath) + "\"}", true});
            }
            plan.rationale = "Code edit requests may only return a reviewable diff-plan.";

            std::ostringstream preview;
            preview << "FILE: " << req.filePath << "\n"
                    << "CHANGE: " << req.primaryText << "\n"
                    << SummarizeQueryResult(query) << "ACTION: review before execution";

            const ReviewablePlan reviewable = session_state_.BuildReviewablePlan(plan, preview.str(), true);
            response.diff = {reviewable.plan, req.primaryText, reviewable.preview, reviewable.requiresApproval};
            response.success = true;
            response.message = response.diff.preview;
            PushSessionMessage(session, "assistant", reviewable.preview);
            finalizePlanGraph(true, reviewable.preview, TrustScore::DeterministicOnly);
            session_state_.AppendExecution(
                {plan, {true, reviewable.preview, ""}, TrustScore::DeterministicOnly, 1, sessionId, "", planIr.planId});
            storeSessionContext(sessionId, session);
            break;
        }
        case IdeRequestKind::AgenticTask:
            response.agentic = process(req.agentRequest);
            response.success = response.agentic.success;
            response.message = response.agentic.success ? response.agentic.final_message : response.agentic.error;
            break;
    }
    return response;
}

AgentResponse MinimalAgentController::HandleIDERequest(const IdeRequest& req)
{
    return EvaluateIDEAction(req);
}

ExecutionPlanIR MinimalAgentController::EmitPlanIR(const IdeRequest& req) const
{
    ExecutionPlanIR emitted;
    const std::string sessionId = EffectiveIdeSessionId(req);

    switch (req.kind)
    {
        case IdeRequestKind::InlineCompletion:
        {
            emitted.planId =
                "ide_inline_" + std::to_string(std::hash<std::string>{}(sessionId + req.filePath + req.primaryText));
            emitted.steps.push_back(
                StepLLMSynthesize{"Return only completion text. Do not explain.\\nfile=" + req.filePath +
                                      "\\nlanguage=" + req.editorContext.language + "\\nprefix:\\n" + req.primaryText,
                                  "result", true, 128});
            break;
        }
        case IdeRequestKind::SelectionRewrite:
        {
            emitted.planId =
                "ide_rewrite_" + std::to_string(std::hash<std::string>{}(sessionId + req.filePath + req.secondaryText));
            emitted.steps.push_back(StepLLMSynthesize{
                "Rewrite the selected text according to the instruction. Return only rewritten text.\\nfile=" +
                    req.filePath + "\\ninstruction=" + req.secondaryText + "\\nselected:\\n" + req.primaryText,
                "result", true, 256});
            break;
        }
        case IdeRequestKind::CodeEdit:
        {
            emitted.planId =
                "ide_edit_" + std::to_string(std::hash<std::string>{}(sessionId + req.filePath + req.primaryText));
            if (!req.filePath.empty())
            {
                emitted.steps.push_back(StepFileRead{req.filePath, "file_content"});
            }
            emitted.steps.push_back(StepLLMSynthesize{"Produce a reviewable diff plan only.\\nfile=" + req.filePath +
                                                          "\\nrequested_change=" + req.primaryText +
                                                          "\\ncurrent_content={{file_content}}",
                                                      "result", true, 384});
            break;
        }
        case IdeRequestKind::AgenticTask:
        {
            emitted = ConvertExecutionPlanToIR(buildExecutionPlan(req.agentRequest.message), req.agentRequest.message);
            break;
        }
    }

    return emitted;
}

ExecutionPlanIR MinimalAgentController::ConvertExecutionPlanToIR(const ExecutionPlan& plan,
                                                                 const std::string& userMessage) const
{
    ExecutionPlanIR emitted;
    emitted.planId = "agentic_" + std::to_string(std::hash<std::string>{}(userMessage));

    for (const auto& step : plan.steps)
    {
        if (step.toolName == "file_read")
        {
            const std::string path = ExtractJsonStringField(step.argumentsJson, "path");
            emitted.steps.push_back(
                StepFileRead{path.empty() ? "default.txt" : path, AliasFromToolName(step.toolName)});
        }
        else if (step.toolName == "terminal_execute")
        {
            const std::string command = ExtractJsonStringField(step.argumentsJson, "command");
            emitted.steps.push_back(StepTerminalExecute{command.empty() ? "cmd /c dir" : command,
                                                        AliasFromToolName(step.toolName), 5000, true});
        }
    }

    if (emitted.steps.empty())
    {
        emitted.steps.push_back(StepLLMSynthesize{"Answer this user question: " + userMessage, "result", true, 512});
        return emitted;
    }

    if (plan.intent == TaskIntent::FileIO)
    {
        emitted.steps.push_back(
            StepLLMSynthesize{"Summarize the file read result: {{file_content}}", "result", true, 256});
    }
    else if (plan.intent == TaskIntent::Terminal)
    {
        emitted.steps.push_back(
            StepLLMSynthesize{"Summarize the command output: {{terminal_result}}", "result", true, 256});
    }
    else if (plan.intent == TaskIntent::Hybrid || plan.intent == TaskIntent::Unknown)
    {
        emitted.steps.push_back(
            StepLLMSynthesize{"Use available step outputs to answer the request: " + userMessage, "result", true, 384});
    }

    return emitted;
}

std::string MinimalAgentController::DescribePlanIR(const ExecutionPlanIR& plan) const
{
    std::ostringstream summary;
    summary << "plan_id=" << plan.planId << "\n";
    int index = 0;
    for (const auto& step : plan.steps)
    {
        summary << index++ << ": ";
        std::visit(
            [&](const auto& concrete)
            {
                using T = std::decay_t<decltype(concrete)>;
                if constexpr (std::is_same_v<T, StepFileRead>)
                {
                    summary << "file_read path=" << concrete.path << " -> " << concrete.alias;
                }
                else if constexpr (std::is_same_v<T, StepTerminalExecute>)
                {
                    summary << "terminal_execute command=" << concrete.command << " -> " << concrete.alias;
                }
                else if constexpr (std::is_same_v<T, StepLLMSynthesize>)
                {
                    summary << "llm_synthesize alias=" << concrete.alias << " tokens=" << concrete.maxTokens;
                }
                else if constexpr (std::is_same_v<T, StepCondition>)
                {
                    summary << "condition " << concrete.lhs << " " << concrete.op << " " << concrete.rhs;
                }
            },
            step);
        summary << "\n";
    }
    return summary.str();
}

}  // namespace rawrxd
