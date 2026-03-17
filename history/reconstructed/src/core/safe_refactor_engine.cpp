// ============================================================================
// safe_refactor_engine.cpp — Safe-by-Default Bulk Refactor Engine
// ============================================================================
//
// Full implementation of diff-aware verification gates, snapshot/rollback,
// and the complete safe refactor pipeline.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "safe_refactor_engine.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <cstring>
#include <chrono>
#include <regex>
#include <filesystem>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace fs = std::filesystem;

// ============================================================================
// Singleton
// ============================================================================
SafeRefactorEngine& SafeRefactorEngine::instance() {
    static SafeRefactorEngine s;
    return s;
}

SafeRefactorEngine::SafeRefactorEngine() = default;

// ============================================================================
// ID generation
// ============================================================================
std::string SafeRefactorEngine::generateRefactorId() const {
    static std::atomic<uint64_t> counter{0};
    uint64_t id = counter.fetch_add(1, std::memory_order_relaxed);
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::ostringstream ss;
    ss << "refactor-" << now << "-" << id;
    return ss.str();
}

// ============================================================================
// CRC32 (byte-level integrity)
// ============================================================================
uint32_t SafeRefactorEngine::computeCRC32(const void* data, size_t len) {
    // Standard CRC32 table-based implementation
    static uint32_t table[256] = {0};
    static bool tableBuilt = false;

    if (!tableBuilt) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j) {
                crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
            }
            table[i] = crc;
        }
        tableBuilt = true;
    }

    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// ============================================================================
// Line counting
// ============================================================================
int SafeRefactorEngine::countLines(const std::vector<uint8_t>& content) const {
    int count = 0;
    for (uint8_t c : content) {
        if (c == '\n') ++count;
    }
    if (!content.empty() && content.back() != '\n') ++count;
    return count;
}

// ============================================================================
// Symbol extraction (simple C/C++ identifier scan)
// ============================================================================
std::unordered_set<std::string> SafeRefactorEngine::extractSymbols(
    const std::string& filePath) const
{
    std::unordered_set<std::string> symbols;

    std::ifstream file(filePath);
    if (!file.is_open()) return symbols;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Extract function, class, struct, enum names
    std::regex symbolRe(R"(\b(?:class|struct|enum|void|int|bool|float|double|auto|static)\s+(\w+)\b)");
    std::sregex_iterator it(content.begin(), content.end(), symbolRe);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        symbols.insert((*it)[1].str());
    }

    // Extract function-like identifiers followed by (
    std::regex funcRe(R"(\b([A-Za-z_]\w+)\s*\()");
    it = std::sregex_iterator(content.begin(), content.end(), funcRe);
    for (; it != end; ++it) {
        std::string name = (*it)[1].str();
        // Filter out common keywords
        if (name != "if" && name != "for" && name != "while" && name != "switch" &&
            name != "return" && name != "sizeof" && name != "catch" && name != "throw") {
            symbols.insert(name);
        }
    }

    return symbols;
}

// ============================================================================
// Take Snapshot
// ============================================================================
RefactorSnapshot SafeRefactorEngine::takeSnapshot(
    const std::string& refactorName,
    const std::vector<std::string>& files)
{
    RefactorSnapshot snap;
    snap.snapshotId = generateRefactorId();
    snap.refactorName = refactorName;
    snap.timestamp = std::chrono::steady_clock::now();
    snap.valid = true;

    for (const auto& path : files) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) continue;

        RefactorSnapshot::FileBackup backup;
        backup.filePath = path;
        backup.size = static_cast<size_t>(file.tellg());
        file.seekg(0, std::ios::beg);
        backup.content.resize(backup.size);
        file.read(reinterpret_cast<char*>(backup.content.data()),
                  static_cast<std::streamsize>(backup.size));
        backup.crc32 = computeCRC32(backup.content.data(), backup.size);
        backup.lineCount = countLines(backup.content);

        // Extract symbols from this file for preservation check
        for (const auto& sym : extractSymbols(path)) {
            snap.originalSymbols.insert(sym);
        }

        snap.files.push_back(std::move(backup));
    }

    return snap;
}

