// ============================================================================
// File: src/agent/memory_hot_patcher.cpp
// Purpose: Memory-level hot patching implementation
// Converted from Qt to pure C++17
// ============================================================================
#include "memory_hot_patcher.hpp"
#include <algorithm>
#include <sstream>
#include <chrono>

MemoryHotPatcher::MemoryHotPatcher()
    : m_idCounter(0), m_totalPatchesApplied(0), m_totalCorrections(0) {}

MemoryHotPatcher::~MemoryHotPatcher() {}

// ── Hallucination Pattern Management ────────────────────────────────────────

bool MemoryHotPatcher::registerPattern(const HallucinationPattern& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_patterns) {
        if (p.patternId == pattern.patternId) {
            logWarning() << "Pattern already registered:" << pattern.patternId;
            return false;
        }
    }
    m_patterns.push_back(pattern);
    onPatternRegistered.emit(pattern);
    logDebug() << "Registered hallucination pattern:" << pattern.patternId
               << "type:" << pattern.patternType;
    return true;
}

bool MemoryHotPatcher::removePattern(const std::string& patternId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_patterns.begin(), m_patterns.end(),
                           [&](const HallucinationPattern& p) { return p.patternId == patternId; });
    if (it != m_patterns.end()) {
        m_patterns.erase(it);
        onPatternRemoved.emit(patternId);
        return true;
    }
    return false;
}

HallucinationPattern* MemoryHotPatcher::findPattern(const std::string& patternId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& p : m_patterns) {
        if (p.patternId == patternId) return &p;
    }
    return nullptr;
}

std::vector<HallucinationPattern> MemoryHotPatcher::getActivePatterns() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<HallucinationPattern> active;
    for (const auto& p : m_patterns) {
        if (p.active) active.push_back(p);
    }
    return active;
}

std::vector<HallucinationPattern> MemoryHotPatcher::getAllPatterns() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_patterns;
}

int MemoryHotPatcher::getPatternHitCount(const std::string& patternId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& p : m_patterns) {
        if (p.patternId == patternId) return p.hitCount;
    }
    return -1;
}

// ── Navigation Correction Management ────────────────────────────────────────

bool MemoryHotPatcher::addNavigationCorrection(const NavigationCorrection& correction) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& c : m_corrections) {
        if (c.correctionId == correction.correctionId) return false;
    }
    m_corrections.push_back(correction);
    return true;
}

bool MemoryHotPatcher::removeNavigationCorrection(const std::string& correctionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_corrections.begin(), m_corrections.end(),
                           [&](const NavigationCorrection& c) { return c.correctionId == correctionId; });
    if (it != m_corrections.end()) {
        m_corrections.erase(it);
        return true;
    }
    return false;
}

std::vector<NavigationCorrection> MemoryHotPatcher::getNavigationCorrections() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_corrections;
}

std::string MemoryHotPatcher::applyNavigationCorrection(const std::string& originalPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<NavigationCorrection*> matching;
    for (auto& c : m_corrections) {
        if (c.originalPath == originalPath ||
            StringUtils::containsCI(originalPath, c.triggerCondition)) {
            matching.push_back(&c);
        }
    }
    if (matching.empty()) return originalPath;
    std::sort(matching.begin(), matching.end(),
              [](const NavigationCorrection* a, const NavigationCorrection* b) {
                  return a->priority > b->priority;
              });
    auto* best = matching.front();
    if (best->autoApply) {
        best->applicationCount++;
        onNavigationCorrected.emit(*best);
        logDebug() << "Applied navigation correction:" << best->correctionId
                   << "from:" << originalPath << "to:" << best->correctedPath;
        return best->correctedPath;
    }
    return originalPath;
}

// ── Memory Intercept Rules ──────────────────────────────────────────────────

bool MemoryHotPatcher::addInterceptRule(const MemoryInterceptRule& rule) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& r : m_interceptRules) {
        if (r.ruleId == rule.ruleId) return false;
    }
    m_interceptRules.push_back(rule);
    return true;
}

bool MemoryHotPatcher::removeInterceptRule(const std::string& ruleId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_interceptRules.begin(), m_interceptRules.end(),
                           [&](const MemoryInterceptRule& r) { return r.ruleId == ruleId; });
    if (it != m_interceptRules.end()) {
        m_interceptRules.erase(it);
        return true;
    }
    return false;
}

