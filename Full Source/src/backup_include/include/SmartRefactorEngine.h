/**
 * @file SmartRefactorEngine.h
 * @brief Smart code refactor/rewrite engine (stub for patchable build).
 */
#ifndef SMART_REFACTOR_ENGINE_H
#define SMART_REFACTOR_ENGINE_H

#include <string>
#include <vector>
#include <utility>

struct CodeIssue {
    std::string type;
    std::string severity;
    int lineNumber;
    int columnNumber;
    std::string suggestedFix;
    float confidence;
};

struct TransformationResult {
    std::string transformedCode;
    bool success;
    std::string errorMessage;
    std::vector<CodeIssue> issues;
};

class SmartRefactorEngine {
public:
    SmartRefactorEngine();
    ~SmartRefactorEngine() = default;
    TransformationResult refactorCode(const std::string& code, const std::string& language,
                                      const std::string& refactoringType);
    TransformationResult optimizeCode(const std::string& code, const std::string& language);
    TransformationResult fixCode(const std::string& code, const std::string& language,
                                  const std::vector<CodeIssue>& issues);
    TransformationResult rewriteForStyle(const std::string& code, const std::string& language,
                                         const std::string& styleGuide);
    std::vector<CodeIssue> detectCodeSmells(const std::string& code, const std::string& language);
    std::vector<CodeIssue> detectBugs(const std::string& code, const std::string& language);
    std::vector<CodeIssue> detectPerformanceIssues(const std::string& code, const std::string& language);
    std::vector<CodeIssue> detectSecurityIssues(const std::string& code, const std::string& language);
    std::vector<std::pair<int, std::string>> generateEdits(const std::string& originalCode,
                                                           const std::string& targetCode);
    bool validateTransformation(const std::string& originalCode, const std::string& transformedCode,
                                const std::string& language);
    bool applyTransformation(const TransformationResult& transformation, std::string& targetCode);
};

#endif
