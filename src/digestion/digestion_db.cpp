// ============================================================================
// digestion_db.cpp — Digestion Database Implementation (SQLite3 C API, no Qt)
// ============================================================================
#include "digestion_db.h"

#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>

DigestionDatabase::DigestionDatabase() {}

DigestionDatabase::~DigestionDatabase() {
    close();
}

bool DigestionDatabase::isOpen() const {
    return m_db != nullptr;
}

void DigestionDatabase::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool DigestionDatabase::open(const std::string &path, std::string *error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }

    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        if (error) {
            const char* msg = sqlite3_errmsg(m_db);
            *error = msg ? msg : "unknown sqlite error";
        }
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // Enable WAL + foreign keys
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    return true;
}

bool DigestionDatabase::ensureSchema(const std::string &schemaPath, std::string *error) {
    std::string schemaSql;
    if (!schemaPath.empty()) {
        std::ifstream ifs(schemaPath);
        if (ifs.is_open()) {
            schemaSql.assign(std::istreambuf_iterator<char>(ifs),
                             std::istreambuf_iterator<char>());
        }
    }
    if (schemaSql.empty()) schemaSql = defaultSchema();

    // Split by semicolons
    std::vector<std::string> statements;
    std::istringstream iss(schemaSql);
    std::string chunk;
    while (std::getline(iss, chunk, ';')) {
        // Trim
        size_t start = chunk.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = chunk.find_last_not_of(" \t\r\n");
        std::string stmt = chunk.substr(start, end - start + 1);
        if (!stmt.empty()) statements.push_back(std::move(stmt));
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        if (error) *error = "Database not open";
        return false;
    }

    char* errmsg = nullptr;
    sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    bool ok = executeBatch(statements, error);

    if (ok) {
        sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, &errmsg);
        sqlite3_free(errmsg);
    } else {
        sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    return ok;
}

bool DigestionDatabase::insertRun(const std::string &rootDir, const DigestionMetrics &metrics,
                                   const std::string &reportJson, int *runId, std::string *error)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        if (error) *error = "Database not open";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO digestion_runs "
        "(root_dir, start_ms, end_ms, elapsed_ms, total_files, scanned_files, "
        "stubs_found, fixes_applied, bytes_processed) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (error) *error = sqlite3_errmsg(m_db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, rootDir.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, metrics.startMs);
    sqlite3_bind_int64(stmt, 3, metrics.endMs);
    sqlite3_bind_int64(stmt, 4, metrics.elapsedMs);
    sqlite3_bind_int(stmt, 5, metrics.totalFiles);
    sqlite3_bind_int(stmt, 6, metrics.scannedFiles);
    sqlite3_bind_int(stmt, 7, metrics.stubsFound);
    sqlite3_bind_int(stmt, 8, metrics.fixesApplied);
    sqlite3_bind_int64(stmt, 9, metrics.bytesProcessed);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        if (error) *error = sqlite3_errmsg(m_db);
        return false;
    }

    int id = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    if (runId) *runId = id;

    // Insert report if provided
    if (!reportJson.empty()) {
        sqlite3_stmt* rptStmt = nullptr;
        if (sqlite3_prepare_v2(m_db,
                "INSERT INTO digestion_reports (run_id, report_json) VALUES (?, ?)",
                -1, &rptStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(rptStmt, 1, id);
            sqlite3_bind_text(rptStmt, 2, reportJson.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(rptStmt) != SQLITE_DONE && error) {
                *error = sqlite3_errmsg(m_db);
            }
            sqlite3_finalize(rptStmt);
        }
    }

    return true;
}

bool DigestionDatabase::insertFileResult(int runId, const DigestionFileObj &fileObj, std::string *error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        if (error) *error = "Database not open";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO digestion_files "
        "(run_id, path, language, size_bytes, stubs_found, hash) "
        "VALUES (?, ?, ?, ?, ?, ?)";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (error) *error = sqlite3_errmsg(m_db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, runId);
    sqlite3_bind_text(stmt, 2, fileObj.file.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, fileObj.language.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, fileObj.sizeBytes);
    sqlite3_bind_int(stmt, 5, fileObj.stubsFound);
    sqlite3_bind_text(stmt, 6, fileObj.hash.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        if (error) *error = sqlite3_errmsg(m_db);
        return false;
    }

    return true;
}

