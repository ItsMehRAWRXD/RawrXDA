/**
 * @file database_layer.cpp
 * @brief Enterprise Database Persistence Layer Implementation (SQLite)
 */

#include "enterprise/database_layer.hpp"
#include <sstream>
#include <iostream>
#include <cstring>
#include <algorithm>

// SQLite3 header - in production, link against sqlite3.lib
// For now, we implement a complete abstraction that can work with SQLite
#ifdef USE_SQLITE
#include <sqlite3.h>
#endif

namespace enterprise {

// =============================================================================
// DbRow Implementation
// =============================================================================

std::optional<DbValue> DbRow::get(const std::string& column) const {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] == column && i < values.size()) {
            return values[i];
        }
    }
    return std::nullopt;
}

int DbRow::getInt(const std::string& column, int defaultValue) const {
    auto val = get(column);
    if (!val) return defaultValue;
    
    return std::visit([defaultValue](auto&& arg) -> int {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) return arg;
        else if constexpr (std::is_same_v<T, long long>) return static_cast<int>(arg);
        else if constexpr (std::is_same_v<T, double>) return static_cast<int>(arg);
        else if constexpr (std::is_same_v<T, std::string>) {
            try { return std::stoi(arg); } catch (...) { return defaultValue; }
        }
        else return defaultValue;
    }, *val);
}

long long DbRow::getInt64(const std::string& column, long long defaultValue) const {
    auto val = get(column);
    if (!val) return defaultValue;
    
    return std::visit([defaultValue](auto&& arg) -> long long {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) return arg;
        else if constexpr (std::is_same_v<T, long long>) return arg;
        else if constexpr (std::is_same_v<T, double>) return static_cast<long long>(arg);
        else if constexpr (std::is_same_v<T, std::string>) {
            try { return std::stoll(arg); } catch (...) { return defaultValue; }
        }
        else return defaultValue;
    }, *val);
}

double DbRow::getDouble(const std::string& column, double defaultValue) const {
    auto val = get(column);
    if (!val) return defaultValue;
    
    return std::visit([defaultValue](auto&& arg) -> double {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) return static_cast<double>(arg);
        else if constexpr (std::is_same_v<T, long long>) return static_cast<double>(arg);
        else if constexpr (std::is_same_v<T, double>) return arg;
        else if constexpr (std::is_same_v<T, std::string>) {
            try { return std::stod(arg); } catch (...) { return defaultValue; }
        }
        else return defaultValue;
    }, *val);
}

std::string DbRow::getString(const std::string& column, const std::string& defaultValue) const {
    auto val = get(column);
    if (!val) return defaultValue;
    
    return std::visit([&defaultValue](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) return defaultValue;
        else if constexpr (std::is_same_v<T, int>) return std::to_string(arg);
        else if constexpr (std::is_same_v<T, long long>) return std::to_string(arg);
        else if constexpr (std::is_same_v<T, double>) return std::to_string(arg);
        else if constexpr (std::is_same_v<T, std::string>) return arg;
        else if constexpr (std::is_same_v<T, std::vector<unsigned char>>) {
            return std::string(arg.begin(), arg.end());
        }
        else return defaultValue;
    }, *val);
}

std::vector<unsigned char> DbRow::getBlob(const std::string& column) const {
    auto val = get(column);
    if (!val) return {};
    
    return std::visit([](auto&& arg) -> std::vector<unsigned char> {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::vector<unsigned char>>) return arg;
        else if constexpr (std::is_same_v<T, std::string>) {
            return std::vector<unsigned char>(arg.begin(), arg.end());
        }
        else return {};
    }, *val);
}

bool DbRow::isNull(const std::string& column) const {
    auto val = get(column);
    if (!val) return true;
    return std::holds_alternative<std::nullptr_t>(*val);
}

// =============================================================================
// QueryBuilder Implementation
// =============================================================================

QueryBuilder& QueryBuilder::select(const std::vector<std::string>& columns) {
    m_type = QueryType::SELECT_T;
    m_columns = columns;
    return *this;
}

QueryBuilder& QueryBuilder::from(const std::string& table) {
    m_table = table;
    return *this;
}

QueryBuilder& QueryBuilder::join(const std::string& table, const std::string& condition, const std::string& type) {
    m_joins.push_back(type + " JOIN " + table + " ON " + condition);
    return *this;
}

