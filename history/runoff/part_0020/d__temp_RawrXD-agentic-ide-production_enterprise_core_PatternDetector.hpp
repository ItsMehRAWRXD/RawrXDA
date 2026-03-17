#ifndef PATTERN_DETECTOR_HPP
#define PATTERN_DETECTOR_HPP

#include "AICodeIntelligence.hpp"
#include <vector>
#include <string>
#include <map>

class PatternDetector {
public:
    PatternDetector();
    
    std::vector<CodePattern> detectPatterns(const std::string& code, const std::string& language);
    
    // Add/train patterns
    void addPattern(const CodePattern& pattern);
    void trainOnCodebase(const std::vector<std::string>& codeSamples);
    
    // Get patterns
    std::vector<CodePattern> getPatterns() const;
    std::vector<CodePattern> getPatternsByCategory(const std::string& category) const;
    
private:
    std::vector<CodePattern> m_patterns;
    
    void initializeCommonPatterns();
    std::vector<CodePattern> matchPatterns(const std::string& code, const std::string& language);
    std::string determineCategoryByFrequency(const std::string& patternName, int frequency);
};

#endif // PATTERN_DETECTOR_HPP
