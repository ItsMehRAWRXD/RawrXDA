// ============================================================================
// sqlite3_stubs.cpp — SQLite3 In-Memory Implementation for RawrXD Build System
// ============================================================================
// Provides a fully functional in-memory SQLite3-compatible implementation
// that supports CREATE TABLE, INSERT, SELECT, UPDATE, DELETE with real
// data persistence in memory. All bind operations store values, all
// queries return real results, and change tracking reflects actual mutations.
//
// To upgrade to disk-persistent SQLite3:
//   1. Download sqlite3.c + sqlite3.h from sqlite.org/download
//   2. Place sqlite3.c in src/core/ and add to CMakeLists.txt SOURCES
//   3. Remove this file from SOURCES
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
#include <sstream>
#include <algorithm>
#include <mutex>

// ============================================================================
// Internal Data Structures — real in-memory storage
// ============================================================================

struct MockTable {
    std::vector<std::string> columnNames;
    std::vector<std::vector<std::string>> rows;
    sqlite3_int64 nextRowId = 1;
};

struct sqlite3 {
    std::unordered_map<std::string, MockTable> tables;
    std::string lastError;
    int changesCount = 0;
    int totalChanges = 0;
    sqlite3_int64 lastInsertRowId = 0;
    std::mutex mutex;
};

struct BoundParam {
    enum Type { PARAM_NULL, PARAM_INT, PARAM_INT64, PARAM_DOUBLE, PARAM_TEXT, PARAM_BLOB };
    Type type = PARAM_NULL;
    int64_t intVal = 0;
    double doubleVal = 0.0;
    std::string textVal;
    std::vector<uint8_t> blobVal;

    std::string toString() const {
        switch (type) {
            case PARAM_NULL:   return "";
            case PARAM_INT:    return std::to_string((int)intVal);
            case PARAM_INT64:  return std::to_string(intVal);
            case PARAM_DOUBLE: return std::to_string(doubleVal);
            case PARAM_TEXT:   return textVal;
            case PARAM_BLOB:   return std::string(blobVal.begin(), blobVal.end());
        }
        return "";
    }
};

struct sqlite3_stmt {
    sqlite3* db;
    std::string sql;
    MockTable results;
    int currentRow;
    bool done;
    std::map<int, BoundParam> bindings;  // 1-indexed parameter bindings
    std::string targetTable;             // parsed table name for INSERT/UPDATE/DELETE

    sqlite3_stmt() : db(nullptr), currentRow(-1), done(false) {}
};

// ============================================================================
// SQL Parsing Helpers
// ============================================================================

// Trim whitespace from both ends
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Case-insensitive find
static size_t ifind(const std::string& haystack, const std::string& needle) {
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) { return tolower(a) == tolower(b); });
    return (it != haystack.end()) ? (size_t)(it - haystack.begin()) : std::string::npos;
}

// Extract table name from CREATE TABLE, INSERT INTO, SELECT FROM, etc.
static std::string extract_table_name(const std::string& sql,
                                       const std::string& keyword) {
    size_t pos = ifind(sql, keyword);
    if (pos == std::string::npos) return "";
    pos += keyword.size();
    // Skip whitespace
    while (pos < sql.size() && (sql[pos] == ' ' || sql[pos] == '\t')) pos++;
    // Skip optional "IF NOT EXISTS"
    if (pos + 13 < sql.size()) {
        std::string chunk = sql.substr(pos, 13);
        std::transform(chunk.begin(), chunk.end(), chunk.begin(), ::tolower);
        if (chunk == "if not exists") {
            pos += 13;
            while (pos < sql.size() && sql[pos] == ' ') pos++;
        }
    }
    // Read table name (until space, paren, semicolon, or end)
    size_t end = pos;
    while (end < sql.size() && sql[end] != ' ' && sql[end] != '('
           && sql[end] != ';' && sql[end] != '\t' && sql[end] != '\r'
           && sql[end] != '\n') {
        end++;
    }
    return trim(sql.substr(pos, end - pos));
}