QueryBuilder& QueryBuilder::leftJoin(const std::string& table, const std::string& condition) {
    return join(table, condition, "LEFT");
}

QueryBuilder& QueryBuilder::rightJoin(const std::string& table, const std::string& condition) {
    return join(table, condition, "RIGHT");
}

QueryBuilder& QueryBuilder::where(const std::string& condition) {
    m_conditions.clear();
    m_conditions.push_back(condition);
    return *this;
}

QueryBuilder& QueryBuilder::whereAnd(const std::string& condition) {
    if (!m_conditions.empty()) {
        m_conditions.push_back("AND " + condition);
    } else {
        m_conditions.push_back(condition);
    }
    return *this;
}

QueryBuilder& QueryBuilder::whereOr(const std::string& condition) {
    if (!m_conditions.empty()) {
        m_conditions.push_back("OR " + condition);
    } else {
        m_conditions.push_back(condition);
    }
    return *this;
}

QueryBuilder& QueryBuilder::whereIn(const std::string& column, const std::vector<DbValue>& values) {
    std::stringstream ss;
    ss << column << " IN (";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "?";
        m_parameters.push_back(values[i]);
    }
    ss << ")";
    return whereAnd(ss.str());
}

QueryBuilder& QueryBuilder::whereBetween(const std::string& column, const DbValue& min, const DbValue& max) {
    m_parameters.push_back(min);
    m_parameters.push_back(max);
    return whereAnd(column + " BETWEEN ? AND ?");
}

QueryBuilder& QueryBuilder::whereNull(const std::string& column) {
    return whereAnd(column + " IS NULL");
}

QueryBuilder& QueryBuilder::whereNotNull(const std::string& column) {
    return whereAnd(column + " IS NOT NULL");
}

QueryBuilder& QueryBuilder::orderBy(const std::string& column, bool ascending) {
    m_orderBy.push_back(column + (ascending ? " ASC" : " DESC"));
    return *this;
}

QueryBuilder& QueryBuilder::groupBy(const std::string& column) {
    m_groupBy.push_back(column);
    return *this;
}

QueryBuilder& QueryBuilder::having(const std::string& condition) {
    m_having = condition;
    return *this;
}

QueryBuilder& QueryBuilder::limit(int count) {
    m_limit = count;
    return *this;
}

QueryBuilder& QueryBuilder::offset(int count) {
    m_offset = count;
    return *this;
}

QueryBuilder& QueryBuilder::insert(const std::string& table) {
    m_type = QueryType::INSERT_T;
    m_table = table;
    return *this;
}

QueryBuilder& QueryBuilder::values(const std::unordered_map<std::string, DbValue>& data) {
    m_data = data;
    return *this;
}

QueryBuilder& QueryBuilder::update(const std::string& table) {
    m_type = QueryType::UPDATE_T;
    m_table = table;
    return *this;
}

QueryBuilder& QueryBuilder::set(const std::unordered_map<std::string, DbValue>& data) {
    m_data = data;
    return *this;
}

QueryBuilder& QueryBuilder::deleteFrom(const std::string& table) {
    m_type = QueryType::DELETE_T;
    m_table = table;
    return *this;
}

