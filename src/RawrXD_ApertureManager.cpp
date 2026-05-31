// ============================================================================
// RawrXD_ApertureManager.cpp - Implementation
// Manifest-Aware Aperture Orchestrator
// ============================================================================

#include "../include/RawrXD_ApertureManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <filesystem>
#include <windows.h>
#include <wincrypt.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

uint64_t alignUpU64(uint64_t v, uint64_t a) {
    if (a == 0) {
        return v;
    }
    const uint64_t r = v % a;
    return (r == 0) ? v : (v + (a - r));
}

std::string trimCopy(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) {
        return "";
    }
    const size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::string normalizePathKey(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    while (!path.empty() && (path.front() == '/' || path.front() == '.')) {
        if (path.front() == '/') {
            path.erase(path.begin());
            continue;
        }
        if (path.size() >= 2 && path[0] == '.' && path[1] == '/') {
            path.erase(0, 2);
            continue;
        }
        break;
    }
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return path;
}

bool hasSnapshotHashLine(const std::string& line) {
    if (line.size() < 67 || line[64] != ' ' || line[65] != '*') {
        return false;
    }
    for (size_t i = 0; i < 64; ++i) {
        const unsigned char c = static_cast<unsigned char>(line[i]);
        if (!std::isxdigit(c)) {
            return false;
        }
    }
    return true;
}

} // namespace

namespace RawrXD {

// ============================================================================
// SHA256 Computation (using Windows CryptoAPI)
// ============================================================================

std::string ApertureManager::computeSHA256(const std::string& file_path, std::string& error_out) {
    error_out.clear();

    HANDLE hFile = CreateFileA(
        file_path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        std::ostringstream oss;
        oss << "Failed to open file: " << file_path << " (error=" << GetLastError() << ")";
        error_out = oss.str();
        return "";
    }

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;

    if (!CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        std::ostringstream oss;
        oss << "CryptAcquireContext failed: " << GetLastError();
        error_out = oss.str();
        CloseHandle(hFile);
        return "";
    }

    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        std::ostringstream oss;
        oss << "CryptCreateHash failed: " << GetLastError();
        error_out = oss.str();
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return "";
    }

    const size_t kBufferSize = 65536;
    std::vector<uint8_t> buffer(kBufferSize);
    DWORD bytesRead = 0;

    while (ReadFile(hFile, buffer.data(), kBufferSize, &bytesRead, nullptr) && bytesRead > 0) {
        if (!CryptHashData(hHash, buffer.data(), bytesRead, 0)) {
            std::ostringstream oss;
            oss << "CryptHashData failed: " << GetLastError();
            error_out = oss.str();
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            CloseHandle(hFile);
            return "";
        }
    }

    DWORD hashSize = 0;
    DWORD hashSizeLen = sizeof(hashSize);
    if (!CryptGetHashParam(hHash, HP_HASHSIZE, (uint8_t*)&hashSize, &hashSizeLen, 0)) {
        std::ostringstream oss;
        oss << "CryptGetHashParam (size) failed: " << GetLastError();
        error_out = oss.str();
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return "";
    }

    std::vector<uint8_t> hash(hashSize);
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash.data(), &hashSize, 0)) {
        std::ostringstream oss;
        oss << "CryptGetHashParam (value) failed: " << GetLastError();
        error_out = oss.str();
        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);
        CloseHandle(hFile);
        return "";
    }

    // Convert to hex string
    std::ostringstream hex;
    for (uint8_t byte : hash) {
        hex << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);
    CloseHandle(hFile);

    return hex.str();
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

ApertureManager::ApertureManager()
    : m_allow_direct_map(false),
      m_manifest_loaded(false),
      m_snapshot_loaded(false),
      m_integrity_validated(false) {
    m_subdivision_table.aperture_base = 0;
    m_subdivision_table.aperture_size_bytes = 0;
    m_subdivision_table.total_mapped_bytes = 0;
    m_subdivision_table.has_placeholder_reservation = false;
    m_subdivision_table.alignment_mode = 0;
}

ApertureManager::~ApertureManager() {
    if (m_subdivision_table.aperture_base != 0 && m_subdivision_table.has_placeholder_reservation) {
        VirtualFree(reinterpret_cast<void*>(m_subdivision_table.aperture_base), 0, MEM_RELEASE);
    }
    m_subdivision_table.aperture_base = 0;
    m_subdivision_table.has_placeholder_reservation = false;
}

// ============================================================================
// Load and Validate Sealed Manifest
// ============================================================================

