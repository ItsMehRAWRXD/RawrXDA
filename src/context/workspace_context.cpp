#include "workspace_context.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace {

constexpr size_t kMaxIndexedFiles = 256;
constexpr size_t kMaxSymbolsPerFile = 12;
constexpr size_t kMaxVisibleGitPaths = 16;
constexpr size_t kMaxRecentCommands = 16;

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    return ToLowerCopy(haystack).find(ToLowerCopy(needle)) != std::string::npos;
}

std::uint64_t ToEpochSeconds(const std::filesystem::file_time_type& timestamp) {
    try {
        const auto fileClockNow = std::filesystem::file_time_type::clock::now();
        const auto systemNow = std::chrono::system_clock::now();
        const auto adjusted = timestamp - fileClockNow + systemNow;
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
            adjusted.time_since_epoch()).count());
    } catch (...) {
        return 0;
    }
}

std::string ExtractSymbolCandidate(const std::string& line, const std::string& prefix) {
    const size_t prefixPos = line.find(prefix);
    if (prefixPos == std::string::npos) {
        return {};
    }

    size_t start = prefixPos + prefix.size();
    while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
        ++start;
    }

    size_t end = start;
    while (end < line.size()) {
        const unsigned char ch = static_cast<unsigned char>(line[end]);
        if (!(std::isalnum(ch) || ch == '_' || ch == ':')) {
            break;
        }
        ++end;
    }

    return end > start ? line.substr(start, end - start) : std::string();
}

std::vector<rawrxd::SymbolNode> ExtractSymbols(const std::filesystem::path& filePath) {
    static const std::pair<std::string_view, std::string_view> patterns[] = {
        {"class ", "class"},
        {"struct ", "struct"},
        {"namespace ", "namespace"},
        {"enum ", "enum"},
        {"void ", "function"},
        {"int ", "function"},
        {"std::string ", "function"},
        {"bool ", "function"},
    };

    std::vector<rawrxd::SymbolNode> symbols;
    std::ifstream stream(filePath, std::ios::binary);
    if (!stream.is_open()) {
        return symbols;
    }

    std::string line;
    size_t linesRead = 0;
    while (std::getline(stream, line) && linesRead < 200 && symbols.size() < kMaxSymbolsPerFile) {
        ++linesRead;
        for (const auto& [pattern, kind] : patterns) {
            const std::string name = ExtractSymbolCandidate(line, std::string(pattern));
            if (!name.empty()) {
                symbols.push_back({name, filePath.string(), std::string(kind)});
                break;
            }
        }
    }

    return symbols;
}

std::string ReadGitBranch(const std::filesystem::path& workspaceRoot) {
    const std::filesystem::path headPath = workspaceRoot / ".git" / "HEAD";
    std::ifstream head(headPath);
    if (!head.is_open()) {
        return {};
    }

    std::string line;
    std::getline(head, line);
    const std::string prefix = "ref: refs/heads/";
    if (line.rfind(prefix, 0) == 0) {
        return line.substr(prefix.size());
    }
    return line;
}

std::vector<std::string> CollectVisiblePaths(const std::filesystem::path& workspaceRoot) {
    std::vector<std::string> paths;
    size_t count = 0;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(workspaceRoot)) {
            if (count >= kMaxVisibleGitPaths) {
                break;
            }
            paths.push_back(entry.path().filename().string());
            ++count;
        }
    } catch (...) {
    }
    return paths;
}

}  // namespace

