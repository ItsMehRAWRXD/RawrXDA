/**
 * @file database_layer.hpp
 * @brief Enterprise Database Persistence Layer
 * 
 * Features:
 * - SQLite embedded database
 * - Connection pooling
 * - Transaction management
 * - Prepared statements
 * - Query builder
 * - Migration system
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <optional>
#include <variant>
#include <chrono>
#include <queue>
#include <condition_variable>

namespace enterprise {

// =============================================================================
// Database Value Types
// =============================================================================

using DbValue = std::variant<
    std::nullptr_t,
    int,
    long long,
    double,
    std::string,
    std::vector<unsigned char>  // BLOB
>;

struct DbRow {
    std::vector<std::string> columns;
    std::vector<DbValue> values;
    
    std::optional<DbValue> get(const std::string& column) const;
    int getInt(const std::string& column, int defaultValue = 0) const;
    long long getInt64(const std::string& column, long long defaultValue = 0) const;
    double getDouble(const std::string& column, double defaultValue = 0.0) const;
    std::string getString(const std::string& column, const std::string& defaultValue = "") const;
    std::vector<unsigned char> getBlob(const std::string& column) const;
    bool isNull(const std::string& column) const;
};

struct DbResult {
    bool success = false;
    std::string error;
    std::vector<DbRow> rows;
    int rowsAffected = 0;
    long long lastInsertId = 0;
    double executionTimeMs = 0;
};

// =============================================================================
// Query Builder
// =============================================================================

class QueryBuilder {
public:
    QueryBuilder& select(const std::vector<std::string>& columns = {"*"});
    QueryBuilder& from(const std::string& table);
    QueryBuilder& join(const std::string& table, const std::string& condition, const std::string& type = "INNER");
    QueryBuilder& leftJoin(const std::string& table, const std::string& condition);
    QueryBuilder& rightJoin(const std::string& table, const std::string& condition);
    QueryBuilder& where(const std::string& condition);
    QueryBuilder& whereAnd(const std::string& condition);
    QueryBuilder& whereOr(const std::string& condition);
    QueryBuilder& whereIn(const std::string& column, const std::vector<DbValue>& values);
    QueryBuilder& whereBetween(const std::string& column, const DbValue& min, const DbValue& max);
    QueryBuilder& whereNull(const std::string& column);
    QueryBuilder& whereNotNull(const std::string& column);
    QueryBuilder& orderBy(const std::string& column, bool ascending = true);
    QueryBuilder& groupBy(const std::string& column);
    QueryBuilder& having(const std::string& condition);
    QueryBuilder& limit(int count);
    QueryBuilder& offset(int count);
    
    QueryBuilder& insert(const std::string& table);
    QueryBuilder& values(const std::unordered_map<std::string, DbValue>& data);
    
    QueryBuilder& update(const std::string& table);
    QueryBuilder& set(const std::unordered_map<std::string, DbValue>& data);
    
    QueryBuilder& deleteFrom(const std::string& table);
    
    std::string build() const;
    std::vector<DbValue> getParameters() const;
    void reset();
    
private:
    enum class QueryType { SELECT_T, INSERT_T, UPDATE_T, DELETE_T };
    QueryType m_type = QueryType::SELECT_T;
    std::string m_table;
    std::vector<std::string> m_columns;
    std::vector<std::string> m_joins;
    std::vector<std::string> m_conditions;
    std::vector<std::string> m_orderBy;
    std::vector<std::string> m_groupBy;
    std::string m_having;
    int m_limit = -1;
    int m_offset = -1;
    std::unordered_map<std::string, DbValue> m_data;
    std::vector<DbValue> m_parameters;
};

// =============================================================================
// Prepared Statement
// =============================================================================

class PreparedStatement {
public:
    PreparedStatement(const std::string& sql);
    ~PreparedStatement();
    
    void bind(int index, std::nullptr_t);
    void bind(int index, int value);
    void bind(int index, long long value);
    void bind(int index, double value);
    void bind(int index, const std::string& value);
    void bind(int index, const std::vector<unsigned char>& value);
    void bind(int index, const DbValue& value);
    
    void bindAll(const std::vector<DbValue>& values);
    void reset();
    
    std::string getSql() const { return m_sql; }
    void* getHandle() const { return m_stmt; }
    void setHandle(void* stmt) { m_stmt = stmt; }
    
private:
    std::string m_sql;
    void* m_stmt = nullptr;
};

// =============================================================================
// Database Connection
// =============================================================================

class DbConnection {
public:
    DbConnection(const std::string& connectionString);
    ~DbConnection();
    
    bool open();
    void close();
    bool isOpen() const;
    
    DbResult execute(const std::string& sql);
    DbResult execute(const std::string& sql, const std::vector<DbValue>& params);
    DbResult execute(PreparedStatement& stmt);
    DbResult execute(QueryBuilder& builder);
    
    bool beginTransaction();
    bool commit();
    bool rollback();
    bool inTransaction() const { return m_inTransaction; }
    
    std::unique_ptr<PreparedStatement> prepare(const std::string& sql);
    
    long long getLastInsertId() const;
    int getChangesCount() const;
    
    std::string getError() const { return m_lastError; }
    void* getHandle() const { return m_db; }
    
private:
    std::string m_connectionString;
    void* m_db = nullptr;
    bool m_inTransaction = false;
    std::string m_lastError;
    mutable std::mutex m_mutex;
};

// =============================================================================
// Connection Pool
// =============================================================================

class ConnectionPool {
public:
    ConnectionPool(const std::string& connectionString, size_t minConnections = 2, size_t maxConnections = 10);
    ~ConnectionPool();
    
    std::shared_ptr<DbConnection> acquire(int timeoutMs = 5000);
    void release(std::shared_ptr<DbConnection> conn);
    
    size_t getActiveCount() const;
    size_t getIdleCount() const;
    size_t getTotalCount() const;
    
    void resize(size_t minConnections, size_t maxConnections);
    void close();
    
private:
    std::string m_connectionString;
    size_t m_minConnections;
    size_t m_maxConnections;
    
    std::queue<std::shared_ptr<DbConnection>> m_idle;
    std::vector<std::shared_ptr<DbConnection>> m_active;
    
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_closed = false;
};

// =============================================================================
// Migration System
// =============================================================================

struct Migration {
    int version;
    std::string name;
    std::string upSql;
    std::string downSql;
    std::chrono::system_clock::time_point appliedAt;
};

class MigrationManager {
public:
    MigrationManager(std::shared_ptr<DbConnection> connection);
    
    void addMigration(int version, const std::string& name, const std::string& upSql, const std::string& downSql = "");
    
    bool migrate();                          // Apply all pending migrations
    bool migrateUp(int version = -1);        // Migrate up to specific version (-1 = latest)
    bool migrateDown(int version = 0);       // Migrate down to specific version (0 = initial)
    bool rollback(int steps = 1);            // Rollback N migrations
    
    int getCurrentVersion() const;
    std::vector<Migration> getPendingMigrations() const;
    std::vector<Migration> getAppliedMigrations() const;
    
private:
    void ensureMigrationsTable();
    
    std::shared_ptr<DbConnection> m_connection;
    std::vector<Migration> m_migrations;
};

// =============================================================================
// Database Layer Interface
// =============================================================================

class DatabaseLayer {
public:
    static DatabaseLayer& instance();
    
    // Initialization
    bool initialize(const std::string& connectionString, size_t poolSize = 5);
    void shutdown();
    
    // Connection management
    std::shared_ptr<DbConnection> getConnection(int timeoutMs = 5000);
    void releaseConnection(std::shared_ptr<DbConnection> conn);
    
    // Convenience methods
    DbResult query(const std::string& sql);
    DbResult query(const std::string& sql, const std::vector<DbValue>& params);
    DbResult query(QueryBuilder& builder);
    
    DbResult insert(const std::string& table, const std::unordered_map<std::string, DbValue>& data);
    DbResult update(const std::string& table, const std::unordered_map<std::string, DbValue>& data, 
                    const std::string& whereClause, const std::vector<DbValue>& whereParams = {});
    DbResult remove(const std::string& table, const std::string& whereClause, 
                    const std::vector<DbValue>& whereParams = {});
    
    std::optional<DbRow> findById(const std::string& table, const std::string& idColumn, const DbValue& id);
    std::vector<DbRow> findAll(const std::string& table, int limit = 1000, int offset = 0);
    std::vector<DbRow> findWhere(const std::string& table, const std::string& whereClause,
                                  const std::vector<DbValue>& params = {}, int limit = 1000);
    
    // Transaction helper
    template<typename Func>
    bool transaction(Func&& func) {
        auto conn = getConnection();
        if (!conn) return false;
        
        if (!conn->beginTransaction()) {
            releaseConnection(conn);
            return false;
        }
        
        try {
            if (func(conn)) {
                conn->commit();
                releaseConnection(conn);
                return true;
            } else {
                conn->rollback();
                releaseConnection(conn);
                return false;
            }
        } catch (...) {
            conn->rollback();
            releaseConnection(conn);
            throw;
        }
    }
    
    // Schema management
    bool createTable(const std::string& name, const std::string& schema);
    bool dropTable(const std::string& name);
    bool tableExists(const std::string& name);
    std::vector<std::string> listTables();
    
    // Migration
    MigrationManager& migrations();
    
    // Statistics
    struct Stats {
        size_t totalQueries = 0;
        size_t totalInserts = 0;
        size_t totalUpdates = 0;
        size_t totalDeletes = 0;
        double avgQueryTimeMs = 0;
        size_t activeConnections = 0;
        size_t idleConnections = 0;
    };
    Stats getStats() const;
    
private:
    DatabaseLayer();
    ~DatabaseLayer();
    DatabaseLayer(const DatabaseLayer&) = delete;
    DatabaseLayer& operator=(const DatabaseLayer&) = delete;
    
    std::unique_ptr<ConnectionPool> m_pool;
    std::unique_ptr<MigrationManager> m_migrations;
    
    mutable std::mutex m_statsMutex;
    Stats m_stats;
};

// =============================================================================
// RAII Transaction Guard
// =============================================================================

class TransactionGuard {
public:
    TransactionGuard(std::shared_ptr<DbConnection> conn);
    ~TransactionGuard();
    
    void commit();
    void rollback();
    
private:
    std::shared_ptr<DbConnection> m_conn;
    bool m_committed = false;
};

} // namespace enterprise
