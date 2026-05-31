#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "RawrXD_Interfaces.h"

namespace RawrXD {
namespace GitIntegrator {

enum class CommitContext {
    Refactor,
    Fix,
    Feature,
    Chore,
    Performance,
    Documentation
};

struct ChangeReview {
    std::string summary;
    std::vector<std::string> issues;
    float confidence;
    bool approved;
};

struct CommitProposal {
    std::string commitMessage;
    CommitContext context;
    std::string conventionalHeader;
    std::string body;
};

class GitMCPBridge {
public:
    GitMCPBridge();
    
    // Tools to implement:
    // git_pr_review: AI reviews PR diff
    ChangeReview reviewPullRequest(uint32_t prNumber, const std::string& owner, const std::string& repo);
    
    // git_commit_smart: Conventional commits from diff
    CommitProposal proposeCommitFromDiff(const std::string& diff);
    
    // git_blame_insight: Risk analysis on specific lines
    std::string analyzeBlameRisk(const std::string& filePath, uint32_t startLine, uint32_t endLine);
    
    // git_time_machine: Get historic version with AI context
    std::string getHistoricalFunctionState(const std::string& symbol, const std::string& timeRange);

private:
    bool executeGitCommand(const std::string& command, std::string& output);
    std::string parseDiffForContext(const std::string& diff);
};

} // namespace GitIntegrator
} // namespace RawrXD