std::string QueryBuilder::build() const {
    std::stringstream sql;
    
    switch (m_type) {
        case QueryType::SELECT_T: {
            sql << "SELECT ";
            for (size_t i = 0; i < m_columns.size(); ++i) {
                if (i > 0) sql << ", ";
                sql << m_columns[i];
            }
            sql << " FROM " << m_table;
            
            for (const auto& join : m_joins) {
                sql << " " << join;
            }
            
            if (!m_conditions.empty()) {
                sql << " WHERE ";
                for (size_t i = 0; i < m_conditions.size(); ++i) {
                    if (i > 0) sql << " ";
                    sql << m_conditions[i];
                }
            }
            
            if (!m_groupBy.empty()) {
                sql << " GROUP BY ";
                for (size_t i = 0; i < m_groupBy.size(); ++i) {
                    if (i > 0) sql << ", ";
                    sql << m_groupBy[i];
                }
            }
            
            if (!m_having.empty()) {
                sql << " HAVING " << m_having;
            }
            
            if (!m_orderBy.empty()) {
                sql << " ORDER BY ";
                for (size_t i = 0; i < m_orderBy.size(); ++i) {
                    if (i > 0) sql << ", ";
                    sql << m_orderBy[i];
                }
            }
            
            if (m_limit >= 0) {
                sql << " LIMIT " << m_limit;
            }
            
            if (m_offset >= 0) {
                sql << " OFFSET " << m_offset;
            }
            break;
        }
        
        case QueryType::INSERT_T: {
            sql << "INSERT INTO " << m_table << " (";
            std::vector<std::string> cols;
            for (const auto& [key, _] : m_data) {
                cols.push_back(key);
            }
            for (size_t i = 0; i < cols.size(); ++i) {
                if (i > 0) sql << ", ";
                sql << cols[i];
            }
            sql << ") VALUES (";
            for (size_t i = 0; i < cols.size(); ++i) {
                if (i > 0) sql << ", ";
                sql << "?";
            }
            sql << ")";
            break;
        }
        
        case QueryType::UPDATE_T: {
            sql << "UPDATE " << m_table << " SET ";
            size_t i = 0;
            for (const auto& [key, _] : m_data) {
                if (i > 0) sql << ", ";
                sql << key << " = ?";
                ++i;
            }
            
            if (!m_conditions.empty()) {
                sql << " WHERE ";
                for (size_t j = 0; j < m_conditions.size(); ++j) {
                    if (j > 0) sql << " ";
                    sql << m_conditions[j];
                }
            }
            break;
        }
        
        case QueryType::DELETE_T: {
            sql << "DELETE FROM " << m_table;
            
            if (!m_conditions.empty()) {
                sql << " WHERE ";
                for (size_t i = 0; i < m_conditions.size(); ++i) {
                    if (i > 0) sql << " ";
                    sql << m_conditions[i];
                }
            }
            break;
        }
    }
    
    return sql.str();
}

std::vector<DbValue> QueryBuilder::getParameters() const {
    std::vector<DbValue> params = m_parameters;
    
    // Add data values for INSERT/UPDATE
    if (m_type == QueryType::INSERT_T || m_type == QueryType::UPDATE_T) {
        for (const auto& [_, value] : m_data) {
            params.push_back(value);
        }
    }
    
    return params;
}

void QueryBuilder::reset() {
    m_type = QueryType::SELECT_T;
    m_table.clear();
    m_columns = {"*"};
    m_joins.clear();
    m_conditions.clear();
    m_orderBy.clear();
    m_groupBy.clear();
    m_having.clear();
    m_limit = -1;
    m_offset = -1;
    m_data.clear();
    m_parameters.clear();
}

// =============================================================================
// PreparedStatement Implementation
// =============================================================================

PreparedStatement::PreparedStatement(const std::string& sql) : m_sql(sql) {}

PreparedStatement::~PreparedStatement() {
#ifdef USE_SQLITE
    if (m_stmt) {
        sqlite3_finalize(static_cast<sqlite3_stmt*>(m_stmt));
    }
#endif
}

void PreparedStatement::bind(int index, std::nullptr_t) {
#ifdef USE_SQLITE
    if (m_stmt) {
        sqlite3_bind_null(static_cast<sqlite3_stmt*>(m_stmt), index);
    }
#endif
    (void)index;
}

void PreparedStatement::bind(int index, int value) {
#ifdef USE_SQLITE
    if (m_stmt) {
        sqlite3_bind_int(static_cast<sqlite3_stmt*>(m_stmt), index, value);
    }
#endif
    (void)index;
    (void)value;
}

void PreparedStatement::bind(int index, long long value) {
#ifdef USE_SQLITE
    if (m_stmt) {
        sqlite3_bind_int64(static_cast<sqlite3_stmt*>(m_stmt), index, value);
    }
#endif
    (void)index;
    (void)value;
}

void PreparedStatement::bind(int index, double value) {
#ifdef USE_SQLITE
    if (m_stmt) {
        sqlite3_bind_double(static_cast<sqlite3_stmt*>(m_stmt), index, value);
    }
#endif
    (void)index;
    (void)value;
}

void PreparedStatement::bind(int index, const std::string& value) {
#ifdef USE_SQLITE
    if (m_stmt) {
        sqlite3_bind_text(static_cast<sqlite3_stmt*>(m_stmt), index, value.c_str(), 
                          static_cast<int>(value.size()), SQLITE_TRANSIENT);
    }
#endif
    (void)index;
    (void)value;
}

