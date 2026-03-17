#pragma once
#include <string>
#include <vector>
#include <git2.h> // libgit2 for native interaction

namespace rawrxd::git {

struct GitFileStatus {
    std::string path;
    bool isStaged;
    bool isModified;
    bool isUntracked;
    bool isDeleted;
};

struct CommitRecord {
    std::string hash;
    std::string message;
    std::string author;
    uint64_t timestamp;
};

class GitWired {
public:
    static GitWired& instance() {
        static GitWired instance;
        return instance;
    }

    // High performance status poll for explorer/editor
    std::vector<GitFileStatus> getStatus(const std::string& repo_root);

    // Fast stage/unstage for Composer
    bool stagePath(const std::string& path);
    bool unstagePath(const std::string& path);

    // Commit with agentic message generation
    bool commit(const std::string& message, 
                const std::string& author_name,
                const std::string& author_email);

    // Branch management for agentic feature work
    bool checkoutBranch(const std::string& branch_name, bool create = false);
    std::vector<std::string> listBranches();

    // Pull/Push wiring for terminal/UI
    bool pull(const std::string& remote = "origin");
    bool push(const std::string& remote = "origin");

    // Unified diff generator for DiffViewer
    std::string getFileDiffToHead(const std::string& file_path);

private:
    git_repository* repo = nullptr;
    std::string current_root;
    bool initRepo(const std::string& root);
};

} // namespace rawrxd::git
