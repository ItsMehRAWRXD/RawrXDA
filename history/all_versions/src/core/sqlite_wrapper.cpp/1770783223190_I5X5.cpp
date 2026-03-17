// ============================================================================
// sqlite_wrapper.cpp — RAII SQLite3 Wrapper Implementation
// ============================================================================
// Pure C++20, no Qt, no exceptions. PatchResult-style error handling.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "core/sqlite_wrapper.hpp"

// SQLite amalgamation header — expects sqlite3.h to be in include path
// If using the amalgamation directly, include sqlite3.c in the build.
#include <sqlite3.h>

#include <cstring>
#include <cstdio>

namespace RawrXD {
namespace Storage {

// ============================================================================
// PreparedStatement
// ============================================================================

PreparedStatement::PreparedStatement(sqlite3_stmt* stmt, sqlite3* db)
    : m_stmt(stmt), m_db(db) {}

PreparedStatement::~PreparedStatement() {
    finalize();
}

PreparedStatement::PreparedStatement(PreparedStatement&& other) noexcept
    : m_stmt(other.m_stmt), m_db(other.m_db) {
    other.m_stmt = nullptr;
    other.m_db = nullptr;
}

PreparedStatement& PreparedStatement::operator=(PreparedStatement&& other) noexcept {
    if (this != &other) {
        finalize();
        m_stmt = other.m_stmt;
        m_db = other.m_db;
        other.m_stmt = nullptr;
        other.m_db = nullptr;
    }
    return *this;
}

void PreparedStatement::finalize() {
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
        m_stmt = nullptr;
    }
}

SqliteResult PreparedStatement::bindInt(int index, int value) {
    if (!m_stmt) return SqliteResult::error("No statement", -1);
    int rc = sqlite3_bind_int(m_stmt, index, value);
    if (rc != SQLITE_OK) {
        return SqliteResult::error(sqlite3_errmsg(m_db), rc);
    }
    return SqliteResult::ok("Bound int");
}

SqliteResult PreparedStatement::bindInt64(int index, int64_t value) {
    if (!m_stmt) return SqliteResult::error("No statement", -1);
    int rc = sqlite3_bind_int64(m_stmt, index, value);
    if (rc != SQLITE_OK) {
        return SqliteResult::error(sqlite3_errmsg(m_db), rc);
    }
    return SqliteResult::ok("Bound int64");
}

SqliteResult PreparedStatement::bindDouble(int index, double value) {
    if (!m_stmt) return SqliteResult::error("No statement", -1);
    int rc = sqlite3_bind_double(m_stmt, index, value);
    if (rc != SQLITE_OK) {
        return SqliteResult::error(sqlite3_errmsg(m_db), rc);
    }
    return SqliteResult::ok("Bound double");
}

SqliteResult PreparedStatement::bindText(int index, const std::string& value) {
    if (!m_stmt) return SqliteResult::error("No statement", -1);
    int rc = sqlite3_bind_text(m_stmt, index, value.c_str(),
                                static_cast<int>(value.size()), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        return SqliteResult::error(sqlite3_errmsg(m_db), rc);
    }
    return SqliteResult::ok("Bound text");
}

SqliteResult PreparedStatement::bindBlob(int index, const void* data, size_t size) {
    if (!m_stmt) return SqliteResult::error("No statement", -1);
    int rc = sqlite3_bind_blob(m_stmt, index, data,
                                static_cast<int>(size), SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        return SqliteResult::error(sqlite3_errmsg(m_db), rc);
    }
    return SqliteResult::ok("Bound blob");
}

SqliteResult PreparedStatement::bindNull(int index) {
    if (!m_stmt) return SqliteResult::error("No statement", -1);
    int rc = sqlite3_bind_null(m_stmt, index);
    if (rc != SQLITE_OK) {
        return SqliteResult::error(sqlite3_errmsg(m_db), rc);
    }
    return SqliteResult::ok("Bound null");
}

SqliteResult PreparedStatement::execute() {
    if (!m_stmt) return SqliteResult::error("No statement", -1);
    int rc = sqlite3_step(m_stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        const char* msg = m_db ? sqlite3_errmsg(m_db) : "Unknown error";
        return SqliteResult::error(msg, rc);
    }
    return SqliteResult::ok("Executed");
}

SqliteResult PreparedStatement::query(QueryResults& out) {
    if (!m_stmt) return SqliteResult::error("No statement", -1);

    int colCount = sqlite3_column_count(m_stmt);
    std::vector<std::string> colNames;
    colNames.reserve(colCount);
    for (int i = 0; i < colCount; ++i) {
        const char* name = sqlite3_column_name(m_stmt, i);
        colNames.push_back(name ? name : "");
    }

    out.clear();
    int rc;
    while ((rc = sqlite3_step(m_stmt)) == SQLITE_ROW) {
        QueryRow row;
        row.columns = colNames;
        row.values.reserve(colCount);
        for (int i = 0; i < colCount; ++i) {
            const unsigned char* text = sqlite3_column_text(m_stmt, i);
            row.values.push_back(text ? reinterpret_cast<const char*>(text) : "");
        }
        out.push_back(std::move(row));
    }

    if (rc != SQLITE_DONE) {
        const char* msg = m_db ? sqlite3_errmsg(m_db) : "Unknown error";
        return SqliteResult::error(msg, rc);
    }
    return SqliteResult::ok("Query complete");
}

SqliteResult PreparedStatement::reset() {
    if (!m_stmt) return SqliteResult::error("No statement", -1);
    sqlite3_reset(m_stmt);
    sqlite3_clear_bindings(m_stmt);
    return SqliteResult::ok("Reset");
}

int64_t PreparedStatement::lastInsertRowId() const {
    if (!m_db) return -1;
    return sqlite3_last_insert_rowid(m_db);
}

int PreparedStatement::changesCount() const {
    if (!m_db) return 0;
    return sqlite3_changes(m_db);
}

// ============================================================================
// SqliteDatabase
// ============================================================================

SqliteDatabase::SqliteDatabase() : m_db(nullptr) {}

SqliteDatabase::~SqliteDatabase() {
    close();
}

SqliteDatabase::SqliteDatabase(SqliteDatabase&& other) noexcept
    : m_db(other.m_db), m_stmtCache(std::move(other.m_stmtCache)) {
    other.m_db = nullptr;
}

SqliteDatabase& SqliteDatabase::operator=(SqliteDatabase&& other) noexcept {
    if (this != &other) {
        close();
        m_db = other.m_db;
        m_stmtCache = std::move(other.m_stmtCache);
        other.m_db = nullptr;
    }
    return *this;
}

SqliteResult SqliteDatabase::open(const std::filesystem::path& dbPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db) {
        return SqliteResult::error("Database already open", -1);
    }

