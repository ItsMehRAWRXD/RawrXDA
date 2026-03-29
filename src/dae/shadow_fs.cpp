// ============================================================================
// shadow_fs.cpp — DAE Shadow Filesystem Implementation
// ============================================================================
#include "shadow_fs.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <filesystem>

// SHA-256 via Windows CNG — no third-party dep required
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace fs = std::filesystem;

namespace RawrXD {
namespace DAE {

// ============================================================================
// Internal Impl
// ============================================================================

struct ShadowFilesystem::Impl {
    // Overlay: path (repo-relative, fwd slashes) → content bytes
    // A nullopt value means the file is marked deleted.
    std::unordered_map<std::string, std::optional<std::string>> overlay;

    // Base snapshot: path → content hash (for divergence detection on commit)
    std::unordered_map<std::string, ContentHash> baseHashes;

    // Original content cache used for patch rollback
    std::unordered_map<std::string, std::string> baseContentCache;
};

// ============================================================================
// Helpers
// ============================================================================

static std::string NormalisePath(std::string_view raw) {
    std::string p(raw);
    std::replace(p.begin(), p.end(), '\\', '/');
    // Strip leading slash
    if (!p.empty() && p[0] == '/') p.erase(0, 1);
    return p;
}

// ============================================================================
// ContentHash via SHA-256 (CNG)
// ============================================================================

ContentHash ShadowFilesystem::ComputeHash(std::string_view data) noexcept {
    ContentHash result{};

    BCRYPT_ALG_HANDLE  hAlg  = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;

    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return result;

    DWORD hashObjLen = 0, cbData = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH,
                      reinterpret_cast<PUCHAR>(&hashObjLen), sizeof(DWORD), &cbData, 0);

    std::vector<BYTE> hashObj(hashObjLen);
    BCryptCreateHash(hAlg, &hHash, hashObj.data(), hashObjLen, nullptr, 0, 0);

    BCryptHashData(hHash,
                   reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())),
                   static_cast<ULONG>(data.size()), 0);

    BCryptFinishHash(hHash, result.bytes, 32, 0);

    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);

    return result;
}

// ============================================================================
// Line ending normalisation (CRLF/CR → LF)
// ============================================================================

std::string ShadowFilesystem::NormaliseLineEndings(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\r') {
            out += '\n';
            if (i + 1 < raw.size() && raw[i + 1] == '\n')
                ++i;
        } else {
            out += raw[i];
        }
    }
    return out;
}

// ============================================================================
// ShadowFilesystem — lifecycle
// ============================================================================

ShadowFilesystem::ShadowFilesystem() : m_impl(std::make_unique<Impl>()) {}
ShadowFilesystem::~ShadowFilesystem() = default;

ShadowResult<void> ShadowFilesystem::SnapshotBase(std::string_view workspaceRoot) {
    AcquireSRWLockExclusive(&m_lock);
    auto guard = [&]{ ReleaseSRWLockExclusive(&m_lock); };

    m_workspaceRoot = std::string(workspaceRoot);
    m_impl->baseHashes.clear();
    m_impl->overlay.clear();

    std::error_code ec;
    fs::path root(m_workspaceRoot);
    for (auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;

        fs::path rel = fs::relative(entry.path(), root, ec);
        if (ec) continue;

        std::string key = NormalisePath(rel.string());

        // Read and hash
        std::ifstream f(entry.path(), std::ios::binary);
        if (!f.is_open()) continue;
        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        content = NormaliseLineEndings(content);

        m_impl->baseHashes[key]       = ComputeHash(content);
        m_impl->baseContentCache[key] = std::move(content);
    }

    m_baseSealed = true;
    guard();
    return {};
}

void ShadowFilesystem::Reset() {
    AcquireSRWLockExclusive(&m_lock);
    m_impl->overlay.clear();
    m_baseSealed      = false;
    m_replayValidated = false;
    ReleaseSRWLockExclusive(&m_lock);
}

// ============================================================================
// Overlay writes
// ============================================================================

ShadowResult<void> ShadowFilesystem::Write(std::string_view path,
                                            std::string_view content) {
    AcquireSRWLockExclusive(&m_lock);
    auto guard = [&]{ ReleaseSRWLockExclusive(&m_lock); };

    std::string key  = NormalisePath(path);
    std::string norm = NormaliseLineEndings(content);
    m_impl->overlay[key] = std::move(norm);

    guard();
    return {};
}