void PreparedStatement::bind(int index, const std::vector<unsigned char>& value) {
#ifdef USE_SQLITE
    if (m_stmt) {
        sqlite3_bind_blob(static_cast<sqlite3_stmt*>(m_stmt), index, value.data(),
                          static_cast<int>(value.size()), SQLITE_TRANSIENT);
    }
#endif
    (void)index;
    (void)value;
}

void PreparedStatement::bind(int index, const DbValue& value) {
    std::visit([this, index](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) bind(index, nullptr);
        else if constexpr (std::is_same_v<T, int>) bind(index, arg);
        else if constexpr (std::is_same_v<T, long long>) bind(index, arg);
        else if constexpr (std::is_same_v<T, double>) bind(index, arg);
        else if constexpr (std::is_same_v<T, std::string>) bind(index, arg);
        else if constexpr (std::is_same_v<T, std::vector<unsigned char>>) bind(index, arg);
    }, value);
}

void PreparedStatement::bindAll(const std::vector<DbValue>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        bind(static_cast<int>(i + 1), values[i]);
    }
}

void PreparedStatement::reset() {
#ifdef USE_SQLITE
    if (m_stmt) {
        sqlite3_reset(static_cast<sqlite3_stmt*>(m_stmt));
        sqlite3_clear_bindings(static_cast<sqlite3_stmt*>(m_stmt));
    }
#endif
}

// =============================================================================
// DbConnection Implementation
// =============================================================================

DbConnection::DbConnection(const std::string& connectionString) 
    : m_connectionString(connectionString) {}

DbConnection::~DbConnection() {
    close();
}

bool DbConnection::open() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_db) return true;  // Already open
    
#ifdef USE_SQLITE
    int rc = sqlite3_open(m_connectionString.c_str(), reinterpret_cast<sqlite3**>(&m_db));
    if (rc != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(static_cast<sqlite3*>(m_db));
        sqlite3_close(static_cast<sqlite3*>(m_db));
        m_db = nullptr;
        return false;
    }
    
    // Enable foreign keys
    sqlite3_exec(static_cast<sqlite3*>(m_db), "PRAGMA foreign_keys = ON", nullptr, nullptr, nullptr);
    
    // Enable WAL mode for better concurrency
    sqlite3_exec(static_cast<sqlite3*>(m_db), "PRAGMA journal_mode = WAL", nullptr, nullptr, nullptr);
#else
    // Simulated database for non-SQLite builds
    m_db = reinterpret_cast<void*>(1);  // Non-null marker
#endif
    
    std::cout << "[Database] Opened connection: " << m_connectionString << std::endl;
    return true;
}

void DbConnection::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_db) return;
    
#ifdef USE_SQLITE
    sqlite3_close(static_cast<sqlite3*>(m_db));
#endif
    
    m_db = nullptr;
    std::cout << "[Database] Closed connection" << std::endl;
}

bool DbConnection::isOpen() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_db != nullptr;
}

DbResult DbConnection::execute(const std::string& sql) {
    return execute(sql, {});
}

DbResult DbConnection::execute(const std::string& sql, const std::vector<DbValue>& params) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    DbResult result;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    if (!m_db) {
        result.error = "Database connection not open";
        return result;
    }
    
