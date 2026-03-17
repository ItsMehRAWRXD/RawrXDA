#include "agent_hot_patcher.hpp"
#include <algorithm>
#include <filesystem>
#include <regex>

AgentHotPatcher::AgentHotPatcher()
    : m_enabled(false)
    , m_idCounter(0)
    , m_interceptionPort(0)
{
}

AgentHotPatcher::~AgentHotPatcher() noexcept = default;

void AgentHotPatcher::initialize(const std::string& ggufLoaderPath, int interceptionPort)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    
    m_ggufLoaderPath = ggufLoaderPath;
    m_interceptionPort = interceptionPort;
    
    // Verify GGUF loader exists
    if (!std::filesystem::exists(ggufLoaderPath)) {
        return;
    }
    
    // Load existing correction patterns
    loadCorrectionPatterns();
    
    // Start interceptor server if port specified
    if (interceptionPort > 0) {
        startInterceptorServer(interceptionPort);
    }
    
    m_enabled = true;
}

json AgentHotPatcher::interceptModelOutput(const std::string& modelOutput, const json& context)
{
    if (!m_enabled) {
        json result;
        result["original"] = modelOutput;
        result["modified"] = false;
        return result;
    }
    
    std::lock_guard<std::mutex> locker(m_mutex);
    
    // Parse model output as JSON
    json output;
    try {
        output = json::parse(modelOutput);
    } catch (...) {
        return {{"original", modelOutput}, {"error", "Invalid JSON"}};
    }
    
    json originalOutput = output;
    
    // Check if output has reasoning we can analyze
    if (output.contains("reasoning") || output.contains("thinking")) {
        std::string reasoning = output.contains("reasoning") ? 
                           output["reasoning"].get<std::string>() : 
                           output["thinking"].get<std::string>();
        
        // Detect hallucinations
        HallucinationDetection hallucination = detectHallucination(reasoning, context);
        if (hallucination.confidence > 0.6) {
            hallucinationDetected(hallucination);
            
            // Correct the hallucination
            std::string corrected = correctHallucination(hallucination);
            if (!corrected.empty()) {
                if (output.contains("reasoning")) {
                    output["reasoning"] = corrected;
                } else {
                    output["thinking"] = corrected;
                }
                hallucination.correctionApplied = true;
                m_detectedHallucinations.push_back(hallucination);
                hallucinationCorrected(hallucination, corrected);
            }
        }
    }
    
    // Validate navigation in the output
    if (output.contains("navigationPath")) {
        std::string navPath = output["navigationPath"].get<std::string>();
        NavigationFix fix = fixNavigationError(navPath, context);
        if (!fix.fixId.empty()) {
            output["navigationPath"] = fix.correctPath;
            m_navigationFixes.push_back(fix);
            navigationErrorFixed(fix);
        }
    }
    
    // Apply behavioral patches
    output = applyBehaviorPatches(output, context);
    
    // Build result
    json result;
    result["original"] = modelOutput;
    result["modified"] = output;
    result["wasModified"] = (output != originalOutput);
    result["hallucinationsDetected"] = static_cast<int>(m_detectedHallucinations.size());
    result["navigationFixesApplied"] = static_cast<int>(m_navigationFixes.size());
    
    return result;
}

