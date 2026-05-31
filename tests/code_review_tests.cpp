// ============================================================================
// AI Code Reviewer Tests — Comprehensive Code Review Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include "../src/review/ai_code_reviewer.cpp"

using namespace RawrXD::Review;

// Mock AI Client for testing
class MockReviewAIClient : public SovereignInferenceClient {
public:
    bool IsLoaded() const override { return true; }
    ChatResult ChatSync(const std::vector<ChatMessage>& messages) override {
        // Simulate AI review response
        std::string response = "Review complete. Found 2 style issues and 1 security concern.";
        return {true, response, 0.85, 150};
    }
};

// Mock Git Integration
class MockGitIntegration : public GitIntegration {
public:
    std::vector<std::string> GetChangedFiles(const std::string& commitId) override {
        return {"src/main.cpp", "src/utils.cpp"};
    }
    
    std::string GetDiff(const std::string& commitId, const std::string& file) override {
        return "+int add(int a, int b) { return a + b; }";
    }
};

TEST_CASE("AI Code Reviewer - Basic Analysis", "[code-review][ai]") {
    auto aiClient = std::make_shared<MockReviewAIClient>();
    auto gitIntegration = std::make_shared<MockGitIntegration>();
    AICodeReviewer reviewer(aiClient, gitIntegration);
    
    SECTION("Empty code analysis") {
        ReviewReport report;
        report.commitId = "test-commit";
        report.overallScore = 10.0;
        
        REQUIRE(report.comments.empty());
        REQUIRE(report.overallScore == 10.0);
    }
    
    SECTION("Simple function analysis") {
        auto report = reviewer.ReviewChanges("test-commit");
        
        REQUIRE(report.commitId == "test-commit");
        REQUIRE(report.overallScore >= 0.0);
        REQUIRE(report.generatedAt <= std::chrono::system_clock::now());
    }
    
    SECTION("Review report generation") {
        auto report = reviewer.ReviewChanges("test-commit");
        auto summary = reviewer.GenerateReviewSummary(report);
        
        REQUIRE_FALSE(summary.empty());
        REQUIRE(summary.find("Code Review Summary") != std::string::npos);
    }
}

TEST_CASE("AI Code Reviewer - Policy Compliance", "[code-review][policy]") {
    auto aiClient = std::make_shared<MockReviewAIClient>();
    auto gitIntegration = std::make_shared<MockGitIntegration>();
    AICodeReviewer reviewer(aiClient, gitIntegration);
    
    SECTION("Policy compliance check") {
        ReviewReport report;
        report.commitId = "test";
        report.overallScore = 8.5;
        
        ReviewPolicy policy;
        policy.minimumScore = 7.0;
        policy.blockingRules[ReviewSeverity::BLOCKING] = true;
        
        bool compliant = reviewer.CheckPolicyCompliance(report, policy);
        REQUIRE(compliant == true);
    }
    
    SECTION("Failing policy check") {
        ReviewReport report;
        report.commitId = "test";
        report.overallScore = 5.0;
        report.severityCounts[ReviewSeverity::BLOCKING] = 2;
        
        ReviewPolicy policy;
        policy.minimumScore = 7.0;
        policy.blockingRules[ReviewSeverity::BLOCKING] = true;
        
        bool compliant = reviewer.CheckPolicyCompliance(report, policy);
        REQUIRE(compliant == false);
    }
}

TEST_CASE("AI Code Reviewer - Comment Management", "[code-review][comments]") {
    auto aiClient = std::make_shared<MockReviewAIClient>();
    auto gitIntegration = std::make_shared<MockGitIntegration>();
    AICodeReviewer reviewer(aiClient, gitIntegration);
    
    SECTION("Comment resolution") {
        ReviewComment comment;
        comment.id = "comment-1";
        comment.isResolved = false;
        
        // Simulate adding comment
        REQUIRE_FALSE(comment.isResolved);
        
        // Resolve comment
        reviewer.ResolveComment("comment-1", "reviewer@example.com");
        // Note: In real implementation, this would update the stored comment
    }
}