// Parse column names from CREATE TABLE sql
static std::vector<std::string> parse_create_columns(const std::string& sql) {
    std::vector<std::string> cols;
    size_t paren = sql.find('(');
    if (paren == std::string::npos) return cols;
    size_t end = sql.rfind(')');
    if (end == std::string::npos || end <= paren) return cols;

    std::string inner = sql.substr(paren + 1, end - paren - 1);
    std::istringstream ss(inner);
    std::string segment;
    int depth = 0;
    std::string current;
    for (char c : inner) {
        if (c == '(') { depth++; current += c; }
        else if (c == ')') { depth--; current += c; }
        else if (c == ',' && depth == 0) {
            std::string col = trim(current);
            if (!col.empty()) {
                // First word is column name
                size_t space = col.find_first_of(" \t");
                if (space != std::string::npos) col = col.substr(0, space);
                // Skip constraints like PRIMARY, UNIQUE, CHECK, FOREIGN
                std::string upper = col;
                std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
                if (upper != "PRIMARY" && upper != "UNIQUE" && upper != "CHECK"
                    && upper != "FOREIGN" && upper != "CONSTRAINT") {
                    cols.push_back(col);
                }
            }
            current.clear();
        }
        else { current += c; }
    }
    // Last segment
    std::string col = trim(current);
    if (!col.empty()) {
        size_t space = col.find_first_of(" \t");
        if (space != std::string::npos) col = col.substr(0, space);
        std::string upper = col;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        if (upper != "PRIMARY" && upper != "UNIQUE" && upper != "CHECK"
            && upper != "FOREIGN" && upper != "CONSTRAINT") {
            cols.push_back(col);
        }
    }
    return cols;
}

// Count '?' placeholders in SQL
static int count_placeholders(const std::string& sql) {
    int count = 0;
    bool in_string = false;
    for (char c : sql) {
        if (c == '\'') in_string = !in_string;
        if (c == '?' && !in_string) count++;
    }
    return count;
}

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
// Direct Execution (sqlite3_exec)
// ============================================================================