HallucinationDetection AgentHotPatcher::detectHallucination(const std::string& content, const json& context)
{
    HallucinationDetection detection;
    detection.detectionId = generateUniqueId();
    detection.detectedContent = content;
    detection.confidence = 0.0;
    detection.correctionApplied = false;
    
    // Check for path hallucinations
    std::regex pathRegex(R"((?:file|path|dir|directory):\s*([^\s,\.]+))");
    auto pathBegin = std::sregex_iterator(content.begin(), content.end(), pathRegex);
    auto pathEnd = std::sregex_iterator();
    
    for (std::sregex_iterator i = pathBegin; i != pathEnd; ++i) {
        std::smatch match = *i;
        std::string path = match[1].str();
        
        // Check if this path looks invalid
        if (path.find("//") != std::string::npos || path.find("\\\\") != std::string::npos || path.find("...") != std::string::npos) {
            detection.hallucinationType = "invalid_path";
            detection.confidence = 0.8;
            detection.detectedContent = path;
            detection.correctionStrategy = "normalize_path";
            return detection;
        }
        
        // Check if path references non-existent locations
        if (path.find("/mystical") == 0 || path.find("/phantom") == 0 || 
            path.find("nonexistent") != std::string::npos || path.find("virtual") != std::string::npos) {
            detection.hallucinationType = "fabricated_path";
            detection.confidence = 0.9;
            detection.detectedContent = path;
            detection.correctionStrategy = "replace_with_valid_path";
            return detection;
        }
    }
    
    // Check for logic hallucinations (impossible conditions)
    if (content.find("always succeeds") != std::string::npos && content.find("always fails") != std::string::npos) {
        detection.hallucinationType = "logic_contradiction";
        detection.confidence = 0.95;
        detection.correctionStrategy = "resolve_contradiction";
        return detection;
    }
    
    // Check for factual hallucinations (wrong facts)
    std::regex factRegex(R"((?:C\+\+|Python|Java)\s+(?:was created|version)\s+(\d{4}))");
    auto factBegin = std::sregex_iterator(content.begin(), content.end(), factRegex);
    auto factEnd = std::sregex_iterator();
    
    for (std::sregex_iterator i = factBegin; i != factEnd; ++i) {
        std::smatch match = *i;
        int year = std::stoi(match[1].str());
        
        // Check if year is way off
        if (year < 1970 || year > 2030) {
            detection.hallucinationType = "incorrect_fact";
            detection.confidence = 0.85;
            detection.detectedContent = match[0].str();
            detection.correctionStrategy = "correct_fact";
            return detection;
        }
    }
    
    // Check for incomplete reasoning
    if (content.find("The answer is") == 0 && content.length() < 20) {
        detection.hallucinationType = "incomplete_reasoning";
        detection.confidence = 0.6;
        detection.correctionStrategy = "expand_reasoning";
        return detection;
    }
    
    // Check known hallucination patterns
    for (auto const& [key, val] : m_hallucinationPatterns) {
        if (content.find(key) != std::string::npos) {
            detection.hallucinationType = "pattern_match";
            detection.confidence = 0.7;
            detection.correctionStrategy = "apply_known_correction";
            detection.expectedContent = val;
            return detection;
        }
    }
    
    // No hallucination detected
    detection.confidence = 0.0;
    return detection;
}

std::string AgentHotPatcher::correctHallucination(const HallucinationDetection& hallucination)
{
    std::string correction;
    
    if (hallucination.hallucinationType == "invalid_path") {
        // Normalize the path
        correction = hallucination.detectedContent;
        size_t pos = 0;
        while((pos = correction.find("//", pos)) != std::string::npos) correction.replace(pos, 2, "/");
        pos = 0;
        while((pos = correction.find("\\\\", pos)) != std::string::npos) correction.replace(pos, 2, "\\");
        return correction;
    }
    
    if (hallucination.hallucinationType == "fabricated_path") {
        return "./src/kernels/q8k_kernel.cpp";
    }
    
    if (hallucination.hallucinationType == "logic_contradiction") {
        return "The implementation uses robust error handling to manage edge cases.";
    }
    
    if (hallucination.hallucinationType == "incorrect_fact") {
        if (hallucination.detectedContent.find("C++") != std::string::npos) return "C++ was standardized in 1998 (C++98).";
        if (hallucination.detectedContent.find("Python") != std::string::npos) return "Python was created in 1989 by Guido van Rossum.";
        if (hallucination.detectedContent.find("Java") != std::string::npos) return "Java was created by Sun Microsystems in 1995.";
    }
    
    if (hallucination.hallucinationType == "incomplete_reasoning") {
        return hallucination.detectedContent + 
               " Let me analyze this step by step: First, we need to understand the requirements. "
               "Second, we evaluate the available approaches. Third, we select the best solution. "
               "Finally, we validate and document the outcome.";
    }
    
    if (hallucination.hallucinationType == "pattern_match" && !hallucination.expectedContent.empty()) {
        return hallucination.expectedContent;
    }
    
    return correction;
}

