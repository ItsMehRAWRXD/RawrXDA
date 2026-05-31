#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace rawrxd {

struct FileNode {
    std::string path;
    std::uintmax_t sizeBytes = 0;
    std::uint64_t modifiedEpochSeconds = 0;
};

struct SymbolNode {
    std::string name;
    std::string filePath;
    std::string kind;
};

struct GitState {
    std::string branch;
    bool hasUncommittedChanges = false;
    std::vector<std::string> visiblePaths;
};

struct RuntimeState {
    std::string cwd;
    std::vector<std::string> recentCommands;
};

struct QueryResult {
    std::string query;
    std::vector<FileNode> matchedFiles;
    std::vector<SymbolNode> matchedSymbols;
    RuntimeState runtimeState;
    GitState gitState;
};

struct WorkspaceSnapshot {
    std::string workspaceRoot;
    std::vector<FileNode> files;
    std::vector<SymbolNode> symbols;
    std::vector<GitState> git;
};

class SemanticCache {
  public:
    QueryResult Query(const std::string& text) const;
    void Invalidate(const std::string& file);
    void UpdateSnapshot(const WorkspaceSnapshot& snapshot);
    void UpdateRuntimeState(const RuntimeState& runtimeState);
    void UpdateGitState(const GitState& gitState);

  private:
    WorkspaceSnapshot snapshot_;
    RuntimeState runtimeState_;
    GitState gitState_;
    mutable std::mutex mutex_;
};

class WorkspaceContext {
  public:
    void Initialize(const std::string& workspaceRoot);
    bool IsInitialized() const;
    WorkspaceSnapshot RefreshSnapshot();
    WorkspaceSnapshot Snapshot() const;
    QueryResult Query(const std::string& text) const;
    void Invalidate(const std::string& file);
    void RecordCommand(const std::string& command);
    RuntimeState runtimeState() const;
    GitState gitState() const;
    std::string workspaceRoot() const;

  private:
    void refreshGitState();

    std::string workspaceRoot_;
    bool initialized_ = false;
    WorkspaceSnapshot snapshot_;
    RuntimeState runtimeState_;
    GitState gitState_;
    SemanticCache cache_;
    mutable std::mutex mutex_;
};

}  // namespace rawrxd
