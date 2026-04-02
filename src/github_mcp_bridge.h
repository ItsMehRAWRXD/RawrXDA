// ============================================================================
// github_mcp_bridge.h — GitHub MCP Server Bridge Header
// Provides GitHub integration via MCP protocol (PR tools, code review, etc.)
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace GitHub {

class GitHubMCPBridge {
public:
    static GitHubMCPBridge& instance();

    // Initialize with GitHub token
    bool initialize(const std::string& github_token);

    // Pull Request operations
    nlohmann::json get_pull_request(const std::string& owner,
                                   const std::string& repo,
                                   int pr_number);

    std::vector<std::string> get_pull_request_files(const std::string& owner,
                                                   const std::string& repo,
                                                   int pr_number);

    // Code review operations
    bool create_review_comment(const std::string& owner,
                              const std::string& repo,
                              int pr_number,
                              const std::string& comment,
                              const std::string& file_path,
                              int line_number);

    // Issue operations
    std::vector<nlohmann::json> get_issues(const std::string& owner,
                                          const std::string& repo);

    // MCP Tool wrappers for agent integration
    static nlohmann::json mcp_get_pr_details(const nlohmann::json& args);
    static nlohmann::json mcp_create_review(const nlohmann::json& args);
    static nlohmann::json mcp_list_issues(const nlohmann::json& args);

private:
    GitHubMCPBridge();
    ~GitHubMCPBridge();

    class Impl;
    std::unique_ptr<Impl> pimpl;
};

}} // namespace RawrXD::GitHub