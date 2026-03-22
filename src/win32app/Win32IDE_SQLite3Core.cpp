// ============================================================================
// Win32IDE_SQLite3Core.cpp — SQLite3 database integration for RawrXD IDE
// Provides persistent storage for settings, telemetry, agent state, and user data
// ============================================================================

#include "Win32IDE.h"
#include <memory>
#include <mutex>
#include <sqlite3.h>
#include <string>
#include <vector>
#include <windows.h>


// ============================================================================
// SQLITE3 DATABASE MANAGER
// ============================================================================

class SQLite3DatabaseManager
{
  private:
    Win32IDE* m_ide;
    sqlite3* m_db;
    std::mutex m_dbMutex;
    std::string m_dbPath;
    bool m_initialized;

    // Prepared statements cache
    struct PreparedStatement
    {
        sqlite3_stmt* stmt;
        std::string sql;
    };
    std::vector<PreparedStatement> m_preparedStatements;

  public:
    SQLite3DatabaseManager(Win32IDE* ide) : m_ide(ide), m_db(nullptr), m_initialized(false) {}

    ~SQLite3DatabaseManager() { closeDatabase(); }

    // Initialize SQLite3 database
    bool initialize(const std::string& dbPath = "")
    {
        std::lock_guard<std::mutex> lock(m_dbMutex);

        if (m_initialized)
            return true;

        // Set default database path if not provided
        if (dbPath.empty())
        {
            m_dbPath = getDefaultDatabasePath();
        }
        else
        {
            m_dbPath = dbPath;
        }

        // Open database
        int result = sqlite3_open(m_dbPath.c_str(), &m_db);
        if (result != SQLITE_OK)
        {
            LOG_ERROR(std::string("Failed to open SQLite database: ") + sqlite3_errmsg(m_db));
            return false;
        }

        // Enable WAL mode for better concurrency
        executeQuery("PRAGMA journal_mode=WAL;");
        executeQuery("PRAGMA synchronous=NORMAL;");
        executeQuery("PRAGMA cache_size=10000;");  // 10MB cache
        executeQuery("PRAGMA temp_store=MEMORY;");

        // Create tables
        if (!createTables())
        {
            LOG_ERROR("Failed to create database tables");
            return false;
        }

        m_initialized = true;
        LOG_INFO(std::string("SQLite3 database initialized at: ") + m_dbPath);
        return true;
    }

    // Close database connection
    void closeDatabase()
    {
        std::lock_guard<std::mutex> lock(m_dbMutex);

        // Finalize all prepared statements
        for (auto& ps : m_preparedStatements)
        {
            if (ps.stmt)
            {
                sqlite3_finalize(ps.stmt);
                ps.stmt = nullptr;
            }
        }
        m_preparedStatements.clear();

        if (m_db)
        {
            sqlite3_close(m_db);
            m_db = nullptr;
        }

        m_initialized = false;
    }

    // Execute SQL query
    bool executeQuery(const std::string& sql, std::vector<std::vector<std::string>>* results = nullptr)
    {
        std::lock_guard<std::mutex> lock(m_dbMutex);

        if (!m_db)
            return false;

        sqlite3_stmt* stmt;
        int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

        if (result != SQLITE_OK)
        {
            LOG_ERROR(std::string("Failed to prepare SQL statement: ") + sqlite3_errmsg(m_db));
            return false;
        }

        // Execute statement
        while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            if (results)
            {
                std::vector<std::string> row;
                int columnCount = sqlite3_column_count(stmt);
                for (int i = 0; i < columnCount; ++i)
                {
                    const char* text = (const char*)sqlite3_column_text(stmt, i);
                    row.push_back(text ? text : "");
                }
                results->push_back(row);
            }
        }

