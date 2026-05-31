// =============================================================================
// SovereignSnapshot.cpp — Phase 50: Binary slot-based state persistence
// =============================================================================
// Each snapshot slot is written to %TEMP%\rawrxd-snap-<slot>.bin
// File layout:
//   [0..7]   magic   = 0x52584453 4E415000  ("RXDSNAP\0")
//   [8..11]  version = 0x0001
//   [12..15] slot    = slot index
//   [16..23] size    = payload byte count
//   [24..24+size-1]  payload
//   [end-8..end]     fnv64 checksum of bytes [0..end-9]
// =============================================================================
#include "SovereignSnapshot.h"
#include "SovereignCRDTSync.h"
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <mutex>
#include <map>
#include <algorithm>

namespace RawrXD::Runtime {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static const uint64_t kMagic          = 0x52584453'4E415000ULL; // "RXDSNAP\0"
static const uint32_t kVersion        = 0x0001;
static const uint64_t kGenesisRoot    = 0x534F564552454947ULL;  // "SOVEREIG"
static const uint32_t kMaxSlots       = 8;
static const size_t   kMaxPayloadBytes = 4 * 1024 * 1024; // 4 MB guard

// ---------------------------------------------------------------------------
// FNV-1a 64-bit
// ---------------------------------------------------------------------------
static uint64_t fnv1a64(const uint8_t* data, size_t len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; ++i)
        h = (h ^ data[i]) * 0x100000001b3ULL;
    return h;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string snapPath(uint32_t slot) {
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    std::ostringstream ss;
    ss << tmp << "rawrxd-snap-" << slot << ".bin";
    return ss.str();
}

// Write little-endian 64-bit value to a byte buffer
static void writeLE64(std::vector<uint8_t>& buf, uint64_t v) {
    for (int i = 0; i < 8; ++i) buf.push_back(static_cast<uint8_t>((v >> (i*8)) & 0xFF));
}
static void writeLE32(std::vector<uint8_t>& buf, uint32_t v) {
    for (int i = 0; i < 4; ++i) buf.push_back(static_cast<uint8_t>((v >> (i*8)) & 0xFF));
}
// Read little-endian values from buffer at offset
static uint64_t readLE64(const std::vector<uint8_t>& buf, size_t off) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (static_cast<uint64_t>(buf[off+i]) << (i*8));
    return v;
}
static uint32_t readLE32(const std::vector<uint8_t>& buf, size_t off) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= (static_cast<uint32_t>(buf[off+i]) << (i*8));
    return v;
}

// ---------------------------------------------------------------------------
// SovereignSnapshot implementation
// ---------------------------------------------------------------------------
static std::mutex g_snapMutex;

SovereignSnapshot& SovereignSnapshot::instance() {
    static SovereignSnapshot instance;
    return instance;
}

bool SovereignSnapshot::verifyGenesisRoot(uint64_t rootHash) {
    // Accept our own genesis constant; also accept XOR-folded runtime variant
    const uint64_t runtimeVariant = kGenesisRoot ^ static_cast<uint64_t>(GetCurrentProcessId());
    return (rootHash == kGenesisRoot) || (rootHash == runtimeVariant);
}

// Serialize the current CRDT index into a compact binary payload.
// Format per entry: [key_len:2][key:N][val_len:2][val:M][ts:8][counter:4]
static std::vector<uint8_t> serializeCRDT() {
    auto index = SovereignCRDTSync::instance().getIndex();
    std::vector<uint8_t> payload;
    payload.reserve(index.size() * 64);
    for (auto& [key, node] : index) {
        uint16_t klen = static_cast<uint16_t>(std::min(key.size(), size_t{65535}));
        uint16_t vlen = static_cast<uint16_t>(std::min(node.value.size(), size_t{65535}));
        payload.push_back(klen & 0xFF);
        payload.push_back((klen >> 8) & 0xFF);
        payload.insert(payload.end(), key.begin(), key.begin() + klen);
        payload.push_back(vlen & 0xFF);
        payload.push_back((vlen >> 8) & 0xFF);
        payload.insert(payload.end(), node.value.begin(), node.value.begin() + vlen);
        writeLE64(payload, node.LWW_Timestamp);
        writeLE32(payload, node.vectorCounter);
    }
    return payload;
}

bool SovereignSnapshot::captureSnapshot(uint32_t slot) {
    if (slot >= kMaxSlots) return false;
    std::lock_guard<std::mutex> lk(g_snapMutex);

    std::vector<uint8_t> payload = serializeCRDT();
    if (payload.size() > kMaxPayloadBytes) return false;

    // Build file buffer
    std::vector<uint8_t> file;
    file.reserve(24 + payload.size() + 8);
    writeLE64(file, kMagic);
    writeLE32(file, kVersion);
    writeLE32(file, slot);
    writeLE64(file, static_cast<uint64_t>(payload.size()));
    file.insert(file.end(), payload.begin(), payload.end());
    uint64_t checksum = fnv1a64(file.data(), file.size());
    writeLE64(file, checksum);

    std::string path = snapPath(slot);
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs.write(reinterpret_cast<const char*>(file.data()),
              static_cast<std::streamsize>(file.size()));
    return ofs.good();
}

bool SovereignSnapshot::restoreSnapshot(uint32_t slot) {
    if (slot >= kMaxSlots) return false;
    std::lock_guard<std::mutex> lk(g_snapMutex);

    std::string path = snapPath(slot);
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) return false;
    auto fileSize = ifs.tellg();
    if (fileSize < 32 || fileSize > static_cast<std::streamoff>(kMaxPayloadBytes + 32)) return false;
    ifs.seekg(0);
    std::vector<uint8_t> file(static_cast<size_t>(fileSize));
    ifs.read(reinterpret_cast<char*>(file.data()), fileSize);
    if (!ifs) return false;

    // Validate checksum
    uint64_t storedCs = readLE64(file, file.size() - 8);
    uint64_t computed = fnv1a64(file.data(), file.size() - 8);
    if (storedCs != computed) return false;

    // Validate magic, version, slot
    if (readLE64(file, 0) != kMagic) return false;
    if (readLE32(file, 8) != kVersion) return false;
    if (readLE32(file, 12) != slot) return false;

    uint64_t payloadSize = readLE64(file, 16);
    if (24 + payloadSize + 8 != file.size()) return false;
    if (payloadSize > kMaxPayloadBytes) return false;

    // Decode entries and merge into CRDT (remote merge with stored timestamp)
    const uint8_t* p   = file.data() + 24;
    const uint8_t* end = p + payloadSize;
    while (p + 4 <= end) {
        uint16_t klen = static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
        p += 2;
        if (p + klen > end) break;
        std::string key(reinterpret_cast<const char*>(p), klen);
        p += klen;
        if (p + 2 > end) break;
        uint16_t vlen = static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
        p += 2;
        if (p + vlen > end) break;
        std::string val(reinterpret_cast<const char*>(p), vlen);
        p += vlen;
        if (p + 12 > end) break;
        uint64_t ts = 0;
        for (int i = 0; i < 8; ++i) ts |= (static_cast<uint64_t>(p[i]) << (i*8));
        p += 12; // skip ts(8) + counter(4)
        SovereignCRDTSync::instance().mergeRemote(key, val, ts);
    }
    return true;
}

void SovereignSnapshot::getLatestMeshSnapshot(std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lk(g_snapMutex);
    data = serializeCRDT();
}

} // namespace RawrXD::Runtime
