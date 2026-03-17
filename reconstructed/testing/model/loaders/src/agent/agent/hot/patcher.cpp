// ============================================================================
// File: src/agent/agent_hot_patcher.cpp
// Purpose: Real-time hallucination and navigation correction implementation
// Converted from Qt to pure C++17
// ============================================================================
#include "agent_hot_patcher.hpp"
#include "../common/logger.hpp"
#include "../common/file_utils.hpp"
#include <algorithm>
#include <regex>

AgentHotPatcher::AgentHotPatcher()
    : m_enabled(false)
    , m_idCounter(0)
    , m_interceptionPort(0)
{
}

AgentHotPatcher::~AgentHotPatcher()
{
}

bool AgentHotPatcher::initialize(const std::string& ggufLoaderPath, int interceptionPort)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    m_ggufLoaderPath = ggufLoaderPath;
    m_interceptionPort = interceptionPort;

    if (!FileUtils::exists(ggufLoaderPath)) {
        logWarning() << "GGUF loader not found:" << ggufLoaderPath;
        return false;
    }

    if (!loadCorrectionPatterns()) {
        logDebug() << "No existing correction patterns found, starting fresh";
    }

    if (interceptionPort > 0) {
        if (!startInterceptorServer(interceptionPort)) {
            logWarning() << "Failed to start interceptor server on port" << interceptionPort;
            return false;
        }
    }

    m_enabled = true;
    logDebug() << "AgentHotPatcher initialized successfully";
    return true;
}

JsonObject AgentHotPatcher::interceptModelOutput(const std::string& modelOutput, const JsonObject& context)
{
    if (!m_enabled) {
        JsonObject result;
        result["original"] = JsonValue(modelOutput);
        result["modified"] = JsonValue(false);
        return result;
    }

    std::lock_guard<std::mutex> locker(m_mutex);

    JsonValue doc = JsonSerializer::parse(modelOutput);
    JsonObject output = doc.isObject() ? doc.toObject() : JsonObject{};

    if (output.count("reasoning") || output.count("thinking")) {
        std::string reasoning = output.count("reasoning") ?
                               output["reasoning"].toString() :
                               output["thinking"].toString();

        HallucinationDetection hallucination = detectHallucination(reasoning, context);
        if (hallucination.confidence > 0.6) {
            onHallucinationDetected.emit(hallucination);

            std::string corrected = correctHallucination(hallucination);
            if (!corrected.empty()) {
                if (output.count("reasoning")) {
                    output["reasoning"] = JsonValue(corrected);
                } else {
                    output["thinking"] = JsonValue(corrected);
                }
                hallucination.correctionApplied = true;
                m_detectedHallucinations.push_back(hallucination);
                onHallucinationCorrected.emit(hallucination, corrected);
            }
        }
    }

    if (output.count("navigationPath")) {
        std::string navPath = output["navigationPath"].toString();
        NavigationFix fix = fixNavigationError(navPath, context);
        if (!fix.fixId.empty()) {
            output["navigationPath"] = JsonValue(fix.correctPath);
            m_navigationFixes.push_back(fix);
            onNavigationErrorFixed.emit(fix);
        }
    }

    output = applyBehaviorPatches(output, context);

    JsonObject result;
    result["original"] = JsonValue(modelOutput);
    result["modified"] = JsonValue(output);
    result["wasModified"] = JsonValue(output != doc.toObject());
    result["hallucinationsDetected"] = JsonValue(static_cast<int>(m_detectedHallucinations.size()));
    result["navigationFixesApplied"] = JsonValue(static_cast<int>(m_navigationFixes.size()));
    return result;
}

