// ============================================================================
// sqlite3.h — Minimal SQLite3 API Stub Header for RawrXD Build System
// ============================================================================
// This provides the minimal type definitions and function declarations
// required by sqlite_wrapper.cpp. Link against a real sqlite3.lib/dll
// at deployment time, or compile sqlite3.c amalgamation into the project.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Return Codes
// ============================================================================

#define SQLITE_OK           0
#define SQLITE_ERROR        1
#define SQLITE_INTERNAL     2
#define SQLITE_PERM         3
#define SQLITE_ABORT        4
#define SQLITE_BUSY         5
#define SQLITE_LOCKED       6
#define SQLITE_NOMEM        7
#define SQLITE_READONLY     8
#define SQLITE_INTERRUPT    9
#define SQLITE_IOERR       10
#define SQLITE_CORRUPT     11
#define SQLITE_NOTFOUND    12
#define SQLITE_FULL        13
#define SQLITE_CANTOPEN    14
#define SQLITE_PROTOCOL    15
#define SQLITE_EMPTY       16
#define SQLITE_SCHEMA      17
#define SQLITE_TOOBIG      18
#define SQLITE_CONSTRAINT  19
#define SQLITE_MISMATCH    20
#define SQLITE_MISUSE      21
#define SQLITE_NOLFS       22
#define SQLITE_AUTH        23
#define SQLITE_FORMAT      24
#define SQLITE_RANGE       25
#define SQLITE_NOTADB      26
#define SQLITE_ROW        100
#define SQLITE_DONE       101

// ============================================================================
// Open Flags
// ============================================================================

#define SQLITE_OPEN_READONLY      0x00000001
#define SQLITE_OPEN_READWRITE     0x00000002
#define SQLITE_OPEN_CREATE        0x00000004
#define SQLITE_OPEN_DELETEONCLOSE 0x00000008
#define SQLITE_OPEN_EXCLUSIVE     0x00000010
#define SQLITE_OPEN_AUTOPROXY     0x00000020
#define SQLITE_OPEN_URI           0x00000040
#define SQLITE_OPEN_MEMORY        0x00000080
#define SQLITE_OPEN_MAIN_DB       0x00000100
#define SQLITE_OPEN_TEMP_DB       0x00000200
#define SQLITE_OPEN_TRANSIENT_DB  0x00000400
#define SQLITE_OPEN_MAIN_JOURNAL  0x00000800
#define SQLITE_OPEN_TEMP_JOURNAL  0x00001000
#define SQLITE_OPEN_SUBJOURNAL    0x00002000
#define SQLITE_OPEN_SUPER_JOURNAL 0x00004000
#define SQLITE_OPEN_NOMUTEX       0x00008000
#define SQLITE_OPEN_FULLMUTEX     0x00010000
#define SQLITE_OPEN_SHAREDCACHE   0x00020000
#define SQLITE_OPEN_PRIVATECACHE  0x00040000
#define SQLITE_OPEN_WAL           0x00080000
#define SQLITE_OPEN_NOFOLLOW      0x01000000
#define SQLITE_OPEN_EXRESCODE     0x02000000

// ============================================================================
// Destructor Types
// ============================================================================

typedef void (*sqlite3_destructor_type)(void*);
#define SQLITE_STATIC      ((sqlite3_destructor_type)0)
#define SQLITE_TRANSIENT   ((sqlite3_destructor_type)(void(*)(void*))-1)

// ============================================================================
// Opaque Types
// ============================================================================

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
typedef struct sqlite3_vfs sqlite3_vfs;

typedef long long sqlite3_int64;
typedef unsigned long long sqlite3_uint64;

// ============================================================================
// Core API Functions
// ============================================================================

// Database connection
int sqlite3_open(const char *filename, sqlite3 **ppDb);
int sqlite3_open_v2(const char *filename, sqlite3 **ppDb, int flags,
                    const char *zVfs);
int sqlite3_close(sqlite3 *db);

// SQL execution (simple)
int sqlite3_exec(sqlite3 *db, const char *sql,
                 int (*callback)(void*, int, char**, char**),
                 void *arg, char **errmsg);
void sqlite3_free(void *ptr);

// Prepared statements
int sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nByte,
                       sqlite3_stmt **ppStmt, const char **pzTail);
int sqlite3_step(sqlite3_stmt *pStmt);
int sqlite3_reset(sqlite3_stmt *pStmt);
int sqlite3_finalize(sqlite3_stmt *pStmt);
int sqlite3_clear_bindings(sqlite3_stmt *pStmt);

// Bind values
int sqlite3_bind_int(sqlite3_stmt *pStmt, int index, int value);
int sqlite3_bind_int64(sqlite3_stmt *pStmt, int index, sqlite3_int64 value);
int sqlite3_bind_double(sqlite3_stmt *pStmt, int index, double value);
int sqlite3_bind_text(sqlite3_stmt *pStmt, int index, const char *value,
                      int nBytes, void(*destructor)(void*));
int sqlite3_bind_blob(sqlite3_stmt *pStmt, int index, const void *value,
                      int nBytes, void(*destructor)(void*));
int sqlite3_bind_null(sqlite3_stmt *pStmt, int index);

// Column access
int sqlite3_column_count(sqlite3_stmt *pStmt);
const char *sqlite3_column_name(sqlite3_stmt *pStmt, int N);
const unsigned char *sqlite3_column_text(sqlite3_stmt *pStmt, int iCol);
int sqlite3_column_int(sqlite3_stmt *pStmt, int iCol);
sqlite3_int64 sqlite3_column_int64(sqlite3_stmt *pStmt, int iCol);
double sqlite3_column_double(sqlite3_stmt *pStmt, int iCol);
const void *sqlite3_column_blob(sqlite3_stmt *pStmt, int iCol);
int sqlite3_column_bytes(sqlite3_stmt *pStmt, int iCol);
int sqlite3_column_type(sqlite3_stmt *pStmt, int iCol);

// Error reporting
const char *sqlite3_errmsg(sqlite3 *db);
int sqlite3_errcode(sqlite3 *db);

// Changes tracking
int sqlite3_changes(sqlite3 *db);
int sqlite3_total_changes(sqlite3 *db);
sqlite3_int64 sqlite3_last_insert_rowid(sqlite3 *db);

#ifdef __cplusplus
}
#endif
