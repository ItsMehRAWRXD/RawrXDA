#pragma once
#ifndef MULTIFILE_SESSION_HPP
#define MULTIFILE_SESSION_HPP

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
//  Enums & Structs
// ─────────────────────────────────────────────────────────────────────────────

enum class EditStatus { Pending, Applied, Rejected, Conflicted };

struct FileDelta {
    std::string file;
    int         startLine;       // 1-based
    int         endLine;         // 1-based, inclusive
    std::string originalContent; // what those lines currently look like
    std::string newContent;      // replacement text
    std::string description;
    EditStatus  status;
    uint64_t    timestamp;
    std::string sourceAgentId;
};

struct EditSession {
    std::string            sessionId;
    std::string            userPrompt;
    std::vector<FileDelta> deltas;
    uint64_t               createdAt;
    uint64_t               updatedAt;
    int                    appliedCount;
    int                    rejectedCount;
    bool                   closed;
};

class MultiFileSession {
public:
    static MultiFileSession& instance();
    std::string              beginSession(const std::string& userPrompt);
    void                     addDelta(const std::string& sessionId, FileDelta delta);
    bool                     applyDelta(const std::string& sessionId, size_t deltaIndex);
    bool                     applyAll(const std::string& sessionId);
    bool                     rejectDelta(const std::string& sessionId, size_t deltaIndex);
    bool                     rejectAll(const std::string& sessionId);
    void                     closeSession(const std::string& sessionId);
    EditSession*             getSession(const std::string& sessionId);
    std::vector<EditSession> getAllSessions() const;
    std::vector<FileDelta>   getPendingDeltas(const std::string& sessionId) const;
    bool                     validateDelta(const FileDelta& delta) const;
    std::string              diffDelta(const FileDelta& delta) const;
    void                     setOnApplied(std::function<void(const FileDelta&)> cb);
    void                     setOnRejected(std::function<void(const FileDelta&)> cb);

private:
    MultiFileSession();
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // MULTIFILE_SESSION_HPP

// ─────────────────────────────────────────────────────────────────────────────
//  Free helpers
// ─────────────────────────────────────────────────────────────────────────────

// Split a string into lines (strips trailing \r).
static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream ss(text);
    std::string l;
    while (std::getline(ss, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    return lines;
}

// Read all lines from a file. Returns false on error.
static bool readFileLines(const std::string& path,
                          std::vector<std::string>& lines) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string l;
    while (std::getline(ifs, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        lines.push_back(l);
    }
    return true;
}

// Write lines back to a file (LF endings).
static bool writeFileLines(const std::string& path,
                           const std::vector<std::string>& lines) {
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;
    for (auto& l : lines) {
        ofs << l << '\n';
    }
    return ofs.good();
}

// Join lines into a single string (no trailing newline on last line).
static std::string joinLines(const std::vector<std::string>& lines) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 < lines.size()) out += '\n';
    }
    return out;
}