bool MemoryHotPatcher::enableInterceptRule(const std::string& ruleId, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& r : m_interceptRules) {
        if (r.ruleId == ruleId) {
            r.enabled = enabled;
            return true;
        }
    }
    return false;
}

std::vector<MemoryInterceptRule> MemoryHotPatcher::getInterceptRules() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_interceptRules;
}

// ── Memory Region Management ────────────────────────────────────────────────

bool MemoryHotPatcher::registerMemoryRegion(const MemoryRegionDescriptor& region) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& r : m_regions) {
        if (r.regionId == region.regionId) return false;
    }
    m_regions.push_back(region);
    return true;
}

bool MemoryHotPatcher::unregisterMemoryRegion(const std::string& regionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_regions.begin(), m_regions.end(),
                           [&](const MemoryRegionDescriptor& r) { return r.regionId == regionId; });
    if (it != m_regions.end()) {
        m_regions.erase(it);
        return true;
    }
    return false;
}

std::vector<MemoryRegionDescriptor> MemoryHotPatcher::getMonitoredRegions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MemoryRegionDescriptor> monitored;
    for (const auto& r : m_regions) {
        if (r.isMonitored) monitored.push_back(r);
    }
    return monitored;
}

// ── Core Hot Patching Operations ────────────────────────────────────────────

CorrectedExecutionContext MemoryHotPatcher::correctExecution(const std::string& inputContext,
                                                              double inputConfidence)
{
    CorrectedExecutionContext ctx;
    ctx.contextId = generateUniqueId();
    ctx.originalContext = inputContext;
    ctx.correctedContext = inputContext;
    ctx.originalConfidence = inputConfidence;
    ctx.correctedConfidence = inputConfidence;
    ctx.timestamp = TimeUtils::now();
    ctx.wasModified = false;

    // Apply hallucination pattern corrections
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& pattern : m_patterns) {
            if (!pattern.active) continue;
            try {
                std::regex rx(pattern.detectionRegex, std::regex_constants::icase);
                if (std::regex_search(ctx.correctedContext, rx)) {
                    pattern.hitCount++;
                    pattern.lastTriggered = TimeUtils::now();

                    if (inputConfidence < pattern.confidenceThreshold) {
                        ctx.correctedContext = std::regex_replace(ctx.correctedContext, rx,
                                                                   pattern.correctionTemplate);
                        ctx.appliedPatches.push_back(pattern.patternId);
                        ctx.wasModified = true;
                        ctx.correctedConfidence += 0.05;
                        ctx.modificationReason += "Applied pattern " + pattern.patternId + "; ";
                        logDebug() << "Applied hallucination patch:" << pattern.patternId;
                    }
                }
            } catch (const std::regex_error& e) {
                logWarning() << "Regex error in pattern" << pattern.patternId << ":" << e.what();
            }
        }
    }

    // Apply navigation corrections
    ctx.correctedContext = applyNavigationCorrection(ctx.correctedContext);

    // Process intercept rules
    processInterceptRules("post_read", ctx);

    if (ctx.correctedConfidence > 1.0) ctx.correctedConfidence = 1.0;
    if (ctx.correctedConfidence < 0.0) ctx.correctedConfidence = 0.0;
    m_totalCorrections++;
    onExecutionCorrected.emit(ctx);
    return ctx;
}

PatchApplicationResult MemoryHotPatcher::applyHallucinationPatch(const std::string& patternId,
                                                                   const std::string& targetText)
{
    PatchApplicationResult result;
    result.patchId = patternId;
    result.appliedAt = TimeUtils::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    HallucinationPattern* pattern = nullptr;
    for (auto& p : m_patterns) {
        if (p.patternId == patternId) { pattern = &p; break; }
    }

    if (!pattern) {
        result.success = false;
        result.detail = "Pattern not found: " + patternId;
        result.confidenceDelta = 0.0;
        result.regionsAffected = 0;
        return result;
    }

    try {
        std::regex rx(pattern->detectionRegex, std::regex_constants::icase);
        if (std::regex_search(targetText, rx)) {
            pattern->hitCount++;
            pattern->lastTriggered = TimeUtils::now();
            result.success = true;
            result.detail = "Pattern matched and applied";
            result.confidenceDelta = 0.05;
            result.regionsAffected = 1;
            m_totalPatchesApplied++;
            onPatchApplied.emit(result);
        } else {
            result.success = false;
            result.detail = "Pattern did not match target text";
            result.confidenceDelta = 0.0;
            result.regionsAffected = 0;
        }
    } catch (const std::regex_error& e) {
        result.success = false;
        result.detail = std::string("Regex error: ") + e.what();
        result.confidenceDelta = 0.0;
        result.regionsAffected = 0;
        onError.emit(result.detail);
    }
    return result;
}