bool ApertureManager::loadSealedManifest(const std::string& dist_root, std::string& error_out) {
    error_out.clear();
    m_manifest_loaded = false;

    try {
        std::string manifest_path = dist_root + "\\manifest.json";
        std::ifstream ifs(manifest_path);
        if (!ifs.good()) {
            error_out = "manifest.json not found at: " + manifest_path;
            return false;
        }

        std::ostringstream manifest_buf;
        manifest_buf << ifs.rdbuf();
        json j = json::parse(manifest_buf.str());

        m_manifest = SealedManifest{};

        m_manifest.version = j.value("version", "");
        m_manifest.golden_seal_status = j.value("golden_seal_status", "");
        if (j.contains("evidence_status") && j["evidence_status"].is_object()) {
            m_manifest.overall_status = j["evidence_status"].value("overall", "");
        }
        if (j.contains("signoff_stamp") && j["signoff_stamp"].is_object()) {
            m_manifest.signoff_mode = j["signoff_stamp"].value("mode", "");
            m_manifest.release_validated_at_utc = j["signoff_stamp"].value("release_validated_at_utc", "");
        }
        m_manifest.sealed_at_utc = j.value("sealed_at_utc", "");
        m_manifest.sealed_by = j.value("sealed_by", "");

        if (j.contains("artifacts") && j["artifacts"].is_array()) {
            for (const auto& art : j["artifacts"]) {
                ManifestArtifact ma;
                ma.name = art.value("name", "");
                ma.artifact = art.value("artifact", "");
                ma.artifact_path = art.value("artifact_path", ma.artifact);
                ma.status = art.value("status", "");
                ma.sha256 = art.value("sha256", "");
                ma.bytes = art.value("bytes", 0);
                if (ma.name.empty()) {
                    ma.name = ma.artifact;
                }
                m_manifest.artifacts.push_back(ma);
            }
        }

        m_dist_root = dist_root;
        m_manifest_loaded = true;

        // Validate sealing
        if (!validateManifestSealing(error_out)) {
            m_manifest_loaded = false;
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        error_out = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

// ============================================================================
// Parse Recursive Snapshot
// ============================================================================

bool ApertureManager::parseSHA256Snapshot(
    const std::string& snapshot_text,
    std::vector<SnapshotEntry>& entries_out,
    std::string& error_out) {
    error_out.clear();
    entries_out.clear();

    std::istringstream iss(snapshot_text);
    std::string line;
    size_t line_num = 0;

    while (std::getline(iss, line)) {
        ++line_num;

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Preferred format:
        //   <64-hex> *<relative-path>
        // Legacy format:
        //   path=<path> sha256=<hex> bytes=<size>

        std::string clean = trimCopy(line);
        if (clean.empty()) {
            continue;
        }

        if (hasSnapshotHashLine(clean)) {
            SnapshotEntry se;
            se.sha256 = clean.substr(0, 64);
            se.path = clean.substr(66);
            se.bytes = 0;
            entries_out.push_back(se);
            continue;
        }

        size_t path_pos = line.find("path=");
        size_t sha_pos = line.find("sha256=");
        size_t bytes_pos = line.find("bytes=");

        if (path_pos == std::string::npos || sha_pos == std::string::npos || bytes_pos == std::string::npos) {
            std::ostringstream oss;
            oss << "Malformed snapshot line " << line_num << ": " << line;
            error_out = oss.str();
            return false;
        }

        SnapshotEntry se;

        // Extract path (from "path=" to next space or sha256=)
        size_t path_start = path_pos + 5;
        size_t path_end = line.find(' ', path_start);
        if (path_end == std::string::npos) {
            path_end = sha_pos;
        }
        se.path = line.substr(path_start, path_end - path_start);

        // Extract sha256
        size_t sha_start = sha_pos + 7;
        size_t sha_end = line.find(' ', sha_start);
        if (sha_end == std::string::npos) {
            sha_end = bytes_pos;
        }
        se.sha256 = line.substr(sha_start, sha_end - sha_start);

        // Extract bytes
        size_t bytes_start = bytes_pos + 6;
        size_t bytes_end = line.find(' ', bytes_start);
        if (bytes_end == std::string::npos) {
            bytes_end = line.length();
        }
        try {
            se.bytes = std::stoull(line.substr(bytes_start, bytes_end - bytes_start));
        } catch (...) {
            std::ostringstream oss;
            oss << "Invalid bytes value on line " << line_num;
            error_out = oss.str();
            return false;
        }

        entries_out.push_back(se);
    }

    if (entries_out.empty()) {
        error_out = "Snapshot contains no valid entries";
        return false;
    }

    return true;
}

bool ApertureManager::loadRecursiveSnapshot(const std::string& dist_root, std::string& error_out) {
    error_out.clear();
    m_snapshot_loaded = false;

    if (!m_manifest_loaded) {
        error_out = "Manifest must be loaded before snapshot";
        return false;
    }

    // Locate recursive_sha256_snapshot.txt in manifest artifacts
    std::string snapshot_artifact_path;
    for (const auto& art : m_manifest.artifacts) {
        std::string candidate = art.artifact_path.empty() ? art.artifact : art.artifact_path;
        if (normalizePathKey(candidate) == "recursive_sha256_snapshot.txt"
            || normalizePathKey(art.name) == "recursive_sha256_snapshot.txt") {
            snapshot_artifact_path = candidate;
            break;
        }
    }

    if (snapshot_artifact_path.empty()) {
        error_out = "recursive_sha256_snapshot.txt not found in manifest";
        return false;
    }

    std::string full_snapshot_path = dist_root + "\\" + snapshot_artifact_path;
    std::ifstream ifs(full_snapshot_path);
    if (!ifs.good()) {
        error_out = "Failed to open snapshot file: " + full_snapshot_path;
        return false;
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string snapshot_text = oss.str();

    if (!parseSHA256Snapshot(snapshot_text, m_snapshot_entries, error_out)) {
        return false;
    }

    // Fill bytes from disk for hash-star format entries.
    for (auto& entry : m_snapshot_entries) {
        const fs::path full = fs::path(dist_root) / fs::path(entry.path);
        std::error_code ec;
        const uint64_t size = fs::file_size(full, ec);
        if (!ec) {
            entry.bytes = size;
        }
    }

    // Build index for O(1) lookup
    m_snapshot_index.clear();
    for (const auto& entry : m_snapshot_entries) {
        m_snapshot_index[normalizePathKey(entry.path)] = entry;
    }

    m_snapshot_loaded = true;
    return true;
}

// ============================================================================
// Validation Functions
// ============================================================================

bool ApertureManager::validateManifestSealing(std::string& error_out) {
    error_out.clear();

    if (!m_manifest.isSealed()) {
        if (!m_manifest.overall_status.empty()) {
            error_out = "Manifest evidence_status.overall is not RELEASE_VALIDATED";
        } else {
            error_out = "Manifest is not sealed (legacy golden_seal_status invalid)";
        }
        return false;
    }

    if (m_manifest.sealed_by != "reconcile_v125_evidence_state") {
        error_out = "Manifest sealed_by is not 'reconcile_v125_evidence_state'";
        return false;
    }

    if (m_manifest.sealed_at_utc.empty()) {
        error_out = "Manifest sealed_at_utc is missing";
        return false;
    }

    if (!m_manifest.overall_status.empty() && m_manifest.release_validated_at_utc.empty()) {
        error_out = "Manifest signoff_stamp.release_validated_at_utc is missing";
        return false;
    }

    return true;
}

bool ApertureManager::validateSnapshotAgainstDisk(const std::string& dist_root, std::string& error_out) {
    error_out.clear();

    if (!m_snapshot_loaded) {
        error_out = "Snapshot not loaded";
        return false;
    }

    for (const auto& entry : m_snapshot_entries) {
        fs::path file_path = fs::path(dist_root) / fs::path(entry.path);

        // Check existence
        if (!fs::exists(file_path)) {
            std::ostringstream oss;
            oss << "Snapshot entry not found on disk: " << entry.path;
            error_out = oss.str();
            return false;
        }

        // Verify hash
        std::string computed_hash = computeSHA256(file_path.string(), error_out);
        if (!error_out.empty()) {
            return false;
        }

        // Case-insensitive hash comparison
        std::string entry_hash_lower = entry.sha256;
        std::transform(entry_hash_lower.begin(), entry_hash_lower.end(), entry_hash_lower.begin(), ::tolower);
        std::transform(computed_hash.begin(), computed_hash.end(), computed_hash.begin(), ::tolower);

        if (entry_hash_lower != computed_hash) {
            std::ostringstream oss;
            oss << "Hash mismatch for: " << entry.path
                << "\n  Expected: " << entry_hash_lower
                << "\n  Got:      " << computed_hash;
            error_out = oss.str();
            return false;
        }
    }

    return true;
}

bool ApertureManager::validateTriFactorIntegrity(std::string& error_out) {
    error_out.clear();

    if (!m_manifest_loaded) {
        error_out = "Manifest not loaded";
        return false;
    }

    if (!m_snapshot_loaded) {
        error_out = "Snapshot not loaded";
        return false;
    }

    // Factor 1: Manifest sealing
    if (!validateManifestSealing(error_out)) {
        return false;
    }

    // Factor 2: Snapshot against disk
    if (!validateSnapshotAgainstDisk(m_dist_root, error_out)) {
        return false;
    }

    // Factor 3: Manifest artifact hashes against snapshot
    for (const auto& artifact : m_manifest.artifacts) {
        if (artifact.name == "recursive_sha256_snapshot.txt") {
            // Skip snapshot validation against itself; it's the reference
            continue;
        }

        const std::string manifest_rel = artifact.artifact_path.empty() ? artifact.artifact : artifact.artifact_path;
        if (normalizePathKey(manifest_rel) == "recursive_sha256_snapshot.txt") {
            continue;
        }
        std::string artifact_path = (fs::path(m_dist_root) / fs::path(manifest_rel)).string();
        if (!validateArtifactHashAgainstSnapshot(artifact, artifact_path, error_out)) {
            return false;
        }
    }

    m_integrity_validated = true;
    return true;
}

bool ApertureManager::validateArtifactHashAgainstSnapshot(
    const ManifestArtifact& artifact,
    const std::string& artifact_path,
    std::string& error_out) {
    error_out.clear();

    // Lookup in snapshot
    const std::string manifest_rel = artifact.artifact_path.empty() ? artifact.artifact : artifact.artifact_path;
    auto it = m_snapshot_index.find(normalizePathKey(manifest_rel));
    if (it == m_snapshot_index.end()) {
        std::ostringstream oss;
        oss << "Artifact not in snapshot: " << manifest_rel;
        error_out = oss.str();
        return false;
    }

    const auto& snapshot_entry = it->second;

    // Compare hashes (case-insensitive)
    std::string artifact_hash = artifact.sha256;
    std::string snapshot_hash = snapshot_entry.sha256;
    std::transform(artifact_hash.begin(), artifact_hash.end(), artifact_hash.begin(), ::tolower);
    std::transform(snapshot_hash.begin(), snapshot_hash.end(), snapshot_hash.begin(), ::tolower);

    if (artifact_hash != snapshot_hash) {
        std::ostringstream oss;
        oss << "Artifact hash mismatch in manifest vs snapshot: " << manifest_rel
            << "\n  Manifest:  " << artifact_hash
            << "\n  Snapshot:  " << snapshot_hash;
        error_out = oss.str();
        return false;
    }

    // Compare sizes
    if (artifact.bytes > 0 && snapshot_entry.bytes > 0 && artifact.bytes != snapshot_entry.bytes) {
        std::ostringstream oss;
        oss << "Artifact size mismatch in manifest vs snapshot: " << manifest_rel
            << "\n  Manifest:  " << artifact.bytes << " bytes"
            << "\n  Snapshot:  " << snapshot_entry.bytes << " bytes";
        error_out = oss.str();
        return false;
    }

    return true;
}

// ============================================================================
// Build Subdivision Table (Phase 1: planning only)
// ============================================================================

bool ApertureManager::buildSubdivisionTable(uint64_t max_aperture_size, std::string& error_out) {
    error_out.clear();

    if (!m_integrity_validated) {
        error_out = "Integrity validation must complete before subdivision building";
        return false;
    }

    m_subdivision_table.entries.clear();
    m_subdivision_table.total_mapped_bytes = 0;
    m_subdivision_table.aperture_size_bytes = 0;
    m_subdivision_table.alignment_mode = 1; // 64KB system granularity path

    if (max_aperture_size == 0) {
        error_out = "max_aperture_size must be greater than zero";
        return false;
    }

    static constexpr uint64_t kChunkAlign = 64ULL * 1024ULL;
    uint64_t running_offset = 0;

    for (const auto& artifact : m_manifest.artifacts) {
        const std::string rel = artifact.artifact_path.empty() ? artifact.artifact : artifact.artifact_path;
        if (rel.empty()) {
            continue;
        }

        const auto it = m_snapshot_index.find(normalizePathKey(rel));
        if (it == m_snapshot_index.end()) {
            error_out = "Artifact missing from snapshot index: " + rel;
            return false;
        }

        const uint64_t logical_size = (it->second.bytes > 0) ? it->second.bytes : artifact.bytes;
        if (logical_size == 0) {
            error_out = "Artifact has zero size and cannot be mapped: " + rel;
            return false;
        }

        const uint64_t aligned_size = alignUpU64(logical_size, kChunkAlign);
        if (running_offset > max_aperture_size || aligned_size > (max_aperture_size - running_offset)) {
            std::ostringstream oss;
            oss << "Aperture size exceeded while adding artifact: " << rel
                << " (offset=" << running_offset << ", chunk=" << aligned_size
                << ", max=" << max_aperture_size << ")";
            error_out = oss.str();
            return false;
        }

        SubdivisionEntry entry{};
        entry.chunk_index = static_cast<uint64_t>(m_subdivision_table.entries.size());
        entry.offset_bytes = running_offset;
        entry.size_bytes = aligned_size;
        const std::string& hash_src = !it->second.sha256.empty() ? it->second.sha256 : artifact.sha256;
        std::memset(entry.sha256_hex, 0, sizeof(entry.sha256_hex));
        std::memcpy(entry.sha256_hex, hash_src.c_str(), std::min<size_t>(64, hash_src.size()));
        entry.artifact_path = artifact.artifact_path.empty() ? artifact.artifact.c_str() : artifact.artifact_path.c_str();
        entry.status = 1; // validated

        m_subdivision_table.entries.push_back(entry);
        running_offset += aligned_size;
    }

    if (m_subdivision_table.entries.empty()) {
        error_out = "No manifest artifacts available to build subdivision table";
        return false;
    }

    m_subdivision_table.total_mapped_bytes = running_offset;
    m_subdivision_table.aperture_size_bytes = running_offset;

    return true;
}

// ============================================================================
// Aperture Reservation (placeholder for MASM integration)
// ============================================================================

bool ApertureManager::reserveAperture(std::string& error_out) {
    error_out.clear();

    if (m_subdivision_table.entries.empty() || m_subdivision_table.aperture_size_bytes == 0) {
        error_out = "Subdivision table must be built before aperture reservation";
        return false;
    }

    if (m_subdivision_table.aperture_base != 0) {
        return true;
    }

    void* reserved = VirtualAlloc(nullptr,
                                  static_cast<SIZE_T>(m_subdivision_table.aperture_size_bytes),
                                  MEM_RESERVE,
                                  PAGE_NOACCESS);
    if (!reserved) {
        if (m_allow_direct_map) {
            m_subdivision_table.aperture_base = 0;
            m_subdivision_table.has_placeholder_reservation = false;
            return true;
        }

        std::ostringstream oss;
        oss << "VirtualAlloc reserve failed (error=" << GetLastError() << ")";
        error_out = oss.str();
        return false;
    }

    m_subdivision_table.aperture_base = reinterpret_cast<uint64_t>(reserved);
    m_subdivision_table.has_placeholder_reservation = true;
    return true;
}

bool ApertureManager::mapSubdivisionChunk(uint32_t chunk_index, std::string& error_out) {
    error_out.clear();

    if (chunk_index >= m_subdivision_table.entries.size()) {
        error_out = "Chunk index out of range";
        return false;
    }

    if (m_subdivision_table.aperture_base == 0 && !m_allow_direct_map) {
        error_out = "Aperture not reserved and direct-map fallback is disabled";
        return false;
    }

    m_subdivision_table.entries[chunk_index].status = 2; // mapped
    return true;
}

bool ApertureManager::slideApertureWindow(uint32_t next_chunk_index, std::string& error_out) {
    error_out.clear();

    if (next_chunk_index >= m_subdivision_table.entries.size()) {
        error_out = "Next chunk index out of range";
        return false;
    }

    for (auto& entry : m_subdivision_table.entries) {
        if (entry.status == 2) {
            entry.status = 1;
        }
    }

    return mapSubdivisionChunk(next_chunk_index, error_out);
}

// ============================================================================
// Convenience Builders
// ============================================================================

std::unique_ptr<ApertureManager> buildApertureFromManifest(
    const std::string& dist_root,
    uint64_t max_aperture_size,
    std::string& error_out) {
    error_out.clear();

    auto mgr = std::make_unique<ApertureManager>();

    if (!mgr->loadSealedManifest(dist_root, error_out)) {
        return nullptr;
    }

    if (!mgr->loadRecursiveSnapshot(dist_root, error_out)) {
        return nullptr;
    }

    if (!mgr->validateTriFactorIntegrity(error_out)) {
        return nullptr;
    }

    if (!mgr->buildSubdivisionTable(max_aperture_size, error_out)) {
        return nullptr;
    }

    if (!mgr->reserveAperture(error_out)) {
        return nullptr;
    }

    return mgr;
}

} // namespace RawrXD
