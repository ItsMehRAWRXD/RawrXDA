#include "PatternDetector.hpp"
#include "CodeAnalysisUtils.hpp"
#include <regex>
#include <algorithm>

PatternDetector::PatternDetector() {
    initializeCommonPatterns();
}

void PatternDetector::initializeCommonPatterns() {
    // Design patterns
    CodePattern singletonPattern;
    singletonPattern.pattern = "static.*getInstance";
    singletonPattern.language = "cpp";
    singletonPattern.category = "design";
    singletonPattern.detectionAccuracy = 0.85;
    m_patterns.push_back(singletonPattern);
    
    CodePattern factoryPattern;
    factoryPattern.pattern = "Factory|Creator";
    factoryPattern.language = "cpp";
    factoryPattern.category = "design";
    factoryPattern.detectionAccuracy = 0.80;
    m_patterns.push_back(factoryPattern);
    
    // Anti-patterns
    CodePattern godObjectPattern;
    godObjectPattern.pattern = R"(class\s+\w+\s*\{.*public:.*\w+\s*\(.*\);.*public:.*\w+\s*\(.*\);)";
    godObjectPattern.language = "cpp";
    godObjectPattern.category = "anti-pattern";
    godObjectPattern.detectionAccuracy = 0.70;
    m_patterns.push_back(godObjectPattern);
    
    // Coding patterns
    CodePattern lambdaPattern;
    lambdaPattern.pattern = R"(\[\w*\]\s*\([^)]*\)\s*\{)";
    lambdaPattern.language = "cpp";
    lambdaPattern.category = "modern-cpp";
    lambdaPattern.detectionAccuracy = 0.95;
    m_patterns.push_back(lambdaPattern);
    
    CodePattern autoPattern;
    autoPattern.pattern = R"(auto\s+\w+\s*=)";
    autoPattern.language = "cpp";
    autoPattern.category = "modern-cpp";
    autoPattern.detectionAccuracy = 0.90;
    m_patterns.push_back(autoPattern);
}

std::vector<CodePattern> PatternDetector::detectPatterns(const std::string& code, const std::string& language) {
    return matchPatterns(code, language);
}

std::vector<CodePattern> PatternDetector::matchPatterns(const std::string& code, const std::string& language) {
    std::vector<CodePattern> detected;
    
    for (const auto& pattern : m_patterns) {
        if (!pattern.language.empty() && pattern.language != language) {
            continue;
        }
        
        if (CodeAnalysisUtils::matchesPattern(code, pattern.pattern)) {
            detected.push_back(pattern);
        }
    }
    
    return detected;
}

void PatternDetector::addPattern(const CodePattern& pattern) {
    // Check if pattern already exists
    for (auto& p : m_patterns) {
        if (p.pattern == pattern.pattern && p.language == pattern.language) {
            p = pattern; // Update existing
            return;
        }
    }
    m_patterns.push_back(pattern);
}