std::vector<PatchApplicationResult> MemoryHotPatcher::applyAllMatchingPatches(const std::string& text) {
    std::vector<PatchApplicationResult> results;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pattern : m_patterns) {
        if (!pattern.active) continue;
        try {
            std::regex rx(pattern.detectionRegex, std::regex_constants::icase);
            if (std::regex_search(text, rx)) {
                PatchApplicationResult r;
                r.patchId = pattern.patternId;
                r.appliedAt = TimeUtils::now();
                r.success = true;
                r.detail = "Matched pattern: " + pattern.description;
                r.confidenceDelta = 0.05;
                r.regionsAffected = 1;
                pattern.hitCount++;
                pattern.lastTriggered = TimeUtils::now();
                m_totalPatchesApplied++;
                results.push_back(r);
            }
        } catch (const std::regex_error&) {
            // skip invalid patterns
        }
    }
    return results;
}

int MemoryHotPatcher::scanAndPatch(const std::string& text) {
    auto results = applyAllMatchingPatches(text);
    int count = 0;
    for (const auto& r : results) {
        if (r.success) count++;
    }
    return count;
}

std::string MemoryHotPatcher::getStatisticsReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream report;
    report << "=== MemoryHotPatcher Statistics ===\n";
    report << "Registered patterns: " << m_patterns.size() << "\n";
    int activeCount = 0;
    for (const auto& p : m_patterns) { if (p.active) activeCount++; }
    report << "Active patterns: " << activeCount << "\n";
    report << "Navigation corrections: " << m_corrections.size() << "\n";
    report << "Intercept rules: " << m_interceptRules.size() << "\n";
    report << "Monitored regions: " << m_regions.size() << "\n";
    report << "Total patches applied: " << m_totalPatchesApplied.load() << "\n";
    report << "Total corrections: " << m_totalCorrections.load() << "\n";
    report << "\n--- Pattern Details ---\n";
    for (const auto& p : m_patterns) {
        report << "  " << p.patternId << " (" << p.patternType << "): "
               << p.hitCount << " hits, active=" << (p.active ? "yes" : "no") << "\n";
    }
    return report.str();
}

// ── Serialization ───────────────────────────────────────────────────────────

JsonObject MemoryHotPatcher::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    JsonObject root;
    JsonArray patternsArr;
    for (const auto& p : m_patterns) {
        JsonObject obj;
        obj["patternId"] = JsonValue(p.patternId);
        obj["patternType"] = JsonValue(p.patternType);
        obj["detectionRegex"] = JsonValue(p.detectionRegex);
        obj["correctionTemplate"] = JsonValue(p.correctionTemplate);
        obj["confidenceThreshold"] = JsonValue(p.confidenceThreshold);
        obj["hitCount"] = JsonValue(p.hitCount);
        obj["active"] = JsonValue(p.active);
        obj["description"] = JsonValue(p.description);
        JsonArray tagsArr;
        for (const auto& t : p.tags) tagsArr.push_back(JsonValue(t));
        obj["tags"] = JsonValue(tagsArr);
        patternsArr.push_back(JsonValue(obj));
    }
    root["patterns"] = JsonValue(patternsArr);

    JsonArray correctionsArr;
    for (const auto& c : m_corrections) {
        JsonObject obj;
        obj["correctionId"] = JsonValue(c.correctionId);
        obj["triggerCondition"] = JsonValue(c.triggerCondition);
        obj["originalPath"] = JsonValue(c.originalPath);
        obj["correctedPath"] = JsonValue(c.correctedPath);
        obj["reasoning"] = JsonValue(c.reasoning);
        obj["priority"] = JsonValue(c.priority);
        obj["autoApply"] = JsonValue(c.autoApply);
        obj["applicationCount"] = JsonValue(c.applicationCount);
        correctionsArr.push_back(JsonValue(obj));
    }
    root["corrections"] = JsonValue(correctionsArr);

    JsonArray rulesArr;
    for (const auto& r : m_interceptRules) {
        JsonObject obj;
        obj["ruleId"] = JsonValue(r.ruleId);
        obj["ruleName"] = JsonValue(r.ruleName);
        obj["interceptPoint"] = JsonValue(r.interceptPoint);
        obj["condition"] = JsonValue(r.condition);
        obj["action"] = JsonValue(r.action);
        obj["transformExpression"] = JsonValue(r.transformExpression);
        obj["priority"] = JsonValue(r.priority);
        obj["enabled"] = JsonValue(r.enabled);
        obj["executionCount"] = JsonValue(r.executionCount);
        rulesArr.push_back(JsonValue(obj));
    }
    root["interceptRules"] = JsonValue(rulesArr);
    root["totalPatchesApplied"] = JsonValue(m_totalPatchesApplied.load());
    root["totalCorrections"] = JsonValue(m_totalCorrections.load());
    return root;
}

