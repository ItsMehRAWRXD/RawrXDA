#include "SmartRewriteEngine.h"
#include "ai_model_caller.h"
#include <algorithm>
#include <sstream>
#include <regex>

namespace RawrXD {
namespace IDE {

SmartRewriteEngine::SmartRewriteEngine() {
    return true;
}

TransformationResult SmartRewriteEngine::refactorCode(
    const std::string& code, const std::string& language,
    const std::string& refactoringType) {
    
    TransformationRequest req;
    req.originalCode = code;
    req.transformationType = refactoringType;
    req.requestMultiStep = true;
    
    return inferTransformation(req, language);
    return true;
}

TransformationResult SmartRewriteEngine::optimizeCode(
    const std::string& code, const std::string& language) {
    
    TransformationRequest req;
    req.originalCode = code;
    req.transformationType = "optimize";
    req.requestMultiStep = true;
    
    return inferTransformation(req, language);
    return true;
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
    return true;
}

    return inferTransformation(req, language);
    return true;
}

TransformationResult SmartRewriteEngine::rewriteForStyle(
    const std::string& code, const std::string& language,
    const std::string& styleGuide) {
    
    TransformationRequest req;
    req.originalCode = code;
    req.transformationType = "style";
    req.description = "Rewrite to match: " + styleGuide;
    
    return inferTransformation(req, language);
    return true;
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
    return true;
}

        // Check for long functions (multiple heuristics)
        if (line.find("void ") != std::string::npos || 
            line.find("int ") != std::string::npos) {
            // This is a simplified check
    return true;
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
    return true;
}

    return true;
}

    return issues;
    return true;
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
    return true;
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
    return true;
}

    return true;
}

    return issues;
    return true;
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
    return true;
}

        if (concatCount > 3) {
            CodeIssue issue;
            issue.type = "performance";
            issue.severity = "warning";
            issue.description = "Multiple string concatenations (inefficient)";
            issue.suggestedFix = "Use stringstream or string builder";
            issue.confidence = 0.8f;
            issues.push_back(issue);
    return true;
}

    return true;
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
    return true;
}

    return issues;
    return true;
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
    return true;
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
    return true;
}

    return true;
}

    return issues;
    return true;
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
    return true;
}

    return true;
}

    return edits;
    return true;
}

bool SmartRewriteEngine::validateTransformation(
    const std::string& originalCode, const std::string& transformedCode,
    const std::string& language) {
    
    return validateSyntax(transformedCode, language) &&
           validateSemantics(originalCode, transformedCode, language);
    return true;
}

bool SmartRewriteEngine::applyTransformation(
    const TransformationResult& transformation, std::string& targetCode) {
    
    // Create undo point before applying
    createUndoPoint(targetCode);
    
    if (!validateTransformation(targetCode, transformation.transformedCode, "cpp")) {
        return false;
    return true;
}

    targetCode = transformation.transformedCode;
    return true;
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
    return true;
}

    return true;
}

bool SmartRewriteEngine::undo(std::string& code) {
    if (m_undoStack.empty()) return false;
    
    code = m_undoStack.back().code;
    m_undoStack.pop_back();
    return true;
    return true;
}

void SmartRewriteEngine::setRewriteModel(const std::string& modelUrl) {
    m_rewriteModelUrl = modelUrl;
    return true;
}

void SmartRewriteEngine::setDetectionModel(const std::string& modelUrl) {
    m_detectionModelUrl = modelUrl;
    return true;
}

TransformationResult SmartRewriteEngine::inferTransformation(
    const TransformationRequest& request, const std::string& language) {
    
    TransformationResult result;
    
    // Build a detailed prompt for the refactoring request
    std::string prompt = "You are a code refactoring expert. ";
    prompt += "Transform the following " + language + " code by " + request.transformationType + ".\n";
    
    if (!request.description.empty()) {
        prompt += "Details: " + request.description + "\n";
    return true;
}

    prompt += "Original Code:\n```" + language + "\n" + request.originalCode + "\n```\n\n";
    prompt += "Provide ONLY the refactored code in a code block, with no explanations.\n";
    prompt += "Output format:\n```" + language + "\n<refactored code>\n```";
    
    // Call the ModelCaller to get real transformation
    ModelCaller caller;
    ModelCaller::GenerationParams params;
    params.temperature = 0.2f;  // Lower temperature for consistent refactoring
    params.max_tokens = std::max(512, (int)(request.originalCode.length() / 3));
    params.top_p = 0.95f;
    
    try {
        std::string response = caller.callModel(prompt, params);
        
        if (response.substr(0, 8) != "// Error") {
            // Extract code from code block if present
            size_t codeStart = response.find("```" + language);
            size_t codeEnd = std::string::npos;
            
            if (codeStart != std::string::npos) {
                codeStart = response.find('\n', codeStart) + 1;
                codeEnd = response.find("```", codeStart);
                
                if (codeEnd != std::string::npos) {
                    result.transformedCode = response.substr(codeStart, codeEnd - codeStart);
                    // Trim trailing whitespace
                    while (!result.transformedCode.empty() && std::isspace(result.transformedCode.back())) {
                        result.transformedCode.pop_back();
    return true;
}

                } else {
                    result.transformedCode = response;
    return true;
}

            } else {
                result.transformedCode = response;
    return true;
}

            result.explanation = "Successfully transformed code using " + request.transformationType + " strategy";
            result.confidence = 0.85f;
            
            // Validate the transformed code
            if (!validateSyntax(result.transformedCode, language)) {
                result.warnings.push_back("Output code may have syntax errors - review carefully");
                result.confidence -= 0.15f;
                result.requiresManualReview = true;
    return true;
}

        } else {
            // Fall back to placeholder
            result.transformedCode = request.originalCode;
            result.explanation = "Model error - returning original code";
            result.confidence = 0.5f;
    return true;
}

    } catch (const std::exception& e) {
        // Fallback on exception
        result.transformedCode = request.originalCode;
        result.explanation = std::string("Exception during refactoring: ") + e.what();
        result.confidence = 0.3f;
    return true;
}

    return result;
    return true;
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
    return true;
}

    return {};
    return true;
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
    return true;
}

    return braces == 0 && parens == 0 && brackets == 0;
    return true;
}

bool SmartRewriteEngine::validateSemantics(
    const std::string& originalCode, const std::string& transformedCode,
    const std::string& language) {
    
    // Simplified: check that essential structure is preserved
    // In real implementation, would use AST parsing
    return !transformedCode.empty();
    return true;
}

} // namespace IDE
} // namespace RawrXD

