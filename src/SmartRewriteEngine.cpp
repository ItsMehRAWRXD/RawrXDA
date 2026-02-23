#include "SmartRewriteEngine.h"
#include <algorithm>
#include <sstream>
#include <regex>

namespace RawrXD {
namespace IDE {

SmartRewriteEngine::SmartRewriteEngine() {
}

TransformationResult SmartRewriteEngine::refactorCode(
    const std::string& code, const std::string& language,
    const std::string& refactoringType) {
    
    TransformationRequest req;
    req.originalCode = code;
    req.transformationType = refactoringType;
    req.requestMultiStep = true;
    
    return inferTransformation(req, language);
}

TransformationResult SmartRewriteEngine::optimizeCode(
    const std::string& code, const std::string& language) {
    
    TransformationRequest req;
    req.originalCode = code;
    req.transformationType = "optimize";
    req.requestMultiStep = true;
    
    return inferTransformation(req, language);
}

TransformationResult SmartRewriteEngine::fixCode(
    const std::string& code, const std::string& language,
    const std::vector<CodeIssue>& issues) {
    
    TransformationRequest req;
    req.originalCode = code;
    req.transformationType = "fix";
    req.requestMultiStep = (issues.size() > 1);
    
    for (const auto& issue : issues) {
        req.description += issue.description + "\n";
    }
    
    return inferTransformation(req, language);
}

TransformationResult SmartRewriteEngine::rewriteForStyle(
    const std::string& code, const std::string& language,
    const std::string& styleGuide) {
    
    TransformationRequest req;
    req.originalCode = code;
    req.transformationType = "style";
    req.description = "Rewrite to match: " + styleGuide;
    
    return inferTransformation(req, language);
}

std::vector<CodeIssue> SmartRewriteEngine::detectCodeSmells(
    const std::string& code, const std::string& language) {
    
    std::vector<CodeIssue> issues;
    
    // Check for long lines
    std::istringstream iss(code);
    std::string line;
    int lineNum = 0;
    
    while (std::getline(iss, line)) {
        lineNum++;
        
        if (line.length() > 120) {
            CodeIssue issue;
            issue.type = "smell";
            issue.severity = "warning";
            issue.description = "Line too long (" + std::to_string(line.length()) + " chars)";
            issue.lineNumber = lineNum;
            issue.suggestedFix = "Break line into multiple lines";
            issue.confidence = 0.9f;
            issues.push_back(issue);
        }
        
        // Check for long functions (multiple heuristics)
        if (line.find("void ") != std::string::npos || 
            line.find("int ") != std::string::npos) {
            // This is a simplified check
        }
        
        // Check for duplicate code patterns
        if (line.find("TODO") != std::string::npos) {
            CodeIssue issue;
            issue.type = "smell";
            issue.severity = "info";
            issue.description = "TODO found - incomplete code";
            issue.lineNumber = lineNum;
            issue.confidence = 0.5f;
            issues.push_back(issue);
        }
    }
    
    return issues;
}

std::vector<CodeIssue> SmartRewriteEngine::detectBugs(
    const std::string& code, const std::string& language) {
    
    std::vector<CodeIssue> issues;
    
    // Check for null pointer dereference patterns
    std::regex nullDerefPattern(R"((\w+)\s*->\s*(\w+))");
    
    std::smatch match;
    std::string::const_iterator searchStart(code.cbegin());
    
    while (std::regex_search(searchStart, code.cend(), match, nullDerefPattern)) {
        CodeIssue issue;
        issue.type = "bug";
        issue.severity = "warning";
        issue.description = "Potential null pointer dereference";
        issue.suggestedFix = "Check for null before dereferencing";
        issue.confidence = 0.7f;
        issues.push_back(issue);
        
        searchStart = match.suffix().first;
    }
    
    // Check for memory leaks (new without delete)
    if (code.find("new ") != std::string::npos) {
        if (code.find("delete ") == std::string::npos) {
            CodeIssue issue;
            issue.type = "bug";
            issue.severity = "warning";
            issue.description = "Possible memory leak (new without delete)";
            issue.suggestedFix = "Use smart pointers or ensure delete is called";
            issue.confidence = 0.6f;
            issues.push_back(issue);
        }
    }
    
    return issues;
}

std::vector<CodeIssue> SmartRewriteEngine::detectPerformanceIssues(
    const std::string& code, const std::string& language) {
    
    std::vector<CodeIssue> issues;
    
    // Check for inefficient string concatenation
    if (code.find("+ \"") != std::string::npos) {
        int concatCount = 0;
        size_t pos = 0;
        while ((pos = code.find("+ \"", pos)) != std::string::npos) {
            concatCount++;
            pos += 3;
        }
        
        if (concatCount > 3) {
            CodeIssue issue;
            issue.type = "performance";
            issue.severity = "warning";
            issue.description = "Multiple string concatenations (inefficient)";
            issue.suggestedFix = "Use stringstream or string builder";
            issue.confidence = 0.8f;
            issues.push_back(issue);
        }
    }
    
    // Check for N+1 query patterns (simplified)
    if (code.find("for (") != std::string::npos && 
        code.find("database") != std::string::npos) {
        CodeIssue issue;
        issue.type = "performance";
        issue.severity = "warning";
        issue.description = "Potential N+1 query pattern";
        issue.suggestedFix = "Consider batch loading or joins";
        issue.confidence = 0.5f;
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<CodeIssue> SmartRewriteEngine::detectSecurityIssues(
    const std::string& code, const std::string& language) {
    
    std::vector<CodeIssue> issues;
    
    // Check for SQL injection patterns
    if (code.find("sprintf") != std::string::npos &&
        code.find("SELECT") != std::string::npos) {
        CodeIssue issue;
        issue.type = "security";
        issue.severity = "error";
        issue.description = "Potential SQL injection (sprintf with SQL)";
        issue.suggestedFix = "Use prepared statements";
        issue.confidence = 0.9f;
        issues.push_back(issue);
    }
    
    // Check for hardcoded passwords/secrets
    if (code.find("password") != std::string::npos ||
        code.find("api_key") != std::string::npos) {
        if (code.find("\"") != std::string::npos) {  // String literal
            CodeIssue issue;
            issue.type = "security";
            issue.severity = "error";
            issue.description = "Potential hardcoded secret/credential";
            issue.suggestedFix = "Use environment variables or secure vault";
            issue.confidence = 0.7f;
            issues.push_back(issue);
        }
    }
    
    return issues;
}

std::vector<std::pair<int, std::string>> SmartRewriteEngine::generateEdits(
    const std::string& originalCode, const std::string& targetCode) {
    
    std::vector<std::pair<int, std::string>> edits;
    
    // Simple line-by-line diff (simplified implementation)
    std::istringstream origStream(originalCode);
    std::istringstream targetStream(targetCode);
    
    std::string origLine, targetLine;
    int lineNum = 0;
    
    while (std::getline(origStream, origLine) && std::getline(targetStream, targetLine)) {
        lineNum++;
        
        if (origLine != targetLine) {
            edits.push_back({lineNum, targetLine});
        }
    }
    
    return edits;
}

bool SmartRewriteEngine::validateTransformation(
    const std::string& originalCode, const std::string& transformedCode,
    const std::string& language) {
    
    return validateSyntax(transformedCode, language) &&
           validateSemantics(originalCode, transformedCode, language);
}

bool SmartRewriteEngine::applyTransformation(
    const TransformationResult& transformation, std::string& targetCode) {
    
    // Create undo point before applying
    createUndoPoint(targetCode);
    
    if (!validateTransformation(targetCode, transformation.transformedCode, "cpp")) {
        return false;
    }
    
    targetCode = transformation.transformedCode;
    return true;
}

void SmartRewriteEngine::createUndoPoint(const std::string& code) {
    UndoPoint point;
    point.code = code;
    point.timestamp = std::chrono::system_clock::now();
    m_undoStack.push_back(point);
    
    // Limit undo stack to 20 items
    if (m_undoStack.size() > 20) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

bool SmartRewriteEngine::undo(std::string& code) {
    if (m_undoStack.empty()) return false;
    
    code = m_undoStack.back().code;
    m_undoStack.pop_back();
    return true;
}

void SmartRewriteEngine::setRewriteModel(const std::string& modelUrl) {
    m_rewriteModelUrl = modelUrl;
}

void SmartRewriteEngine::setDetectionModel(const std::string& modelUrl) {
    m_detectionModelUrl = modelUrl;
}

TransformationResult SmartRewriteEngine::inferTransformation(
    const TransformationRequest& request, const std::string& language) {
    
    TransformationResult result;
    std::string code = request.originalCode;
    result.explanation = "Applied " + request.transformationType;
    result.confidence = 0.85f;
    
    std::string type = request.transformationType;
    
    if (type == "modernize" || type == "MODERNIZE") {
        // C++ modernization transforms
        // Replace NULL with nullptr
        size_t pos = 0;
        while ((pos = code.find("NULL", pos)) != std::string::npos) {
            // Verify it's not part of a larger word
            bool isBoundary = (pos == 0 || !std::isalnum(code[pos-1])) &&
                              (pos + 4 >= code.size() || !std::isalnum(code[pos+4]));
            if (isBoundary) {
                code.replace(pos, 4, "nullptr");
                pos += 7;
            } else {
                pos += 4;
            }
        }
        // Replace typedef with using
        pos = 0;
        while ((pos = code.find("typedef ", pos)) != std::string::npos) {
            size_t lineEnd = code.find(';', pos);
            if (lineEnd != std::string::npos) {
                std::string typeLine = code.substr(pos + 8, lineEnd - pos - 8);
                // Find last word (the alias name)
                size_t lastSpace = typeLine.find_last_of(" \t");
                if (lastSpace != std::string::npos) {
                    std::string alias = typeLine.substr(lastSpace + 1);
                    std::string target = typeLine.substr(0, lastSpace);
                    code.replace(pos, lineEnd - pos, "using " + alias + " = " + target);
                }
            }
            pos++;
        }
        result.explanation = "Modernized C++ code (NULL->nullptr, typedef->using)";
    }
    else if (type == "optimize" || type == "OPTIMIZE") {
        // Add const where possible, use reserve() hints
        // Replace push_back with emplace_back where applicable
        size_t pos = 0;
        while ((pos = code.find(".push_back(", pos)) != std::string::npos) {
            // Check if argument is a constructor call or temporary
            size_t argStart = pos + 11;
            size_t argEnd = code.find(')', argStart);
            if (argEnd != std::string::npos) {
                std::string arg = code.substr(argStart, argEnd - argStart);
                if (arg.find('(') != std::string::npos || arg.find('{') != std::string::npos) {
                    code.replace(pos + 1, 9, "emplace_back(");
                    result.explanation += "; push_back->emplace_back for temporaries";
                }
            }
            pos++;
        }
    }
    else if (type == "document" || type == "DOCUMENT") {
        // Add documentation comments
        std::istringstream iss(code);
        std::string line;
        std::string documented;
        bool prevWasBlank = true;
        while (std::getline(iss, line)) {
            // Detect function/method definitions
            if (prevWasBlank && (line.find('(') != std::string::npos) && 
                (line.find('{') != std::string::npos || line.find(';') == std::string::npos)) {
                size_t nameEnd = line.find('(');
                if (nameEnd != std::string::npos) {
                    size_t nameStart = line.find_last_of(" \t*&", nameEnd - 1);
                    if (nameStart == std::string::npos) nameStart = 0; else nameStart++;
                    std::string funcName = line.substr(nameStart, nameEnd - nameStart);
                    if (!funcName.empty() && funcName != "if" && funcName != "for" && 
                        funcName != "while" && funcName != "switch") {
                        documented += "/**\n * @brief " + funcName + "\n */\n";
                    }
                }
            }
            documented += line + "\n";
            prevWasBlank = line.find_first_not_of(" \t") == std::string::npos;
        }
        code = documented;
        result.explanation = "Added documentation comments to functions";
    }
    else if (type == "security" || type == "SECURITY") {
        // Replace unsafe functions
        size_t pos = 0;
        while ((pos = code.find("strcpy(", pos)) != std::string::npos) {
            code.replace(pos, 6, "strncpy(");
            pos += 7;
        }
        pos = 0;
        while ((pos = code.find("sprintf(", pos)) != std::string::npos) {
            code.replace(pos, 7, "snprintf(");
            pos += 8;
        }
        result.explanation = "Replaced unsafe C functions with bounded alternatives";
    }
    
    result.transformedCode = code;
    if (code == request.originalCode) {
        result.confidence = 0.5f;
        result.explanation += " (no changes needed)";
    }
    
    return result;
}

std::vector<CodeIssue> SmartRewriteEngine::analyzeCode(
    const std::string& code, const std::string& language,
    const std::string& analysisType) {
    
    if (analysisType == "smells") {
        return detectCodeSmells(code, language);
    } else if (analysisType == "bugs") {
        return detectBugs(code, language);
    } else if (analysisType == "performance") {
        return detectPerformanceIssues(code, language);
    } else if (analysisType == "security") {
        return detectSecurityIssues(code, language);
    }
    
    return {};
}

bool SmartRewriteEngine::validateSyntax(
    const std::string& code, const std::string& language) {
    
    // Simplified: check for balanced braces
    int braces = 0, parens = 0, brackets = 0;
    
    for (char c : code) {
        if (c == '{') braces++;
        else if (c == '}') braces--;
        else if (c == '(') parens++;
        else if (c == ')') parens--;
        else if (c == '[') brackets++;
        else if (c == ']') brackets--;
        
        // Negative counts indicate imbalance
        if (braces < 0 || parens < 0 || brackets < 0) return false;
    }
    
    return braces == 0 && parens == 0 && brackets == 0;
}

bool SmartRewriteEngine::validateSemantics(
    const std::string& originalCode, const std::string& transformedCode,
    const std::string& language) {
    
    // Simplified: check that essential structure is preserved
    // In real implementation, would use AST parsing
    return !transformedCode.empty();
}

} // namespace IDE
} // namespace RawrXD