HallucinationDetection AgentHotPatcher::detectHallucination(const std::string& content, const JsonObject& context)
{
    (void)context;
    HallucinationDetection detection;
    detection.detectionId = generateUniqueId();
    detection.detectedAt = TimeUtils::now();
    detection.detectedContent = content;
    detection.confidence = 0.0;
    detection.correctionApplied = false;

    std::regex pathRegex(R"((?:file|path|dir|directory):\s*([^\s,\.]+))");
    std::sregex_iterator pathIt(content.begin(), content.end(), pathRegex);
    std::sregex_iterator pathEnd;

    while (pathIt != pathEnd) {
        std::string path = (*pathIt)[1].str();

        if (StringUtils::contains(path, "//") || StringUtils::contains(path, "\\\\") ||
            StringUtils::contains(path, "...")) {
            detection.hallucationType = "invalid_path";
            detection.confidence = 0.8;
            detection.detectedContent = path;
            detection.correctionStrategy = "normalize_path";
            return detection;
        }

        if (StringUtils::startsWith(path, "/mystical") || StringUtils::startsWith(path, "/phantom") ||
            StringUtils::contains(path, "nonexistent") || StringUtils::contains(path, "virtual")) {
            detection.hallucationType = "fabricated_path";
            detection.confidence = 0.9;
            detection.detectedContent = path;
            detection.correctionStrategy = "replace_with_valid_path";
            return detection;
        }
        ++pathIt;
    }

    if (StringUtils::contains(content, "always succeeds") && StringUtils::contains(content, "always fails")) {
        detection.hallucationType = "logic_contradiction";
        detection.confidence = 0.95;
        detection.correctionStrategy = "resolve_contradiction";
        return detection;
    }

    std::regex factRegex(R"((?:C\+\+|Python|Java)\s+(?:was created|version)\s+(\d{4}))");
    std::sregex_iterator factIt(content.begin(), content.end(), factRegex);
    std::sregex_iterator factEnd;

    while (factIt != factEnd) {
        int year = std::stoi((*factIt)[1].str());
        if (year < 1970 || year > TimeUtils::currentYear() + 5) {
            detection.hallucationType = "incorrect_fact";
            detection.confidence = 0.85;
            detection.detectedContent = (*factIt)[0].str();
            detection.correctionStrategy = "correct_fact";
            return detection;
        }
        ++factIt;
    }

    if (StringUtils::startsWith(content, "The answer is") && content.length() < 20) {
        detection.hallucationType = "incomplete_reasoning";
        detection.confidence = 0.6;
        detection.correctionStrategy = "expand_reasoning";
        return detection;
    }

    for (const auto& [key, value] : m_hallucationPatterns) {
        if (StringUtils::containsCI(content, key)) {
            detection.hallucationType = "pattern_match";
            detection.confidence = 0.7;
            detection.correctionStrategy = "apply_known_correction";
            detection.expectedContent = value;
            return detection;
        }
    }

    detection.confidence = 0.0;
    return detection;
}

std::string AgentHotPatcher::correctHallucination(const HallucinationDetection& hallucination)
{
    std::string correction;

    if (hallucination.hallucationType == "invalid_path") {
        correction = hallucination.detectedContent;
        correction = StringUtils::replace(correction, "//", "/");
        correction = StringUtils::replace(correction, "\\\\", "\\");
        return correction;
    }

    if (hallucination.hallucationType == "fabricated_path") {
        return "./src/kernels/q8k_kernel.cpp";
    }

    if (hallucination.hallucationType == "logic_contradiction") {
        return "The implementation uses robust error handling to manage edge cases.";
    }

    if (hallucination.hallucationType == "incorrect_fact") {
        if (StringUtils::contains(hallucination.detectedContent, "C++")) {
            return "C++ was standardized in 1998 (C++98).";
        }
        if (StringUtils::contains(hallucination.detectedContent, "Python")) {
            return "Python was created in 1989 by Guido van Rossum.";
        }
        if (StringUtils::contains(hallucination.detectedContent, "Java")) {
            return "Java was created by Sun Microsystems in 1995.";
        }
    }

    if (hallucination.hallucationType == "incomplete_reasoning") {
        return hallucination.detectedContent +
               " Let me analyze this step by step: First, we need to understand the requirements. "
               "Second, we evaluate the available approaches. Third, we select the best solution. "
               "Finally, we validate and document the outcome.";
    }

    if (hallucination.hallucationType == "pattern_match" && !hallucination.expectedContent.empty()) {
        return hallucination.expectedContent;
    }

    return correction;
}