#ifdef USE_SQLITE
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql.c_str(), -1, &stmt, nullptr);
    
    if (rc != SQLITE_OK) {
        result.error = sqlite3_errmsg(static_cast<sqlite3*>(m_db));
        return result;
    }
    
    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        int idx = static_cast<int>(i + 1);
        std::visit([stmt, idx](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                sqlite3_bind_null(stmt, idx);
            } else if constexpr (std::is_same_v<T, int>) {
                sqlite3_bind_int(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, long long>) {
                sqlite3_bind_int64(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, double>) {
                sqlite3_bind_double(stmt, idx, arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                sqlite3_bind_text(stmt, idx, arg.c_str(), static_cast<int>(arg.size()), SQLITE_TRANSIENT);
            } else if constexpr (std::is_same_v<T, std::vector<unsigned char>>) {
                sqlite3_bind_blob(stmt, idx, arg.data(), static_cast<int>(arg.size()), SQLITE_TRANSIENT);
            }
        }, params[i]);
    }
    
    // Get column names
    int colCount = sqlite3_column_count(stmt);
    std::vector<std::string> columns;
    for (int i = 0; i < colCount; ++i) {
        const char* name = sqlite3_column_name(stmt, i);
        columns.push_back(name ? name : "");
    }
    
    // Execute and fetch results
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        DbRow row;
        row.columns = columns;
        
        for (int i = 0; i < colCount; ++i) {
            int type = sqlite3_column_type(stmt, i);
            switch (type) {
                case SQLITE_NULL:
                    row.values.push_back(nullptr);
                    break;
                case SQLITE_INTEGER:
                    row.values.push_back(sqlite3_column_int64(stmt, i));
                    break;
                case SQLITE_FLOAT:
                    row.values.push_back(sqlite3_column_double(stmt, i));
                    break;
                case SQLITE_TEXT: {
                    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                    row.values.push_back(text ? std::string(text) : std::string());
                    break;
                }
                case SQLITE_BLOB: {
                    const unsigned char* blob = static_cast<const unsigned char*>(sqlite3_column_blob(stmt, i));
                    int size = sqlite3_column_bytes(stmt, i);
                    row.values.push_back(std::vector<unsigned char>(blob, blob + size));
                    break;
                }
            }
        }
        
        result.rows.push_back(std::move(row));
    }
    
    if (rc != SQLITE_DONE) {
        result.error = sqlite3_errmsg(static_cast<sqlite3*>(m_db));
        sqlite3_finalize(stmt);
        return result;
    }
    
    result.rowsAffected = sqlite3_changes(static_cast<sqlite3*>(m_db));
    result.lastInsertId = sqlite3_last_insert_rowid(static_cast<sqlite3*>(m_db));
    
    sqlite3_finalize(stmt);
#else
    // Simulated execution for non-SQLite builds
    result.rowsAffected = 1;
    result.lastInsertId = 1;
    (void)sql;
    (void)params;
#endif
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    result.success = true;
    
    return result;
}

DbResult DbConnection::execute(PreparedStatement& stmt) {
    return execute(stmt.getSql(), {});
}

DbResult DbConnection::execute(QueryBuilder& builder) {
    return execute(builder.build(), builder.getParameters());
}

bool DbConnection::beginTransaction() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_inTransaction) return false;
    
#ifdef USE_SQLITE
    int rc = sqlite3_exec(static_cast<sqlite3*>(m_db), "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(static_cast<sqlite3*>(m_db));
        return false;
    }
#endif
    
    m_inTransaction = true;
    return true;
}

bool DbConnection::commit() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_inTransaction) return false;
    
#ifdef USE_SQLITE
    int rc = sqlite3_exec(static_cast<sqlite3*>(m_db), "COMMIT", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(static_cast<sqlite3*>(m_db));
        return false;
    }
#endif
    
    m_inTransaction = false;
    return true;
}

bool DbConnection::rollback() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_inTransaction) return false;
    
#ifdef USE_SQLITE
    int rc = sqlite3_exec(static_cast<sqlite3*>(m_db), "ROLLBACK", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        m_lastError = sqlite3_errmsg(static_cast<sqlite3*>(m_db));
        return false;
    }
#endif
    
    m_inTransaction = false;
    return true;
}

std::unique_ptr<PreparedStatement> DbConnection::prepare(const std::string& sql) {
    auto stmt = std::make_unique<PreparedStatement>(sql);
    
#ifdef USE_SQLITE
    sqlite3_stmt* sqliteStmt = nullptr;
    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(m_db), sql.c_str(), -1, &sqliteStmt, nullptr);
    if (rc == SQLITE_OK) {
        stmt->setHandle(sqliteStmt);
    }
#endif
    
    return stmt;
}

long long DbConnection::getLastInsertId() const {
#ifdef USE_SQLITE
    return sqlite3_last_insert_rowid(static_cast<sqlite3*>(m_db));
#else
    return 0;
#endif
}

int DbConnection::getChangesCount() const {
#ifdef USE_SQLITE
    return sqlite3_changes(static_cast<sqlite3*>(m_db));
#else
    return 0;
#endif
}

// =============================================================================
// ConnectionPool Implementation
// =============================================================================

ConnectionPool::ConnectionPool(const std::string& connectionString, size_t minConnections, size_t maxConnections)
    : m_connectionString(connectionString)
    , m_minConnections(minConnections)
    , m_maxConnections(maxConnections)
{
    // Create minimum connections
    for (size_t i = 0; i < m_minConnections; ++i) {
        auto conn = std::make_shared<DbConnection>(m_connectionString);
        if (conn->open()) {
            m_idle.push(conn);
        }
    }
    
    std::cout << "[ConnectionPool] Initialized with " << m_idle.size() << " connections" << std::endl;
}

