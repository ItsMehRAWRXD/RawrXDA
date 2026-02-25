// ============================================================================
// ai_merge_resolver.cpp — AI-Assisted Merge Conflict Resolution
// ============================================================================
// Core implementation of merge conflict detection and resolution.
// ============================================================================

#include "ai_merge_resolver.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <cmath>

namespace RawrXD {
namespace Git {

// ============================================================================
// String utilities
// ============================================================================

static std::string trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }
    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

static std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

static bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
           str.compare(0, prefix.size(), prefix) == 0;
}

static bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ============================================================================
// Constructor/Destructor
// ============================================================================

AIMergeResolver::AIMergeResolver() {
}

AIMergeResolver::~AIMergeResolver() {
}

// ============================================================================
// Conflict Detection and Parsing
// ============================================================================

std::vector<ConflictMarker> AIMergeResolver::parseConflicts(
    const std::string& fileContent,
    const std::string& filePath) {
    
    std::vector<ConflictMarker> markers;
    std::istringstream iss(fileContent);
    std::string line;
    int lineNum = 0;
    std::string buffer;
    int conflictStart = -1;
    std::string oursSection, theirsSection, baseSection;
    bool inOurs = false, inBase = false, inTheirs = false;

    while (std::getline(iss, line)) {
        lineNum++;

        // Check for conflict markers
        if (startsWith(line, "<<<<<<<")) {
            if (conflictStart >= 0) {
                // Nested conflict (shouldn't happen, but handle it)
                continue;
            }
            conflictStart = lineNum;
            oursSection.clear();
            baseSection.clear();
            theirsSection.clear();
            inOurs = true;
            inBase = false;
            inTheirs = false;
        }
        else if (startsWith(line, "||||||| ")) {
            // 3-way merge marker (merge base)
            if (inOurs) {
                inOurs = false;
                inBase = true;
                inTheirs = false;
            }
        }
        else if (startsWith(line, "=======")) {
            // Transition from ours to theirs
            if (inOurs) {
                inOurs = false;
                inTheirs = true;
            } else if (inBase) {
                inBase = false;
                inTheirs = true;
            }
        }
        else if (startsWith(line, ">>>>>>>")) {
            // Conflict end
            if (conflictStart >= 0) {
                ConflictMarker marker;
                marker.startLine = conflictStart;
                marker.endLine = lineNum;
                marker.ours = oursSection;
                marker.theirs = theirsSection;
                marker.base = baseSection;
                marker.filePath = filePath;
                marker.is3Way = !baseSection.empty();
                markers.push_back(marker);
                
                conflictStart = -1;
                inOurs = false;
                inBase = false;
                inTheirs = false;
            }
        }
        else if (conflictStart >= 0) {
            // Accumulate conflict content
            if (inOurs) {
                oursSection += line + "\n";
            } else if (inBase) {
                baseSection += line + "\n";
            } else if (inTheirs) {
                theirsSection += line + "\n";
            }
        }
    }

    return markers;
}

std::optional<ConflictMarker> AIMergeResolver::extractConflict(
    const std::string& content,
    int startLine) {
    
    auto conflicts = parseConflicts(content);
    for (const auto& conflict : conflicts) {
        if (conflict.startLine == startLine) {
            return conflict;
        }
    }
    return std::nullopt;
}

bool AIMergeResolver::hasConflicts(const std::string& content) const {
    return content.find("<<<<<<<") != std::string::npos &&
           content.find("=======") != std::string::npos &&
           content.find(">>>>>>>") != std::string::npos;
}

int AIMergeResolver::countConflicts(const std::string& content) const {
    int count = 0;
    size_t pos = 0;
    while ((pos = content.find("<<<<<<<", pos)) != std::string::npos) {
        count++;
        pos++;
    }
    return count;
}

// ============================================================================
// Simple Conflict Removal
// ============================================================================

