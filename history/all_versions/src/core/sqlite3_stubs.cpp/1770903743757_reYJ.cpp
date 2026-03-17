// ============================================================================
// sqlite3_stubs.cpp — SQLite3 Link Stubs for RawrXD Build System
// ============================================================================
// Provides link-time stubs for sqlite3 API functions so the project
// can build without sqlite3.lib. Functions return SQLITE_ERROR or
// sensible no-op values. Replace with real sqlite3 amalgamation or
// library for production use.
//
// To use real SQLite3:
//   1. Download sqlite3.c + sqlite3.h from sqlite.org/download
//   2. Place sqlite3.c in src/core/ and add to CMakeLists.txt SOURCES
//   3. Remove this stub file from SOURCES
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include <sqlite3.h>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <unordered_map>
#include <map>

// ============================================================================
// Internal Mock Data Structures
// ============================================================================

struct MockTable {
    std::vector<std::string> columnNames;
    std::vector<std::vector<std::string>> rows;
};

struct sqlite3 {
    std::unordered_map<std::string, MockTable> tables;
    std::string lastError;
};

struct sqlite3_stmt {
    sqlite3* db;
    std::string sql;
    MockTable results;
    int currentRow;
    bool done;
    
    sqlite3_stmt() : db(nullptr), currentRow(-1), done(false) {}
};

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Connection management
// ============================================================================

int sqlite3_open(const char* filename, sqlite3** ppDb) {
    *ppDb = new sqlite3();
    return SQLITE_OK;
}

int sqlite3_open_v2(const char* filename, sqlite3** ppDb, int flags,
                    const char* zVfs) {
    return sqlite3_open(filename, ppDb);
}

int sqlite3_close(sqlite3* db) {
    if (db) delete db;
    return SQLITE_OK;
}

// ============================================================================
// Execution
// ============================================================================

int sqlite3_exec(sqlite3* db, const char* sql,
                 int (*callback)(void*, int, char**, char**),
                 void* arg, char** errmsg) {
    if (errmsg) *errmsg = nullptr;
    // Simple mock execution: just handle CREATE TABLE
    std::string s(sql);
    if (s.find("CREATE TABLE") != std::string::npos) {
        // Extract table name (very crude)
        size_t pos = s.find("TABLE") + 6;
        size_t end = s.find("(", pos);
        if (end != std::string::npos) {
            std::string name = s.substr(pos, end - pos);
            // Trim
            name.erase(0, name.find_first_not_of(" \t\r\n"));
            name.erase(name.find_last_not_of(" \t\r\n") + 1);
            if (db) db->tables[name] = MockTable();
        }
        return SQLITE_OK;
    }
    return SQLITE_OK; 
}

void sqlite3_free(void* ptr) {
    if (ptr) free(ptr);
}

// ============================================================================
// Prepared statements
// ============================================================================

int sqlite3_prepare_v2(sqlite3* db, const char* zSql, int nByte,
                       sqlite3_stmt** ppStmt, const char** pzTail) {
    auto stmt = new sqlite3_stmt();
    stmt->db = db;
    stmt->sql = zSql;
    
    // If it's a SELECT, pre-populate results if table exists
    if (stmt->sql.find("SELECT") != std::string::npos) {
        for (auto& pair : db->tables) {
            if (stmt->sql.find(pair.first) != std::string::npos) {
                stmt->results = pair.second;
                break;
            }
        }
    }
    
    *ppStmt = stmt;
    if (pzTail) *pzTail = nullptr;
    return SQLITE_OK;
}

int sqlite3_step(sqlite3_stmt* pStmt) {
    if (!pStmt) return SQLITE_ERROR;
    
    if (pStmt->sql.find("INSERT") != std::string::npos) {
        // Just mock success for inserts
        return SQLITE_DONE;
    }
    
    if (pStmt->sql.find("SELECT") != std::string::npos) {
        pStmt->currentRow++;
        if (pStmt->currentRow < (int)pStmt->results.rows.size()) {
            return SQLITE_ROW;
        }
        return SQLITE_DONE;
    }
    
    return SQLITE_DONE;
}

int sqlite3_reset(sqlite3_stmt* pStmt) {
    return SQLITE_OK;
}

int sqlite3_finalize(sqlite3_stmt* pStmt) {
    return SQLITE_OK;
}

int sqlite3_clear_bindings(sqlite3_stmt* pStmt) {
    return SQLITE_OK;
}

// ============================================================================
// Bind values
// ============================================================================

int sqlite3_bind_int(sqlite3_stmt* pStmt, int index, int value) {
    return SQLITE_ERROR;
}

int sqlite3_bind_int64(sqlite3_stmt* pStmt, int index, sqlite3_int64 value) {
    return SQLITE_ERROR;
}

int sqlite3_bind_double(sqlite3_stmt* pStmt, int index, double value) {
    return SQLITE_ERROR;
}

int sqlite3_bind_text(sqlite3_stmt* pStmt, int index, const char* value,
                      int nBytes, void(*destructor)(void*)) {
    return SQLITE_ERROR;
}

int sqlite3_bind_blob(sqlite3_stmt* pStmt, int index, const void* value,
                      int nBytes, void(*destructor)(void*)) {
    return SQLITE_ERROR;
}

int sqlite3_bind_null(sqlite3_stmt* pStmt, int index) {
    return SQLITE_ERROR;
}

// ============================================================================
// Column access
// ============================================================================

int sqlite3_column_count(sqlite3_stmt* pStmt) {
    return 0;
}

const char* sqlite3_column_name(sqlite3_stmt* pStmt, int N) {
    return "";
}

const unsigned char* sqlite3_column_text(sqlite3_stmt* pStmt, int iCol) {
    static const unsigned char empty[] = "";
    return empty;
}

int sqlite3_column_int(sqlite3_stmt* pStmt, int iCol) {
    return 0;
}

sqlite3_int64 sqlite3_column_int64(sqlite3_stmt* pStmt, int iCol) {
    return 0;
}

double sqlite3_column_double(sqlite3_stmt* pStmt, int iCol) {
    return 0.0;
}

const void* sqlite3_column_blob(sqlite3_stmt* pStmt, int iCol) {
    return nullptr;
}

int sqlite3_column_bytes(sqlite3_stmt* pStmt, int iCol) {
    return 0;
}

int sqlite3_column_type(sqlite3_stmt* pStmt, int iCol) {
    return 0;
}

// ============================================================================
// Error reporting
// ============================================================================

const char* sqlite3_errmsg(sqlite3* db) {
    return "SQLite3 stub: no database implementation linked";
}

int sqlite3_errcode(sqlite3* db) {
    return SQLITE_ERROR;
}

// ============================================================================
// Changes tracking
// ============================================================================

int sqlite3_changes(sqlite3* db) {
    return 0;
}

int sqlite3_total_changes(sqlite3* db) {
    return 0;
}

sqlite3_int64 sqlite3_last_insert_rowid(sqlite3* db) {
    return 0;
}

#ifdef __cplusplus
}
#endif
