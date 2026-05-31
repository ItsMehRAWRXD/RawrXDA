// ============================================================================
// AI Code Reviewer — Intelligent Code Review Automation
// Automated peer review with AI-powered suggestions
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../git/git_integration.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>

namespace RawrXD::Review {

enum class ReviewSeverity {
    INFO,
    SUGGESTION,
    WARNING,
    ERROR,
    BLOCKING
};

enum class ReviewCategory {
    STYLE,
    PERFORMANCE,
    SECURITY,
    MAINTAINABILITY,
    TESTING,
    DOCUMENTATION
};

struct ReviewComment {
    std::string id;
    std::string filePath;
    int lineNumber;
    int column;
    ReviewSeverity severity;
    ReviewCategory category;
    std::string message;
    std::string suggestedCode;
    std::string originalCode;
    std::vector<std::string> references;
    std::chrono::system_clock::time_point createdAt;
    bool isResolved;
    std::string resolvedBy;
};

struct ReviewReport {
    std::string commitId;
    std::string branch;
    std::vector<ReviewComment> comments;
    std::map<ReviewSeverity, int> severityCounts;
    std::map<ReviewCategory, int> categoryCounts;
    double overallScore;
    std::chrono::system_clock::time_point generatedAt;
};

struct ReviewPolicy {
    std::string name;
    std::map<ReviewSeverity, bool> blockingRules;
    std::vector<ReviewCategory> requiredCategories;
    double minimumScore;
    bool requireTests;
    bool requireDocumentation;
};

class AICodeReviewer {
public:
    explicit AICodeReviewer(std::shared_ptr<SovereignInferenceClient> aiClient,
                           std::shared_ptr<GitIntegration> gitIntegration)
        : m_aiClient(aiClient)
        , m_gitIntegration(gitIntegration) {}

    ReviewReport ReviewChanges(const std::string& commitId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        ReviewReport report;
        report.commitId = commitId;
        report.generatedAt = std::chrono::system_clock::now();
        
        // Get changed files
        auto changedFiles = m_gitIntegration->GetChangedFiles(commitId);
        
        for (const auto& file : changedFiles) {
            auto diff = m_gitIntegration->GetDiff(commitId, file);
            auto fileComments = ReviewFile(file, diff);
            report.comments.insert(report.comments.end(), fileComments.begin(), fileComments.end());
        }
        
        // Calculate statistics
        for (const auto& comment : report.comments) {
            report.severityCounts[comment.severity]++;
            report.categoryCounts[comment.category]++;
        }
        
        // Calculate overall score
        report.overallScore = CalculateOverallScore(report);
        
        m_reports[commitId] = report;
        return report;
    }

    std::vector<ReviewComment> ReviewFile(const std::string& filePath, const std::string& diff) {
        std::vector<ReviewComment> comments;
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return comments;
        }

        std::string prompt = "Review this code change:\n\nFile: " + filePath + "\n```diff\n" + diff + "\n```";
        
        std::vector<ChatMessage> messages = {
            {"system", "You are an expert code reviewer. Provide constructive feedback."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            // Parse AI response into review comments
            ReviewComment comment;
            comment.id = GenerateCommentId();
            comment.filePath = filePath;
            comment.lineNumber = 1;
            comment.severity = ReviewSeverity::SUGGESTION;
            comment.category = ReviewCategory::STYLE;
            comment.message = result.response;
            comment.createdAt = std::chrono::system_clock::now();
            comment.isResolved = false;
            comments.push_back(comment);
        }
        
        return comments;
    }

    bool CheckPolicyCompliance(const ReviewReport& report, const ReviewPolicy& policy) {
        // Check minimum score
        if (report.overallScore < policy.minimumScore) {
            return false;
        }
        
        // Check blocking issues
        for (const auto& [severity, isBlocking] : policy.blockingRules) {
            if (isBlocking && report.severityCounts[severity] > 0) {
                return false;
            }
        }
        
        // Check required categories
        for (const auto& category : policy.requiredCategories) {
            if (report.categoryCounts[category] == 0) {
                return false;
            }
        }
        
        return true;
    }

    void ResolveComment(const std::string& commentId, const std::string& resolvedBy) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (auto& [commitId, report] : m_reports) {
            for (auto& comment : report.comments) {
                if (comment.id == commentId) {
                    comment.isResolved = true;
                    comment.resolvedBy = resolvedBy;
                    return;
                }
            }
        }
    }

    std::string GenerateReviewSummary(const ReviewReport& report) {
        std::ostringstream summary;
        summary << "# Code Review Summary\n\n";
        summary << "**Commit:** " << report.commitId << "\n";
        summary << "**Overall Score:** " << std::fixed << std::setprecision(1) 
               << report.overallScore << "/10.0\n\n";
        
        summary << "## Issues by Severity\n";
        for (const auto& [severity, count] : report.severityCounts) {
            summary << "- " << SeverityToString(severity) << ": " << count << "\n";
        }
        
        summary << "\n## Issues by Category\n";
        for (const auto& [category, count] : report.categoryCounts) {
            summary << "- " << CategoryToString(category) << ": " << count << "\n";
        }
        
        if (!report.comments.empty()) {
            summary << "\n## Detailed Comments\n";
            for (const auto& comment : report.comments) {
                if (!comment.isResolved) {
                    summary << "### [" << SeverityToString(comment.severity) << "] " 
                           << comment.filePath << ":" << comment.lineNumber << "\n";
                    summary << comment.message << "\n\n";
                }
            }
        }
        
        return summary.str();
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    std::shared_ptr<GitIntegration> m_gitIntegration;
    mutable std::mutex m_mutex;
    std::map<std::string, ReviewReport> m_reports;

    double CalculateOverallScore(const ReviewReport& report) {
        double score = 10.0;
        
        // Deduct for issues
        score -= report.severityCounts[ReviewSeverity::BLOCKING] * 2.0;
        score -= report.severityCounts[ReviewSeverity::ERROR] * 1.0;
        score -= report.severityCounts[ReviewSeverity::WARNING] * 0.5;
        score -= report.severityCounts[ReviewSeverity::SUGGESTION] * 0.1;
        
        return std::max(0.0, score);
    }

    std::string GenerateCommentId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "review_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::string SeverityToString(ReviewSeverity severity) {
        switch (severity) {
            case ReviewSeverity::INFO: return "Info";
            case ReviewSeverity::SUGGESTION: return "Suggestion";
            case ReviewSeverity::WARNING: return "Warning";
            case ReviewSeverity::ERROR: return "Error";
            case ReviewSeverity::BLOCKING: return "Blocking";
            default: return "Unknown";
        }
    }

    std::string CategoryToString(ReviewCategory category) {
        switch (category) {
            case ReviewCategory::STYLE: return "Style";
            case ReviewCategory::PERFORMANCE: return "Performance";
            case ReviewCategory::SECURITY: return "Security";
            case ReviewCategory::MAINTAINABILITY: return "Maintainability";
            case ReviewCategory::TESTING: return "Testing";
            case ReviewCategory::DOCUMENTATION: return "Documentation";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::Review