        if (result != SQLITE_DONE && result != SQLITE_OK)
        {
            LOG_ERROR(std::string("Failed to execute SQL statement: ") + sqlite3_errmsg(m_db));
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

    // Execute parameterized query
    bool executeParameterizedQuery(const std::string& sql, const std::vector<std::string>& parameters,
                                   std::vector<std::vector<std::string>>* results = nullptr)
    {
        std::lock_guard<std::mutex> lock(m_dbMutex);

        if (!m_db)
            return false;

        sqlite3_stmt* stmt;
        int result = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);

        if (result != SQLITE_OK)
        {
            LOG_ERROR(std::string("Failed to prepare parameterized SQL: ") + sqlite3_errmsg(m_db));
            return false;
        }

        // Bind parameters
        for (size_t i = 0; i < parameters.size(); ++i)
        {
            sqlite3_bind_text(stmt, i + 1, parameters[i].c_str(), -1, SQLITE_TRANSIENT);
        }

        // Execute
        while ((result = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            if (results)
            {
                std::vector<std::string> row;
                int columnCount = sqlite3_column_count(stmt);
                for (int i = 0; i < columnCount; ++i)
                {
                    const char* text = (const char*)sqlite3_column_text(stmt, i);
                    row.push_back(text ? text : "");
                }
                results->push_back(row);
            }
        }

        if (result != SQLITE_DONE && result != SQLITE_OK)
        {
            LOG_ERROR(std::string("Failed to execute parameterized query: ") + sqlite3_errmsg(m_db));
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_finalize(stmt);
        return true;
    }

    // Settings management
    bool saveSetting(const std::string& key, const std::string& value)
    {
        std::string sql = "INSERT OR REPLACE INTO settings (key, value, updated_at) VALUES (?, ?, datetime('now'));";
        std::vector<std::string> params = {key, value};
        return executeParameterizedQuery(sql, params);
    }

    std::string loadSetting(const std::string& key, const std::string& defaultValue = "")
    {
        std::string sql = "SELECT value FROM settings WHERE key = ?;";
        std::vector<std::string> params = {key};
        std::vector<std::vector<std::string>> results;

        if (executeParameterizedQuery(sql, params, &results) && !results.empty() && !results[0].empty())
        {
            return results[0][0];
        }

        return defaultValue;
    }

    // Telemetry storage
    bool storeTelemetryEvent(const std::string& eventType, const std::string& eventData)
    {
        std::string sql =
            "INSERT INTO telemetry_events (event_type, event_data, timestamp) VALUES (?, ?, datetime('now'));";
        std::vector<std::string> params = {eventType, eventData};
        return executeParameterizedQuery(sql, params);
    }

    bool recordAgenticApprovalEntry(const std::string& eventKind, const std::string& jsonPayload)
    {
        std::string sql = "INSERT INTO agentic_approval_audit (event_kind, payload_json) VALUES (?, ?);";
        std::vector<std::string> params = {eventKind, jsonPayload};
        return executeParameterizedQuery(sql, params);
    }

    // Agent state persistence
    bool saveAgentState(const std::string& agentId, const std::string& stateData)
    {
        std::string sql =
            "INSERT OR REPLACE INTO agent_states (agent_id, state_data, updated_at) VALUES (?, ?, datetime('now'));";
        std::vector<std::string> params = {agentId, stateData};
        return executeParameterizedQuery(sql, params);
    }

    std::string loadAgentState(const std::string& agentId)
    {
        std::string sql = "SELECT state_data FROM agent_states WHERE agent_id = ?;";
        std::vector<std::string> params = {agentId};
        std::vector<std::vector<std::string>> results;

        if (executeParameterizedQuery(sql, params, &results) && !results.empty() && !results[0].empty())
        {
            return results[0][0];
        }

        return "{}";  // Return empty JSON object if no state found
    }

    // Query execution with results
    std::vector<std::vector<std::string>> query(const std::string& sql)
    {
        std::vector<std::vector<std::string>> results;
        executeQuery(sql, &results);
        return results;
    }

  private:
    std::string getDefaultDatabasePath()
    {
        // Use APPDATA for user-specific data
        char* appData = getenv("APPDATA");
        if (appData)
        {
            return std::string(appData) + "\\RawrXD\\rawrxd.db";
        }

        // Fallback to current directory
        return "rawrxd.db";
    }

    bool createTables()
    {
        // Settings table
        if (!executeQuery("CREATE TABLE IF NOT EXISTS settings ("
                          "key TEXT PRIMARY KEY,"
                          "value TEXT NOT NULL,"
                          "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
                          ");"))
        {
            return false;
        }

        // Telemetry events table
        if (!executeQuery("CREATE TABLE IF NOT EXISTS telemetry_events ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "event_type TEXT NOT NULL,"
                          "event_data TEXT,"
                          "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
                          ");"))
        {
            return false;
        }

        // Agent states table
        if (!executeQuery("CREATE TABLE IF NOT EXISTS agent_states ("
                          "agent_id TEXT PRIMARY KEY,"
                          "state_data TEXT NOT NULL,"
                          "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
                          ");"))
        {
            return false;
        }

        if (!executeQuery("CREATE TABLE IF NOT EXISTS agentic_approval_audit ("
                          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                          "event_kind TEXT NOT NULL,"
                          "payload_json TEXT NOT NULL,"
                          "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
                          ");"))
        {
            return false;
        }

        // Create indexes for performance
        executeQuery("CREATE INDEX IF NOT EXISTS idx_telemetry_type ON telemetry_events(event_type);");
        executeQuery("CREATE INDEX IF NOT EXISTS idx_telemetry_timestamp ON telemetry_events(timestamp);");
        executeQuery("CREATE INDEX IF NOT EXISTS idx_agentic_audit_created ON agentic_approval_audit(created_at);");

        return true;
    }
};

// ============================================================================
// WIN32IDE INTEGRATION
// ============================================================================

void Win32IDE::initSQLite3Core()
{
    if (!m_sqliteManager)
    {
        m_sqliteManager = std::make_unique<SQLite3DatabaseManager>(this);
        if (!m_sqliteManager->initialize())
        {
            LOG_ERROR("Failed to initialize SQLite3 database");
            m_sqliteManager.reset();
            return;
        }
    }
    LOG_INFO("SQLite3 core initialized");
}

void Win32IDE::shutdownSQLite3Core()
{
    m_sqliteManager.reset();
    LOG_INFO("SQLite3 core shut down");
}

bool Win32IDE::saveSetting(const std::string& key, const std::string& value)
{
    if (!m_sqliteManager)
        initSQLite3Core();
    return m_sqliteManager ? m_sqliteManager->saveSetting(key, value) : false;
}

std::string Win32IDE::loadSetting(const std::string& key, const std::string& defaultValue)
{
    if (!m_sqliteManager)
        initSQLite3Core();
    return m_sqliteManager ? m_sqliteManager->loadSetting(key, defaultValue) : defaultValue;
}

bool Win32IDE::storeTelemetryEvent(const std::string& eventType, const std::string& eventData)
{
    if (!m_sqliteManager)
        initSQLite3Core();
    return m_sqliteManager ? m_sqliteManager->storeTelemetryEvent(eventType, eventData) : false;
}

bool Win32IDE::saveAgentState(const std::string& agentId, const std::string& stateData)
{
    if (!m_sqliteManager)
        initSQLite3Core();
    return m_sqliteManager ? m_sqliteManager->saveAgentState(agentId, stateData) : false;
}

std::string Win32IDE::loadAgentState(const std::string& agentId)
{
    if (!m_sqliteManager)
        initSQLite3Core();
    return m_sqliteManager ? m_sqliteManager->loadAgentState(agentId) : "{}";
}

std::vector<std::vector<std::string>> Win32IDE::executeDatabaseQuery(const std::string& sql)
{
    if (!m_sqliteManager)
        initSQLite3Core();
    return m_sqliteManager ? m_sqliteManager->query(sql) : std::vector<std::vector<std::string>>();
}

bool Win32IDE::recordAgenticApprovalAudit(const std::string& eventKind, const std::string& jsonPayload)
{
    if (!m_sqliteManager)
        initSQLite3Core();
    return m_sqliteManager ? m_sqliteManager->recordAgenticApprovalEntry(eventKind, jsonPayload) : false;
}