bool DigestionDatabase::insertTask(int fileId, const DigestionTaskObj &taskObj, std::string *error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        if (error) *error = "Database not open";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO digestion_tasks "
        "(file_id, line_number, stub_type, context, suggested_fix, confidence, applied, backup_id) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (error) *error = sqlite3_errmsg(m_db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, fileId);
    sqlite3_bind_int(stmt, 2, taskObj.line);
    sqlite3_bind_text(stmt, 3, taskObj.type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, taskObj.context.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, taskObj.suggestedFix.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, taskObj.confidence.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, taskObj.applied ? 1 : 0);
    sqlite3_bind_text(stmt, 8, taskObj.backupId.c_str(), -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        if (error) *error = sqlite3_errmsg(m_db);
        return false;
    }
    return true;
}

std::vector<DigestionRunRow> DigestionDatabase::fetchRecentRuns(int limit, std::string *error) const {
    std::vector<DigestionRunRow> runs;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        if (error) *error = "Database not open";
        return runs;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, root_dir, start_ms, end_ms, elapsed_ms, total_files, scanned_files, "
        "stubs_found, fixes_applied, bytes_processed, created_at "
        "FROM digestion_runs ORDER BY id DESC LIMIT ?";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (error) *error = sqlite3_errmsg(m_db);
        return runs;
    }

    sqlite3_bind_int(stmt, 1, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DigestionRunRow row{};
        row.id = sqlite3_column_int(stmt, 0);
        const char* rd = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        row.rootDir = rd ? rd : "";
        row.startMs = sqlite3_column_int64(stmt, 2);
        row.endMs = sqlite3_column_int64(stmt, 3);
        row.elapsedMs = sqlite3_column_int64(stmt, 4);
        row.totalFiles = sqlite3_column_int(stmt, 5);
        row.scannedFiles = sqlite3_column_int(stmt, 6);
        row.stubsFound = sqlite3_column_int(stmt, 7);
        row.fixesApplied = sqlite3_column_int(stmt, 8);
        row.bytesProcessed = sqlite3_column_int64(stmt, 9);
        const char* ca = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
        row.createdAt = ca ? ca : "";
        runs.push_back(std::move(row));
    }
    sqlite3_finalize(stmt);
    return runs;
}

bool DigestionDatabase::deleteRun(int runId, std::string *error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        if (error) *error = "Database not open";
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "DELETE FROM digestion_runs WHERE id = ?",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        if (error) *error = sqlite3_errmsg(m_db);
        return false;
    }

    sqlite3_bind_int(stmt, 1, runId);
    int rc = sqlite3_step(stmt);
    int changes = sqlite3_changes(m_db);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        if (error) *error = sqlite3_errmsg(m_db);
        return false;
    }
    return changes > 0;
}

std::string DigestionDatabase::defaultSchema() {
    return R"SQL(
CREATE TABLE IF NOT EXISTS digestion_runs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    root_dir TEXT NOT NULL,
    start_ms INTEGER,
    end_ms INTEGER,
    elapsed_ms INTEGER,
    total_files INTEGER,
    scanned_files INTEGER,
    stubs_found INTEGER,
    fixes_applied INTEGER,
    bytes_processed INTEGER,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS digestion_reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL,
    report_json TEXT,
    FOREIGN KEY(run_id) REFERENCES digestion_runs(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS digestion_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL,
    path TEXT NOT NULL,
    language TEXT,
    size_bytes INTEGER,
    stubs_found INTEGER,
    hash TEXT,
    FOREIGN KEY(run_id) REFERENCES digestion_runs(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS digestion_tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    line_number INTEGER,
    stub_type TEXT,
    context TEXT,
    suggested_fix TEXT,
    confidence TEXT,
    applied INTEGER,
    backup_id TEXT,
    FOREIGN KEY(file_id) REFERENCES digestion_files(id) ON DELETE CASCADE
);
)SQL";
}

bool DigestionDatabase::executeBatch(const std::vector<std::string> &statements, std::string *error) {
    for (const auto &stmt : statements) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(m_db, stmt.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            if (error) *error = errmsg ? errmsg : "unknown error";
            sqlite3_free(errmsg);
            return false;
        }
        sqlite3_free(errmsg);
    }
    return true;
}

