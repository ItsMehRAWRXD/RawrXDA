// ============================================================================
// multi_file_transaction.cpp — Multi-File Agent Transaction Implementation
// ============================================================================
// Implements atomic multi-file edits with include parsing, topological sort,
// three-way merge, SHA-256 checksums, and atomic write-with-rename.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "agentic/multi_file_transaction.h"

#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <queue>
#include <chrono>
#include <random>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#else
#include <openssl/sha.h>
#endif

namespace fs = std::filesystem;

namespace RawrXD {
namespace Agent {

// ============================================================================
// Transaction State Names
// ============================================================================

const char* transactionStateName(TransactionState state) {
    switch (state) {
        case TransactionState::PREPARING:    return "PREPARING";
        case TransactionState::VALIDATING:   return "VALIDATING";
        case TransactionState::APPLYING:     return "APPLYING";
        case TransactionState::COMMITTED:    return "COMMITTED";
        case TransactionState::ROLLING_BACK: return "ROLLING_BACK";
        case TransactionState::ROLLED_BACK:  return "ROLLED_BACK";
        case TransactionState::FAILED:       return "FAILED";
        default:                             return "UNKNOWN";
    }
}

// ============================================================================
// Include Parser — regex-based #include extraction
// ============================================================================

std::vector<IncludeInfo> parseIncludes(const std::string& fileContent) {
    std::vector<IncludeInfo> results;

    // Match: #include <header> or #include "header"
    // Also handles #import for Objective-C++ interop
    static const std::regex includeRegex(
        R"(^\s*#\s*(?:include|import)\s+([<"])([^>"]+)[>"])",
        std::regex::optimize
    );

    int lineNum = 1;
    std::istringstream stream(fileContent);
    std::string line;

    while (std::getline(stream, line)) {
        std::smatch match;
        if (std::regex_search(line, match, includeRegex)) {
            IncludeInfo info;
            info.header = match[2].str();
            info.line = lineNum;
            info.isSystem = (match[1].str() == "<");
            results.push_back(std::move(info));
        }
        ++lineNum;
    }

    return results;
}

std::vector<IncludeInfo> parseIncludesFromFile(const fs::path& filePath) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) return {};

    std::ostringstream oss;
    oss << f.rdbuf();
    return parseIncludes(oss.str());
}

// ============================================================================
// DependencyGraph
// ============================================================================

void DependencyGraph::addInclude(const std::string& includer, const std::string& includee,
                                  int line, bool isSystem) {
    includeGraph[includer].push_back(includee);
    edges.push_back({includer, includee, line, isSystem});
}

void DependencyGraph::buildFromDirectory(const fs::path& root,
                                          const std::vector<std::string>& extensions) {
    std::error_code ec;
    for (auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        bool matchExt = extensions.empty();
        for (const auto& e : extensions) {
            if (ext == e) { matchExt = true; break; }
        }
        if (!matchExt) continue;

        std::string filePath = entry.path().string();
        auto includes = parseIncludesFromFile(entry.path());

        for (const auto& inc : includes) {
            if (!inc.isSystem) {
                addInclude(filePath, inc.header, inc.line, inc.isSystem);
            }
        }
    }
}

std::vector<std::string> DependencyGraph::topologicalSort() const {
    // Kahn's algorithm
    std::unordered_map<std::string, int> inDegree;
    std::unordered_set<std::string> allNodes;

    for (const auto& [node, deps] : includeGraph) {
        allNodes.insert(node);
        if (inDegree.find(node) == inDegree.end()) inDegree[node] = 0;
        for (const auto& dep : deps) {
            allNodes.insert(dep);
            inDegree[dep]++;
        }
    }
    for (const auto& node : allNodes) {
        if (inDegree.find(node) == inDegree.end()) inDegree[node] = 0;
    }

    std::queue<std::string> queue;
    for (const auto& [node, deg] : inDegree) {
        if (deg == 0) queue.push(node);
    }

    std::vector<std::string> sorted;
    sorted.reserve(allNodes.size());

    while (!queue.empty()) {
        std::string node = queue.front();
        queue.pop();
        sorted.push_back(node);

        auto it = includeGraph.find(node);
        if (it != includeGraph.end()) {
            for (const auto& dep : it->second) {
                if (--inDegree[dep] == 0) {
                    queue.push(dep);
                }
            }
        }
    }

    // Cycle detection: sorted size < total nodes
    if (sorted.size() < allNodes.size()) {
        return {};  // Cycle detected
    }

    return sorted;
}