std::string AIMergeResolver::keepOurs(const std::string& content) const {
    std::string result;
    std::istringstream iss(content);
    std::string line;
    bool inConflict = false;
    bool inOurs = false;

    while (std::getline(iss, line)) {
        if (startsWith(line, "<<<<<<<")) {
            inConflict = true;
            inOurs = true;
        }
        else if (startsWith(line, "||||||| ")) {
            inOurs = false;
        }
        else if (startsWith(line, "=======")) {
            inOurs = false;
        }
        else if (startsWith(line, ">>>>>>>")) {
            inConflict = false;
        }
        else if (!inConflict) {
            result += line + "\n";
        }
        else if (inOurs) {
            result += line + "\n";
        }
    }

    return result;
}

std::string AIMergeResolver::keepTheirs(const std::string& content) const {
    std::string result;
    std::istringstream iss(content);
    std::string line;
    bool inConflict = false;
    bool inTheirs = false;

    while (std::getline(iss, line)) {
        if (startsWith(line, "<<<<<<<")) {
            inConflict = true;
            inTheirs = false;
        }
        else if (startsWith(line, "=======")) {
            inTheirs = true;
        }
        else if (startsWith(line, ">>>>>>>")) {
            inConflict = false;
        }
        else if (!inConflict) {
            result += line + "\n";
        }
        else if (inTheirs) {
            result += line + "\n";
        }
    }

    return result;
}

// ============================================================================
// Semantic Analysis
// ============================================================================

std::vector<std::string> AIMergeResolver::extractMeaningfulLines(
    const std::string& content) const {
    
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        std::string trimmed = trim(line);
        if (!trimmed.empty()) {
            lines.push_back(trimmed);
        }
    }

    return lines;
}

bool AIMergeResolver::isCodeContent(const std::string& content) const {
    // Very simple heuristic: check for common code patterns
    return content.find('{') != std::string::npos ||
           content.find('}') != std::string::npos ||
           content.find('(') != std::string::npos ||
           content.find(')') != std::string::npos ||
           content.find(';') != std::string::npos ||
           content.find("function") != std::string::npos ||
           content.find("class") != std::string::npos ||
           content.find("def ") != std::string::npos;
}

float AIMergeResolver::stringSimilarity(const std::string& a,
                                        const std::string& b) const {
    // Simplified similarity metric: count common substrings
    if (a == b) return 1.0f;
    if (a.empty() || b.empty()) return 0.0f;

    int matching = 0;
    int minLen = std::min(a.length(), b.length());
    for (int i = 0; i < minLen; ++i) {
        if (a[i] == b[i]) matching++;
    }

    return (float)matching / (float)std::max(a.length(), b.length());
}

AIMergeResolver::ConflictPattern AIMergeResolver::detectPattern(
    const ConflictMarker& conflict) const {
    
    ConflictPattern pattern;
    pattern.isImportConflict = false;
    pattern.isBlankLineConflict = false;
    pattern.isBothAddsToSameBlock = false;
    pattern.isSimpleLineChange = false;
    pattern.isCommentConflict = false;

    auto oursLines = extractMeaningfulLines(conflict.ours);
    auto theirsLines = extractMeaningfulLines(conflict.theirs);

    // Blank line conflict
    if (oursLines.empty() || theirsLines.empty()) {
        pattern.isBlankLineConflict = true;
        return pattern;
    }

    // Import conflict (Python, JavaScript, C++)
    if ((oursLines[0].find("import ") != std::string::npos ||
         oursLines[0].find("from ") != std::string::npos ||
         oursLines[0].find("#include") != std::string::npos) &&
        (theirsLines[0].find("import ") != std::string::npos ||
         theirsLines[0].find("from ") != std::string::npos ||
         theirsLines[0].find("#include") != std::string::npos)) {
        pattern.isImportConflict = true;
        return pattern;
    }

    // Comment conflict
    if ((oursLines[0][0] == '#' || oursLines[0][0] == '/' || oursLines[0][0] == '-') &&
        (theirsLines[0][0] == '#' || theirsLines[0][0] == '/' || theirsLines[0][0] == '-')) {
        pattern.isCommentConflict = true;
    }

    // Single-line change
    if (oursLines.size() == 1 && theirsLines.size() == 1) {
        pattern.isSimpleLineChange = true;
    }

    // Both additions to same logical block
    if (oursLines[0].find('{') != std::string::npos && 
        theirsLines[0].find('{') != std::string::npos) {
        pattern.isBothAddsToSameBlock = true;
    }

    return pattern;
}