ConnectionPool::~ConnectionPool() {
    close();
}

std::shared_ptr<DbConnection> ConnectionPool::acquire(int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    if (m_closed) return nullptr;
    
    // Try to get an idle connection
    if (!m_idle.empty()) {
        auto conn = m_idle.front();
        m_idle.pop();
        m_active.push_back(conn);
        return conn;
    }
    
    // Create new connection if under max
    if (m_active.size() < m_maxConnections) {
        auto conn = std::make_shared<DbConnection>(m_connectionString);
        if (conn->open()) {
            m_active.push_back(conn);
            return conn;
        }
    }
    
    // Wait for a connection to become available
    if (m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
        return !m_idle.empty() || m_closed;
    })) {
        if (m_closed) return nullptr;
        
        auto conn = m_idle.front();
        m_idle.pop();
        m_active.push_back(conn);
        return conn;
    }
    
    return nullptr;  // Timeout
}

void ConnectionPool::release(std::shared_ptr<DbConnection> conn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Remove from active
    auto it = std::find(m_active.begin(), m_active.end(), conn);
    if (it != m_active.end()) {
        m_active.erase(it);
    }
    
    // Return to idle pool if still open
    if (conn->isOpen() && !m_closed) {
        m_idle.push(conn);
    }
    
    m_cv.notify_one();
}

size_t ConnectionPool::getActiveCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_active.size();
}

size_t ConnectionPool::getIdleCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_idle.size();
}

size_t ConnectionPool::getTotalCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_active.size() + m_idle.size();
}

void ConnectionPool::resize(size_t minConnections, size_t maxConnections) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minConnections = minConnections;
    m_maxConnections = maxConnections;
}

void ConnectionPool::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_closed = true;
    
    // Close all connections
    while (!m_idle.empty()) {
        m_idle.front()->close();
        m_idle.pop();
    }
    
    for (auto& conn : m_active) {
        conn->close();
    }
    m_active.clear();
    
    m_cv.notify_all();
    
    std::cout << "[ConnectionPool] Closed all connections" << std::endl;
}

// =============================================================================
// MigrationManager Implementation
// =============================================================================

MigrationManager::MigrationManager(std::shared_ptr<DbConnection> connection)
    : m_connection(connection)
{
    ensureMigrationsTable();
}

void MigrationManager::ensureMigrationsTable() {
    m_connection->execute(R"(
        CREATE TABLE IF NOT EXISTS _migrations (
            version INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            applied_at TEXT NOT NULL
        )
    )");
}

void MigrationManager::addMigration(int version, const std::string& name, 
                                     const std::string& upSql, const std::string& downSql) {
    Migration migration;
    migration.version = version;
    migration.name = name;
    migration.upSql = upSql;
    migration.downSql = downSql;
    m_migrations.push_back(migration);
}

bool MigrationManager::migrate() {
    return migrateUp(-1);
}

bool MigrationManager::migrateUp(int targetVersion) {
    int currentVersion = getCurrentVersion();
    
    // Sort migrations by version
    std::sort(m_migrations.begin(), m_migrations.end(), 
              [](const Migration& a, const Migration& b) { return a.version < b.version; });
    
    for (const auto& migration : m_migrations) {
        if (migration.version <= currentVersion) continue;
        if (targetVersion >= 0 && migration.version > targetVersion) break;
        
        std::cout << "[Migration] Applying: " << migration.name << " (v" << migration.version << ")" << std::endl;
        
        if (!m_connection->beginTransaction()) return false;
        
        auto result = m_connection->execute(migration.upSql);
        if (!result.success) {
            std::cout << "[Migration] Failed: " << result.error << std::endl;
            m_connection->rollback();
            return false;
        }
        
        // Record migration
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char timeStr[64];
        std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
        
        m_connection->execute(
            "INSERT INTO _migrations (version, name, applied_at) VALUES (?, ?, ?)",
            {migration.version, migration.name, std::string(timeStr)}
        );
        
        m_connection->commit();
    }
    
    return true;
}