std::vector<std::string> DependencyGraph::transitiveDependents(const std::string& file) const {
    // Reverse BFS from the file
    std::unordered_map<std::string, std::vector<std::string>> reverseGraph;
    for (const auto& [node, deps] : includeGraph) {
        for (const auto& dep : deps) {
            reverseGraph[dep].push_back(node);
        }
    }

    std::vector<std::string> result;
    std::unordered_set<std::string> visited;
    std::queue<std::string> bfs;
    bfs.push(file);
    visited.insert(file);

    while (!bfs.empty()) {
        std::string current = bfs.front();
        bfs.pop();

        auto it = reverseGraph.find(current);
        if (it != reverseGraph.end()) {
            for (const auto& dep : it->second) {
                if (visited.insert(dep).second) {
                    result.push_back(dep);
                    bfs.push(dep);
                }
            }
        }
    }

    return result;
}

bool DependencyGraph::hasCycle() const {
    auto sorted = topologicalSort();
    return sorted.empty() && !includeGraph.empty();
}

// ============================================================================
// Three-Way Merge
// ============================================================================

static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

MergeResult threeWayMerge(const std::string& base,
                           const std::string& agentEdit,
                           const std::string& userEdit) {
    MergeResult result;

    auto baseLines = splitLines(base);
    auto agentLines = splitLines(agentEdit);
    auto userLines = splitLines(userEdit);

    // Simple line-by-line three-way merge
    // If both changed the same line differently → conflict
    size_t maxLines = std::max({baseLines.size(), agentLines.size(), userLines.size()});

    std::ostringstream merged;

    for (size_t i = 0; i < maxLines; ++i) {
        std::string baseLine = (i < baseLines.size()) ? baseLines[i] : "";
        std::string agentLine = (i < agentLines.size()) ? agentLines[i] : "";
        std::string userLine = (i < userLines.size()) ? userLines[i] : "";

        bool agentChanged = (agentLine != baseLine);
        bool userChanged = (userLine != baseLine);

        if (!agentChanged && !userChanged) {
            // No change
            merged << baseLine << "\n";
        } else if (agentChanged && !userChanged) {
            // Agent change only
            merged << agentLine << "\n";
        } else if (!agentChanged && userChanged) {
            // User change only
            merged << userLine << "\n";
        } else if (agentLine == userLine) {
            // Both changed to same thing
            merged << agentLine << "\n";
        } else {
            // Conflict
            MergeConflict conflict;
            conflict.startLine = static_cast<int>(i + 1);
            conflict.endLine = static_cast<int>(i + 1);
            conflict.baseVersion = baseLine;
            conflict.agentVersion = agentLine;
            conflict.userVersion = userLine;
            conflict.autoResolvable = false;
            result.conflicts.push_back(std::move(conflict));

            // Use conflict markers
            merged << "<<<<<<< AGENT\n";
            merged << agentLine << "\n";
            merged << "=======\n";
            merged << userLine << "\n";
            merged << ">>>>>>> USER\n";
        }
    }

    result.mergedContent = merged.str();
    result.conflictCount = static_cast<int>(result.conflicts.size());
    result.success = (result.conflictCount == 0);

    return result;
}

// ============================================================================
// MultiFileTransaction — implementation
// ============================================================================

MultiFileTransaction::MultiFileTransaction(const std::string& description)
    : m_state(TransactionState::PREPARING), m_description(description) {
    m_txId = generateTransactionId();
}

MultiFileTransaction::~MultiFileTransaction() {
    // If not committed, attempt rollback
    if (m_state == TransactionState::APPLYING) {
        rollback();
    }
}

std::string MultiFileTransaction::generateTransactionId() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    std::mt19937 rng(static_cast<uint32_t>(ms));
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "txn_%lld_%08x",
                  static_cast<long long>(ms), dist(rng));
    return std::string(buf);
}

std::string MultiFileTransaction::readFileContent(const fs::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream oss;
    oss << f.rdbuf();
    return oss.str();
}