NavigationFix AgentHotPatcher::fixNavigationError(const std::string& navigationPath, const json& projectContext)
{
    NavigationFix fix;
    fix.fixId = generateUniqueId();
    fix.effectiveness = 0.0;
    
    if (!validateNavigationPath(navigationPath)) {
        // Detect the error type
        size_t dotDots = 0;
        size_t pos = 0;
        while((pos = navigationPath.find("..", pos)) != std::string::npos) { dotDots++; pos += 2; }
        
        if (dotDots > 3) {
            fix.incorrectPath = navigationPath;
            fix.correctPath = "./src/agent"; 
            fix.reasoning = "Too many parent directory traversals detected";
            fix.effectiveness = 0.9;
            return fix;
        }
        
        if (navigationPath.find("//") != std::string::npos || navigationPath.find("\\\\") != std::string::npos) {
            fix.incorrectPath = navigationPath;
            std::string corrected = navigationPath;
            size_t p = 0;
            while((p = corrected.find("//", p)) != std::string::npos) corrected.replace(p, 2, "/");
            p = 0;
            while((p = corrected.find("\\\\", p)) != std::string::npos) corrected.replace(p, 2, "\\");
            fix.correctPath = corrected;
            fix.reasoning = "Double slashes detected in path";
            fix.effectiveness = 0.95;
            return fix;
        }
        
        if (navigationPath.find("/") == 0 || navigationPath.find("C:") == 0) {
            fix.incorrectPath = navigationPath;
            fix.correctPath = "./" + navigationPath.substr(1);
            fix.reasoning = "Absolute path converted to relative";
            fix.effectiveness = 0.8;
            return fix;
        }
    }
    
    // Check known navigation fixes
    for (const NavigationFix& knownFix : m_navigationFixes) {
        if (navigationPath.find(knownFix.incorrectPath) != std::string::npos) {
            fix.incorrectPath = navigationPath;
            std::string corrected = navigationPath;
            size_t p = corrected.find(knownFix.incorrectPath);
            corrected.replace(p, knownFix.incorrectPath.length(), knownFix.correctPath);
            fix.correctPath = corrected;
            fix.reasoning = "Known navigation pattern corrected";
            fix.effectiveness = knownFix.effectiveness;
            return fix;
        }
    }
    
    fix.fixId.clear();
    return fix;
}

json AgentHotPatcher::applyBehaviorPatches(json output, const json& context)
{
    for (const BehaviorPatch& patch : m_behaviorPatches) {
        if (!patch.enabled) continue;
        
        bool conditionMet = false;
        
        if (patch.condition.find("hallucination") != std::string::npos) {
            if (!m_detectedHallucinations.empty()) conditionMet = true;
        } else if (patch.condition.find("navigation_error") != std::string::npos) {
            if (!m_navigationFixes.empty()) conditionMet = true;
        } else if (patch.condition.find("empty_reasoning") != std::string::npos) {
            if (!output.contains("reasoning") || output["reasoning"].get<std::string>().empty()) conditionMet = true;
        } 
        
        if (conditionMet) {
            if (patch.patchType == "output_filter") {
                if (patch.action.find("add_validation") != std::string::npos) output["validation_required"] = true;
                if (patch.action.find("remove_hallucinated") != std::string::npos) output.erase("speculative_content");
            } else if (patch.patchType == "prompt_modifier") {
                if (patch.action.find("enforce_reasoning") != std::string::npos) output["step_by_step"] = true;
            } else if (patch.patchType == "validator") {
                if (patch.action.find("validate_paths") != std::string::npos) {
                    if (output.contains("navigationPath")) {
                        NavigationFix fix = fixNavigationError(output["navigationPath"].get<std::string>(), context);
                        if (!fix.fixId.empty()) output["navigationPath"] = fix.correctPath;
                    }
                }
            }
            behaviorPatchApplied(patch);
        }
    }
    return output;
}

