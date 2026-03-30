// ============================================================================
// LocalVectorDB_Persistence.hpp - SQLite3-Backed Persistent Index Layer
// Phase 5: Incremental Indexing for Enterprise-Scale RAG
// 
// Purpose:
//   Replace the in-memory HNSW rebuild-on-boot model with a persistent,
//   incremental index stored in SQLite3. File hashes track changes; only
//   modified files are re-embedded and reindexed on subsequent boots.
//
// Schema Tables:
//   1. embeddings: (id, source_file, line_number, content_hash, 
//                   semantic_vector_blob, lexical_signature, timestamp)
//   2. index_metadata: (source_file, last_modified_time, file_hash,
//                       vector_count, indexed_at)
//   3. index_state: (key, value) — global state (last_full_index_time, etc.)
//
// Integration Points:
//   - Called from SwarmOrchestrator::InitializeLocalVectorDB()
//   - Lazy-loads on first Search(); background thread updates on file changes
//   - ReadDirectoryChangesW hook signals reindex of modified files
// ============================================================================

#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <chrono>

namespace RawrXD {

// ============================================================================
// SQLite3 Persistent Index for LocalVectorDB
// ============================================================================

class VectorDBPersistence {
public:
    struct EmbeddingRecord {
        int64_t id;
        std::string source_file;
        int line_number;
        std::string content_hash;  // SHA256 of code line for change detection
        std::vector<float> semantic_vector;  // 384-dim TinyBERT embedding
        std::string lexical_signature;  // BM25 term frequency fingerprint
        std::chrono::system_clock::time_point timestamp;
    };

    struct IndexMetadata {
        std::string source_file;
        std::chrono::system_clock::time_point last_modified_time;
        std::string file_hash;  // SHA256 of entire file (quick dirty-check)
        int64_t vector_count;
        std::chrono::system_clock::time_point indexed_at;
    };

    VectorDBPersistence() : m_db(nullptr), m_initialized(false) {}

    ~VectorDBPersistence() {
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
    }

    // Initialize SQLite3 database and create schema if not exists
    bool Initialize(const std::string& dbPath) {
        int rc = sqlite3_open(dbPath.c_str(), &m_db);
        if (rc != SQLITE_OK) {
            return false;
        }

        // Enable WAL mode for concurrent read + write
        sqlite3_exec(m_db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(m_db, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);

        if (!CreateSchema()) {
            sqlite3_close(m_db);
            m_db = nullptr;
            return false;
        }

        m_initialized = true;
        return true;
    }

    // Insert or update embedding record
    bool StoreEmbedding(const EmbeddingRecord& record) {
        if (!m_db) return false;

        const char* sql = R"(
            INSERT OR REPLACE INTO embeddings 
            (source_file, line_number, content_hash, semantic_vector, lexical_signature, timestamp)
            VALUES (?, ?, ?, ?, ?, ?)
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return false;
        }

        // Serialize semantic_vector as blob
        std::vector<uint8_t> vectorBlob(record.semantic_vector.size() * sizeof(float));
        std::memcpy(vectorBlob.data(), record.semantic_vector.data(), vectorBlob.size());

        sqlite3_bind_text(stmt, 1, record.source_file.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, record.line_number);
        sqlite3_bind_text(stmt, 3, record.content_hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 4, vectorBlob.data(), vectorBlob.size(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, record.lexical_signature.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 6, std::chrono::system_clock::now().time_since_epoch().count());

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return rc == SQLITE_DONE;
    }

    // Query embeddings by file (for reindex after file change)
    std::vector<EmbeddingRecord> QueryByFile(const std::string& sourceFile) {
        std::vector<EmbeddingRecord> results;
        if (!m_db) return results;

        const char* sql = R"(
            SELECT id, source_file, line_number, content_hash, semantic_vector, 
                   lexical_signature, timestamp 
            FROM embeddings 
            WHERE source_file = ? 
            ORDER BY line_number ASC
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return results;
        }

        sqlite3_bind_text(stmt, 1, sourceFile.c_str(), -1, SQLITE_STATIC);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            EmbeddingRecord rec;
            rec.id = sqlite3_column_int64(stmt, 0);
            rec.source_file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            rec.line_number = sqlite3_column_int(stmt, 2);
            rec.content_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

            // Deserialize semantic_vector blob
            const void* vecBlob = sqlite3_column_blob(stmt, 4);
            int vecSize = sqlite3_column_bytes(stmt, 4);
            if (vecBlob && vecSize > 0) {
                rec.semantic_vector.resize(vecSize / sizeof(float));
                std::memcpy(rec.semantic_vector.data(), vecBlob, vecSize);
            }

            rec.lexical_signature = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            int64_t ts = sqlite3_column_int64(stmt, 6);
            rec.timestamp = std::chrono::system_clock::time_point(
                std::chrono::system_clock::duration(ts));

            results.push_back(rec);
        }

