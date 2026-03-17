#pragma once
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace RawrXD {
namespace Tools {

struct GitResult {
    int exit_code = -1;
    std::string stdout_text;
    std::string stderr_text;
};

class GitClient {
public:
    explicit GitClient(const std::string& repo_root);

    // Core
    GitResult version() const;
    GitResult status(bool short_format = false) const;
    GitResult add(const std::vector<std::string>& paths) const;
    GitResult commit(const std::string& message, bool sign_off = false) const;
    GitResult checkout(const std::string& branch_or_commit) const;
    GitResult createBranch(const std::string& branch_name) const;
    GitResult currentBranch() const;
    GitResult diff(const std::string& spec = "") const; // empty -> staged vs HEAD
    GitResult stashSave(const std::string& message = "") const;
    GitResult stashPop() const;

    // Remote
    GitResult fetch(const std::string& remote = "origin") const;
    GitResult pull(const std::string& remote = "origin", const std::string& branch = "") const;
    GitResult push(const std::string& remote = "origin", const std::string& branch = "") const;

    // Helpers
    static bool isGitAvailable();
    static bool isRepo(const std::string& root);

private:
    std::string m_root;
    GitResult run(const std::vector<std::string>& args) const;
};

} // namespace Tools
} // namespace RawrXD