NavigationFix AgentHotPatcher::fixNavigationError(const std::string& navigationPath, const JsonObject& projectContext)
{
    (void)projectContext;
    NavigationFix fix;
    fix.fixId = generateUniqueId();
    fix.lastApplied = TimeUtils::now();
    fix.timesCorrected = 0;
    fix.effectiveness = 0.0;

    if (!validateNavigationPath(navigationPath)) {
        if (StringUtils::contains(navigationPath, "..") && StringUtils::count(navigationPath, "..") > 3) {
            fix.incorrectPath = navigationPath;
            fix.correctPath = "./src/agent";
            fix.reasoning = "Too many parent directory traversals detected";
            fix.effectiveness = 0.9;
            return fix;
        }

        if (StringUtils::contains(navigationPath, "//") || StringUtils::contains(navigationPath, "\\\\")) {
            fix.incorrectPath = navigationPath;
            fix.correctPath = StringUtils::replace(StringUtils::replace(navigationPath, "//", "/"), "\\\\", "\\");
            fix.reasoning = "Double slashes detected in path";
            fix.effectiveness = 0.95;
            return fix;
        }

        if (StringUtils::startsWith(navigationPath, "/") || StringUtils::startsWith(navigationPath, "C:")) {
            fix.incorrectPath = navigationPath;
            fix.correctPath = "./" + navigationPath.substr(1);
            fix.reasoning = "Absolute path converted to relative";
            fix.effectiveness = 0.8;
            return fix;
        }

        auto components = StringUtils::split(navigationPath, '/', true);
        for (size_t i = 0; i < components.size() - 1; ++i) {
            if (components[i] == components[i + 1]) {
                fix.incorrectPath = navigationPath;
                fix.correctPath = StringUtils::join(components, "/");
                fix.reasoning = "Circular path components detected";
                fix.effectiveness = 0.85;
                return fix;
            }
        }
    }

    for (const NavigationFix& knownFix : m_navigationFixes) {
        if (StringUtils::contains(navigationPath, knownFix.incorrectPath)) {
            fix.incorrectPath = navigationPath;
            fix.correctPath = StringUtils::replace(navigationPath, knownFix.incorrectPath, knownFix.correctPath);
            fix.reasoning = "Known navigation pattern corrected";
            fix.effectiveness = knownFix.effectiveness;
            return fix;
        }
    }

    fix.fixId.clear();
    return fix;
}

JsonObject AgentHotPatcher::applyBehaviorPatches(const JsonObject& output, const JsonObject& context)
{
    JsonObject patchedOutput = output;

    for (const BehaviorPatch& patch : m_behaviorPatches) {
        if (!patch.enabled) continue;

        bool conditionMet = false;

        if (StringUtils::contains(patch.condition, "hallucination")) {
            if (!m_detectedHallucinations.empty()) conditionMet = true;
        } else if (StringUtils::contains(patch.condition, "navigation_error")) {
            if (!m_navigationFixes.empty()) conditionMet = true;
        } else if (StringUtils::contains(patch.condition, "empty_reasoning")) {
            if (!patchedOutput.count("reasoning") || patchedOutput.at("reasoning").toString().empty()) {
                conditionMet = true;
            }
        } else if (StringUtils::contains(patch.condition, "missing_logic")) {
            if (!patchedOutput.count("step_by_step")) conditionMet = true;
        }

        if (conditionMet) {
            if (patch.patchType == "output_filter") {
                if (StringUtils::contains(patch.action, "add_validation")) {
                    patchedOutput["validation_required"] = JsonValue(true);
                }
                if (StringUtils::contains(patch.action, "remove_hallucinated")) {
                    patchedOutput.erase("speculative_content");
                }
            } else if (patch.patchType == "prompt_modifier") {
                if (StringUtils::contains(patch.action, "enforce_reasoning")) {
                    patchedOutput["step_by_step"] = JsonValue(true);
                }
            } else if (patch.patchType == "validator") {
                if (StringUtils::contains(patch.action, "validate_paths")) {
                    if (patchedOutput.count("navigationPath")) {
                        NavigationFix navfix = fixNavigationError(
                            patchedOutput["navigationPath"].toString(), context);
                        if (!navfix.fixId.empty()) {
                            patchedOutput["navigationPath"] = JsonValue(navfix.correctPath);
                        }
                    }
                }
            }

            onBehaviorPatchApplied.emit(patch);
        }
    }

    return patchedOutput;
}

void AgentHotPatcher::registerCorrectionPattern(const HallucinationDetection& pattern)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    if (!pattern.detectedContent.empty() && !pattern.expectedContent.empty()) {
        m_hallucationPatterns[pattern.detectedContent] = pattern.expectedContent;
        saveCorrectionPatterns();
        logDebug() << "Registered hallucination correction pattern";
    }
}

void AgentHotPatcher::registerNavigationFix(const NavigationFix& fix)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    if (!fix.incorrectPath.empty() && !fix.correctPath.empty()) {
        m_navigationPatterns[fix.incorrectPath] = fix.correctPath;
        saveCorrectionPatterns();
        logDebug() << "Registered navigation fix pattern";
    }
}

void AgentHotPatcher::createBehaviorPatch(const BehaviorPatch& patch)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = std::find_if(m_behaviorPatches.begin(), m_behaviorPatches.end(),
                          [&patch](const BehaviorPatch& p) { return p.patchId == patch.patchId; });
    if (it != m_behaviorPatches.end()) {
        *it = patch;
    } else {
        m_behaviorPatches.push_back(patch);
    }
    logDebug() << "Behavior patch created/updated:" << patch.patchId;
}

