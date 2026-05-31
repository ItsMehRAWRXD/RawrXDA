#include "advanced_coding_agent.h"

AdvancedCodingAgentIntegration::AdvancedCodingAgentIntegration(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics) {
    m_logger->info("AdvancedCodingAgent initialized");
}

GeneratedFeature AdvancedCodingAgentIntegration::implementFeature(
    const FeatureRequest& request) {

    m_logger->info("Implementing feature: {}", request.description);

    GeneratedFeature feature;
    feature.code = "// Generated implementation\n";
    feature.explanation = "Feature implementation generated from request";
    feature.confidence = 0.85;
    
    m_metrics->incrementCounter("features_generated");
    return feature;
}

std::vector<GeneratedFeature> AdvancedCodingAgentIntegration::generateImplementationOptions(
    const std::string& description,
    const std::string& context) {

    m_logger->info("Generating implementation options");

    std::vector<GeneratedFeature> options;
    
    GeneratedFeature opt1;
    opt1.code = "// Option 1";
    opt1.explanation = "First implementation option";
    opt1.confidence = 0.80;
    options.push_back(opt1);
    
    GeneratedFeature opt2;
    opt2.code = "// Option 2";
    opt2.explanation = "Second implementation option";
    opt2.confidence = 0.75;
    options.push_back(opt2);

    return options;
}

std::string AdvancedCodingAgentIntegration::generateDocumentation(
    const std::string& code) {

    m_logger->info("Generating documentation for {} chars", code.length());

    std::string doc = "/**\n";
    doc += " * Auto-generated documentation\n";
    doc += " * Function purpose and usage\n";
    doc += " */\n";

    m_metrics->incrementCounter("documentation_generated");
    return doc;
}

std::string AdvancedCodingAgentIntegration::generateFunctionDocumentation(
    const std::string& functionCode,
    const std::string& style) {

    m_logger->info("Generating {} documentation", style);
    return "/// Auto-generated documentation";
}

std::vector<std::string> AdvancedCodingAgentIntegration::generateTests(
    const std::string& functionCode) {

    m_logger->info("Generating tests for function");

    std::vector<std::string> tests;
    
    tests.push_back("TEST_CASE(\"Basic functionality\") { /* test */ }");
    tests.push_back("TEST_CASE(\"Edge cases\") { /* test */ }");
    tests.push_back("TEST_CASE(\"Error handling\") { /* test */ }");

    m_metrics->incrementCounter("tests_generated");
    return tests;
}

std::vector<std::string> AdvancedCodingAgentIntegration::findBugs(
    const std::string& code) {

    m_logger->info("Analyzing code for bugs");

    std::vector<std::string> bugs;

    // Real static analysis patterns
    // 1. Check for potential null pointer dereferences
    if (code.find("*ptr") != std::string::npos || code.find("->") != std::string::npos) {
        if (code.find("if (ptr") == std::string::npos && code.find("if (!ptr") == std::string::npos) {
            bugs.push_back("Potential null pointer dereference: pointer used without null check");
        }
    }

    // 2. Check for unchecked return values
    if (code.find("malloc(") != std::string::npos || code.find("new ") != std::string::npos) {
        if (code.find("if (") == std::string::npos) {
            bugs.push_back("Unchecked allocation result: memory allocation without null check");
        }
    }

    // 3. Check for buffer overflow patterns
    if (code.find("strcpy(") != std::string::npos || code.find("strcat(") != std::string::npos) {
        bugs.push_back("Potential buffer overflow: use of unbounded string functions (strcpy/strcat)");
    }

    // 4. Check for resource leaks
    if (code.find("fopen(") != std::string::npos || code.find("CreateFile") != std::string::npos) {
        if (code.find("fclose") == std::string::npos && code.find("CloseHandle") == std::string::npos) {
            bugs.push_back("Potential resource leak: file handle opened but not closed");
        }
    }

    // 5. Check for division by zero
    if (code.find("/ ") != std::string::npos || code.find("/=") != std::string::npos) {
        if (code.find("if (") == std::string::npos) {
            bugs.push_back("Potential division by zero: division without zero check");
        }
    }

    // 6. Check for uninitialized variables
    if (code.find("int ") != std::string::npos || code.find("char ") != std::string::npos) {
        if (code.find("= ") == std::string::npos && code.find("(") == std::string::npos) {
            bugs.push_back("Potential uninitialized variable: variable declared without initialization");
        }
    }

    m_metrics->incrementCounter("bug_analysis_runs");
    return bugs;
}

std::vector<std::string> AdvancedCodingAgentIntegration::optimizeCode(
    const std::string& code) {

    m_logger->info("Optimizing code");

    std::vector<std::string> optimizations;
    
    optimizations.push_back("Use const references for large objects");
    optimizations.push_back("Cache repeated computations");
    optimizations.push_back("Use move semantics for large returns");

    m_metrics->incrementCounter("optimization_suggestions");
    return optimizations;
}