ShadowResult<void> ShadowFilesystem::Delete(std::string_view path) {
    AcquireSRWLockExclusive(&m_lock);
    std::string key = NormalisePath(path);
    m_impl->overlay[key] = std::nullopt;   // nullopt == deleted
    ReleaseSRWLockExclusive(&m_lock);
    return {};
}

ShadowResult<void> ShadowFilesystem::Move(std::string_view fromPath,
                                           std::string_view toPath) {
    AcquireSRWLockExclusive(&m_lock);
    auto guard = [&]{ ReleaseSRWLockExclusive(&m_lock); };

    std::string src = NormalisePath(fromPath);
    std::string dst = NormalisePath(toPath);

    // Resolve source content: overlay first, then base cache
    std::string content;
    auto oit = m_impl->overlay.find(src);
    if (oit != m_impl->overlay.end()) {
        if (!oit->second.has_value()) {
            guard();
            return std::unexpected(ShadowError::PathNotFound);
        }
        content = *oit->second;
    } else {
        auto bit = m_impl->baseContentCache.find(src);
        if (bit == m_impl->baseContentCache.end()) {
            guard();
            return std::unexpected(ShadowError::PathNotFound);
        }
        content = bit->second;
    }

    m_impl->overlay[dst] = std::move(content);
    m_impl->overlay[src] = std::nullopt;   // mark source deleted

    guard();
    return {};
}

ShadowResult<void> ShadowFilesystem::Patch(std::string_view path,
                                            std::string_view /*unifiedDiff*/) {
    // Full unified-diff application is deferred to a follow-up; for now write
    // is the primary mutation path and Patch is a stub that validates the file
    // exists, which is sufficient for P0 replay.
    if (!Exists(path))
        return std::unexpected(ShadowError::PathNotFound);
    return {};
}

// ============================================================================
// Merged reads
// ============================================================================

ShadowResult<std::string> ShadowFilesystem::Read(std::string_view path) const {
    AcquireSRWLockShared(&const_cast<SRWLOCK&>(m_lock));
    auto guard = [&]{ ReleaseSRWLockShared(&const_cast<SRWLOCK&>(m_lock)); };

    std::string key = NormalisePath(path);

    auto oit = m_impl->overlay.find(key);
    if (oit != m_impl->overlay.end()) {
        if (!oit->second.has_value()) {
            guard();
            return std::unexpected(ShadowError::PathNotFound);   // deleted
        }
        std::string result = *oit->second;
        guard();
        return result;
    }

    auto bit = m_impl->baseContentCache.find(key);
    if (bit != m_impl->baseContentCache.end()) {
        std::string result = bit->second;
        guard();
        return result;
    }

    guard();
    return std::unexpected(ShadowError::PathNotFound);
}

bool ShadowFilesystem::Exists(std::string_view path) const {
    AcquireSRWLockShared(&const_cast<SRWLOCK&>(m_lock));
    auto guard = [&]{ ReleaseSRWLockShared(&const_cast<SRWLOCK&>(m_lock)); };

    std::string key = NormalisePath(path);

    auto oit = m_impl->overlay.find(key);
    if (oit != m_impl->overlay.end()) {
        bool exists = oit->second.has_value();
        guard();
        return exists;
    }

    bool inBase = m_impl->baseContentCache.count(key) > 0;
    guard();
    return inBase;
}

ShadowResult<std::vector<std::string>>
ShadowFilesystem::ListDir(std::string_view dirPath) const {
    AcquireSRWLockShared(&const_cast<SRWLOCK&>(m_lock));
    auto guard = [&]{ ReleaseSRWLockShared(&const_cast<SRWLOCK&>(m_lock)); };

    std::string prefix = NormalisePath(dirPath);
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';

    std::vector<std::string> result;

    auto addIfDirect = [&](const std::string& key) {
        if (key.rfind(prefix, 0) != 0) return;
        std::string_view rest(key.data() + prefix.size(),
                               key.size() - prefix.size());
        auto slash = rest.find('/');
        // Only direct children
        if (slash == std::string_view::npos) {
            std::string entry(rest);
            if (std::find(result.begin(), result.end(), entry) == result.end())
                result.push_back(entry);
        }
    };

    for (auto& [key, _] : m_impl->baseContentCache) addIfDirect(key);

    // Overlay: add new, remove deleted
    for (auto& [key, val] : m_impl->overlay) {
        if (key.rfind(prefix, 0) != 0) continue;
        std::string_view rest(key.data() + prefix.size(),
                               key.size() - prefix.size());
        auto slash = rest.find('/');
        if (slash != std::string_view::npos) continue;

        std::string entry(rest);
        auto it = std::find(result.begin(), result.end(), entry);
        if (!val.has_value()) {
            if (it != result.end()) result.erase(it);  // deleted
        } else {
            if (it == result.end()) result.push_back(entry);  // new
        }
    }

    std::sort(result.begin(), result.end());
    guard();
    return result;
}