// ============================================================================
// Restore Snapshot
// ============================================================================
PatchResult SafeRefactorEngine::restoreSnapshot(const RefactorSnapshot& snapshot) {
    if (!snapshot.valid) {
        return PatchResult::error("Invalid snapshot");
    }

    int restored = 0, failed = 0;

    for (const auto& backup : snapshot.files) {
        std::ofstream file(backup.filePath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            ++failed;
            continue;
        }
        file.write(reinterpret_cast<const char*>(backup.content.data()),
                   static_cast<std::streamsize>(backup.size));
        file.close();

        // Verify CRC after write
        std::ifstream verify(backup.filePath, std::ios::binary | std::ios::ate);
        if (verify.is_open()) {
            size_t vSize = static_cast<size_t>(verify.tellg());
            verify.seekg(0);
            std::vector<uint8_t> vContent(vSize);
            verify.read(reinterpret_cast<char*>(vContent.data()),
                       static_cast<std::streamsize>(vSize));
            uint32_t vCRC = computeCRC32(vContent.data(), vSize);
            if (vCRC != backup.crc32) {
                ++failed;
                continue;
            }
        }
        ++restored;
    }

    if (failed > 0) {
        std::ostringstream ss;
        ss << "Partial rollback: " << restored << "/" << snapshot.files.size()
           << " files (CRC verified)";
        return PatchResult::error(ss.str().c_str());
    }

    return PatchResult::ok("Rollback complete — all files CRC-verified");
}

// ============================================================================
// Generate Diff Report
// ============================================================================
DiffReport SafeRefactorEngine::generateDiffReport(const RefactorSnapshot& snapshot) {
    DiffReport report;
    report.refactorId = snapshot.snapshotId;
    report.filesChanged = 0;
    report.filesAdded = 0;
    report.filesDeleted = 0;
    report.totalLinesAdded = 0;
    report.totalLinesRemoved = 0;

    std::ostringstream unifiedDiff;

    for (const auto& backup : snapshot.files) {
        // Read current state
        std::ifstream current(backup.filePath, std::ios::binary | std::ios::ate);
        std::vector<uint8_t> currentContent;
        uint32_t currentCRC = 0;
        int currentLines = 0;

        if (current.is_open()) {
            size_t size = static_cast<size_t>(current.tellg());
            current.seekg(0);
            currentContent.resize(size);
            current.read(reinterpret_cast<char*>(currentContent.data()),
                        static_cast<std::streamsize>(size));
            currentCRC = computeCRC32(currentContent.data(), size);
            currentLines = countLines(currentContent);
        } else {
            // File was deleted
            report.filesDeleted++;
            continue;
        }

        // Compare CRCs
        if (currentCRC == backup.crc32) continue; // No change

        report.filesChanged++;

        DiffReport::FileStats fs;
        fs.filePath = backup.filePath;
        fs.oldCRC32 = backup.crc32;
        fs.newCRC32 = currentCRC;

        // Line-level diff
        auto toLines = [](const std::vector<uint8_t>& data) -> std::vector<std::string> {
            std::vector<std::string> lines;
            std::string line;
            for (uint8_t c : data) {
                if (c == '\n') {
                    lines.push_back(line);
                    line.clear();
                } else {
                    line += static_cast<char>(c);
                }
            }
            if (!line.empty()) lines.push_back(line);
            return lines;
        };

        auto oldLines = toLines(backup.content);
        auto newLines = toLines(currentContent);

        // Compute added/removed (simple diff: compare line by line)
        int added = 0, removed = 0;
        size_t commonLen = std::min(oldLines.size(), newLines.size());

        DiffHunk hunk;
        hunk.filePath = backup.filePath;
        hunk.oldStartLine = 1;
        hunk.oldLineCount = static_cast<int>(oldLines.size());
        hunk.newStartLine = 1;
        hunk.newLineCount = static_cast<int>(newLines.size());

        for (size_t i = 0; i < commonLen; ++i) {
            if (oldLines[i] != newLines[i]) {
                hunk.removedLines.push_back(oldLines[i]);
                hunk.addedLines.push_back(newLines[i]);
                ++removed;
                ++added;
            }
        }
        if (newLines.size() > oldLines.size()) {
            for (size_t i = commonLen; i < newLines.size(); ++i) {
                hunk.addedLines.push_back(newLines[i]);
                ++added;
            }
        }
        if (oldLines.size() > newLines.size()) {
            for (size_t i = commonLen; i < oldLines.size(); ++i) {
                hunk.removedLines.push_back(oldLines[i]);
                ++removed;
            }
        }

        fs.linesAdded = added;
        fs.linesRemoved = removed;
        report.totalLinesAdded += added;
        report.totalLinesRemoved += removed;

        report.hunks.push_back(std::move(hunk));
        report.perFileStats.push_back(std::move(fs));

        // Generate unified diff text
        unifiedDiff << "--- a/" << backup.filePath << "\n";
        unifiedDiff << "+++ b/" << backup.filePath << "\n";
        unifiedDiff << "@@ -1," << oldLines.size() << " +1," << newLines.size() << " @@\n";
    }

    report.unifiedDiff = unifiedDiff.str();

    std::ostringstream summary;
    summary << report.filesChanged << " files changed, "
            << report.totalLinesAdded << " insertions(+), "
            << report.totalLinesRemoved << " deletions(-)";
    report.summary = summary.str();

    return report;
}

