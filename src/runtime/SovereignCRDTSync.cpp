// =============================================================================
// SovereignCRDTSync.cpp — Phase 51: Log-append CRDT with binary delta codec
// =============================================================================
// Convergence strategy: Last-Write-Wins (LWW) ordered by monotonic timestamp.
//
// Delta wire format (little-endian):
//   [version:1=0x01][key_len:2][key:N][val_len:2][val:M][ts:8][counter:4]
//
// Full export format:
//   [entry_count:4][entry0][entry1]...
// =============================================================================
#include "SovereignCRDTSync.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>

namespace RawrXD::Runtime {

// ---------------------------------------------------------------------------
// Internal wire encoding helpers
// ---------------------------------------------------------------------------
static void appendU8(std::vector<uint8_t>& buf, uint8_t v)  { buf.push_back(v); }
static void appendU16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(v & 0xFF);
    buf.push_back((v >> 8) & 0xFF);
}
static void appendU32(std::vector<uint8_t>& buf, uint32_t v) {
    for (int i = 0; i < 4; ++i) buf.push_back((v >> (i*8)) & 0xFF);
}
static void appendU64(std::vector<uint8_t>& buf, uint64_t v) {
    for (int i = 0; i < 8; ++i) buf.push_back((v >> (i*8)) & 0xFF);
}
static void appendStr(std::vector<uint8_t>& buf, const std::string& s, size_t maxLen = 65535) {
    uint16_t len = static_cast<uint16_t>(std::min(s.size(), maxLen));
    appendU16(buf, len);
    buf.insert(buf.end(), s.begin(), s.begin() + len);
}

// Decode single entry from wire buffer; advance *pos.  Returns false on underrun.
static bool decodeEntry(const std::vector<uint8_t>& buf, size_t& pos,
                        std::string& key, std::string& val,
                        uint64_t& ts, uint32_t& counter) {
    // version byte
    if (pos + 1 > buf.size()) return false;
    if (buf[pos++] != 0x01) return false;
    // key
    if (pos + 2 > buf.size()) return false;
    uint16_t klen = static_cast<uint16_t>(buf[pos]) | (static_cast<uint16_t>(buf[pos+1]) << 8);
    pos += 2;
    if (pos + klen > buf.size()) return false;
    key.assign(reinterpret_cast<const char*>(buf.data() + pos), klen);
    pos += klen;
    // value
    if (pos + 2 > buf.size()) return false;
    uint16_t vlen = static_cast<uint16_t>(buf[pos]) | (static_cast<uint16_t>(buf[pos+1]) << 8);
    pos += 2;
    if (pos + vlen > buf.size()) return false;
    val.assign(reinterpret_cast<const char*>(buf.data() + pos), vlen);
    pos += vlen;
    // timestamp (8) + counter (4)
    if (pos + 12 > buf.size()) return false;
    ts = 0;
    for (int i = 0; i < 8; ++i) ts |= (static_cast<uint64_t>(buf[pos+i]) << (i*8));
    pos += 8;
    counter = 0;
    for (int i = 0; i < 4; ++i) counter |= (static_cast<uint32_t>(buf[pos+i]) << (i*8));
    pos += 4;
    return true;
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
SovereignCRDTSync& SovereignCRDTSync::instance() {
    static SovereignCRDTSync inst;
    return inst;
}

// ---------------------------------------------------------------------------
// Local propose — creates a node and appends it to the delta log
// ---------------------------------------------------------------------------
bool SovereignCRDTSync::proposeUpdate(const std::string& key, const std::string& value) {
    if (key.empty() || key.size() > 65535 || value.size() > 65535) return false;
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t now = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() / 1000u);
    ++m_localCounter;

    CRDTNode node{ value, now, m_localCounter };
    m_index[key] = node;

    // Append serialized delta for mesh broadcast
    std::vector<uint8_t> delta;
    delta.reserve(1 + 2 + key.size() + 2 + value.size() + 12);
    appendU8 (delta, 0x01);
    appendStr(delta, key);
    appendStr(delta, value);
    appendU64(delta, now);
    appendU32(delta, m_localCounter);
    m_deltaLog.push_back(std::move(delta));

    return true;
}

// ---------------------------------------------------------------------------
// Merge an inbound remote delta (LWW: highest timestamp wins)
// ---------------------------------------------------------------------------
bool SovereignCRDTSync::mergeRemote(const std::string& key, const std::string& value,
                                    uint64_t timestamp) {
    if (key.empty() || key.size() > 65535 || value.size() > 65535) return false;
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_index.find(key);
    if (it == m_index.end() || timestamp > it->second.LWW_Timestamp) {
        m_index[key] = { value, timestamp, 0 };
        return true;
    }
    return false; // local is newer — reject
}

// ---------------------------------------------------------------------------
// Serialise a single pending delta for broadcast (pops it from the log)
// ---------------------------------------------------------------------------
std::vector<uint8_t> SovereignCRDTSync::nextDelta() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_deltaLog.empty()) return {};
    auto d = std::move(m_deltaLog.front());
    m_deltaLog.pop_front();
    return d;
}

// ---------------------------------------------------------------------------
// Apply an inbound wire delta (single entry as produced by nextDelta/serializeDelta)
// ---------------------------------------------------------------------------
bool SovereignCRDTSync::applyDelta(const std::vector<uint8_t>& delta) {
    size_t pos = 0;
    std::string key, val;
    uint64_t ts = 0;
    uint32_t counter = 0;
    if (!decodeEntry(delta, pos, key, val, ts, counter)) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_index.find(key);
    if (it == m_index.end() || ts > it->second.LWW_Timestamp) {
        m_index[key] = { val, ts, 0 };
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Full state export: [entry_count:4][entry...]
// ---------------------------------------------------------------------------
std::vector<uint8_t> SovereignCRDTSync::exportFull() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<uint8_t> out;
    appendU32(out, static_cast<uint32_t>(m_index.size()));
    for (auto& [k, node] : m_index) {
        appendU8 (out, 0x01);
        appendStr(out, k);
        appendStr(out, node.value);
        appendU64(out, node.LWW_Timestamp);
        appendU32(out, node.vectorCounter);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Import from a full-export blob (idempotent merge)
// ---------------------------------------------------------------------------
bool SovereignCRDTSync::importFull(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return false;
    uint32_t count = 0;
    for (int i = 0; i < 4; ++i) count |= (static_cast<uint32_t>(data[i]) << (i*8));

    std::lock_guard<std::mutex> lock(m_mutex);
    size_t pos = 4;
    for (uint32_t i = 0; i < count; ++i) {
        std::string key, val;
        uint64_t ts = 0;
        uint32_t counter = 0;
        if (!decodeEntry(data, pos, key, val, ts, counter)) return false;
        auto it = m_index.find(key);
        if (it == m_index.end() || ts > it->second.LWW_Timestamp)
            m_index[key] = { val, ts, counter };
    }
    return true;
}

// ---------------------------------------------------------------------------
// Read view
// ---------------------------------------------------------------------------
std::map<std::string, CRDTNode> SovereignCRDTSync::getIndex() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_index;
}

} // namespace RawrXD::Runtime
