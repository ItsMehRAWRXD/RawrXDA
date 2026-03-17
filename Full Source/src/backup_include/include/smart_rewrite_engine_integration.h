#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "logging/logger.h"
#include "metrics/metrics.h"

enum class RewriteType {
    REFACTOR,
    OPTIMIZE,
    DOCUMENT,
    TEST,
    BUG_FIX,
    MODERNIZE,
    CUSTOM
};

struct RewriteSuggestion {
    std::string originalCode;
    std::string suggestedCode;
    std::string explanation;
    RewriteType type;
    double confidence;
    std::vector<std::string> affectedFiles;
    bool isSafe;
};

struct DiffHunk {
    int startLine;
    int endLine;
    std::vector<std::string> removedLines;
    std::vector<std::string> addedLines;
    std::string context;
};

class SmartRewriteEngineIntegration {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;

public:
    SmartRewriteEngineIntegration(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics
    );

    // Core rewrite interface
    std::vector<RewriteSuggestion> getRewriteSuggestions(
        const std::string& code,
        RewriteType type,
        const std::string& context = ""
    );

    // Advanced rewrite features
    std::vector<RewriteSuggestion> refactorFunction(
        const std::string& functionCode,
        const std::string& goal = ""
    );

    std::vector<RewriteSuggestion> optimizePerformance(
        const std::string& code,
        const std::string& performanceGoal = ""
    );

    std::vector<RewriteSuggestion> generateTests(
        const std::string& functionCode,
        const std::string& testFramework = "catch2"
    );

    std::vector<RewriteSuggestion> fixBugs(
        const std::string& code,
        const std::string& bugDescription = ""
    );

    // Interactive features
    bool applySuggestion(const RewriteSuggestion& suggestion);
    bool previewSuggestion(const RewriteSuggestion& suggestion);
    void undoLastChange();

    // Diff visualization
    std::vector<DiffHunk> generateDiff(
        const std::string& original,
        const std::string& modified
    );

    std::string formatDiff(const std::vector<DiffHunk>& hunks);

private:
    std::string buildRewritePrompt(
        const std::string& code,
        RewriteType type,
        const std::string& context
    );

    RewriteSuggestion createSuggestion(
        const std::string& original,
        const std::string& suggested,
        const std::string& explanation,
        RewriteType type
    );

    bool analyzeSafety(const std::string& original, const std::string& suggested);
};