bool MigrationManager::migrateDown(int targetVersion) {
    int currentVersion = getCurrentVersion();
    
    // Sort migrations by version descending
    std::sort(m_migrations.begin(), m_migrations.end(),
              [](const Migration& a, const Migration& b) { return a.version > b.version; });
    
    for (const auto& migration : m_migrations) {
        if (migration.version > currentVersion) continue;
        if (migration.version <= targetVersion) break;
        if (migration.downSql.empty()) {
            std::cout << "[Migration] No down migration for: " << migration.name << std::endl;
            return false;
        }
        
        std::cout << "[Migration] Reverting: " << migration.name << " (v" << migration.version << ")" << std::endl;
        
        if (!m_connection->beginTransaction()) return false;
        
        auto result = m_connection->execute(migration.downSql);
        if (!result.success) {
            std::cout << "[Migration] Failed: " << result.error << std::endl;
            m_connection->rollback();
            return false;
        }
        
        m_connection->execute("DELETE FROM _migrations WHERE version = ?", {migration.version});
        m_connection->commit();
    }
    
    return true;
}

bool MigrationManager::rollback(int steps) {
    int currentVersion = getCurrentVersion();
    
    auto applied = getAppliedMigrations();
    if (applied.size() < static_cast<size_t>(steps)) {
        steps = static_cast<int>(applied.size());
    }
    
    if (steps <= 0) return true;
    
    int targetVersion = applied[steps - 1].version - 1;
    return migrateDown(targetVersion);
}

int MigrationManager::getCurrentVersion() const {
    auto result = m_connection->execute("SELECT MAX(version) as version FROM _migrations");
    if (result.success && !result.rows.empty()) {
        return result.rows[0].getInt("version", 0);
    }
    return 0;
}

std::vector<Migration> MigrationManager::getPendingMigrations() const {
    int currentVersion = getCurrentVersion();
    std::vector<Migration> pending;
    
    for (const auto& migration : m_migrations) {
        if (migration.version > currentVersion) {
            pending.push_back(migration);
        }
    }
    
    return pending;
}

std::vector<Migration> MigrationManager::getAppliedMigrations() const {
    std::vector<Migration> applied;
    
    auto result = m_connection->execute(
        "SELECT version, name, applied_at FROM _migrations ORDER BY version DESC"
    );
    
    if (result.success) {
        for (const auto& row : result.rows) {
            Migration m;
            m.version = row.getInt("version");
            m.name = row.getString("name");
            applied.push_back(m);
        }
    }
    
    return applied;
}

// =============================================================================
// DatabaseLayer Implementation
// =============================================================================

DatabaseLayer& DatabaseLayer::instance() {
    static DatabaseLayer inst;
    return inst;
}

DatabaseLayer::DatabaseLayer() {}

DatabaseLayer::~DatabaseLayer() {
    shutdown();
}

bool DatabaseLayer::initialize(const std::string& connectionString, size_t poolSize) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    m_pool = std::make_unique<ConnectionPool>(connectionString, 2, poolSize);
    
    auto conn = m_pool->acquire();
    if (!conn) {
        std::cout << "[DatabaseLayer] Failed to initialize" << std::endl;
        return false;
    }
    
    m_migrations = std::make_unique<MigrationManager>(conn);
    m_pool->release(conn);
    
    std::cout << "[DatabaseLayer] Initialized with pool size: " << poolSize << std::endl;
    return true;
}

void DatabaseLayer::shutdown() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    m_migrations.reset();
    if (m_pool) {
        m_pool->close();
        m_pool.reset();
    }
    
    std::cout << "[DatabaseLayer] Shutdown complete" << std::endl;
}

std::shared_ptr<DbConnection> DatabaseLayer::getConnection(int timeoutMs) {
    if (!m_pool) return nullptr;
    return m_pool->acquire(timeoutMs);
}

void DatabaseLayer::releaseConnection(std::shared_ptr<DbConnection> conn) {
    if (m_pool && conn) {
        m_pool->release(conn);
    }
}

DbResult DatabaseLayer::query(const std::string& sql) {
    return query(sql, {});
}

DbResult DatabaseLayer::query(const std::string& sql, const std::vector<DbValue>& params) {
    auto conn = getConnection();
    if (!conn) {
        DbResult result;
        result.error = "Failed to acquire database connection";
        return result;
    }
    
    auto result = conn->execute(sql, params);
    releaseConnection(conn);
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalQueries++;
        m_stats.avgQueryTimeMs = (m_stats.avgQueryTimeMs * (m_stats.totalQueries - 1) + result.executionTimeMs) / m_stats.totalQueries;
    }
    
    return result;
}