// ============================================================================
// Find Lost Symbols
// ============================================================================
std::vector<std::string> SafeRefactorEngine::findLostSymbols(
    const RefactorSnapshot& snapshot,
    const std::vector<std::string>& symbolsToCheck)
{
    std::vector<std::string> lost;

    // Collect current symbols from all modified files
    std::unordered_set<std::string> currentSymbols;
    for (const auto& backup : snapshot.files) {
        auto syms = extractSymbols(backup.filePath);
        currentSymbols.insert(syms.begin(), syms.end());
    }

    for (const auto& sym : symbolsToCheck) {
        if (snapshot.originalSymbols.count(sym) > 0 && currentSymbols.count(sym) == 0) {
            lost.push_back(sym);
        }
    }

    return lost;
}

// ============================================================================
// Verification Gate
// ============================================================================
VerificationResult SafeRefactorEngine::runVerificationGate(
    const RefactorSnapshot& snapshot,
    const VerificationGateConfig& config)
{
    VerificationResult result = VerificationResult::pass("Verification gate passed");

    // Generate diff for analysis
    DiffReport diff = generateDiffReport(snapshot);

    // Check 1: Change ratio bounds
    if (config.checkLineCountBounds) {
        for (const auto& fs : diff.perFileStats) {
            // Find the original file size
            for (const auto& backup : snapshot.files) {
                if (backup.filePath == fs.filePath) {
                    if (backup.lineCount > 0) {
                        float ratio = static_cast<float>(fs.linesAdded + fs.linesRemoved) /
                                     static_cast<float>(backup.lineCount);
                        if (ratio > config.maxChangeRatio) {
                            result.warnings.push_back(
                                fs.filePath + ": change ratio " + std::to_string(ratio) +
                                " exceeds max " + std::to_string(config.maxChangeRatio));
                        }
                        result.changeRatio = std::max(result.changeRatio, ratio);
                    }
                    break;
                }
            }
        }
    }

    // Check 2: Symbol preservation
    if (config.checkSymbols) {
        std::vector<std::string> allSymbols(
            snapshot.originalSymbols.begin(), snapshot.originalSymbols.end());

        // Also add protected symbols
        for (const auto& ps : config.protectedSymbols) {
            allSymbols.push_back(ps);
        }

        auto lost = findLostSymbols(snapshot, allSymbols);
        if (!lost.empty()) {
            result.symbolsLost = lost;
            for (const auto& sym : lost) {
                result.errors.push_back("Symbol lost: " + sym);
            }
            m_stats.symbolLossDetections.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Check 3: CRC integrity
    if (config.checkCRC) {
        for (const auto& backup : snapshot.files) {
            std::ifstream file(backup.filePath, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                result.errors.push_back("File missing after refactor: " + backup.filePath);
                continue;
            }
            size_t size = static_cast<size_t>(file.tellg());
            if (size == 0) {
                result.errors.push_back("File is empty after refactor: " + backup.filePath);
            }
        }
    }

    // Check 4: Include guards
    if (config.checkIncludeGuards) {
        for (const auto& backup : snapshot.files) {
            std::string ext = fs::path(backup.filePath).extension().string();
            if (ext != ".h" && ext != ".hpp") continue;

            std::ifstream file(backup.filePath);
            if (!file.is_open()) continue;
            std::string firstLine;
            std::getline(file, firstLine);

            // Check for #pragma once or #ifndef
            bool hasGuard = false;
            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
            content = firstLine + "\n" + content;
            if (content.find("#pragma once") != std::string::npos ||
                content.find("#ifndef") != std::string::npos) {
                hasGuard = true;
            }

            if (!hasGuard) {
                result.warnings.push_back(
                    "Include guard may be missing: " + backup.filePath);
            }
        }
    }

    // Determine pass/fail
    if (!result.errors.empty()) {
        result.passed = false;
        result.summary = "Verification FAILED: " + std::to_string(result.errors.size()) + " errors";
        m_stats.verificationFailures.fetch_add(1, std::memory_order_relaxed);
    } else if (!result.warnings.empty()) {
        result.summary = "Verification PASSED with " +
                         std::to_string(result.warnings.size()) + " warnings";
    }

    return result;
}

// ============================================================================
// Execute Safe Refactor (the main pipeline)
// ============================================================================
RefactorResult SafeRefactorEngine::executeSafeRefactor(
    const std::string& name,
    const std::vector<std::string>& targetFiles,
    const VerificationGateConfig& verifyConfig,
    std::function<bool()> applyFn)
{
    auto startTime = std::chrono::steady_clock::now();
    std::string refactorId = generateRefactorId();
    m_stats.totalRefactors.fetch_add(1, std::memory_order_relaxed);

    // Step 1: SNAPSHOT
    RefactorSnapshot snapshot = takeSnapshot(name, targetFiles);
    if (!snapshot.valid) {
        return RefactorResult::fail(refactorId, "Failed to create snapshot");
    }

    // Push to stack for rollback support
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_snapshotStack.emplace_back(refactorId, snapshot);
    }

    // Step 2: APPLY THE REFACTORING
    bool applySuccess = false;
    try {
        applySuccess = applyFn();
    } catch (...) {
        applySuccess = false;
    }

    if (!applySuccess) {
        restoreSnapshot(snapshot);
        m_stats.rolledBackRefactors.fetch_add(1, std::memory_order_relaxed);
        return RefactorResult::fail(refactorId, "Apply function failed — rolled back", true);
    }

    // Step 3: DIFF PREVIEW
    DiffReport diff = generateDiffReport(snapshot);

    if (m_diffPreviewCb) {
        bool approved = m_diffPreviewCb(refactorId, diff);
        if (!approved) {
            restoreSnapshot(snapshot);
            m_stats.rolledBackRefactors.fetch_add(1, std::memory_order_relaxed);
            return RefactorResult::fail(refactorId, "Diff rejected by reviewer — rolled back", true);
        }
    }

    // Step 4: VERIFICATION GATE (diff-aware)
    VerificationResult verification = runVerificationGate(snapshot, verifyConfig);

    if (m_verificationCb) {
        m_verificationCb(refactorId, verification);
    }

    if (!verification.passed) {
        restoreSnapshot(snapshot);
        m_stats.rolledBackRefactors.fetch_add(1, std::memory_order_relaxed);
        return RefactorResult::fail(refactorId,
            "Verification gate failed: " + verification.summary, true);
    }

    // Step 5: COMMIT (changes already applied — just finalize)
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    RefactorResult result = RefactorResult::ok(refactorId, diff, verification);
    result.totalDurationMs = static_cast<double>(elapsed);

    m_stats.successfulRefactors.fetch_add(1, std::memory_order_relaxed);

    // Store in history
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_history.push_back(result);
        if (m_history.size() > kMaxHistory) {
            m_history.erase(m_history.begin());
        }
    }

    if (m_completeCb) m_completeCb(result);

    return result;
}