    // Ensure parent directory exists
    auto parent = dbPath.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        // Ignore ec; open will fail if dir can't be created
    }

    std::string pathStr = dbPath.string();
    int rc = sqlite3_open_v2(pathStr.c_str(), &m_db,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                              SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        const char* msg = m_db ? sqlite3_errmsg(m_db) : "Failed to open database";
        if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
        return SqliteResult::error(msg, rc);
    }

    // Set pragmas for performance
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA busy_timeout=5000;", nullptr, nullptr, nullptr);

    return SqliteResult::ok("Database opened");
}

SqliteResult SqliteDatabase::openInMemory() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db) {
        return SqliteResult::error("Database already open", -1);
    }

    int rc = sqlite3_open(":memory:", &m_db);
    if (rc != SQLITE_OK) {
        const char* msg = m_db ? sqlite3_errmsg(m_db) : "Failed to open in-memory DB";
        if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
        return SqliteResult::error(msg, rc);
    }
    return SqliteResult::ok("In-memory database opened");
}

void SqliteDatabase::close() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clear statement cache before closing (statements must be finalized first)
    m_stmtCache.clear();

    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

SqliteResult SqliteDatabase::execute(const char* sql) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_db) return SqliteResult::error("Database not open", -1);

    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "Unknown SQL error";
        sqlite3_free(errMsg);
        return SqliteResult::error(msg.c_str(), rc);
    }
    return SqliteResult::ok("Executed");
}

SqliteResult SqliteDatabase::query(const char* sql, QueryResults& out) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_db) return SqliteResult::error("Database not open", -1);

    PreparedStatement stmt;
    sqlite3_stmt* raw = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &raw, nullptr);
    if (rc != SQLITE_OK) {
        return SqliteResult::error(sqlite3_errmsg(m_db), rc);
    }
    stmt = PreparedStatement(raw, m_db);
    return stmt.query(out);
}

