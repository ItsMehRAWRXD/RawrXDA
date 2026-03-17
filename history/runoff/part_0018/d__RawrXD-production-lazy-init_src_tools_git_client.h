#pragma once

#include <string>
#include <vector>

namespace RawrXD {
namespace Tools {

struct GitResult {
    int exit_code = 0;
    std::string stdout_text;
    std::string stderr_text;
};

class GitClient {
public:
    explicit GitClient(const std::string& repo_root);

    static bool isGitAvailable();
    static bool isRepo(const std::string& root);

    GitResult run(const std::vector<std::string>& args) const;

    GitResult version() const;
    GitResult status(bool short_format) const;
    GitResult add(const std::vector<std::string>& paths) const;
    GitResult commit(const std::string& message, bool sign_off) const;
    GitResult checkout(const std::string& branch_or_commit) const;
    GitResult createBranch(const std::string& branch_name) const;
    GitResult currentBranch() const;
    GitResult diff(const std::string& spec) const;
    GitResult stashSave(const std::string& message) const;
    GitResult stashPop() const;
    GitResult fetch(const std::string& remote) const;
    GitResult pull(const std::string& remote, const std::string& branch) const;
    GitResult push(const std::string& remote, const std::string& branch) const;

private:
    std::string m_root;
};

} // namespace Tools
} // namespace RawrXD
