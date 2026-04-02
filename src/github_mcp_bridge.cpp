// ============================================================================
// github_mcp_bridge.cpp — GitHub MCP Server Bridge for RawrXD IDE
// Provides GitHub integration via MCP protocol (PR tools, code review, etc.)
// ============================================================================

#include "github_mcp_bridge.h"
#include "mcp_client.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <regex>

namespace RawrXD {
namespace GitHub {

class GitHubMCPBridge::Impl {
public:
    std::unique_ptr<MCP::MCPClient> mcp_client;
    std::string github_token;
    httplib::Client http_client;

    Impl() : http_client("https://api.github.com") {}

    // Initialize with GitHub token
    bool initialize(const std::string& token) {
        github_token = token;

        // Set auth header
        http_client.set_bearer_token_auth(token);

        // Initialize MCP client for GitHub MCP server
        // This would connect to a GitHub MCP server implementation
        mcp_client = std::make_unique<MCP::MCPClient>("http://localhost:3001"); // Example URL

        try {
            return mcp_client->initialize();
        } catch (...) {
            std::cerr << "[GitHub MCP] Failed to connect to GitHub MCP server" << std::endl;
            return false;
        }
    }

    // Get pull request details
    nlohmann::json get_pull_request(const std::string& owner, const std::string& repo, int pr_number) {
        std::string path = "/repos/" + owner + "/" + repo + "/pulls/" + std::to_string(pr_number);

        auto response = http_client.Get(path.c_str());
        if (response && response->status == 200) {
            return nlohmann::json::parse(response->body);
        }

        throw std::runtime_error("Failed to fetch PR details");
    }

    // List pull request files
    std::vector<std::string> get_pull_request_files(const std::string& owner,
                                                   const std::string& repo,
                                                   int pr_number) {
        std::string path = "/repos/" + owner + "/" + repo + "/pulls/" +
                          std::to_string(pr_number) + "/files";

        auto response = http_client.Get(path.c_str());
        if (response && response->status == 200) {
            auto json = nlohmann::json::parse(response->body);
            std::vector<std::string> files;

            for (const auto& file : json) {
                files.push_back(file["filename"]);
            }

            return files;
        }

        return {};
    }

    // Create a code review comment
    bool create_review_comment(const std::string& owner, const std::string& repo,
                              int pr_number, const std::string& body,
                              const std::string& path, int line) {
        nlohmann::json comment = {
            {"body", body},
            {"path", path},
            {"line", line},
            {"side", "RIGHT"}
        };

        std::string path_str = "/repos/" + owner + "/" + repo +
                              "/pulls/" + std::to_string(pr_number) + "/comments";

        auto response = http_client.Post(path_str.c_str(), comment.dump(), "application/json");
        return response && response->status == 201;
    }

    // Get repository issues
    std::vector<nlohmann::json> get_issues(const std::string& owner, const std::string& repo) {
        std::string path = "/repos/" + owner + "/" + repo + "/issues";

        auto response = http_client.Get(path.c_str());
        if (response && response->status == 200) {
            return nlohmann::json::parse(response->body);
        }

        return {};
    }
};

GitHubMCPBridge::GitHubMCPBridge() : pimpl(std::make_unique<Impl>()) {}

GitHubMCPBridge::~GitHubMCPBridge() = default;

GitHubMCPBridge& GitHubMCPBridge::instance() {
    static GitHubMCPBridge instance;
    return instance;
}

bool GitHubMCPBridge::initialize(const std::string& github_token) {
    return pimpl->initialize(github_token);
}

nlohmann::json GitHubMCPBridge::get_pull_request(const std::string& owner,
                                                const std::string& repo,
                                                int pr_number) {
    return pimpl->get_pull_request(owner, repo, pr_number);
}

std::vector<std::string> GitHubMCPBridge::get_pull_request_files(const std::string& owner,
                                                                const std::string& repo,
                                                                int pr_number) {
    return pimpl->get_pull_request_files(owner, repo, pr_number);
}

bool GitHubMCPBridge::create_review_comment(const std::string& owner,
                                           const std::string& repo,
                                           int pr_number,
                                           const std::string& comment,
                                           const std::string& file_path,
                                           int line_number) {
    return pimpl->create_review_comment(owner, repo, pr_number, comment, file_path, line_number);
}

std::vector<nlohmann::json> GitHubMCPBridge::get_issues(const std::string& owner,
                                                       const std::string& repo) {
    return pimpl->get_issues(owner, repo);
}

// MCP Tool wrappers for agent integration
nlohmann::json GitHubMCPBridge::mcp_get_pr_details(const nlohmann::json& args) {
    std::string owner = args["owner"];
    std::string repo = args["repo"];
    int pr_number = args["pr_number"];

    auto pr = get_pull_request(owner, repo, pr_number);
    return {
        {"title", pr["title"]},
        {"body", pr["body"]},
        {"state", pr["state"]},
        {"user", pr["user"]["login"]},
        {"files", get_pull_request_files(owner, repo, pr_number)}
    };
}

nlohmann::json GitHubMCPBridge::mcp_create_review(const nlohmann::json& args) {
    std::string owner = args["owner"];
    std::string repo = args["repo"];
    int pr_number = args["pr_number"];
    std::string comment = args["comment"];
    std::string file_path = args["file_path"];
    int line_number = args["line_number"];

    bool success = create_review_comment(owner, repo, pr_number, comment, file_path, line_number);

    return {{"success", success}};
}

nlohmann::json GitHubMCPBridge::mcp_list_issues(const nlohmann::json& args) {
    std::string owner = args["owner"];
    std::string repo = args["repo"];

    auto issues = get_issues(owner, repo);
    nlohmann::json result = nlohmann::json::array();

    for (const auto& issue : issues) {
        result.push_back({
            {"number", issue["number"]},
            {"title", issue["title"]},
            {"state", issue["state"]},
            {"user", issue["user"]["login"]}
        });
    }

    return result;
}

}} // namespace RawrXD::GitHub