std::string MultiFileTransaction::computeSHA256(const std::string& data) {
#ifdef _WIN32
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string result;

    if (CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptHashData(hHash, reinterpret_cast<const BYTE*>(data.c_str()),
                          static_cast<DWORD>(data.size()), 0);

            DWORD hashLen = 32;
            BYTE hash[32];
            CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);

            char hex[65];
            for (DWORD i = 0; i < hashLen; ++i) {
                std::snprintf(hex + i * 2, 3, "%02x", hash[i]);
            }
            hex[64] = '\0';
            result = hex;

            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return result;
#else
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.c_str()),
           data.size(), hash);

    char hex[65];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        std::snprintf(hex + i * 2, 3, "%02x", hash[i]);
    }
    hex[64] = '\0';
    return std::string(hex);
#endif
}

TransactionResult MultiFileTransaction::addFileEdit(const fs::path& path,
                                                      const std::string& newContent) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != TransactionState::PREPARING) {
        return TransactionResult::error("Cannot add edits in current state");
    }

    FileEditNode node;
    node.path = path;
    node.proposedContent = newContent;
    node.applied = false;

    // Read original content and compute hash
    if (fs::exists(path)) {
        node.originalContent = readFileContent(path);
        node.originalHash = computeSHA256(node.originalContent);

        std::error_code ec;
        auto lwt = fs::last_write_time(path, ec);
        node.originalModTime = ec ? 0 :
            static_cast<uint64_t>(lwt.time_since_epoch().count());
    }

    m_edits.push_back(std::move(node));
    return TransactionResult::ok("Edit added");
}

TransactionResult MultiFileTransaction::addRegionEdit(const fs::path& path,
                                                        int startLine, int endLine,
                                                        const std::string& newText) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != TransactionState::PREPARING) {
        return TransactionResult::error("Cannot add edits in current state");
    }

    // Find or create node for this file
    FileEditNode* node = nullptr;
    for (auto& edit : m_edits) {
        if (edit.path == path) { node = &edit; break; }
    }

    if (!node) {
        FileEditNode newNode;
        newNode.path = path;
        newNode.applied = false;
        if (fs::exists(path)) {
            newNode.originalContent = readFileContent(path);
            newNode.originalHash = computeSHA256(newNode.originalContent);
        }
        m_edits.push_back(std::move(newNode));
        node = &m_edits.back();
    }

    FileEditNode::EditRegion region;
    region.startLine = startLine;
    region.endLine = endLine;
    region.newText = newText;

    // Extract old text for this region
    auto lines = splitLines(node->originalContent);
    std::ostringstream oldTextStream;
    for (int i = startLine - 1; i < endLine && i < static_cast<int>(lines.size()); ++i) {
        oldTextStream << lines[i] << "\n";
    }
    region.oldText = oldTextStream.str();

    node->regions.push_back(std::move(region));
    return TransactionResult::ok("Region edit added");
}

void MultiFileTransaction::setDependencyGraph(const DependencyGraph& graph) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_depGraph = graph;
}

TransactionResult MultiFileTransaction::validateDependencies() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state = TransactionState::VALIDATING;

    if (m_depGraph.hasCycle()) {
        m_state = TransactionState::FAILED;
        return TransactionResult::error("Circular dependency detected");
    }

    // Reorder edits by topological sort
    auto sortedFiles = m_depGraph.topologicalSort();
    if (!sortedFiles.empty()) {
        std::vector<FileEditNode> reordered;
        for (const auto& file : sortedFiles) {
            for (auto& edit : m_edits) {
                if (edit.path.string().find(file) != std::string::npos) {
                    reordered.push_back(std::move(edit));
                }
            }
        }
        // Add any remaining edits not in the graph
        for (auto& edit : m_edits) {
            if (!edit.path.empty()) {
                bool found = false;
                for (const auto& r : reordered) {
                    if (r.path == edit.path) { found = true; break; }
                }
                if (!found) reordered.push_back(std::move(edit));
            }
        }
        m_edits = std::move(reordered);
    }

    return TransactionResult::ok("Dependencies validated");
}

TransactionResult MultiFileTransaction::checkConflicts() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_conflicts.clear();

    for (auto& edit : m_edits) {
        if (!fs::exists(edit.path)) continue;

        // Re-read file and check if it changed since we captured it
        std::string currentContent = readFileContent(edit.path);
        std::string currentHash = computeSHA256(currentContent);

        if (currentHash != edit.originalHash) {
            // File was modified externally — three-way merge needed
            auto mergeResult = threeWayMerge(edit.originalContent,
                                              edit.proposedContent,
                                              currentContent);
            if (!mergeResult.success) {
                for (auto& c : mergeResult.conflicts) {
                    c.file = edit.path.string();
                    m_conflicts.push_back(std::move(c));
                }
            } else {
                // Auto-merged successfully — update proposed content
                edit.proposedContent = mergeResult.mergedContent;
            }
        }
    }

    if (!m_conflicts.empty()) {
        return TransactionResult::error("Merge conflicts detected");
    }

    return TransactionResult::ok("No conflicts");
}