std::string AIMergeResolver::findCommonContext(const std::string& ours,
                                               const std::string& theirs) const {
    // Find longest common substring prefix/suffix
    auto oursLines = extractMeaningfulLines(ours);
    auto theirsLines = extractMeaningfulLines(theirs);

    // For now, return a simple heuristic
    if (oursLines.empty() && theirsLines.empty()) {
        return "";
    }

    if (oursLines.empty()) return theirsLines[0];
    if (theirsLines.empty()) return oursLines[0];

    return oursLines[0];  // Simple default
}

std::string AIMergeResolver::merge3Way(const ConflictMarker& conflict) {
    // 3-way merge: compare ours and theirs against base
    if (conflict.base.empty()) {
        // Not actually 3-way
        return combineIntelligently(conflict, detectPattern(conflict));
    }

    auto baseLines = extractMeaningfulLines(conflict.base);
    auto oursLines = extractMeaningfulLines(conflict.ours);
    auto theirsLines = extractMeaningfulLines(conflict.theirs);

    // Simple heuristic: if ours == base, take theirs (they changed it)
    // If theirs == base, take ours (we changed it)
    if (trim(conflict.ours) == trim(conflict.base)) {
        return conflict.theirs;  // They made a change
    }
    if (trim(conflict.theirs) == trim(conflict.base)) {
        return conflict.ours;    // We made a change
    }

    // Both changed - auto-merge if possible
    return combineIntelligently(conflict, detectPattern(conflict));
}

std::string AIMergeResolver::combineIntelligently(
    const ConflictMarker& conflict,
    const ConflictPattern& pattern) {
    
    std::stringstream result;

    if (pattern.isBlankLineConflict) {
        // Just take the non-empty one
        if (!trim(conflict.ours).empty()) {
            return conflict.ours;
        }
        return conflict.theirs;
    }

    if (pattern.isImportConflict) {
        // Combine imports intelligently
        auto oursLines = split(conflict.ours, '\n');
        auto theirsLines = split(conflict.theirs, '\n');
        
        for (const auto& line : oursLines) {
            if (!trim(line).empty()) {
                result << trim(line) << "\n";
            }
        }
        for (const auto& line : theirsLines) {
            std::string trimmed = trim(line);
            if (!trimmed.empty()) {
                // Avoid duplicate imports
                std::string resultStr = result.str();
                if (resultStr.find(trimmed) == std::string::npos) {
                    result << trimmed << "\n";
                }
            }
        }
        return result.str();
    }

    if (pattern.isCommentConflict) {
        // Comments: try to include both
        result << conflict.ours << "\n" << conflict.theirs << "\n";
        return result.str();
    }

    if (pattern.isSimpleLineChange) {
        // Single line: check similarity
        float sim = stringSimilarity(conflict.ours, conflict.theirs);
        if (sim > 0.8f) {
            // Very similar - just take ours
            return conflict.ours;
        }
        // Different enough - return both (will need manual merge)
        result << conflict.ours << "\n" << conflict.theirs << "\n";
        return result.str();
    }

    // Default: combine with both versions separated by comment
    result << conflict.ours << "\n// --- alternative ---\n" << conflict.theirs << "\n";
    return result.str();
}

ResolutionStrategy AIMergeResolver::suggestStrategy(
    const ConflictMarker& conflict) const {
    
    auto pattern = detectPattern(conflict);

    if (pattern.isBlankLineConflict) {
        return ResolutionStrategy::COMBINED;
    }

    if (pattern.isImportConflict) {
        return ResolutionStrategy::SEMANTIC;
    }

    if (pattern.isCommentConflict) {
        return ResolutionStrategy::COMBINED;
    }

    // Check if they're identical
    if (trim(conflict.ours) == trim(conflict.theirs)) {
        return ResolutionStrategy::OURS;
    }

    // Check if it's 3-way with clear winner
    if (conflict.is3Way) {
        if (trim(conflict.ours) == trim(conflict.base)) {
            return ResolutionStrategy::THEIRS;
        }
        if (trim(conflict.theirs) == trim(conflict.base)) {
            return ResolutionStrategy::OURS;
        }
    }

    // Default to semantic analysis
    return ResolutionStrategy::SEMANTIC;
}

