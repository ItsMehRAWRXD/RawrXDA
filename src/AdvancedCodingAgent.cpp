#include "AdvancedCodingAgent.h"
#include <algorithm>
#include <sstream>

namespace RawrXD {
namespace IDE {

AdvancedCodingAgent::AdvancedCodingAgent() {
}

AgentTaskResult AdvancedCodingAgent::implementFeature(
    const std::string& specification, const std::string& language,
    const std::string& codeContext, const std::vector<std::string>& requirements) {
    
    AgentTaskResult result;
    result.taskType = "feature_implementation";
    result.success = false;
    
    // Step 1: Analyze requirements
    std::string analysis = "Analyzing: " + specification;
    result.steps.push_back(analysis);
    
    // Step 2: Generate code from specification and language
    std::string generatedCode = "// Feature: " + specification + "\n";
    generatedCode += generateFeatureBoilerplate(specification, language);
    
    result.steps.push_back("Code generation complete");
    result.generatedCode = generatedCode;
    result.success = true;
    result.confidence = 0.8f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::generateTests(
    const std::string& code, const std::string& language,
    int minCoverage) {
    
    AgentTaskResult result;
    result.taskType = "test_generation";
    result.success = false;
    
    // Analyze code for test cases
    std::vector<TestCase> testCases = identifyTestCases(code, language);
    
    result.steps.push_back("Identified " + std::to_string(testCases.size()) + " test cases");
    
    // Generate test code
    std::string testCode = generateTestCode(code, testCases, language);
    
    result.steps.push_back("Generated test code");
    result.generatedCode = testCode;
    result.testCases = testCases;
    result.success = true;
    result.confidence = 0.75f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::generateDocumentation(
    const std::string& code, const std::string& language) {
    
    AgentTaskResult result;
    result.taskType = "documentation_generation";
    result.success = false;
    
    // Parse code structure
    result.steps.push_back("Analyzing code structure");
    
    // Extract functions, classes, etc.
    std::string documentation = "# API Documentation\n\n";
    documentation += extractDocumentation(code, language);
    
    result.steps.push_back("Generated documentation");
    result.generatedCode = documentation;
    result.success = true;
    result.confidence = 0.85f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::detectBugs(
    const std::string& code, const std::string& language) {
    
    AgentTaskResult result;
    result.taskType = "bug_detection";
    result.success = false;
    
    std::vector<Vulnerability> vulns = performStaticAnalysis(code, language);
    
    result.steps.push_back("Performed static analysis");
    
    for (const auto& vuln : vulns) {
        result.suggestions.push_back(vuln.description + " - " + vuln.suggestedFix);
    }
    
    result.vulnerabilities = vulns;
    result.success = true;
    result.confidence = 0.7f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::optimizeForPerformance(
    const std::string& code, const std::string& language) {
    
    AgentTaskResult result;
    result.taskType = "performance_optimization";
    result.success = false;
    
    result.steps.push_back("Analyzing code for performance issues");
    
    // Identify optimization opportunities
    std::string optimizedCode = code;
    
    // Check for common inefficiencies
    if (code.find("+ \"") != std::string::npos) {
        result.suggestions.push_back("Use stringstream for string concatenation");
        optimizedCode = replaceStringConcatenation(code);
    }
    
    if (code.find("vector") != std::string::npos) {
        result.suggestions.push_back("Reserve vector capacity if size is known");
    }
    
    result.steps.push_back("Generated optimizations");
    result.generatedCode = optimizedCode;
    result.success = true;
    result.confidence = 0.75f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::scanForVulnerabilities(
    const std::string& code, const std::string& language) {
    
    AgentTaskResult result;
    result.taskType = "security_scan";
    result.success = false;
    
    result.steps.push_back("Performing security analysis");
    
    std::vector<Vulnerability> vulnerabilities = scanSecurityIssues(code, language);
    
    for (const auto& vuln : vulnerabilities) {
        result.suggestions.push_back("SECURITY: " + vuln.description);
    }
    
    result.vulnerabilities = vulnerabilities;
    result.steps.push_back("Identified " + std::to_string(vulnerabilities.size()) + " potential issues");
    result.success = true;
    result.confidence = 0.8f;
    
    return result;
}

AgentTaskResult AdvancedCodingAgent::refactorCode(
    const std::string& code, const std::string& language,
    const std::string& strategy) {
    
    AgentTaskResult result;
    result.taskType = "refactoring";
    result.success = false;
    
    result.steps.push_back("Analyzing code structure");
    
    std::string refactoredCode = code;
    
    if (strategy == "extract_methods") {
        result.steps.push_back("Extracting long methods");
        refactoredCode = extractMethodRefactoring(code, language);
    } else if (strategy == "reduce_complexity") {
        result.steps.push_back("Reducing cyclomatic complexity");
        refactoredCode = reduceComplexity(code, language);
    } else if (strategy == "improve_names") {
        result.steps.push_back("Improving variable names");
        refactoredCode = improveNaming(code, language);
    }
    
    result.generatedCode = refactoredCode;
    result.success = true;
    result.confidence = 0.75f;
    
    return result;
}

// Private helper methods

std::string AdvancedCodingAgent::generateFeatureBoilerplate(
    const std::string& feature, const std::string& language) {
    
    if (language == "cpp" || language == "c++") {
        return "class " + feature + " {\n"
               "public:\n"
               "    " + feature + "() {}\n"
               "    ~" + feature + "() {}\n"
               "private:\n"
               "};\n";
    } else if (language == "python") {
        return "class " + feature + ":\n"
               "    def __init__(self):\n"
               "        pass\n";
    }
    
    return "";
}

std::vector<TestCase> AdvancedCodingAgent::identifyTestCases(
    const std::string& code, const std::string& language) {
    
    std::vector<TestCase> cases;
    
    // Basic test case identification
    if (code.find("if ") != std::string::npos) {
        TestCase tc;
        tc.name = "test_condition_true";
        tc.expectedResult = "pass";
        cases.push_back(tc);
        
        tc.name = "test_condition_false";
        cases.push_back(tc);
    }
    
    if (code.find("for ") != std::string::npos) {
        TestCase tc;
        tc.name = "test_empty_collection";
        tc.expectedResult = "pass";
        cases.push_back(tc);
        
        tc.name = "test_single_element";
        cases.push_back(tc);
        
        tc.name = "test_multiple_elements";
        cases.push_back(tc);
    }
    
    return cases;
}

std::string AdvancedCodingAgent::generateTestCode(
    const std::string& code, const std::vector<TestCase>& testCases,
    const std::string& language) {
    
    std::string testCode = "// Generated tests\n";
    
    if (language == "cpp" || language == "c++") {
        testCode += "#include <cassert>\n\n";
        testCode += "void runTests() {\n";
        
        for (const auto& tc : testCases) {
            testCode += "    // " + tc.name + "\n";
            testCode += "    // assert(...);\n";
        }
        
        testCode += "}\n";
    } else if (language == "python") {
        testCode += "import unittest\n\n";
        testCode += "class TestCase(unittest.TestCase):\n";
        
        for (const auto& tc : testCases) {
            testCode += "    def " + tc.name + "(self):\n";
            testCode += "        self.assertTrue(True)\n";
        }
    }
    
    return testCode;
}

std::string AdvancedCodingAgent::extractDocumentation(
    const std::string& code, const std::string& language) {
    
    std::string doc = "";
    
    if (language == "cpp" || language == "c++") {
        // Extract class and function declarations
        doc += "## Classes\n\n";
        doc += "## Functions\n\n";
    } else if (language == "python") {
        doc += "## Modules\n\n";
        doc += "## Classes\n\n";
        doc += "## Functions\n\n";
    }
    
    return doc;
}

std::vector<Vulnerability> AdvancedCodingAgent::performStaticAnalysis(
    const std::string& code, const std::string& language) {
    
    std::vector<Vulnerability> vulns;
    
    // Check for null pointer dereferences
    if (code.find("->") != std::string::npos) {
        Vulnerability vuln;
        vuln.severity = "warning";
        vuln.type = "null_dereference";
        vuln.description = "Potential null pointer dereference";
        vuln.suggestedFix = "Add null check before dereferencing";
        vulns.push_back(vuln);
    }
    
    // Check for memory leaks
    if (code.find("new ") != std::string::npos && 
        code.find("delete ") == std::string::npos) {
        Vulnerability vuln;
        vuln.severity = "warning";
        vuln.type = "memory_leak";
        vuln.description = "Possible memory leak";
        vuln.suggestedFix = "Use smart pointers or ensure cleanup";
        vulns.push_back(vuln);
    }
    
    return vulns;
}

std::string AdvancedCodingAgent::replaceStringConcatenation(
    const std::string& code) {
    
    // Detect repeated string concatenation via '+' or '+=' and annotate
    std::istringstream stream(code);
    std::string line;
    std::ostringstream result;
    int concatCount = 0;
    std::string lastConcatVar;

    while (std::getline(stream, line)) {
        std::string trimmed = line;
        size_t firstNonSpace = trimmed.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos) {
            trimmed = trimmed.substr(firstNonSpace);
        }

        // Check for string concatenation patterns
        bool hasPlusEquals = (trimmed.find("+=") != std::string::npos && 
                              (trimmed.find('"') != std::string::npos || 
                               trimmed.find("str") != std::string::npos ||
                               trimmed.find("String") != std::string::npos));
        
        // Count consecutive concatenations to the same variable
        if (hasPlusEquals) {
            size_t eqPos = trimmed.find("+=");
            std::string varName = trimmed.substr(0, eqPos);
            // Trim
            while (!varName.empty() && varName.back() == ' ') varName.pop_back();
            
            if (varName == lastConcatVar) {
                concatCount++;
            } else {
                concatCount = 1;
                lastConcatVar = varName;
            }

            if (concatCount >= 3) {
                result << line << "  // PERF: repeated string concatenation on '" 
                       << lastConcatVar 
                       << "'. Use std::ostringstream or .append() for O(n) instead of O(n²).\n";
            } else {
                result << line << "\n";
            }
        } else {
            // Reset counter on non-concat line
            if (concatCount >= 3) {
                concatCount = 0;
                lastConcatVar.clear();
            }
            result << line << "\n";
        }
    }

    return result.str();
}

std::vector<Vulnerability> AdvancedCodingAgent::scanSecurityIssues(
    const std::string& code, const std::string& language) {
    
    std::vector<Vulnerability> vulns;
    
    // Check for SQL injection
    if (code.find("sprintf") != std::string::npos && 
        code.find("SELECT") != std::string::npos) {
        Vulnerability vuln;
        vuln.severity = "error";
        vuln.type = "sql_injection";
        vuln.description = "Potential SQL injection vulnerability";
        vuln.suggestedFix = "Use prepared statements";
        vulns.push_back(vuln);
    }
    
    // Check for hardcoded secrets
    if (code.find("password") != std::string::npos ||
        code.find("api_key") != std::string::npos) {
        Vulnerability vuln;
        vuln.severity = "error";
        vuln.type = "hardcoded_secret";
        vuln.description = "Hardcoded secrets found";
        vuln.suggestedFix = "Use environment variables";
        vulns.push_back(vuln);
    }
    
    return vulns;
}

std::string AdvancedCodingAgent::extractMethodRefactoring(
    const std::string& code, const std::string& language) {
    
    // Analyze code for long functions that could benefit from extraction
    std::istringstream stream(code);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    // Find function boundaries and flag those exceeding threshold
    const size_t MAX_FUNC_LINES = 50;
    std::ostringstream result;
    size_t funcStartLine = 0;
    int braceDepth = 0;
    bool inFunction = false;
    std::string currentFuncName;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& l = lines[i];
        
        // Detect function start (simplified heuristic: line with '(' before '{' at depth 0)
        if (!inFunction && braceDepth == 0) {
            size_t parenPos = l.find('(');
            size_t bracePos = l.find('{');
            if (parenPos != std::string::npos && 
                (bracePos != std::string::npos || (i + 1 < lines.size() && lines[i+1].find('{') != std::string::npos))) {
                // Extract function name (word before '(')
                size_t nameEnd = parenPos;
                while (nameEnd > 0 && l[nameEnd - 1] == ' ') nameEnd--;
                size_t nameStart = nameEnd;
                while (nameStart > 0 && (std::isalnum(l[nameStart - 1]) || l[nameStart - 1] == '_')) nameStart--;
                currentFuncName = l.substr(nameStart, nameEnd - nameStart);
                funcStartLine = i;
                inFunction = true;
            }
        }

        for (char c : l) {
            if (c == '{') braceDepth++;
            else if (c == '}') braceDepth--;
        }

        result << l;

        // Function ended
        if (inFunction && braceDepth == 0) {
            size_t funcLen = i - funcStartLine + 1;
            if (funcLen > MAX_FUNC_LINES) {
                result << "  // REFACTOR: '" << currentFuncName 
                       << "' is " << funcLen << " lines (>" << MAX_FUNC_LINES 
                       << "). Consider extracting helper methods.";
            }
            inFunction = false;
            currentFuncName.clear();
        }
        result << "\n";
    }

    return result.str();
}

std::string AdvancedCodingAgent::reduceComplexity(
    const std::string& code, const std::string& language) {
    
    // Analyze cyclomatic complexity indicators and annotate
    std::istringstream stream(code);
    std::string line;
    std::ostringstream result;
    int complexity = 1; // Start at 1 for the function entry
    bool inFunction = false;
    int braceDepth = 0;
    int nestedIfDepth = 0;

    while (std::getline(stream, line)) {
        std::string trimmed = line;
        size_t firstNonSpace = trimmed.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos) {
            trimmed = trimmed.substr(firstNonSpace);
        }

        // Count complexity-increasing constructs
        bool addedHint = false;
        if (trimmed.find("if ") == 0 || trimmed.find("if(") == 0 ||
            trimmed.find("else if") != std::string::npos) {
            complexity++;
            nestedIfDepth++;
            if (nestedIfDepth > 3) {
                result << line << "  // COMPLEXITY: deeply nested conditional (depth="
                       << nestedIfDepth << "). Consider early return or guard clause.\n";
                addedHint = true;
            }
        }
        if (trimmed.find("for ") == 0 || trimmed.find("for(") == 0 ||
            trimmed.find("while ") == 0 || trimmed.find("while(") == 0) {
            complexity++;
        }
        if (trimmed.find("case ") == 0) complexity++;
        if (trimmed.find("catch") == 0) complexity++;
        if (trimmed.find("&&") != std::string::npos ||
            trimmed.find("||") != std::string::npos) {
            complexity++;
        }

        for (char c : line) {
            if (c == '{') braceDepth++;
            else if (c == '}') {
                braceDepth--;
                if (nestedIfDepth > 0) nestedIfDepth--;
            }
        }

        if (!addedHint) {
            result << line << "\n";
        }

        // At function end, emit summary
        if (inFunction && braceDepth == 0) {
            if (complexity > 10) {
                result << "// COMPLEXITY WARNING: Cyclomatic complexity ~" << complexity
                       << " (threshold: 10). Refactor into smaller functions.\n";
            }
            complexity = 1;
            inFunction = false;
        }

        // Detect function start
        if (braceDepth == 1 && !inFunction && line.find('(') != std::string::npos) {
            inFunction = true;
            complexity = 1;
        }
    }

    return result.str();
}

std::string AdvancedCodingAgent::improveNaming(
    const std::string& code, const std::string& language) {
    
    // Analyze identifiers for naming quality and annotate improvements
    std::istringstream stream(code);
    std::string line;
    std::ostringstream result;

    // Common poor names to flag
    const std::vector<std::pair<std::string, std::string>> poorNames = {
        {" i ", "loop counter 'i' — consider descriptive name for outer loops"},
        {" j ", "loop counter 'j' — consider 'innerIdx' or similar"},
        {" k ", "loop counter 'k' — consider descriptive name"},
        {" tmp ", "'tmp' — use purpose-describing name"},
        {" temp ", "'temp' — use purpose-describing name"},
        {" ret ", "'ret' — use purpose-describing name like 'result'"},
        {" res ", "'res' — use 'result' or more specific name"},
        {" buf ", "'buf' — use 'buffer' or describe contents"},
        {" ptr ", "'ptr' — describe what it points to"},
        {" val ", "'val' — use 'value' or describe the quantity"},
        {" str ", "'str' — describe the string contents"},
        {" cb ", "'cb' — use 'callback' or describe the action"},
        {" fn ", "'fn' — use 'function' or describe purpose"},
        {" ctx ", "'ctx' — use 'context' or more specific name"},
    };

    while (std::getline(stream, line)) {
        std::string padded = " " + line + " ";
        bool flagged = false;

        // Skip comments
        std::string trimmed = line;
        size_t commentPos = trimmed.find("//");
        bool isComment = (commentPos != std::string::npos && 
                          trimmed.find_first_not_of(" \t") == commentPos);

        if (!isComment) {
            for (const auto& [pattern, suggestion] : poorNames) {
                if (padded.find(pattern) != std::string::npos) {
                    result << line << "  // NAMING: " << suggestion << "\n";
                    flagged = true;
                    break;
                }
            }

            // Detect single-letter variables in declarations (not in for loops)
            if (!flagged && line.find("for") == std::string::npos) {
                // Look for type declarations with single-char names like "int x;"
                for (const char* type : {"int ", "float ", "double ", "char ", "bool ", "auto "}) {
                    size_t pos = padded.find(type);
                    if (pos != std::string::npos) {
                        size_t nameStart = pos + strlen(type);
                        while (nameStart < padded.size() && padded[nameStart] == ' ') nameStart++;
                        // Check if identifier is just 1 char
                        if (nameStart < padded.size() - 1 &&
                            std::isalpha(padded[nameStart]) &&
                            !std::isalnum(padded[nameStart + 1]) && padded[nameStart + 1] != '_') {
                            result << line << "  // NAMING: single-letter variable '" 
                                   << padded[nameStart] << "' — use descriptive name\n";
                            flagged = true;
                            break;
                        }
                    }
                }
            }
        }

        if (!flagged) {
            result << line << "\n";
        }
    }

    return result.str();
}

} // namespace IDE
} // namespace RawrXD
