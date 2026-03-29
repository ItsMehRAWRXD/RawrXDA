#include "trace_log.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>

namespace RawrXD {
namespace DAE {

namespace {

std::string HashToHex(const ContentHash& h) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.resize(64);
    for (int i = 0; i < 32; ++i) {
        out[2 * i] = kHex[(h.bytes[i] >> 4) & 0xF];
        out[2 * i + 1] = kHex[h.bytes[i] & 0xF];
    }
    return out;
}

bool HexToHash(std::string_view s, ContentHash& out) {
    if (s.size() != 64) return false;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    for (int i = 0; i < 32; ++i) {
        int hi = nib(s[2 * i]);
        int lo = nib(s[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

ContentHash EventHash(const TraceEvent& e) {
    std::ostringstream os;
    os << e.seq << '|' << e.timestampUs << '|' << e.opTypeName << '|' << e.targetPath << '|'
       << HashToHex(e.stateBeforeHash) << '|' << HashToHex(e.actionDigest) << '|'
       << HashToHex(e.stateAfterHash) << '|' << HashToHex(e.prevEventHash) << '|'
       << (e.succeeded ? 1 : 0);
    return ShadowFilesystem::ComputeHash(os.str());
}

std::string EncodeLine(const TraceEvent& e) {
    std::ostringstream os;
    os << e.seq << '\t' << e.timestampUs << '\t' << e.opTypeName << '\t' << e.targetPath << '\t'
       << HashToHex(e.stateBeforeHash) << '\t' << HashToHex(e.actionDigest) << '\t'
       << HashToHex(e.stateAfterHash) << '\t' << HashToHex(e.prevEventHash) << '\t'
       << (e.succeeded ? 1 : 0);
    return os.str();
}

bool DecodeLine(const std::string& line, TraceEvent& out) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, '\t')) parts.push_back(item);
    if (parts.size() != 9) return false;

    char* end0 = nullptr;
    char* end1 = nullptr;
    out.seq = std::strtoull(parts[0].c_str(), &end0, 10);
    out.timestampUs = std::strtoull(parts[1].c_str(), &end1, 10);
    if (end0 == nullptr || *end0 != '\0' || end1 == nullptr || *end1 != '\0') {
        return false;
    }

    out.opTypeName = parts[2];
    out.targetPath = parts[3];
    if (!HexToHash(parts[4], out.stateBeforeHash)) return false;
    if (!HexToHash(parts[5], out.actionDigest)) return false;
    if (!HexToHash(parts[6], out.stateAfterHash)) return false;
    if (!HexToHash(parts[7], out.prevEventHash)) return false;
    out.succeeded = (parts[8] == "1");
    return true;
}

} // namespace

struct TraceLog::Impl {
    mutable std::mutex  mu;
    std::vector<TraceEvent> events;
    std::string         path;
    bool                open = false;
    ContentHash         lastEventHash{};
};

TraceResult<void> TraceLog::Open(std::string_view logPath) {
    m_impl = std::make_unique<Impl>();
    m_impl->path = std::string(logPath);
    m_impl->events.clear();

    // Start fresh session log by truncating file.
    std::ofstream out(m_impl->path, std::ios::trunc);
    if (!out.is_open()) {
        return std::unexpected(TraceError::IoError);
    }
    m_impl->open = true;
    return {};
}

void TraceLog::Close() {
    if (!m_impl) return;
    std::lock_guard<std::mutex> lock(m_impl->mu);
    m_impl->open = false;
}

bool TraceLog::IsOpen() const noexcept {
    return m_impl && m_impl->open;
}

TraceResult<void> TraceLog::Append(TraceEvent event) {
    if (!m_impl || !m_impl->open) {
        return std::unexpected(TraceError::AlreadyClosed);
    }

    std::lock_guard<std::mutex> lock(m_impl->mu);

    event.prevEventHash = m_impl->events.empty() ? ContentHash::zero() : m_impl->lastEventHash;
    if (event.seq == 0) {
        event.seq = static_cast<uint64_t>(m_impl->events.size() + 1);
    }

    std::ofstream out(m_impl->path, std::ios::app);
    if (!out.is_open()) {
        return std::unexpected(TraceError::IoError);
    }
    out << EncodeLine(event) << "\n";

    m_impl->lastEventHash = EventHash(event);
    m_impl->events.push_back(std::move(event));
    return {};
}

TraceResult<std::vector<TraceEvent>> TraceLog::ReadAll() const {
    if (!m_impl) {
        return std::vector<TraceEvent>{};
    }
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->events;
}

ContentHash TraceLog::LastEventHash() const noexcept {
    if (!m_impl) return ContentHash::zero();
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return m_impl->lastEventHash;
}

uint64_t TraceLog::EventCount() const noexcept {
    if (!m_impl) return 0;
    std::lock_guard<std::mutex> lock(m_impl->mu);
    return static_cast<uint64_t>(m_impl->events.size());
}

TraceResult<void> TraceLog::VerifyChain() const {
    if (!m_impl) return {};
    std::lock_guard<std::mutex> lock(m_impl->mu);

    ContentHash prev = ContentHash::zero();
    for (const auto& e : m_impl->events) {
        if (e.prevEventHash != prev) {
            return std::unexpected(TraceError::Corrupted);
        }
        prev = EventHash(e);
    }
    return {};
}

ContentHash TraceLog::StructuralFingerprint() const {
    if (!m_impl) return ContentHash::zero();
    std::lock_guard<std::mutex> lock(m_impl->mu);

    std::ostringstream os;
    for (const auto& e : m_impl->events) {
        os << e.opTypeName << ':' << (e.succeeded ? '1' : '0') << ';';
    }
    return ShadowFilesystem::ComputeHash(os.str());
}

void TraceLog::IndexForLookup() {
    // In-memory indexing is represented by StructuralFingerprint().
}

bool TraceLog::MatchesFingerprint(const ContentHash& fp) const noexcept {
    return StructuralFingerprint() == fp;
}

TraceResult<void> TraceIndex::LoadTrace(std::string_view logPath) {
    std::ifstream in(std::string(logPath));
    if (!in.is_open()) {
        return std::unexpected(TraceError::IoError);
    }

    bool allSucceeded = true;
    std::ostringstream sig;
    std::string line;
    while (std::getline(in, line)) {
        TraceEvent e;
        if (!DecodeLine(line, e)) {
            return std::unexpected(TraceError::Corrupted);
        }
        sig << e.opTypeName << ':' << (e.succeeded ? '1' : '0') << ';';
        allSucceeded = allSucceeded && e.succeeded;
    }

    Entry entry;
    entry.path = std::string(logPath);
    entry.fingerprint = ShadowFilesystem::ComputeHash(sig.str());
    entry.allSucceeded = allSucceeded;
    m_entries.push_back(std::move(entry));
    return {};
}

std::string TraceIndex::FindSimilar(const ContentHash& fingerprint) const {
    for (const auto& e : m_entries) {
        if (e.fingerprint == fingerprint && e.allSucceeded) {
            return e.path;
        }
    }
    return {};
}

size_t TraceIndex::TracesLoaded() const noexcept {
    return m_entries.size();
}

} // namespace DAE
} // namespace RawrXD