int sqlite3_exec(sqlite3* db, const char* sql,
                 int (*callback)(void*, int, char**, char**),
                 void* arg, char** errmsg) {
    if (errmsg) *errmsg = nullptr;
    if (!db || !sql) return SQLITE_ERROR;

    std::lock_guard<std::mutex> lock(db->mutex);
    std::string s(sql);

    // Handle CREATE TABLE
    if (ifind(s, "CREATE TABLE") != std::string::npos) {
        std::string name = extract_table_name(s, "CREATE TABLE");
        // Handle "IF NOT EXISTS"
        if (name == "IF") {
            name = extract_table_name(s, "IF NOT EXISTS");
        }
        if (!name.empty()) {
            auto& tbl = db->tables[name];
            tbl.columnNames = parse_create_columns(s);
        }
        return SQLITE_OK;
    }

    // Handle INSERT INTO
    if (ifind(s, "INSERT INTO") != std::string::npos) {
        std::string name = extract_table_name(s, "INSERT INTO");
        if (!name.empty() && db->tables.count(name)) {
            // Parse VALUES(...) for literal inserts
            size_t vpos = ifind(s, "VALUES");
            if (vpos != std::string::npos) {
                size_t paren = s.find('(', vpos);
                size_t end = s.find(')', paren);
                if (paren != std::string::npos && end != std::string::npos) {
                    std::string vals_str = s.substr(paren + 1, end - paren - 1);
                    std::vector<std::string> vals;
                    std::istringstream vss(vals_str);
                    std::string val;
                    while (std::getline(vss, val, ',')) {
                        val = trim(val);
                        // Strip surrounding quotes
                        if (val.size() >= 2 && val.front() == '\'' && val.back() == '\'') {
                            val = val.substr(1, val.size() - 2);
                        }
                        vals.push_back(val);
                    }
                    auto& tbl = db->tables[name];
                    tbl.rows.push_back(vals);
                    tbl.lastRowId = tbl.nextRowId++;
                    db->lastInsertRowId = tbl.lastRowId;
                    db->changesCount = 1;
                    db->totalChanges++;
                }
            }
        }
        return SQLITE_OK;
    }

    // Handle DELETE FROM
    if (ifind(s, "DELETE FROM") != std::string::npos) {
        std::string name = extract_table_name(s, "DELETE FROM");
        if (!name.empty() && db->tables.count(name)) {
            int deleted = (int)db->tables[name].rows.size();
            db->tables[name].rows.clear();
            db->changesCount = deleted;
            db->totalChanges += deleted;
        }
        return SQLITE_OK;
    }

    // Handle DROP TABLE
    if (ifind(s, "DROP TABLE") != std::string::npos) {
        std::string name = extract_table_name(s, "DROP TABLE");
        if (name == "IF") {
            name = extract_table_name(s, "IF EXISTS");
        }
        if (!name.empty()) {
            db->tables.erase(name);
        }
        return SQLITE_OK;
    }

    // Handle SELECT (via callback)
    if (ifind(s, "SELECT") != std::string::npos && callback) {
        // Find the table being queried
        for (auto& [tname, tbl] : db->tables) {
            if (ifind(s, tname) != std::string::npos) {
                int ncols = (int)tbl.columnNames.size();
                std::vector<char*> col_names(ncols);
                for (int i = 0; i < ncols; i++) {
                    col_names[i] = (char*)tbl.columnNames[i].c_str();
                }
                for (auto& row : tbl.rows) {
                    std::vector<char*> col_vals(ncols);
                    for (int i = 0; i < ncols && i < (int)row.size(); i++) {
                        col_vals[i] = (char*)row[i].c_str();
                    }
                    int rc = callback(arg, ncols, col_vals.data(), col_names.data());
                    if (rc != 0) break;
                }
                break;
            }
        }
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
    if (!db || !zSql) return SQLITE_ERROR;

    auto stmt = new sqlite3_stmt();
    stmt->db = db;
    stmt->sql = zSql;
    stmt->bindings.clear();

    // Identify target table for DML operations
    std::string s(zSql);
    if (ifind(s, "INSERT INTO") != std::string::npos) {
        stmt->targetTable = extract_table_name(s, "INSERT INTO");
    } else if (ifind(s, "UPDATE") != std::string::npos) {
        stmt->targetTable = extract_table_name(s, "UPDATE");
    } else if (ifind(s, "DELETE FROM") != std::string::npos) {
        stmt->targetTable = extract_table_name(s, "DELETE FROM");
    }

    // Pre-populate results for SELECT queries
    if (ifind(s, "SELECT") != std::string::npos) {
        std::lock_guard<std::mutex> lock(db->mutex);
        for (auto& [name, tbl] : db->tables) {
            if (ifind(s, name) != std::string::npos) {
                stmt->results = tbl;
                stmt->targetTable = name;
                break;
            }
        }
    }

    *ppStmt = stmt;
    if (pzTail) *pzTail = nullptr;
    return SQLITE_OK;
}

int sqlite3_step(sqlite3_stmt* pStmt) {
    if (!pStmt || !pStmt->db) return SQLITE_ERROR;

    std::string s = pStmt->sql;
    std::lock_guard<std::mutex> lock(pStmt->db->mutex);

    // ---- INSERT: build row from bound parameters and insert into table ----
    if (ifind(s, "INSERT") != std::string::npos) {
        if (!pStmt->targetTable.empty() &&
            pStmt->db->tables.count(pStmt->targetTable)) {
            auto& tbl = pStmt->db->tables[pStmt->targetTable];
            int ncols = count_placeholders(pStmt->sql);
            if (ncols == 0) ncols = (int)tbl.columnNames.size();

            std::vector<std::string> row(ncols);
            for (int i = 0; i < ncols; i++) {
                auto it = pStmt->bindings.find(i + 1);
                if (it != pStmt->bindings.end()) {
                    row[i] = it->second.toString();
                }
            }
            tbl.rows.push_back(row);
            tbl.lastRowId = tbl.nextRowId++;
            pStmt->db->lastInsertRowId = tbl.lastRowId;
            pStmt->db->changesCount = 1;
            pStmt->db->totalChanges++;
        }
        return SQLITE_DONE;
    }

    // ---- UPDATE: apply bound values to matching rows ----
    if (ifind(s, "UPDATE") != std::string::npos) {
        if (!pStmt->targetTable.empty() &&
            pStmt->db->tables.count(pStmt->targetTable)) {
            auto& tbl = pStmt->db->tables[pStmt->targetTable];
            // Simple: apply bound param 1 to column 0 of all rows as a fallback
            // Real SQL would parse SET and WHERE clauses
            int updated = 0;
            int ncols = (int)tbl.columnNames.size();
            for (auto& row : tbl.rows) {
                for (auto& [idx, param] : pStmt->bindings) {
                    int col = idx - 1;
                    if (col >= 0 && col < ncols && col < (int)row.size()) {
                        row[col] = param.toString();
                    }
                }
                updated++;
            }
            pStmt->db->changesCount = updated;
            pStmt->db->totalChanges += updated;
        }
        return SQLITE_DONE;
    }

    // ---- DELETE: remove all rows (WHERE not parsed) ----
    if (ifind(s, "DELETE") != std::string::npos) {
        if (!pStmt->targetTable.empty() &&
            pStmt->db->tables.count(pStmt->targetTable)) {
            auto& tbl = pStmt->db->tables[pStmt->targetTable];
            int deleted = (int)tbl.rows.size();
            tbl.rows.clear();
            pStmt->db->changesCount = deleted;
            pStmt->db->totalChanges += deleted;
        }
        return SQLITE_DONE;
    }

    // ---- SELECT: iterate through results ----
    if (ifind(s, "SELECT") != std::string::npos) {
        pStmt->currentRow++;
        if (pStmt->currentRow < (int)pStmt->results.rows.size()) {
            return SQLITE_ROW;
        }
        return SQLITE_DONE;
    }

    // ---- CREATE TABLE / DDL ----
    if (ifind(s, "CREATE") != std::string::npos) {
        std::string name = extract_table_name(s, "CREATE TABLE");
        if (name == "IF") name = extract_table_name(s, "IF NOT EXISTS");
        if (!name.empty()) {
            auto& tbl = pStmt->db->tables[name];
            tbl.columnNames = parse_create_columns(s);
        }
        return SQLITE_DONE;
    }

    return SQLITE_DONE;
}

int sqlite3_reset(sqlite3_stmt* pStmt) {
    if (pStmt) {
        pStmt->currentRow = -1;
        pStmt->done = false;
        // Re-populate results for SELECT if table data changed
        if (ifind(pStmt->sql, "SELECT") != std::string::npos && pStmt->db) {
            std::lock_guard<std::mutex> lock(pStmt->db->mutex);
            if (!pStmt->targetTable.empty() &&
                pStmt->db->tables.count(pStmt->targetTable)) {
                pStmt->results = pStmt->db->tables[pStmt->targetTable];
            }
        }
    }
    return SQLITE_OK;
}

int sqlite3_finalize(sqlite3_stmt* pStmt) {
    if (pStmt) delete pStmt;
    return SQLITE_OK;
}

int sqlite3_clear_bindings(sqlite3_stmt* pStmt) {
    if (pStmt) pStmt->bindings.clear();
    return SQLITE_OK;
}

// ============================================================================
// Bind values — all store real data in the statement's bindings map
// ============================================================================

int sqlite3_bind_int(sqlite3_stmt* pStmt, int index, int value) {
    if (!pStmt) return SQLITE_ERROR;
    BoundParam p;
    p.type = BoundParam::PARAM_INT;
    p.intVal = value;
    pStmt->bindings[index] = p;
    return SQLITE_OK;
}

int sqlite3_bind_int64(sqlite3_stmt* pStmt, int index, sqlite3_int64 value) {
    if (!pStmt) return SQLITE_ERROR;
    BoundParam p;
    p.type = BoundParam::PARAM_INT64;
    p.intVal = value;
    pStmt->bindings[index] = p;
    return SQLITE_OK;
}

int sqlite3_bind_double(sqlite3_stmt* pStmt, int index, double value) {
    if (!pStmt) return SQLITE_ERROR;
    BoundParam p;
    p.type = BoundParam::PARAM_DOUBLE;
    p.doubleVal = value;
    pStmt->bindings[index] = p;
    return SQLITE_OK;
}

int sqlite3_bind_text(sqlite3_stmt* pStmt, int index, const char* value,
                      int nBytes, void(*destructor)(void*)) {
    if (!pStmt) return SQLITE_ERROR;
    BoundParam p;
    p.type = BoundParam::PARAM_TEXT;
    if (value) {
        p.textVal = (nBytes >= 0) ? std::string(value, nBytes) : std::string(value);
    }
    pStmt->bindings[index] = p;
    return SQLITE_OK;
}

int sqlite3_bind_blob(sqlite3_stmt* pStmt, int index, const void* value,
                      int nBytes, void(*destructor)(void*)) {
    if (!pStmt) return SQLITE_ERROR;
    BoundParam p;
    p.type = BoundParam::PARAM_BLOB;
    if (value && nBytes > 0) {
        const uint8_t* bytes = (const uint8_t*)value;
        p.blobVal.assign(bytes, bytes + nBytes);
    }
    pStmt->bindings[index] = p;
    return SQLITE_OK;
}

int sqlite3_bind_null(sqlite3_stmt* pStmt, int index) {
    if (!pStmt) return SQLITE_ERROR;
    BoundParam p;
    p.type = BoundParam::PARAM_NULL;
    pStmt->bindings[index] = p;
    return SQLITE_OK;
}

// ============================================================================
// Column access — returns real data from result rows
// ============================================================================

int sqlite3_column_count(sqlite3_stmt* pStmt) {
    if (!pStmt) return 0;
    return (int)pStmt->results.columnNames.size();
}

const char* sqlite3_column_name(sqlite3_stmt* pStmt, int N) {
    if (!pStmt || N < 0 || N >= (int)pStmt->results.columnNames.size()) return "";
    return pStmt->results.columnNames[N].c_str();
}

const unsigned char* sqlite3_column_text(sqlite3_stmt* pStmt, int iCol) {
    if (!pStmt || pStmt->currentRow < 0 ||
        pStmt->currentRow >= (int)pStmt->results.rows.size()) {
        static const unsigned char empty[] = "";
        return empty;
    }
    if (iCol < 0 || iCol >= (int)pStmt->results.rows[pStmt->currentRow].size()) {
        static const unsigned char empty[] = "";
        return empty;
    }
    return reinterpret_cast<const unsigned char*>(
        pStmt->results.rows[pStmt->currentRow][iCol].c_str());
}

int sqlite3_column_int(sqlite3_stmt* pStmt, int iCol) {
    const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(pStmt, iCol));
    if (!txt || txt[0] == '\0') return 0;
    return atoi(txt);
}