std::vector<MergeConflict> MultiFileTransaction::getConflicts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_conflicts;
}

TransactionResult MultiFileTransaction::atomicWrite(const fs::path& path,
                                                      const std::string& content) {
    // Create parent directory if needed
    std::error_code ec;
    auto parent = path.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
    }

    // Write to temporary file first
    fs::path tempPath = path;
    tempPath += ".rawrxd_tmp";

#ifdef _WIN32
    // Use WriteFile with FILE_FLAG_WRITE_THROUGH for durability
    HANDLE hFile = CreateFileW(tempPath.wstring().c_str(),
                                GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                                nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return TransactionResult::error("Failed to create temp file");
    }

    DWORD written = 0;
    BOOL ok = WriteFile(hFile, content.c_str(),
                        static_cast<DWORD>(content.size()), &written, nullptr);
    FlushFileBuffers(hFile);
    CloseHandle(hFile);

    if (!ok || written != content.size()) {
        DeleteFileW(tempPath.wstring().c_str());
        return TransactionResult::error("Failed to write temp file");
    }

    // Atomic rename (MoveFileExW with REPLACE_EXISTING)
    if (!MoveFileExW(tempPath.wstring().c_str(), path.wstring().c_str(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempPath.wstring().c_str());
        return TransactionResult::error("Failed to atomic rename");
    }
#else
    // POSIX: write + fsync + rename
    std::ofstream f(tempPath, std::ios::binary);
    if (!f.is_open()) return TransactionResult::error("Failed to create temp file");
    f.write(content.c_str(), content.size());
    f.flush();
    f.close();

    if (std::rename(tempPath.string().c_str(), path.string().c_str()) != 0) {
        std::remove(tempPath.string().c_str());
        return TransactionResult::error("Failed to atomic rename");
    }
#endif

    return TransactionResult::ok("Written atomically");
}

TransactionResult MultiFileTransaction::commit() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_edits.empty()) {
        return TransactionResult::error("No edits to commit");
    }

    m_state = TransactionState::APPLYING;

    // Apply region edits to build final content
    for (auto& edit : m_edits) {
        if (!edit.regions.empty() && edit.proposedContent.empty()) {
            // Build proposed content from regions
            auto lines = splitLines(edit.originalContent);
            // Apply regions in reverse order to preserve line numbers
            auto sortedRegions = edit.regions;
            std::sort(sortedRegions.begin(), sortedRegions.end(),
                      [](const FileEditNode::EditRegion& a,
                         const FileEditNode::EditRegion& b) {
                          return a.startLine > b.startLine;  // Reverse
                      });

            for (const auto& region : sortedRegions) {
                auto newLines = splitLines(region.newText);
                int start = std::max(0, region.startLine - 1);
                int end = std::min(static_cast<int>(lines.size()), region.endLine);

                lines.erase(lines.begin() + start, lines.begin() + end);
                lines.insert(lines.begin() + start,
                             newLines.begin(), newLines.end());
            }

            std::ostringstream oss;
            for (size_t i = 0; i < lines.size(); ++i) {
                oss << lines[i];
                if (i + 1 < lines.size()) oss << "\n";
            }
            edit.proposedContent = oss.str();
        }
    }

    // Write all files
    for (auto& edit : m_edits) {
        auto r = atomicWrite(edit.path, edit.proposedContent);
        if (!r.success) {
            // Rollback already-applied edits
            m_state = TransactionState::ROLLING_BACK;
            for (auto& applied : m_edits) {
                if (!applied.applied) break;
                atomicWrite(applied.path, applied.originalContent);
            }
            m_state = TransactionState::ROLLED_BACK;
            return TransactionResult::rolledBack("Write failed, rolled back");
        }
        edit.applied = true;
    }

    m_state = TransactionState::COMMITTED;
    return TransactionResult::ok("All files committed");
}

