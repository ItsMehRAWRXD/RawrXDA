// ============================================================================
// RawrXD_ApertureManager.h
// Manifest-Aware Aperture Orchestrator
//
// Bridges sealed v1.2.5-fused manifest + recursive snapshot to MASM64 aperture
// kernels. Implements tri-factor validation (manifest → snapshot → disk) and
// subdivision metadata population.
//
// Core Responsibility:
//   1. Load and parse sealed manifest.json
//   2. Validate recursive_sha256_snapshot.txt against disk state
//   3. Build subdivision metadata from artifact hashes
//   4. Orchestrate aperture reservation and view mapping
//   5. Provide error-tolerant fallback paths for VA fragmentation
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

namespace RawrXD {

// ============================================================================
// SUBDIVISION_TABLE: Metadata for MASM64 aperture kernel calls
// ============================================================================

struct SubdivisionEntry {
    uint64_t chunk_index;           // Ordinal position in aperture
    uint64_t offset_bytes;          // File offset start
    uint64_t size_bytes;            // Chunk size (usually 64KB-aligned)
    char sha256_hex[65];            // SHA256 hex string (null-terminated)
    const char* artifact_path;      // Pointer to artifact path string
    uint8_t status;                 // 0=unvalidated, 1=validated, 2=mapped
};

struct SubdivisionTable {
    std::vector<SubdivisionEntry> entries;
    uint64_t aperture_base;
    uint64_t aperture_size_bytes;
    uint64_t total_mapped_bytes;
    bool has_placeholder_reservation;
    uint32_t alignment_mode;        // kAlignModeLarge2MB or kAlignModeSys64KB
};

// ============================================================================
// Manifest Schema (parsed from sealed distribution)
// ============================================================================

struct ManifestArtifact {
    std::string name;
    std::string artifact;           // File path relative to dist root
    std::string artifact_path;      // Canonical relative path (preferred)
    std::string status;
    std::string sha256;
    uint64_t bytes;
};

struct SealedManifest {
    std::string version;
    std::string golden_seal_status; // Legacy sealing field
    std::string overall_status;     // evidence_status.overall
    std::string signoff_mode;       // signoff_stamp.mode
    std::string release_validated_at_utc; // signoff_stamp.release_validated_at_utc
    std::string sealed_at_utc;
    std::string sealed_by;
    std::vector<ManifestArtifact> artifacts;

    bool isSealed() const {
        if (!overall_status.empty()) {
            return overall_status == "RELEASE_VALIDATED";
        }

        // Backward compatibility for earlier v1.2.5 manifests.
        return golden_seal_status == "SEALED" || golden_seal_status == "SEALED_WITH_WAIVER";
    }
};

// ============================================================================
// Recursive Snapshot Entry: from recursive_sha256_snapshot.txt
// ============================================================================

struct SnapshotEntry {
    std::string path;               // Relative path from dist root
    std::string sha256;             // SHA256 hex
    uint64_t bytes;
};

// ============================================================================
// ApertureManager: Main orchestrator
// ============================================================================

class ApertureManager {
public:
    ApertureManager();
    ~ApertureManager();

    // Load and validate sealed manifest from distribution path
    // Returns nullptr on failure (logs error via last_error)
    bool loadSealedManifest(const std::string& dist_root, std::string& error_out);

    // Load and parse recursive snapshot
    // Validates that all entries are present and hashes match disk state
    bool loadRecursiveSnapshot(const std::string& dist_root, std::string& error_out);

    // Tri-factor validation: manifest → snapshot → disk
    // Returns false if any validation step fails
    bool validateTriFactorIntegrity(std::string& error_out);

    // Build subdivision table from manifest artifacts
    // Chunks are aligned to 64KB granularity
    // Returns false on memory allocation or hash mismatch
    bool buildSubdivisionTable(uint64_t max_aperture_size, std::string& error_out);

    // Reserve sliding aperture from kernel
    // Populates aperture_base and aperture_size in subdivision_table
    bool reserveAperture(std::string& error_out);

    // Map a single subdivision chunk into active aperture window
    // Updates subdivision_table entries[chunk_idx].status to MAPPED
    bool mapSubdivisionChunk(uint32_t chunk_index, std::string& error_out);

    // Unmap and slide aperture window for next chunk
    bool slideApertureWindow(uint32_t next_chunk_index, std::string& error_out);

    // Accessor methods
    const SealedManifest& getManifest() const { return m_manifest; }
    const std::vector<SnapshotEntry>& getSnapshot() const { return m_snapshot_entries; }
    const SubdivisionTable& getSubdivisionTable() const { return m_subdivision_table; }
    SubdivisionTable& getSubdivisionTableMutable() { return m_subdivision_table; }

    // Fallback mode: allow direct map when placeholder reservation fails
    void setAllowDirectMap(bool allow) { m_allow_direct_map = allow; }
    bool getAllowDirectMap() const { return m_allow_direct_map; }

    // Error state
    std::string getLastError() const { return m_last_error; }

private:
    // Internal validation helpers
    bool validateManifestSealing(std::string& error_out);
    bool validateSnapshotAgainstDisk(const std::string& dist_root, std::string& error_out);
    bool validateArtifactHashAgainstSnapshot(
        const ManifestArtifact& artifact,
        const std::string& artifact_path,
        std::string& error_out);

    // Hash computation
    static std::string computeSHA256(const std::string& file_path, std::string& error_out);
    static bool parseSHA256Snapshot(
        const std::string& snapshot_text,
        std::vector<SnapshotEntry>& entries_out,
        std::string& error_out);

    // State
    SealedManifest m_manifest;
    std::vector<SnapshotEntry> m_snapshot_entries;
    std::unordered_map<std::string, SnapshotEntry> m_snapshot_index;
    SubdivisionTable m_subdivision_table;
    std::string m_dist_root;
    std::string m_last_error;
    bool m_allow_direct_map;
    bool m_manifest_loaded;
    bool m_snapshot_loaded;
    bool m_integrity_validated;
};

// ============================================================================
// Helper: Build aperture from manifest (convenience function)
// ============================================================================

std::unique_ptr<ApertureManager> buildApertureFromManifest(
    const std::string& dist_root,
    uint64_t max_aperture_size,
    std::string& error_out);

} // namespace RawrXD