// Generate a session ID: hex of tick + 4 hex random nibbles.
static std::string generateSessionId() {
    uint64_t tick = GetTickCount64();
    // Use CryptGenRandom if available; fall back to rand for 4 nibbles.
    BYTE rnd[2] = {};
    HCRYPTPROV hProv = 0;
    if (CryptAcquireContextW(&hProv, nullptr, nullptr,
                             PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, sizeof(rnd), rnd);
        CryptReleaseContext(hProv, 0);
    } else {
        rnd[0] = static_cast<BYTE>(rand() & 0xFF);
        rnd[1] = static_cast<BYTE>(rand() & 0xFF);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%016llX%04X",
             static_cast<unsigned long long>(tick),
             static_cast<unsigned>(rnd[0] << 8 | rnd[1]));
    return std::string(buf);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pimpl body
// ─────────────────────────────────────────────────────────────────────────────

struct MultiFileSession::Impl {
    std::unordered_map<std::string, EditSession> sessions_;
    mutable std::mutex                           mutex_;
    std::function<void(const FileDelta&)>        onApplied_;
    std::function<void(const FileDelta&)>        onRejected_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  MultiFileSession public API
// ─────────────────────────────────────────────────────────────────────────────

MultiFileSession::MultiFileSession()
    : m_impl(std::make_unique<Impl>()) {}

MultiFileSession& MultiFileSession::instance() {
    static MultiFileSession singleton;
    return singleton;
}

void MultiFileSession::setOnApplied(std::function<void(const FileDelta&)> cb) {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    m_impl->onApplied_ = std::move(cb);
}

void MultiFileSession::setOnRejected(std::function<void(const FileDelta&)> cb) {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    m_impl->onRejected_ = std::move(cb);
}

std::string MultiFileSession::beginSession(const std::string& userPrompt) {
    std::string id = generateSessionId();
    EditSession sess{};
    sess.sessionId    = id;
    sess.userPrompt   = userPrompt;
    sess.createdAt    = GetTickCount64();
    sess.updatedAt    = sess.createdAt;
    sess.appliedCount = 0;
    sess.rejectedCount= 0;
    sess.closed       = false;

    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    m_impl->sessions_.emplace(id, std::move(sess));
    return id;
}

void MultiFileSession::addDelta(const std::string& sessionId, FileDelta delta) {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    auto it = m_impl->sessions_.find(sessionId);
    if (it == m_impl->sessions_.end()) return;
    if (it->second.closed) return;

    delta.timestamp = GetTickCount64();
    delta.status    = EditStatus::Pending;
    it->second.deltas.push_back(std::move(delta));
    it->second.updatedAt = GetTickCount64();
}

bool MultiFileSession::validateDelta(const FileDelta& delta) const {
    // File must exist
    std::error_code ec;
    if (!std::filesystem::exists(delta.file, ec)) return false;

    std::vector<std::string> fileLines;
    if (!readFileLines(delta.file, fileLines)) return false;

    int totalLines = static_cast<int>(fileLines.size());

    // Line numbers are 1-based
    if (delta.startLine < 1 || delta.endLine < delta.startLine) return false;
    if (delta.endLine > totalLines) return false;

    // Extract the specified range
    std::vector<std::string> rangeLines(
        fileLines.begin() + (delta.startLine - 1),
        fileLines.begin() + delta.endLine);
    std::string rangeText = joinLines(rangeLines);

    // Compare to originalContent (normalise line endings)
    std::string orig = delta.originalContent;
    // Strip trailing \r\n artefacts before comparison
    while (!orig.empty() && (orig.back() == '\n' || orig.back() == '\r'))
        orig.pop_back();
    while (!rangeText.empty() &&
           (rangeText.back() == '\n' || rangeText.back() == '\r'))
        rangeText.pop_back();

    return orig == rangeText;
}

bool MultiFileSession::applyDelta(const std::string& sessionId, size_t deltaIndex) {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    auto it = m_impl->sessions_.find(sessionId);
    if (it == m_impl->sessions_.end()) return false;
    EditSession& sess = it->second;
    if (deltaIndex >= sess.deltas.size()) return false;
    FileDelta& delta = sess.deltas[deltaIndex];
    if (delta.status != EditStatus::Pending) return false;

    // ── validate (file exists + content matches) ──────────────────────────
    {
        std::error_code ec;
        if (!std::filesystem::exists(delta.file, ec)) {
            delta.status = EditStatus::Conflicted;
            return false;
        }
    }

    std::vector<std::string> fileLines;
    if (!readFileLines(delta.file, fileLines)) {
        delta.status = EditStatus::Conflicted;
        return false;
    }

    int totalLines = static_cast<int>(fileLines.size());
    if (delta.startLine < 1 || delta.endLine < delta.startLine ||
        delta.endLine > totalLines) {
        delta.status = EditStatus::Conflicted;
        return false;
    }

    // Verify original content matches
    std::vector<std::string> existingRange(
        fileLines.begin() + (delta.startLine - 1),
        fileLines.begin() + delta.endLine);
    std::string existing = joinLines(existingRange);
    std::string expected = delta.originalContent;
    // Normalise trailing whitespace for comparison
    while (!existing.empty() && (existing.back() == '\n' || existing.back() == '\r'))
        existing.pop_back();
    while (!expected.empty() && (expected.back() == '\n' || expected.back() == '\r'))
        expected.pop_back();

    if (existing != expected) {
        delta.status = EditStatus::Conflicted;
        return false;
    }

    // ── splice in the new content ─────────────────────────────────────────
    std::vector<std::string> newLines = splitLines(delta.newContent);

    // Erase [startLine-1 .. endLine-1] (0-based)
    fileLines.erase(fileLines.begin() + (delta.startLine - 1),
                    fileLines.begin() + delta.endLine);

    // Insert replacement lines at the same position
    fileLines.insert(fileLines.begin() + (delta.startLine - 1),
                     newLines.begin(), newLines.end());

    // Write back
    if (!writeFileLines(delta.file, fileLines)) {
        delta.status = EditStatus::Conflicted;
        return false;
    }

    delta.status = EditStatus::Applied;
    delta.timestamp = GetTickCount64();
    ++sess.appliedCount;
    sess.updatedAt = GetTickCount64();

    // Fire callback (copy delta before releasing lock is fine here since we
    // invoke after the state is stable)
    if (m_impl->onApplied_) {
        FileDelta copy = delta;
        // Unlock not possible here without restructuring — call under lock.
        // Callback must not re-enter MultiFileSession.
        m_impl->onApplied_(copy);
    }
    return true;
}

bool MultiFileSession::applyAll(const std::string& sessionId) {
    // Collect pending indices first (under lock), then apply one by one.
    std::vector<size_t> pending;
    {
        std::lock_guard<std::mutex> lk(m_impl->mutex_);
        auto it = m_impl->sessions_.find(sessionId);
        if (it == m_impl->sessions_.end()) return false;
        auto& deltas = it->second.deltas;
        for (size_t i = 0; i < deltas.size(); ++i)
            if (deltas[i].status == EditStatus::Pending)
                pending.push_back(i);
    }

    bool allOk = true;
    for (size_t idx : pending) {
        if (!applyDelta(sessionId, idx))
            allOk = false;
    }
    return allOk;
}

bool MultiFileSession::rejectDelta(const std::string& sessionId, size_t deltaIndex) {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    auto it = m_impl->sessions_.find(sessionId);
    if (it == m_impl->sessions_.end()) return false;
    EditSession& sess = it->second;
    if (deltaIndex >= sess.deltas.size()) return false;
    FileDelta& delta = sess.deltas[deltaIndex];
    if (delta.status != EditStatus::Pending) return false;

    delta.status    = EditStatus::Rejected;
    delta.timestamp = GetTickCount64();
    ++sess.rejectedCount;
    sess.updatedAt = GetTickCount64();

    if (m_impl->onRejected_) {
        FileDelta copy = delta;
        m_impl->onRejected_(copy);
    }
    return true;
}

bool MultiFileSession::rejectAll(const std::string& sessionId) {
    std::vector<size_t> pending;
    {
        std::lock_guard<std::mutex> lk(m_impl->mutex_);
        auto it = m_impl->sessions_.find(sessionId);
        if (it == m_impl->sessions_.end()) return false;
        auto& deltas = it->second.deltas;
        for (size_t i = 0; i < deltas.size(); ++i)
            if (deltas[i].status == EditStatus::Pending)
                pending.push_back(i);
    }
    bool allOk = true;
    for (size_t idx : pending) {
        if (!rejectDelta(sessionId, idx)) allOk = false;
    }
    return allOk;
}

void MultiFileSession::closeSession(const std::string& sessionId) {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    auto it = m_impl->sessions_.find(sessionId);
    if (it == m_impl->sessions_.end()) return;
    it->second.closed    = true;
    it->second.updatedAt = GetTickCount64();
}

EditSession* MultiFileSession::getSession(const std::string& sessionId) {
    // NOTE: caller must not store this pointer across calls that modify sessions_.
    auto it = m_impl->sessions_.find(sessionId);
    if (it == m_impl->sessions_.end()) return nullptr;
    return &it->second;
}

std::vector<EditSession> MultiFileSession::getAllSessions() const {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    std::vector<EditSession> out;
    out.reserve(m_impl->sessions_.size());
    for (auto& kv : m_impl->sessions_)
        out.push_back(kv.second);
    return out;
}

std::vector<FileDelta> MultiFileSession::getPendingDeltas(
    const std::string& sessionId) const {
    std::lock_guard<std::mutex> lk(m_impl->mutex_);
    auto it = m_impl->sessions_.find(sessionId);
    if (it == m_impl->sessions_.end()) return {};
    std::vector<FileDelta> out;
    for (auto& d : it->second.deltas)
        if (d.status == EditStatus::Pending)
            out.push_back(d);
    return out;
}

std::string MultiFileSession::diffDelta(const FileDelta& delta) const {
    // Unified diff format:
    //   --- file
    //   +++ file
    //   @@ -startLine,oldCount +startLine,newCount @@
    //   -old lines...
    //   +new lines...

    std::vector<std::string> oldLines = splitLines(delta.originalContent);
    std::vector<std::string> newLines = splitLines(delta.newContent);

    int oldCount = static_cast<int>(oldLines.size());
    int newCount = static_cast<int>(newLines.size());

    std::ostringstream oss;
    oss << "--- " << delta.file << "\n";
    oss << "+++ " << delta.file << "\n";
    oss << "@@ -" << delta.startLine << "," << oldCount
        << " +" << delta.startLine << "," << newCount << " @@";
    if (!delta.description.empty())
        oss << " " << delta.description;
    oss << "\n";

    for (auto& l : oldLines) oss << "-" << l << "\n";
    for (auto& l : newLines) oss << "+" << l << "\n";

    return oss.str();
}
