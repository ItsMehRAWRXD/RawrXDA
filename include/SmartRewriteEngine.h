#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace RawrXD {
namespace IDE {

// Code transformation request
struct TransformationRequest {
    std::string originalCode;
    std::string targetCode;
    std::string transformationType;  // "refactor", "optimize", "fix", "rewrite"
    std::string description;         // What needs to be changed
    bool requestMultiStep;           // Allow multi-step transformations
};

// Code transformation result
struct TransformationResult {
    std::string transformedCode;
    std::vector<std::string> steps;   // Multi-step transformation steps
    std::string explanation;          // Why this transformation
    float confidence;                 // 0.0-1.0 confidence score
    std::vector<std::string> warnings; // Warnings about the transformation
    bool requiresManualReview;        // Needs user approval
};

// Code quality issue
struct CodeIssue {
    std::string type;               // "smell", "bug", "performance", "security"
    std::string severity;           // "info", "warning", "error"
    std::string description;
    int lineNumber;
    int columnNumber;
    std::string suggestedFix;
    float confidence;
};

// Smart code rewriting engine
class SmartRewriteEngine {
public:
    SmartRewriteEngine();
    ~SmartRewriteEngine() = default;

    // Generate refactored code
    TransformationResult refactorCode(
        const std::string& code,
        const std::string& language,
        const std::string& refactoringType
    );

    // Generate optimized code
    TransformationResult optimizeCode(
        const std::string& code,
        const std::string& language
    );

    // Fix detected issues
    TransformationResult fixCode(
        const std::string& code,
        const std::string& language,
        const std::vector<CodeIssue>& issues
    );

    // Rewrite code to match style
    TransformationResult rewriteForStyle(
        const std::string& code,
        const std::string& language,
        const std::string& styleGuide
    );

    // Detect code smells
    std::vector<CodeIssue> detectCodeSmells(
        const std::string& code,
        const std::string& language
    );

    // Detect potential bugs
    std::vector<CodeIssue> detectBugs(
        const std::string& code,
        const std::string& language
    );

    // Detect performance issues
    std::vector<CodeIssue> detectPerformanceIssues(
        const std::string& code,
        const std::string& language
    );

    // Detect security issues
    std::vector<CodeIssue> detectSecurityIssues(
        const std::string& code,
        const std::string& language
    );

    // Generate multi-line edits (diffs)
    std::vector<std::pair<int, std::string>> generateEdits(
        const std::string& originalCode,
        const std::string& targetCode
    );

    // Validate transformation safety
    bool validateTransformation(
        const std::string& originalCode,
        const std::string& transformedCode,
        const std::string& language
    );

    // Apply transformation with undo support
    bool applyTransformation(
        const TransformationResult& transformation,
        std::string& targetCode
    );

    // Create undo point
    void createUndoPoint(const std::string& code);

    // Undo last transformation
    bool undo(std::string& code);

    // Configure models
    void setRewriteModel(const std::string& modelUrl);
    void setDetectionModel(const std::string& modelUrl);

private:
    struct UndoPoint {
        std::string code;
        std::chrono::system_clock::time_point timestamp;
    };

    // Inference
    TransformationResult inferTransformation(
        const TransformationRequest& request,
        const std::string& language
    );

    // Analysis
    std::vector<CodeIssue> analyzeCode(
        const std::string& code,
        const std::string& language,
        const std::string& analysisType
    );

    // Validation
    bool validateSyntax(
        const std::string& code,
        const std::string& language
    );

    bool validateSemantics(
        const std::string& originalCode,
        const std::string& transformedCode,
        const std::string& language
    );

    std::string m_rewriteModelUrl;
    std::string m_detectionModelUrl;
    std::vector<UndoPoint> m_undoStack;
};

} // namespace IDE
} // namespace RawrXD