bool MemoryHotPatcher::fromJson(const JsonObject& json) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_patterns.clear();
    m_corrections.clear();
    m_interceptRules.clear();

    auto patternsIt = json.find("patterns");
    if (patternsIt != json.end() && patternsIt->second.isArray()) {
        for (const auto& item : patternsIt->second.toArray()) {
            if (!item.isObject()) continue;
            auto obj = item.toObject();
            HallucinationPattern p;
            p.patternId = obj.count("patternId") ? obj.at("patternId").toString() : "";
            p.patternType = obj.count("patternType") ? obj.at("patternType").toString() : "";
            p.detectionRegex = obj.count("detectionRegex") ? obj.at("detectionRegex").toString() : "";
            p.correctionTemplate = obj.count("correctionTemplate") ? obj.at("correctionTemplate").toString() : "";
            p.confidenceThreshold = obj.count("confidenceThreshold") ? obj.at("confidenceThreshold").toDouble() : 0.5;
            p.hitCount = obj.count("hitCount") ? obj.at("hitCount").toInt() : 0;
            p.active = obj.count("active") ? obj.at("active").toBool() : true;
            p.description = obj.count("description") ? obj.at("description").toString() : "";
            if (obj.count("tags") && obj.at("tags").isArray()) {
                for (const auto& t : obj.at("tags").toArray()) {
                    p.tags.push_back(t.toString());
                }
            }
            m_patterns.push_back(p);
        }
    }

    auto correctionsIt = json.find("corrections");
    if (correctionsIt != json.end() && correctionsIt->second.isArray()) {
        for (const auto& item : correctionsIt->second.toArray()) {
            if (!item.isObject()) continue;
            auto obj = item.toObject();
            NavigationCorrection c;
            c.correctionId = obj.count("correctionId") ? obj.at("correctionId").toString() : "";
            c.triggerCondition = obj.count("triggerCondition") ? obj.at("triggerCondition").toString() : "";
            c.originalPath = obj.count("originalPath") ? obj.at("originalPath").toString() : "";
            c.correctedPath = obj.count("correctedPath") ? obj.at("correctedPath").toString() : "";
            c.reasoning = obj.count("reasoning") ? obj.at("reasoning").toString() : "";
            c.priority = obj.count("priority") ? obj.at("priority").toInt() : 0;
            c.autoApply = obj.count("autoApply") ? obj.at("autoApply").toBool() : false;
            c.applicationCount = obj.count("applicationCount") ? obj.at("applicationCount").toInt() : 0;
            c.createdAt = TimeUtils::now();
            m_corrections.push_back(c);
        }
    }

    auto rulesIt = json.find("interceptRules");
    if (rulesIt != json.end() && rulesIt->second.isArray()) {
        for (const auto& item : rulesIt->second.toArray()) {
            if (!item.isObject()) continue;
            auto obj = item.toObject();
            MemoryInterceptRule r;
            r.ruleId = obj.count("ruleId") ? obj.at("ruleId").toString() : "";
            r.ruleName = obj.count("ruleName") ? obj.at("ruleName").toString() : "";
            r.interceptPoint = obj.count("interceptPoint") ? obj.at("interceptPoint").toString() : "";
            r.condition = obj.count("condition") ? obj.at("condition").toString() : "";
            r.action = obj.count("action") ? obj.at("action").toString() : "";
            r.transformExpression = obj.count("transformExpression") ? obj.at("transformExpression").toString() : "";
            r.priority = obj.count("priority") ? obj.at("priority").toInt() : 0;
            r.enabled = obj.count("enabled") ? obj.at("enabled").toBool() : true;
            r.executionCount = obj.count("executionCount") ? obj.at("executionCount").toInt() : 0;
            m_interceptRules.push_back(r);
        }
    }

    if (json.count("totalPatchesApplied"))
        m_totalPatchesApplied = json.at("totalPatchesApplied").toInt();
    if (json.count("totalCorrections"))
        m_totalCorrections = json.at("totalCorrections").toInt();
    return true;
}

