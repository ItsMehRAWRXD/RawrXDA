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

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Connection management
// ============================================================================

int sqlite3_open(const char* filename, sqlite3** ppDb) {
    *ppDb = nullptr;
    return SQLITE_ERROR;  // Stub: no real database
}

int sqlite3_open_v2(const char* filename, sqlite3** ppDb, int flags,
                    const char* zVfs) {
    *ppDb = nullptr;
    return SQLITE_ERROR;  // Stub: no real database
}

int sqlite3_close(sqlite3* db) {
    return SQLITE_OK;
}

// ============================================================================
// Execution
// ============================================================================

int sqlite3_exec(sqlite3* db, const char* sql,
                 int (*callback)(void*, int, char**, char**),
                 void* arg, char** errmsg) {
    if (errmsg) {
        *errmsg = nullptr;
    }
    return SQLITE_ERROR;  // Stub
}

void sqlite3_free(void* ptr) {
    if (ptr) free(ptr);
}

// ============================================================================
// Prepared statements
// ============================================================================

int sqlite3_prepare_v2(sqlite3* db, const char* zSql, int nByte,
                       sqlite3_stmt** ppStmt, const char** pzTail) {
    *ppStmt = nullptr;
    if (pzTail) *pzTail = nullptr;
    return SQLITE_ERROR;  // Stub
}

int sqlite3_step(sqlite3_stmt* pStmt) {
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