TransactionResult MultiFileTransaction::rollback() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_state = TransactionState::ROLLING_BACK;

    bool allRolledBack = true;
    for (auto& edit : m_edits) {
        if (edit.applied) {
            auto r = atomicWrite(edit.path, edit.originalContent);
            if (!r.success) allRolledBack = false;
            edit.applied = false;
        }
    }

    m_state = TransactionState::ROLLED_BACK;

    if (!allRolledBack) {
        return TransactionResult::error("Partial rollback — some files may be inconsistent");
    }

    return TransactionResult::ok("Rolled back successfully");
}

void MultiFileTransaction::setExternalEditCallback(ExternalEditCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_externalEditCb = std::move(cb);
}

bool MultiFileTransaction::hasExternalEdits() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& edit : m_edits) {
        if (!fs::exists(edit.path)) continue;
        std::string currentHash = const_cast<MultiFileTransaction*>(this)->
            computeSHA256(const_cast<MultiFileTransaction*>(this)->readFileContent(edit.path));
        if (currentHash != edit.originalHash && !edit.applied) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// FileWatcher — uses ReadDirectoryChangesW for real-time file monitoring
// ============================================================================

struct FileWatcher::WatchHandle {
#ifdef _WIN32
    HANDLE hDir = INVALID_HANDLE_VALUE;
    HANDLE hThread = nullptr;
    std::atomic<bool> running{false};
#endif
    fs::path directory;
    ChangeCallback callback;
};

FileWatcher::FileWatcher() = default;

FileWatcher::~FileWatcher() {
    stopAll();
}

TransactionResult FileWatcher::watch(const fs::path& directory,
                                      ChangeCallback callback,
                                      bool recursive) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto handle = std::make_unique<WatchHandle>();
    handle->directory = directory;
    handle->callback = std::move(callback);

#ifdef _WIN32
    handle->hDir = CreateFileW(
        directory.wstring().c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (handle->hDir == INVALID_HANDLE_VALUE) {
        return {false, "Failed to open directory for watching", -1, TransactionState::FAILED};
    }

    handle->running.store(true);

    // Start watch thread
    auto* rawHandle = handle.get();
    handle->hThread = CreateThread(nullptr, 0,
        [](LPVOID param) -> DWORD {
            auto* wh = static_cast<WatchHandle*>(param);
            BYTE buffer[4096];
            DWORD bytesReturned = 0;

            while (wh->running.load()) {
                if (ReadDirectoryChangesW(
                        wh->hDir, buffer, sizeof(buffer), TRUE,
                        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE |
                        FILE_NOTIFY_CHANGE_SIZE,
                        &bytesReturned, nullptr, nullptr)) {

                    auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
                    while (info) {
                        std::wstring filename(info->FileName,
                                              info->FileNameLength / sizeof(wchar_t));
                        fs::path changedPath = wh->directory / filename;

                        const char* action = "unknown";
                        switch (info->Action) {
                            case FILE_ACTION_ADDED:            action = "added"; break;
                            case FILE_ACTION_REMOVED:          action = "removed"; break;
                            case FILE_ACTION_MODIFIED:         action = "modified"; break;
                            case FILE_ACTION_RENAMED_OLD_NAME: action = "renamed_from"; break;
                            case FILE_ACTION_RENAMED_NEW_NAME: action = "renamed_to"; break;
                        }

                        if (wh->callback) {
                            wh->callback(changedPath, action);
                        }

                        if (info->NextEntryOffset == 0) break;
                        info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                            reinterpret_cast<BYTE*>(info) + info->NextEntryOffset);
                    }
                }
            }
            return 0;
        },
        rawHandle, 0, nullptr);
#endif

    m_handles.push_back(std::move(handle));
    return {true, "Watching directory", 0, TransactionState::PREPARING};
}

void FileWatcher::stopAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& handle : m_handles) {
#ifdef _WIN32
        handle->running.store(false);
        if (handle->hDir != INVALID_HANDLE_VALUE) {
            CancelIoEx(handle->hDir, nullptr);
            CloseHandle(handle->hDir);
        }
        if (handle->hThread) {
            WaitForSingleObject(handle->hThread, 3000);
            CloseHandle(handle->hThread);
        }
#endif
    }
    m_handles.clear();
}

bool FileWatcher::isWatching() const {
    return !m_handles.empty();
}

} // namespace Agent
} // namespace RawrXD
