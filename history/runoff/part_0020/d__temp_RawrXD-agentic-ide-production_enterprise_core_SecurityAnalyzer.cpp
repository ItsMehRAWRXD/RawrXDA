#include "SecurityAnalyzer.hpp"
#include "CodeAnalysisUtils.hpp"
#include <regex>

SecurityAnalyzer::SecurityAnalyzer() {}

std::vector<CodeInsight> SecurityAnalyzer::analyze(const std::string& code, const std::string& language, const std::string& filePath) {
    return checkCommonVulnerabilities(code, language, filePath);
}

std::vector<CodeInsight> SecurityAnalyzer::checkCommonVulnerabilities(const std::string& code, const std::string& language, const std::string& filePath) {
    std::vector<CodeInsight> insights;
    
    // SQL Injection checks
    if (hasSQLInjection(code)) {
        CodeInsight insight;
        insight.type = "security";
        insight.severity = "critical";
        insight.description = "Potential SQL injection vulnerability detected";
        insight.suggestion = "Use parameterized queries or prepared statements";
        insight.filePath = filePath;
        insight.confidenceScore = 0.85;
        insights.push_back(insight);
    }
    
    // XSS checks
    if (hasCrossScripting(code)) {
        CodeInsight insight;
        insight.type = "security";
        insight.severity = "critical";
        insight.description = "Potential Cross-Site Scripting (XSS) vulnerability";
        insight.suggestion = "Sanitize and escape user input before rendering";
        insight.filePath = filePath;
        insight.confidenceScore = 0.80;
        insights.push_back(insight);
    }
    
    // Buffer overflow checks
    if (hasBufferOverflow(code)) {
        CodeInsight insight;
        insight.type = "security";
        insight.severity = "critical";
        insight.description = "Potential buffer overflow vulnerability";
        insight.suggestion = "Use bounds checking or safer string functions";
        insight.filePath = filePath;
        insight.confidenceScore = 0.75;
        insights.push_back(insight);
    }
    
    // Hardcoded secrets
    if (hasHardcodedSecrets(code)) {
        CodeInsight insight;
        insight.type = "security";
        insight.severity = "critical";
        insight.description = "Hardcoded secrets or credentials detected";
        insight.suggestion = "Move secrets to environment variables or secure vaults";
        insight.filePath = filePath;
        insight.confidenceScore = 0.90;
        insights.push_back(insight);
    }
    
    // Insecure randomness
    if (hasInsecureRandomness(code)) {
        CodeInsight insight;
        insight.type = "security";
        insight.severity = "high";
        insight.description = "Insecure random number generation detected";
        insight.suggestion = "Use cryptographically secure RNG (e.g., std::random_device, /dev/urandom)";
        insight.filePath = filePath;
        insight.confidenceScore = 0.70;
        insights.push_back(insight);
    }
    
    // Weak cryptography
    if (hasWeakCryptography(code)) {
        CodeInsight insight;
        insight.type = "security";
        insight.severity = "high";
        insight.description = "Weak cryptographic algorithm detected";
        insight.suggestion = "Use strong algorithms like AES-256, SHA-256, or higher";
        insight.filePath = filePath;
        insight.confidenceScore = 0.80;
        insights.push_back(insight);
    }
    
    // Path traversal
    if (hasPathTraversal(code)) {
        CodeInsight insight;
        insight.type = "security";
        insight.severity = "high";
        insight.description = "Potential path traversal vulnerability";
        insight.suggestion = "Validate and sanitize file paths, use whitelisting";
        insight.filePath = filePath;
        insight.confidenceScore = 0.75;
        insights.push_back(insight);
    }
    
    // Command injection
    if (hasCommandInjection(code)) {
        CodeInsight insight;
        insight.type = "security";
        insight.severity = "critical";
        insight.description = "Potential command injection vulnerability";
        insight.suggestion = "Avoid system() calls, use safer APIs or input validation";
        insight.filePath = filePath;
        insight.confidenceScore = 0.85;
        insights.push_back(insight);
    }
    
    return insights;
}

bool SecurityAnalyzer::hasSQLInjection(const std::string& code) {
    // Check for string concatenation in SQL queries
    std::regex sqlPattern(R"((?:SELECT|INSERT|UPDATE|DELETE|DROP|EXECUTE)\s+.*\s*\+\s*\w+)");
    if (std::regex_search(code, sqlPattern)) return true;
    
    // Check for unescaped quotes in database operations
    if (StringUtils::contains(code, "executeQuery") && StringUtils::contains(code, "\"" + StringUtils::toLower(code) + "\"")) {
        return true;
    }
    
    return false;
}

bool SecurityAnalyzer::hasCrossScripting(const std::string& code) {
    // Check for innerHTML/textContent usage with user input
    if (StringUtils::contains(code, "innerHTML") && (StringUtils::contains(code, "request") || StringUtils::contains(code, "input"))) {
        return true;
    }
    if (StringUtils::contains(code, "eval(") || StringUtils::contains(code, "Function(")) {
        return true;
    }
    return false;
}

bool SecurityAnalyzer::hasBufferOverflow(const std::string& code) {
    // Check for strcpy, strcat, sprintf without bounds checking
    std::regex bufferPattern(R"(\b(strcpy|strcat|sprintf|gets|scanf)\s*\()");
    return std::regex_search(code, bufferPattern);
}

bool SecurityAnalyzer::hasHardcodedSecrets(const std::string& code) {
    // Look for common secret patterns
    std::regex secretPattern(R"((?:password|api_key|secret|token|credential)\s*=\s*["\'][\w\-]+["\'])");
    if (std::regex_search(code, secretPattern)) return true;
    
    // Check for AWS keys, API keys patterns
    if (std::regex_search(code, std::regex(R"(AKIA[0-9A-Z]{16})"))) return true;
    if (std::regex_search(code, std::regex(R"(sk_[a-z]{2}_[a-zA-Z0-9]{24,})"))) return true;
    
    return false;
}

bool SecurityAnalyzer::hasInsecureRandomness(const std::string& code) {
    // Check for rand() or Math.random() usage
    std::regex randPattern(R"(\b(rand|Math\.random|random\.random)\s*\()");
    return std::regex_search(code, randPattern);
}

bool SecurityAnalyzer::hasWeakCryptography(const std::string& code) {
    // Check for weak cryptographic algorithms
    std::regex weakCryptoPattern(R"(\b(MD5|SHA1|DES|RC4|RC2|MD4)\b)");
    return std::regex_search(code, weakCryptoPattern);
}

bool SecurityAnalyzer::hasPathTraversal(const std::string& code) {
    // Check for ../ or ..\\ in file operations
    if (StringUtils::contains(code, "../") || StringUtils::contains(code, "..\\")) {
        return true;
    }
    // Check for open() or fopen() with unchecked paths
    if (StringUtils::contains(code, "open(") && (StringUtils::contains(code, "request") || StringUtils::contains(code, "input"))) {
        return true;
    }
    return false;
}

bool SecurityAnalyzer::hasCommandInjection(const std::string& code) {
    // Check for system(), exec(), shell_exec() calls
    std::regex cmdPattern(R"(\b(system|exec|shell_exec|popen|backtick)\s*\()");
    if (std::regex_search(code, cmdPattern)) {
        return true;
    }
    // Check for string concatenation before system calls
    if (StringUtils::contains(code, "system") && StringUtils::contains(code, "+")) {
        return true;
    }
    return false;
}