        sqlite3_finalize(stmt);
        return results;
    }

    // Store/update file metadata for dirty-check
    bool StoreFileMetadata(const IndexMetadata& metadata) {
        if (!m_db) return false;

        const char* sql = R"(
            INSERT OR REPLACE INTO index_metadata 
            (source_file, last_modified_time, file_hash, vector_count, indexed_at)
            VALUES (?, ?, ?, ?, ?)
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, metadata.source_file.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, metadata.last_modified_time.time_since_epoch().count());
        sqlite3_bind_text(stmt, 3, metadata.file_hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 4, metadata.vector_count);
        sqlite3_bind_int64(stmt, 5, metadata.indexed_at.time_since_epoch().count());

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return rc == SQLITE_DONE;
    }

    // Query file metadata (for change detection)
    bool GetFileMetadata(const std::string& sourceFile, IndexMetadata& outMetadata) {
        if (!m_db) return false;

        const char* sql = R"(
            SELECT source_file, last_modified_time, file_hash, vector_count, indexed_at
            FROM index_metadata 
            WHERE source_file = ?
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, sourceFile.c_str(), -1, SQLITE_STATIC);

        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            outMetadata.source_file = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            int64_t lmt = sqlite3_column_int64(stmt, 1);
            outMetadata.last_modified_time = std::chrono::system_clock::time_point(
                std::chrono::system_clock::duration(lmt));
            outMetadata.file_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            outMetadata.vector_count = sqlite3_column_int64(stmt, 3);
            int64_t ia = sqlite3_column_int64(stmt, 4);
            outMetadata.indexed_at = std::chrono::system_clock::time_point(
                std::chrono::system_clock::duration(ia));
            found = true;
        }

        sqlite3_finalize(stmt);
        return found;
    }

    // Delete embeddings for a file (to purge before re-index)
    bool DeleteFileEmbeddings(const std::string& sourceFile) {
        if (!m_db) return false;

        const char* sql = "DELETE FROM embeddings WHERE source_file = ?";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return false;
        }

        sqlite3_bind_text(stmt, 1, sourceFile.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        return rc == SQLITE_DONE;
    }

    // Get total embedding count for diagnostics
    int64_t GetEmbeddingCount() {
        if (!m_db) return 0;

        const char* sql = "SELECT COUNT(*) FROM embeddings";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            return 0;
        }

        int64_t count = 0;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int64(stmt, 0);
        }

        sqlite3_finalize(stmt);
        return count;
    }

private:
    sqlite3* m_db;
    bool m_initialized;

    bool CreateSchema() {
        const char* schema = R"(
            CREATE TABLE IF NOT EXISTS embeddings (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                source_file TEXT NOT NULL,
                line_number INTEGER NOT NULL,
                content_hash TEXT NOT NULL,
                semantic_vector BLOB NOT NULL,
                lexical_signature TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                UNIQUE(source_file, line_number)
            );

            CREATE INDEX IF NOT EXISTS idx_embeddings_source 
                ON embeddings(source_file);
            CREATE INDEX IF NOT EXISTS idx_embeddings_hash 
                ON embeddings(content_hash);

            CREATE TABLE IF NOT EXISTS index_metadata (
                source_file TEXT PRIMARY KEY,
                last_modified_time INTEGER NOT NULL,
                file_hash TEXT NOT NULL,
                vector_count INTEGER NOT NULL,
                indexed_at INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS index_state (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL
            );

            INSERT OR IGNORE INTO index_state (key, value) 
                VALUES ('last_full_index_time', '0');
            INSERT OR IGNORE INTO index_state (key, value) 
                VALUES ('version', '1');
        )";

        char* errMsg = nullptr;
        int rc = sqlite3_exec(m_db, schema, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            if (errMsg) {
                sqlite3_free(errMsg);
            }
            return false;
        }

        return true;
    }
};

}  // namespace RawrXD
