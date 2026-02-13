// ============================================================================
// sqlite_wrapper.hpp — RAII SQLite3 Wrapper
// ============================================================================
// Provides a safe, PatchResult-style C++20 interface around the SQLite3 C API.
// Supports prepared statements, transactions, schema migrations, and JSON.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <filesystem>

// Forward-declare sqlite3 types to avoid header pollution
struct sqlite3;
struct sqlite3_stmt;

namespace RawrXD {
namespace Storage {

// ============================================================================
// Result Types
// ============================================================================

struct SqliteResult {
    bool success;
    const char* detail;
    int errorCode;      // SQLite error code (SQLITE_OK = 0)

    static SqliteResult ok(const char* msg = "OK") {
        return {true, msg, 0};
    }
    static SqliteResult error(const char* msg, int code = -1) {
        return {false, msg, code};
    }
};

// ============================================================================
// Query Row — represents a single row from a SELECT
// ============================================================================

struct QueryRow {
    std::vector<std::string> columns;
    std::vector<std::string> values;

    std::string get(const std::string& col) const {
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i] == col) return values[i];
        }
        return "";
    }

    int getInt(const std::string& col) const {
        std::string v = get(col);
        if (v.empty()) return 0;
        return std::atoi(v.c_str());
    }

    int64_t getInt64(const std::string& col) const {
        std::string v = get(col);
        if (v.empty()) return 0;
        return std::atoll(v.c_str());
    }

    double getDouble(const std::string& col) const {
        std::string v = get(col);
        if (v.empty()) return 0.0;
        return std::atof(v.c_str());
    }
};

using QueryResults = std::vector<QueryRow>;

// ============================================================================
// Schema Migration Entry
// ============================================================================

struct Migration {
    int version;
    const char* description;
    const char* sql;
};

// ============================================================================
// Prepared Statement Wrapper (RAII)
// ============================================================================

class PreparedStatement {
public:
    PreparedStatement() = default;
    PreparedStatement(sqlite3_stmt* stmt, sqlite3* db);
    ~PreparedStatement();

    // Move-only
    PreparedStatement(PreparedStatement&& other) noexcept;
    PreparedStatement& operator=(PreparedStatement&& other) noexcept;
    PreparedStatement(const PreparedStatement&) = delete;
    PreparedStatement& operator=(const PreparedStatement&) = delete;

    // Binding
    SqliteResult bindInt(int index, int value);
    SqliteResult bindInt64(int index, int64_t value);
    SqliteResult bindDouble(int index, double value);
    SqliteResult bindText(int index, const std::string& value);
    SqliteResult bindBlob(int index, const void* data, size_t size);
    SqliteResult bindNull(int index);

    // Execution
    SqliteResult execute();                 // INSERT/UPDATE/DELETE
    SqliteResult query(QueryResults& out);  // SELECT
    SqliteResult reset();                   // Re-use statement

    int64_t lastInsertRowId() const;
    int changesCount() const;

    bool isValid() const { return m_stmt != nullptr; }

private:
    sqlite3_stmt* m_stmt = nullptr;
    sqlite3* m_db = nullptr;
    void finalize();
};

// ============================================================================
// SQLite Database Wrapper (RAII)
// ============================================================================

class SqliteDatabase {
public:
    SqliteDatabase();
    ~SqliteDatabase();

    // Move-only
    SqliteDatabase(SqliteDatabase&& other) noexcept;
    SqliteDatabase& operator=(SqliteDatabase&& other) noexcept;
    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;

    // ---- Open / Close ----
    SqliteResult open(const std::filesystem::path& dbPath);
    SqliteResult openInMemory();
    void close();
    bool isOpen() const { return m_db != nullptr; }

    // ---- Raw Execution ----
    SqliteResult execute(const char* sql);
    SqliteResult query(const char* sql, QueryResults& out);

    // ---- Prepared Statements ----
    SqliteResult prepare(const char* sql, PreparedStatement& out);

    // ---- Statement Cache ----
    // Re-uses prepared statements keyed by SQL string
    PreparedStatement* getCachedStatement(const char* sql);

    // ---- Transactions ----
    SqliteResult beginTransaction();
    SqliteResult commit();
    SqliteResult rollback();

    // RAII transaction guard
    class TransactionGuard {
    public:
        TransactionGuard(SqliteDatabase& db);
        ~TransactionGuard();
        SqliteResult commit();
        bool committed() const { return m_committed; }
    private:
        SqliteDatabase& m_db;
        bool m_committed = false;
    };
    TransactionGuard transaction();

    // ---- Schema Migrations ----
    SqliteResult applyMigrations(const Migration* migrations, size_t count);
    int getCurrentSchemaVersion();

    // ---- Utility ----
    int64_t lastInsertRowId() const;
    int totalChanges() const;
    const char* lastErrorMessage() const;

    // ---- WAL Mode ----
    SqliteResult enableWAL();

    // ---- Thread Safety ----
    std::mutex& getMutex() { return m_mutex; }

private:
    sqlite3* m_db = nullptr;
    std::mutex m_mutex;
    std::unordered_map<std::string, std::unique_ptr<PreparedStatement>> m_stmtCache;

    SqliteResult setJournalMode(const char* mode);
    SqliteResult createMigrationsTable();
};

} // namespace Storage
} // namespace RawrXD