void PatternDetector::trainOnCodebase(const std::vector<std::string>& codeSamples) {
    if (codeSamples.empty()) {
        return;
    }
    
    // Analyze code samples to extract patterns
    std::map<std::string, int> patternFrequency;
    std::map<std::string, std::vector<std::string>> patternMatches;
    
    for (const auto& code : codeSamples) {
        // Extract singleton patterns
        std::regex singletonRegex(R"(static\s+\w+\*\s+instance\(\)|\bstatic\s+\w+\s+\w+\s*=\s*nullptr)");
        std::smatch match;
        if (std::regex_search(code, match, singletonRegex)) {
            patternFrequency["Singleton"]++;
            patternMatches["Singleton"].push_back(match.str());
        }
        
        // Extract factory patterns
        std::regex factoryRegex(R"(class\s+\w+Factory|static\s+\w+\*\s+create\(|std::unique_ptr<\w+>\s+create)");
        if (std::regex_search(code, match, factoryRegex)) {
            patternFrequency["Factory"]++;
            patternMatches["Factory"].push_back(match.str());
        }
        
        // Extract observer patterns
        std::regex observerRegex(R"(void\s+attach\(|void\s+detach\(|void\s+notify\(|std::vector.*Observer)");
        if (std::regex_search(code, match, observerRegex)) {
            patternFrequency["Observer"]++;
            patternMatches["Observer"].push_back(match.str());
        }
        
        // Extract strategy patterns
        std::regex strategyRegex(R"(class\s+\w+Strategy|virtual\s+void\s+execute|std::function|std::unique_ptr<.*Strategy)");
        if (std::regex_search(code, match, strategyRegex)) {
            patternFrequency["Strategy"]++;
            patternMatches["Strategy"].push_back(match.str());
        }
        
        // Extract lambda patterns (modern C++)
        std::regex lambdaRegex(R"(\[\s*[^]]*\s*\]\s*\([^)]*\)\s*\{)");
        if (std::regex_search(code, match, lambdaRegex)) {
            patternFrequency["Lambda"]++;
            patternMatches["Lambda"].push_back(match.str());
        }
        
        // Extract auto keyword patterns (modern C++)
        std::regex autoRegex(R"(\bauto\s+\w+\s*=)");
        std::string::const_iterator searchStart(code.cbegin());
        while (std::regex_search(searchStart, code.cend(), match, autoRegex)) {
            patternFrequency["Auto"]++;
            patternMatches["Auto"].push_back(match.str());
            searchStart = match.suffix().first;
        }
        
        // Detect anti-patterns
        // God object pattern (large class with many methods)
        std::regex largeClassRegex(R"(class\s+\w+\s*\{[^}]{5000,})");
        if (std::regex_search(code, match, largeClassRegex)) {
            patternFrequency["GodObject"]++;
            patternMatches["GodObject"].push_back("Detected large class definition");
        }
        
        // Feature envy pattern (methods accessing other objects frequently)
        std::regex featureEnvyRegex(R"(\w+\.\w+\s*\([^)]*\);.*\w+\.\w+\s*\([^)]*\);.*\w+\.\w+\s*\([^)]*\);)");
        if (std::regex_search(code, match, featureEnvyRegex)) {
            patternFrequency["FeatureEnvy"]++;
            patternMatches["FeatureEnvy"].push_back("Detected excessive method calls on external objects");
        }
        
        // Primitive obsession pattern (excessive use of primitives)
        std::regex primitiveRegex(R"(\b(int|float|double|bool)\b.*\b(int|float|double|bool)\b.*\b(int|float|double|bool)\b)");
        if (std::regex_search(code, match, primitiveRegex)) {
            patternFrequency["PrimitiveObsession"]++;
            patternMatches["PrimitiveObsession"].push_back("Detected excessive primitive type usage");
        }
    }
    
    // Create new patterns based on analysis
    for (const auto& [patternName, frequency] : patternFrequency) {
        if (frequency >= codeSamples.size() / 2) {  // Pattern found in at least 50% of samples
            CodePattern pattern;
            pattern.pattern = patternName;
            pattern.language = "cpp";
            pattern.category = determineCategoryByFrequency(patternName, frequency);
            pattern.detectionAccuracy = std::min(1.0, frequency / static_cast<double>(codeSamples.size()));
            
            // Update or add pattern
            addPattern(pattern);
        }
    }
}

std::string PatternDetector::determineCategoryByFrequency(const std::string& patternName, int frequency) {
    if (patternName == "Singleton" || patternName == "Factory" || 
        patternName == "Observer" || patternName == "Strategy") {
        return "design";
    } else if (patternName == "Lambda" || patternName == "Auto") {
        return "modern_cpp";
    } else if (patternName == "GodObject" || patternName == "FeatureEnvy" || 
               patternName == "PrimitiveObsession") {
        return "anti_pattern";
    }
    return "other";
}

std::vector<CodePattern> PatternDetector::getPatterns() const {
    return m_patterns;
}

std::vector<CodePattern> PatternDetector::getPatternsByCategory(const std::string& category) const {
    std::vector<CodePattern> filtered;
    for (const auto& pattern : m_patterns) {
        if (pattern.category == category) {
            filtered.push_back(pattern);
        }
    }
    return filtered;
}
