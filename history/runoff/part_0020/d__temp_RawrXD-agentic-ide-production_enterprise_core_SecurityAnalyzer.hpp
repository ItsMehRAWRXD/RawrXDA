#ifndef SECURITY_ANALYZER_HPP
#define SECURITY_ANALYZER_HPP

#include "AICodeIntelligence.hpp"
#include <vector>
#include <string>

class SecurityAnalyzer {
public:
    SecurityAnalyzer();
    
    // Analyze code for security vulnerabilities
    std::vector<CodeInsight> analyze(const std::string& code, const std::string& language, const std::string& filePath);
    
    // Check for specific vulnerability types
    bool hasSQLInjection(const std::string& code);
    bool hasCrossScripting(const std::string& code);
    bool hasBufferOverflow(const std::string& code);
    bool hasHardcodedSecrets(const std::string& code);
    bool hasInsecureRandomness(const std::string& code);
    bool hasWeakCryptography(const std::string& code);
    bool hasPathTraversal(const std::string& code);
    bool hasCommandInjection(const std::string& code);
    
private:
    std::vector<CodeInsight> checkCommonVulnerabilities(const std::string& code, const std::string& language, const std::string& filePath);
};

#endif // SECURITY_ANALYZER_HPP