SqliteResult SqliteDatabase::prepare(const char* sql, PreparedStatement& out) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_db) return SqliteResult::error("Database not open", -1);

    sqlite3_stmt* raw = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &raw, nullptr);
    if (rc != SQLITE_OK) {
        return SqliteResult::error(sqlite3_errmsg(m_db), rc);
    }
    out = PreparedStatement(raw, m_db);
    return SqliteResult::ok("Prepared");
}

PreparedStatement* SqliteDatabase::getCachedStatement(const char* sql) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_db) return nullptr;

    std::string key(sql);
    auto it = m_stmtCache.find(key);
    if (it != m_stmtCache.end()) {
        it->second->reset();
        return it->second.get();
    }

    sqlite3_stmt* raw = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &raw, nullptr);
    if (rc != SQLITE_OK) return nullptr;

    auto stmt = std::make_unique<PreparedStatement>(raw, m_db);
    PreparedStatement* ptr = stmt.get();
    m_stmtCache[key] = std::move(stmt);
    return ptr;
}

SqliteResult SqliteDatabase::beginTransaction() {
    return execute("BEGIN IMMEDIATE;");
}

SqliteResult SqliteDatabase::commit() {
    return execute("COMMIT;");
}

SqliteResult SqliteDatabase::rollback() {
    return execute("ROLLBACK;");
}

SqliteDatabase::TransactionGuard::TransactionGuard(SqliteDatabase& db) : m_db(db) {
    m_db.beginTransaction();
}

SqliteDatabase::TransactionGuard::~TransactionGuard() {
    if (!m_committed) {
        m_db.rollback();
    }
}

SqliteResult SqliteDatabase::TransactionGuard::commit() {
    auto r = m_db.commit();
    if (r.success) m_committed = true;
    return r;
}

SqliteDatabase::TransactionGuard SqliteDatabase::transaction() {
    return TransactionGuard(*this);
}

SqliteResult SqliteDatabase::createMigrationsTable() {
    return execute(
        "CREATE TABLE IF NOT EXISTS _schema_migrations ("
        "  version INTEGER PRIMARY KEY,"
        "  description TEXT NOT NULL,"
        "  applied_at TEXT DEFAULT (datetime('now'))"
        ");"
    );
}

int SqliteDatabase::getCurrentSchemaVersion() {
    auto r = createMigrationsTable();
    if (!r.success) return -1;

    QueryResults rows;
    r = query("SELECT MAX(version) as v FROM _schema_migrations;", rows);
    if (!r.success || rows.empty()) return 0;
    return rows[0].getInt("v");
}

SqliteResult SqliteDatabase::applyMigrations(const Migration* migrations, size_t count) {
    auto r = createMigrationsTable();
    if (!r.success) return r;

    int current = getCurrentSchemaVersion();

    for (size_t i = 0; i < count; ++i) {
        if (migrations[i].version <= current) continue;

        auto txn = transaction();

        r = execute(migrations[i].sql);
        if (!r.success) {
            return SqliteResult::error("Migration failed", r.errorCode);
        }

        // Record migration
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "INSERT INTO _schema_migrations (version, description) VALUES (%d, '%s');",
            migrations[i].version, migrations[i].description);
        r = execute(buf);
        if (!r.success) {
            return SqliteResult::error("Migration record failed", r.errorCode);
        }

        r = txn.commit();
        if (!r.success) return r;
    }

    return SqliteResult::ok("Migrations applied");
}

int64_t SqliteDatabase::lastInsertRowId() const {
    if (!m_db) return -1;
    return sqlite3_last_insert_rowid(m_db);
}

int SqliteDatabase::totalChanges() const {
    if (!m_db) return 0;
    return sqlite3_total_changes(m_db);
}

const char* SqliteDatabase::lastErrorMessage() const {
    if (!m_db) return "Database not open";
    return sqlite3_errmsg(m_db);
}

SqliteResult SqliteDatabase::enableWAL() {
    return setJournalMode("WAL");
}

SqliteResult SqliteDatabase::setJournalMode(const char* mode) {
    char sql[64];
    std::snprintf(sql, sizeof(sql), "PRAGMA journal_mode=%s;", mode);
    return execute(sql);
}

} // namespace Storage
} // namespace RawrXD