// ============================================================================
// Inspection
// ============================================================================

std::vector<FileEntry> ShadowFilesystem::DirtyEntries() const {
    AcquireSRWLockShared(&const_cast<SRWLOCK&>(m_lock));
    auto guard = [&]{ ReleaseSRWLockShared(&const_cast<SRWLOCK&>(m_lock)); };

    std::vector<FileEntry> entries;
    for (auto& [key, val] : m_impl->overlay) {
        FileEntry fe;
        fe.path      = key;
        fe.isDeleted = !val.has_value();
        fe.isNew     = m_impl->baseHashes.count(key) == 0;

        auto bit = m_impl->baseHashes.find(key);
        if (bit != m_impl->baseHashes.end())
            fe.baseHash = bit->second;

        if (val.has_value()) {
            fe.overlayHash = ComputeHash(*val);
            fe.sizeBytes   = val->size();
        }

        entries.push_back(std::move(fe));
    }

    guard();
    return entries;
}

std::optional<ContentHash> ShadowFilesystem::OverlayHash(std::string_view path) const {
    AcquireSRWLockShared(&const_cast<SRWLOCK&>(m_lock));
    auto guard = [&]{ ReleaseSRWLockShared(&const_cast<SRWLOCK&>(m_lock)); };

    std::string key = NormalisePath(path);
    auto it = m_impl->overlay.find(key);
    if (it == m_impl->overlay.end() || !it->second.has_value()) {
        guard();
        return std::nullopt;
    }

    ContentHash h = ComputeHash(*it->second);
    guard();
    return h;
}

// ============================================================================
// Commit
// ============================================================================

void ShadowFilesystem::SetReplayValidated(bool val) noexcept {
    m_replayValidated = val;
}

ShadowResult<void> ShadowFilesystem::Commit() {
    if (!m_replayValidated)
        return std::unexpected(ShadowError::NotCommittable);

    AcquireSRWLockExclusive(&m_lock);
    auto guard = [&]{ ReleaseSRWLockExclusive(&m_lock); };

    fs::path root(m_workspaceRoot);
    struct Rollback { fs::path path; std::string original; bool wasAbsent; };
    std::vector<Rollback> rollbackLog;

    for (auto& [key, val] : m_impl->overlay) {
        fs::path absPath = root / fs::path(key);
        std::string original;
        bool wasAbsent = !fs::exists(absPath);

        if (!wasAbsent) {
            std::ifstream fin(absPath, std::ios::binary);
            original.assign((std::istreambuf_iterator<char>(fin)),
                              std::istreambuf_iterator<char>());
        }

        rollbackLog.push_back({ absPath, std::move(original), wasAbsent });

        if (!val.has_value()) {
            // Delete
            std::error_code ec;
            fs::remove(absPath, ec);
            if (ec) {
                // Rollback everything written so far
                for (auto& rb : rollbackLog) {
                    if (rb.wasAbsent) fs::remove(rb.path);
                    else {
                        std::ofstream fout(rb.path, std::ios::binary | std::ios::trunc);
                        fout.write(rb.original.data(), rb.original.size());
                    }
                }
                guard();
                return std::unexpected(ShadowError::IoError);
            }
        } else {
            // Write
            fs::create_directories(absPath.parent_path());
            std::ofstream fout(absPath, std::ios::binary | std::ios::trunc);
            if (!fout.is_open()) {
                for (auto& rb : rollbackLog) {
                    if (rb.wasAbsent) fs::remove(rb.path);
                    else {
                        std::ofstream r(rb.path, std::ios::binary | std::ios::trunc);
                        r.write(rb.original.data(), rb.original.size());
                    }
                }
                guard();
                return std::unexpected(ShadowError::IoError);
            }
            fout.write(val->data(), val->size());
        }
    }

    // All succeeded — clear overlay
    m_impl->overlay.clear();
    m_replayValidated = false;

    guard();
    return {};
}

} // namespace DAE
} // namespace RawrXD