// ── Private Helpers ─────────────────────────────────────────────────────────

std::string MemoryHotPatcher::generateUniqueId() {
    return "mhp_" + std::to_string(m_idCounter++);
}

bool MemoryHotPatcher::evaluateCondition(const std::string& condition, const std::string& context) {
    if (condition.empty()) return true;
    if (StringUtils::startsWith(condition, "contains:")) {
        std::string needle = condition.substr(9);
        return StringUtils::contains(context, StringUtils::trimmed(needle));
    }
    if (StringUtils::startsWith(condition, "regex:")) {
        std::string pattern = condition.substr(6);
        try {
            std::regex rx(StringUtils::trimmed(pattern), std::regex_constants::icase);
            return std::regex_search(context, rx);
        } catch (...) { return false; }
    }
    if (StringUtils::startsWith(condition, "length>")) {
        int threshold = std::stoi(condition.substr(7));
        return static_cast<int>(context.size()) > threshold;
    }
    return StringUtils::contains(context, condition);
}

std::string MemoryHotPatcher::applyTransform(const std::string& expression, const std::string& input) {
    if (expression.empty()) return input;
    if (StringUtils::startsWith(expression, "replace:")) {
        auto parts = StringUtils::split(expression.substr(8), ':');
        if (parts.size() >= 2) {
            return StringUtils::replace(input, parts[0], parts[1]);
        }
    }
    if (StringUtils::startsWith(expression, "prefix:")) {
        return expression.substr(7) + input;
    }
    if (StringUtils::startsWith(expression, "suffix:")) {
        return input + expression.substr(7);
    }
    if (expression == "uppercase") return StringUtils::toUpper(input);
    if (expression == "lowercase") return StringUtils::toLower(input);
    if (expression == "trim") return StringUtils::trimmed(input);
    return input;
}

void MemoryHotPatcher::processInterceptRules(const std::string& interceptPoint,
                                              CorrectedExecutionContext& ctx)
{
    std::vector<MemoryInterceptRule*> matching;
    for (auto& rule : m_interceptRules) {
        if (!rule.enabled) continue;
        if (rule.interceptPoint == interceptPoint) {
            matching.push_back(&rule);
        }
    }
    std::sort(matching.begin(), matching.end(),
              [](const MemoryInterceptRule* a, const MemoryInterceptRule* b) {
                  return a->priority > b->priority;
              });
    for (auto* rule : matching) {
        if (evaluateCondition(rule->condition, ctx.correctedContext)) {
            if (rule->action == "modify") {
                ctx.correctedContext = applyTransform(rule->transformExpression,
                                                      ctx.correctedContext);
                ctx.wasModified = true;
                ctx.modificationReason += "Intercept rule " + rule->ruleId + "; ";
            } else if (rule->action == "log") {
                logDebug() << "Intercept rule triggered:" << rule->ruleId
                           << "at:" << interceptPoint;
            } else if (rule->action == "block") {
                ctx.correctedContext = "[BLOCKED by rule " + rule->ruleId + "]";
                ctx.wasModified = true;
            } else if (rule->action == "redirect") {
                ctx.correctedContext = applyTransform(rule->transformExpression,
                                                      ctx.correctedContext);
                ctx.wasModified = true;
            }
            rule->executionCount++;
            rule->lastExecuted = TimeUtils::now();
            ctx.interceptedAccesses.push_back(rule->ruleId);
            onInterceptTriggered.emit(*rule, interceptPoint);
        }
    }
}
