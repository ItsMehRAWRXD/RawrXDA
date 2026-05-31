#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <memory>

namespace RawrXD {
namespace IDE {

enum class GitStatus {
    Unmodified,
    Modified,
    Added,
    Deleted,
    Renamed,
    Copied,
    Untracked,
    Ignored
};

struct GitFileStatus {
    std::string filePath;
    GitStatus status;
    GitStatus indexStatus;
};

class GitRepository {
public:
    GitRepository();
    ~GitRepository();

    bool initialize(const std::string& repoPath);
    std::string getBranchName() const;
    std::vector<GitFileStatus> getStatus();
    bool stageFile(const std::string& filePath);
    bool unstageFile(const std::string& filePath);
    bool commit(const std::string& message);
    std::string getDiff(const std::string& filePath);
    std::string getBlame(const std::string& filePath);
    std::string getLog(int maxCount = 50);
    bool createBranch(const std::string& branchName);
    bool switchBranch(const std::string& branchName);

private:
    class GitRepositoryImpl;
    std::unique_ptr<GitRepositoryImpl> m_impl;
};

} // namespace IDE
} // namespace RawrXD