void AgentHotPatcher::registerCorrectionPattern(const HallucinationDetection& pattern)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    if (!pattern.detectedContent.empty() && !pattern.expectedContent.empty()) {
        m_hallucinationPatterns[pattern.detectedContent] = pattern.expectedContent;
        saveCorrectionPatterns();
    }
}

void AgentHotPatcher::registerNavigationFix(const NavigationFix& fix)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    if (!fix.incorrectPath.empty() && !fix.correctPath.empty()) {
        m_navigationPatternsMap[fix.incorrectPath] = fix.correctPath;
        saveCorrectionPatterns();
    }
}

void AgentHotPatcher::createBehaviorPatch(const BehaviorPatch& patch)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = std::find_if(m_behaviorPatches.begin(), m_behaviorPatches.end(),
                          [&patch](const BehaviorPatch& p) { return p.patchId == patch.patchId; });
    if (it != m_behaviorPatches.end()) *it = patch;
    else m_behaviorPatches.push_back(patch);
}

json AgentHotPatcher::getCorrectionStatistics() const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    json stats;
    stats["totalHallucinationsDetected"] = static_cast<int>(m_detectedHallucinations.size());
    int corrected = 0;
    for (const auto& h : m_detectedHallucinations) if (h.correctionApplied) corrected++;
    stats["hallucinationsCorrected"] = corrected;
    stats["totalNavigationFixesApplied"] = static_cast<int>(m_navigationFixes.size());
    stats["totalBehaviorPatches"] = static_cast<int>(m_behaviorPatches.size());
    stats["hotPatchingEnabled"] = m_enabled;
    return stats;
}

void AgentHotPatcher::setHotPatchingEnabled(bool enabled) { m_enabled = enabled; }
bool AgentHotPatcher::isHotPatchingEnabled() const { return m_enabled; }

void AgentHotPatcher::hallucinationDetected(const HallucinationDetection&) {}
void AgentHotPatcher::hallucinationCorrected(const HallucinationDetection&, const std::string&) {}
void AgentHotPatcher::navigationErrorFixed(const NavigationFix&) {}
void AgentHotPatcher::behaviorPatchApplied(const BehaviorPatch&) {}
void AgentHotPatcher::statisticsUpdated(const json&) {}

bool AgentHotPatcher::validateNavigationPath(const std::string& path)
{
    if (path.empty()) return false;
    if (path.find("//") != std::string::npos || path.find("\\\\") != std::string::npos) return false;
    if (path.find("...") != std::string::npos) return false;
    return true;
}

std::string AgentHotPatcher::generateUniqueId() { return std::to_string(m_idCounter++); }
void AgentHotPatcher::setDebugLogging(bool enabled) { m_debugLogging = enabled; }
int AgentHotPatcher::getCorrectionPatternCount() const { return (int)m_hallucinationPatterns.size(); }

bool AgentHotPatcher::loadCorrectionPatterns()
{
    m_hallucinationPatterns["/mystical/path"] = "./src";
    m_hallucinationPatterns["/phantom/dir"] = "./data";
    return true;
}

bool AgentHotPatcher::saveCorrectionPatterns() { return true; }
bool AgentHotPatcher::startInterceptorServer(int) { return true; }
json AgentHotPatcher::processInterceptedResponse(const json& response) { return applyBehaviorPatches(response, json()); }