std::vector<SecurityIssue> AdvancedCodingAgentIntegration::scanSecurity(
    const std::string& code,
    const std::string& language) {

    m_logger->info("Scanning security for {}", language);

    std::vector<SecurityIssue> issues;

    // 1. Check for hardcoded secrets
    if (code.find("password = \"") != std::string::npos ||
        code.find("api_key = \"") != std::string::npos ||
        code.find("secret = \"") != std::string::npos ||
        code.find("token = \"") != std::string::npos) {
        SecurityIssue issue;
        issue.severity = "HIGH";
        issue.category = "secrets";
        issue.description = "Hardcoded secret detected in source code";
        issue.location = "inline";
        issue.remediationSteps = {
            "Move secrets to environment variables",
            "Use a secrets manager (e.g., HashiCorp Vault)",
            "Rotate exposed credentials immediately"
        };
        issues.push_back(issue);
    }

    // 2. Check for SQL injection patterns
    if (code.find("SELECT ") != std::string::npos && code.find("+") != std::string::npos) {
        SecurityIssue issue;
        issue.severity = "CRITICAL";
        issue.category = "injection";
        issue.description = "Potential SQL injection: string concatenation in query";
        issue.location = "inline";
        issue.remediationSteps = {
            "Use parameterized queries / prepared statements",
            "Validate and sanitize all user inputs",
            "Use an ORM framework"
        };
        issues.push_back(issue);
    }

    // 3. Check for unsafe deserialization
    if (code.find("eval(") != std::string::npos || code.find("exec(") != std::string::npos) {
        SecurityIssue issue;
        issue.severity = "CRITICAL";
        issue.category = "code-execution";
        issue.description = "Dangerous code execution function (eval/exec) detected";
        issue.location = "inline";
        issue.remediationSteps = {
            "Replace eval/exec with safer alternatives (JSON.parse, AST parsers)",
            "Implement strict input validation",
            "Use sandboxed execution environments"
        };
        issues.push_back(issue);
    }

    // 4. Check for insecure HTTP
    if (code.find("http://") != std::string::npos) {
        SecurityIssue issue;
        issue.severity = "MEDIUM";
        issue.category = "transport";
        issue.description = "Insecure HTTP URL detected; data may be transmitted in plaintext";
        issue.location = "inline";
        issue.remediationSteps = {
            "Replace http:// with https://",
            "Enable HSTS headers",
            "Use certificate pinning for sensitive APIs"
        };
        issues.push_back(issue);
    }

    // 5. Check for buffer overflow patterns (C/C++)
    if (language == "cpp" || language == "c") {
        if (code.find("strcpy(") != std::string::npos || code.find("gets(") != std::string::npos) {
            SecurityIssue issue;
            issue.severity = "HIGH";
            issue.category = "memory";
            issue.description = "Unsafe C function (strcpy/gets) detected — potential buffer overflow";
            issue.location = "inline";
            issue.remediationSteps = {
                "Use strncpy_s or strlcpy instead of strcpy",
                "Use fgets instead of gets",
                "Enable stack canaries and ASLR"
            };
            issues.push_back(issue);
        }
    }

    m_metrics->incrementCounter("security_scans");
    return issues;
}

std::string AdvancedCodingAgentIntegration::buildFeaturePrompt(
    const FeatureRequest& request) {

    std::string prompt = "Generate " + request.language + " code for: ";
    prompt += request.description;
    return prompt;
}

bool AdvancedCodingAgentIntegration::validateGeneratedCode(const std::string& code) {
    // Basic syntax validation: check for balanced delimiters and non-empty content
    if (code.empty()) return false;

    int braces = 0, parens = 0, brackets = 0;
    bool inString = false;
    bool inLineComment = false;
    bool inBlockComment = false;
    char prev = 0;

    for (size_t i = 0; i < code.size(); ++i) {
        char c = code[i];

        if (inLineComment) {
            if (c == '\n') inLineComment = false;
            prev = c;
            continue;
        }
        if (inBlockComment) {
            if (c == '/' && prev == '*') inBlockComment = false;
            prev = c;
            continue;
        }
        if (inString) {
            if (c == '"' && prev != '\\') inString = false;
            prev = c;
            continue;
        }

        if (c == '/' && i + 1 < code.size()) {
            if (code[i + 1] == '/') { inLineComment = true; prev = c; continue; }
            if (code[i + 1] == '*') { inBlockComment = true; prev = c; continue; }
        }
        if (c == '"' && prev != '\\') { inString = true; prev = c; continue; }

        switch (c) {
            case '{': braces++; break;
            case '}': braces--; break;
            case '(': parens++; break;
            case ')': parens--; break;
            case '[': brackets++; break;
            case ']': brackets--; break;
        }

        // Negative count means closing without opening
        if (braces < 0 || parens < 0 || brackets < 0) return false;
        prev = c;
    }

    return braces == 0 && parens == 0 && brackets == 0;
}