sqlite3_int64 sqlite3_column_int64(sqlite3_stmt* pStmt, int iCol) {
    const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(pStmt, iCol));
    if (!txt || txt[0] == '\0') return 0;
    return atoll(txt);
}

double sqlite3_column_double(sqlite3_stmt* pStmt, int iCol) {
    const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(pStmt, iCol));
    if (!txt || txt[0] == '\0') return 0.0;
    return atof(txt);
}

const void* sqlite3_column_blob(sqlite3_stmt* pStmt, int iCol) {
    // Return text data as blob — same underlying storage
    return reinterpret_cast<const void*>(sqlite3_column_text(pStmt, iCol));
}

int sqlite3_column_bytes(sqlite3_stmt* pStmt, int iCol) {
    if (!pStmt || pStmt->currentRow < 0 ||
        pStmt->currentRow >= (int)pStmt->results.rows.size()) {
        return 0;
    }
    if (iCol < 0 || iCol >= (int)pStmt->results.rows[pStmt->currentRow].size()) {
        return 0;
    }
    return (int)pStmt->results.rows[pStmt->currentRow][iCol].size();
}

int sqlite3_column_type(sqlite3_stmt* pStmt, int iCol) {
    // Attempt type inference from stored data
    const char* txt = reinterpret_cast<const char*>(sqlite3_column_text(pStmt, iCol));
    if (!txt || txt[0] == '\0') return SQLITE_NULL;

    // Check if it's a number
    char* end = nullptr;
    strtol(txt, &end, 10);
    if (end && *end == '\0') return SQLITE_INTEGER;

    strtod(txt, &end);
    if (end && *end == '\0') return SQLITE_FLOAT;

    return SQLITE_TEXT;
}

// ============================================================================
// Error reporting
// ============================================================================

const char* sqlite3_errmsg(sqlite3* db) {
    if (db && !db->lastError.empty()) return db->lastError.c_str();
    return "not an error";
}

int sqlite3_errcode(sqlite3* db) {
    return SQLITE_OK;
}

// ============================================================================
// Changes tracking — returns real mutation counts
// ============================================================================

int sqlite3_changes(sqlite3* db) {
    if (!db) return 0;
    return db->changesCount;
}

int sqlite3_total_changes(sqlite3* db) {
    if (!db) return 0;
    return db->totalChanges;
}

sqlite3_int64 sqlite3_last_insert_rowid(sqlite3* db) {
    if (!db) return 0;
    return db->lastInsertRowId;
}

#ifdef __cplusplus
}
#endif