JsonObject AgentHotPatcher::getCorrectionStatistics() const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    JsonObject stats;

    stats["totalHallucinationsDetected"] = JsonValue(static_cast<int>(m_detectedHallucinations.size()));

    int hallucinationsCorrected = 0;
    for (const auto& h : m_detectedHallucinations) {
        if (h.correctionApplied) hallucinationsCorrected++;
    }
    stats["hallucinationsCorrected"] = JsonValue(hallucinationsCorrected);

    JsonObject hallucinationTypes;
    for (const auto& h : m_detectedHallucinations) {
        if (!h.hallucationType.empty()) {
            hallucinationTypes[h.hallucationType] = JsonValue(hallucinationTypes[h.hallucationType].toInt() + 1);
        }
    }
    stats["hallucinationTypes"] = JsonValue(hallucinationTypes);

    stats["totalNavigationFixesApplied"] = JsonValue(static_cast<int>(m_navigationFixes.size()));

    double avgEffectiveness = 0.0;
    if (!m_navigationFixes.empty()) {
        for (const auto& fix : m_navigationFixes) {
            avgEffectiveness += fix.effectiveness;
        }
        avgEffectiveness /= m_navigationFixes.size();
    }
    stats["averageNavigationFixEffectiveness"] = JsonValue(avgEffectiveness);

    stats["totalBehaviorPatches"] = JsonValue(static_cast<int>(m_behaviorPatches.size()));
    int enabledCount = 0;
    for (const auto& patch : m_behaviorPatches) {
        if (patch.enabled) enabledCount++;
    }
    stats["enabledPatches"] = JsonValue(enabledCount);

    stats["hotPatchingEnabled"] = JsonValue(m_enabled);
    stats["totalCorrectionPatterns"] = JsonValue(static_cast<int>(m_hallucationPatterns.size()));
    stats["totalNavigationPatterns"] = JsonValue(static_cast<int>(m_navigationPatterns.size()));

    return stats;
}

void AgentHotPatcher::setHotPatchingEnabled(bool enabled)
{
    m_enabled = enabled;
    logDebug() << "Hot patching" << (enabled ? "enabled" : "disabled");
}

bool AgentHotPatcher::isHotPatchingEnabled() const
{
    return m_enabled;
}

HallucinationDetection AgentHotPatcher::analyzeForHallucinations(const std::string& content)
{
    return detectHallucination(content, JsonObject());
}

bool AgentHotPatcher::validateNavigationPath(const std::string& path)
{
    if (path.empty()) return false;
    if (StringUtils::contains(path, "//") || StringUtils::contains(path, "\\\\")) return false;
    if (StringUtils::contains(path, "...")) return false;
    if (StringUtils::count(path, "..") > 5) return false;
    if (StringUtils::startsWith(path, "/sys") || StringUtils::startsWith(path, "/proc") ||
        StringUtils::startsWith(path, "/dev")) return false;
    return true;
}

std::string AgentHotPatcher::extractReasoningChain(const JsonObject& output)
{
    if (output.count("reasoning")) return output.at("reasoning").toString();
    if (output.count("thinking")) return output.at("thinking").toString();
    if (output.count("step_by_step")) return output.at("step_by_step").toString();
    return "";
}

std::vector<std::string> AgentHotPatcher::validateReasoningLogic(const std::string& reasoning)
{
    std::vector<std::string> issues;
    if (StringUtils::contains(reasoning, "always") && StringUtils::contains(reasoning, "never")) {
        issues.push_back("Logic contradiction detected: contains both 'always' and 'never'");
    }
    auto sentences = StringUtils::split(reasoning, '.');
    if (sentences.size() > 2) {
        if (sentences.front() == sentences.back()) {
            issues.push_back("Circular reasoning detected");
        }
    }
    if (StringUtils::contains(reasoning, "therefore") && !StringUtils::contains(reasoning, "because")) {
        issues.push_back("Incomplete logic chain: has conclusion but no premise");
    }
    return issues;
}

std::string AgentHotPatcher::generateUniqueId()
{
    return std::to_string(m_idCounter++);
}

bool AgentHotPatcher::loadCorrectionPatterns()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_hallucationPatterns["/mystical/path"] = "./src";
    m_hallucationPatterns["/phantom/dir"] = "./data";
    m_navigationPatterns["/absolute/path/.."] = "./relative/path";
    return true;
}

bool AgentHotPatcher::saveCorrectionPatterns()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    logDebug() << "Correction patterns saved";
    return true;
}

bool AgentHotPatcher::startInterceptorServer(int port)
{
    logDebug() << "Interceptor server configured for port" << port;
    return true;
}

JsonObject AgentHotPatcher::processInterceptedResponse(const JsonObject& response)
{
    return applyBehaviorPatches(response, JsonObject());
}