namespace rawrxd {

QueryResult SemanticCache::Query(const std::string& text) const {
    std::lock_guard<std::mutex> lock(mutex_);

    QueryResult result;
    result.query = text;
    result.runtimeState = runtimeState_;
    result.gitState = gitState_;

    for (const auto& file : snapshot_.files) {
        if (result.matchedFiles.size() >= 8) {
            break;
        }
        if (ContainsIgnoreCase(file.path, text)) {
            result.matchedFiles.push_back(file);
        }
    }

    for (const auto& symbol : snapshot_.symbols) {
        if (result.matchedSymbols.size() >= 8) {
            break;
        }
        if (ContainsIgnoreCase(symbol.name, text) || ContainsIgnoreCase(symbol.filePath, text)) {
            result.matchedSymbols.push_back(symbol);
        }
    }

    return result;
}

void SemanticCache::Invalidate(const std::string& file) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.files.erase(
        std::remove_if(snapshot_.files.begin(), snapshot_.files.end(),
                       [&](const FileNode& node) { return node.path == file; }),
        snapshot_.files.end());
    snapshot_.symbols.erase(
        std::remove_if(snapshot_.symbols.begin(), snapshot_.symbols.end(),
                       [&](const SymbolNode& node) { return node.filePath == file; }),
        snapshot_.symbols.end());
}

void SemanticCache::UpdateSnapshot(const WorkspaceSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = snapshot;
}

void SemanticCache::UpdateRuntimeState(const RuntimeState& runtimeState) {
    std::lock_guard<std::mutex> lock(mutex_);
    runtimeState_ = runtimeState;
}

void SemanticCache::UpdateGitState(const GitState& gitState) {
    std::lock_guard<std::mutex> lock(mutex_);
    gitState_ = gitState;
}

void WorkspaceContext::Initialize(const std::string& workspaceRoot) {
    std::lock_guard<std::mutex> lock(mutex_);
    workspaceRoot_ = workspaceRoot.empty() ? std::filesystem::current_path().string() : workspaceRoot;
    runtimeState_.cwd = workspaceRoot_;
    initialized_ = true;
}

bool WorkspaceContext::IsInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

WorkspaceSnapshot WorkspaceContext::RefreshSnapshot() {
    std::string root;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            workspaceRoot_ = std::filesystem::current_path().string();
            runtimeState_.cwd = workspaceRoot_;
            initialized_ = true;
        }
        root = workspaceRoot_;
    }

    WorkspaceSnapshot snapshot;
    snapshot.workspaceRoot = root;

    size_t fileCount = 0;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied)) {
            if (fileCount >= kMaxIndexedFiles) {
                break;
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            if (entry.path().string().find("\\.git\\") != std::string::npos) {
                continue;
            }

            FileNode file;
            file.path = entry.path().string();
            try {
                file.sizeBytes = entry.file_size();
                file.modifiedEpochSeconds = ToEpochSeconds(entry.last_write_time());
            } catch (...) {
            }
            snapshot.files.push_back(file);

            auto symbols = ExtractSymbols(entry.path());
            snapshot.symbols.insert(snapshot.symbols.end(), symbols.begin(), symbols.end());
            ++fileCount;
        }
    } catch (...) {
    }

    refreshGitState();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot.git = {gitState_};
        snapshot_ = snapshot;
        cache_.UpdateSnapshot(snapshot_);
        cache_.UpdateRuntimeState(runtimeState_);
        cache_.UpdateGitState(gitState_);
        return snapshot_;
    }
}

WorkspaceSnapshot WorkspaceContext::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

QueryResult WorkspaceContext::Query(const std::string& text) const {
    return cache_.Query(text);
}

void WorkspaceContext::Invalidate(const std::string& file) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.Invalidate(file);
}

void WorkspaceContext::RecordCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!command.empty()) {
        runtimeState_.recentCommands.push_back(command);
        if (runtimeState_.recentCommands.size() > kMaxRecentCommands) {
            runtimeState_.recentCommands.erase(runtimeState_.recentCommands.begin());
        }
        cache_.UpdateRuntimeState(runtimeState_);
    }
}

RuntimeState WorkspaceContext::runtimeState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return runtimeState_;
}

GitState WorkspaceContext::gitState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return gitState_;
}

std::string WorkspaceContext::workspaceRoot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return workspaceRoot_;
}

void WorkspaceContext::refreshGitState() {
    std::lock_guard<std::mutex> lock(mutex_);
    gitState_.branch = ReadGitBranch(workspaceRoot_);
    gitState_.visiblePaths = CollectVisiblePaths(workspaceRoot_);
}

}  // namespace rawrxd