// ============================================================================
// Main Resolution Logic
// ============================================================================

ResolutionResult AIMergeResolver::resolveConflict(
    const ConflictMarker& conflict,
    const std::string& context) {
    
    ResolutionResult result;
    result.confidence = 0.0f;
    result.explanation = "";

    // Determine strategy
    result.strategy = suggestStrategy(conflict);

    switch (result.strategy) {
        case ResolutionStrategy::OURS: {
            if (trim(conflict.ours) == trim(conflict.theirs)) {
                result.resolved = conflict.ours;
                result.confidence = 1.0f;
                result.explanation = "Identical content in both versions";
            } else if (conflict.is3Way && 
                     trim(conflict.ours) == trim(conflict.base)) {
                result.resolved = conflict.theirs;
                result.confidence = 0.95f;
                result.explanation = "Theirs modified base, ours didn't";
            } else {
                result.resolved = conflict.ours;
                result.confidence = 0.6f;
                result.explanation = "Choosing our version by default";
            }
            break;
        }

        case ResolutionStrategy::THEIRS: {
            result.resolved = conflict.theirs;
            result.confidence = 0.95f;
            result.explanation = "Ours didn't change from base";
            break;
        }

        case ResolutionStrategy::COMBINED: {
            result.strategy = ResolutionStrategy::COMBINED;
            auto pattern = detectPattern(conflict);
            result.resolved = combineIntelligently(conflict, pattern);
            result.confidence = 0.75f;
            result.explanation = "Intelligently combined versions";
            break;
        }

        case ResolutionStrategy::SEMANTIC: {
            if (conflict.is3Way) {
                result.resolved = merge3Way(conflict);
                result.confidence = 0.80f;
                result.explanation = "3-way merge resolution";
            } else {
                auto pattern = detectPattern(conflict);
                result.resolved = combineIntelligently(conflict, pattern);
                // Check confidence based on similarity
                float sim = stringSimilarity(conflict.ours, conflict.theirs);
                result.confidence = 0.5f + (sim * 0.3f);  // 0.5-0.8 range
                result.explanation = "Semantic analysis with auto-combine";
            }
            break;
        }

        case ResolutionStrategy::MANUAL: {
            result.resolved = "";
            result.confidence = 0.0f;
            result.explanation = "Unable to auto-resolve, manual intervention required";
            break;
        }
    }

    return result;
}

bool AIMergeResolver::analyzeSemantic(const ConflictMarker& conflict,
                                     std::string& resolvedContent,
                                     float& confidence) {
    // This is where more sophisticated semantic analysis would go
    // For now, delegate to resolve logic
    auto result = resolveConflict(conflict);
    if (result.confidence > 0.0f) {
        resolvedContent = result.resolved;
        confidence = result.confidence;
        return true;
    }
    return false;
}

std::string AIMergeResolver::autoResolve(const std::string& fileContent,
                                        const std::string& filePath) {
    
    auto conflicts = parseConflicts(fileContent, filePath);
    
    if (conflicts.empty()) {
        return fileContent;  // No conflicts
    }

    // Process conflicts and rebuild content
    std::string result = fileContent;
    
    // Process in reverse order to maintain line numbers
    for (auto it = conflicts.rbegin(); it != conflicts.rend(); ++it) {
        auto resolution = resolveConflict(*it);
        
        // Only apply if confidence is reasonable
        if (resolution.confidence >= 0.6f) {
            // Find and replace the conflict marker section
            std::string conflictSection = "<<<<<<<";
            size_t pos = result.find(conflictSection);
            
            while (pos != std::string::npos) {
                // Find the end marker
                size_t endPos = result.find(">>>>>>>", pos);
                if (endPos != std::string::npos) {
                    // Find end of line after >>>>>>>
                    size_t endOfLine = result.find('\n', endPos);
                    if (endOfLine == std::string::npos) {
                        endOfLine = result.length();
                    }

                    // Replace entire conflict section with resolved content
                    result.replace(pos, endOfLine - pos, resolution.resolved);
                    break;
                } else {
                    break;
                }
            }
        }
    }

    return result;
}

} // namespace Git
} // namespace RawrXD