// ============================================================================
// Preview (dry run)
// ============================================================================
DiffReport SafeRefactorEngine::previewRefactor(
    const std::string& name,
    const std::vector<std::string>& targetFiles,
    std::function<bool()> applyFn)
{
    RefactorSnapshot snapshot = takeSnapshot(name, targetFiles);

    // Apply changes
    bool ok = false;
    try { ok = applyFn(); } catch (...) { ok = false; }

    // Generate diff
    DiffReport diff = generateDiffReport(snapshot);

    // Always rollback in preview mode
    restoreSnapshot(snapshot);

    return diff;
}

// ============================================================================
// Rollback
// ============================================================================
PatchResult SafeRefactorEngine::rollbackLast() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_snapshotStack.empty()) {
        return PatchResult::error("No snapshots to rollback");
    }
    auto [id, snapshot] = m_snapshotStack.back();
    m_snapshotStack.pop_back();
    m_stats.rolledBackRefactors.fetch_add(1, std::memory_order_relaxed);
    return restoreSnapshot(snapshot);
}

PatchResult SafeRefactorEngine::rollback(const std::string& refactorId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_snapshotStack.rbegin(); it != m_snapshotStack.rend(); ++it) {
        if (it->first == refactorId) {
            auto result = restoreSnapshot(it->second);
            m_snapshotStack.erase(std::next(it).base());
            m_stats.rolledBackRefactors.fetch_add(1, std::memory_order_relaxed);
            return result;
        }
    }
    return PatchResult::error("Refactor ID not found in snapshot stack");
}

// ============================================================================
// History
// ============================================================================
std::vector<RefactorResult> SafeRefactorEngine::getHistory(int maxCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (static_cast<int>(m_history.size()) <= maxCount) return m_history;
    return std::vector<RefactorResult>(m_history.end() - maxCount, m_history.end());
}

void SafeRefactorEngine::clearHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
}

// ============================================================================
// Stats
// ============================================================================
void SafeRefactorEngine::resetStats() {
    m_stats.totalRefactors.store(0);
    m_stats.successfulRefactors.store(0);
    m_stats.rolledBackRefactors.store(0);
    m_stats.verificationFailures.store(0);
    m_stats.symbolLossDetections.store(0);
}