DbResult DatabaseLayer::query(QueryBuilder& builder) {
    return query(builder.build(), builder.getParameters());
}

DbResult DatabaseLayer::insert(const std::string& table, const std::unordered_map<std::string, DbValue>& data) {
    QueryBuilder builder;
    builder.insert(table).values(data);
    
    auto result = query(builder);
    
    if (result.success) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalInserts++;
    }
    
    return result;
}

DbResult DatabaseLayer::update(const std::string& table, const std::unordered_map<std::string, DbValue>& data,
                                const std::string& whereClause, const std::vector<DbValue>& whereParams) {
    QueryBuilder builder;
    builder.update(table).set(data);
    if (!whereClause.empty()) {
        builder.where(whereClause);
    }
    
    auto params = builder.getParameters();
    params.insert(params.end(), whereParams.begin(), whereParams.end());
    
    auto result = query(builder.build(), params);
    
    if (result.success) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalUpdates++;
    }
    
    return result;
}

DbResult DatabaseLayer::remove(const std::string& table, const std::string& whereClause,
                                const std::vector<DbValue>& whereParams) {
    QueryBuilder builder;
    builder.deleteFrom(table);
    if (!whereClause.empty()) {
        builder.where(whereClause);
    }
    
    auto result = query(builder.build(), whereParams);
    
    if (result.success) {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalDeletes++;
    }
    
    return result;
}

std::optional<DbRow> DatabaseLayer::findById(const std::string& table, const std::string& idColumn, const DbValue& id) {
    QueryBuilder builder;
    builder.select().from(table).where(idColumn + " = ?").limit(1);
    
    auto result = query(builder.build(), {id});
    if (result.success && !result.rows.empty()) {
        return result.rows[0];
    }
    return std::nullopt;
}

std::vector<DbRow> DatabaseLayer::findAll(const std::string& table, int limit, int offset) {
    QueryBuilder builder;
    builder.select().from(table).limit(limit).offset(offset);
    
    auto result = query(builder);
    return result.rows;
}

std::vector<DbRow> DatabaseLayer::findWhere(const std::string& table, const std::string& whereClause,
                                             const std::vector<DbValue>& params, int limit) {
    QueryBuilder builder;
    builder.select().from(table).where(whereClause).limit(limit);
    
    auto result = query(builder.build(), params);
    return result.rows;
}

bool DatabaseLayer::createTable(const std::string& name, const std::string& schema) {
    std::string sql = "CREATE TABLE IF NOT EXISTS " + name + " (" + schema + ")";
    auto result = query(sql);
    return result.success;
}

bool DatabaseLayer::dropTable(const std::string& name) {
    std::string sql = "DROP TABLE IF EXISTS " + name;
    auto result = query(sql);
    return result.success;
}

bool DatabaseLayer::tableExists(const std::string& name) {
    auto result = query("SELECT name FROM sqlite_master WHERE type='table' AND name=?", {name});
    return result.success && !result.rows.empty();
}

std::vector<std::string> DatabaseLayer::listTables() {
    std::vector<std::string> tables;
    auto result = query("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");
    if (result.success) {
        for (const auto& row : result.rows) {
            tables.push_back(row.getString("name"));
        }
    }
    return tables;
}

MigrationManager& DatabaseLayer::migrations() {
    return *m_migrations;
}

DatabaseLayer::Stats DatabaseLayer::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    Stats stats = m_stats;
    if (m_pool) {
        stats.activeConnections = m_pool->getActiveCount();
        stats.idleConnections = m_pool->getIdleCount();
    }
    return stats;
}

// =============================================================================
// TransactionGuard Implementation
// =============================================================================

TransactionGuard::TransactionGuard(std::shared_ptr<DbConnection> conn) : m_conn(conn) {
    if (m_conn) {
        m_conn->beginTransaction();
    }
}

TransactionGuard::~TransactionGuard() {
    if (m_conn && !m_committed) {
        m_conn->rollback();
    }
}

void TransactionGuard::commit() {
    if (m_conn && !m_committed) {
        m_conn->commit();
        m_committed = true;
    }
}

void TransactionGuard::rollback() {
    if (m_conn && !m_committed) {
        m_conn->rollback();
        m_committed = true;  // Prevent double rollback in destructor
    }
}

} // namespace enterprise
