// ============================================================================
// agent_memory.cpp — Persistent Agent Memory System for RawrXD IDE
// Provides cross-session context and memory for agentic workflows
// ============================================================================

#include "agent_memory.h"
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <chrono>
#include <filesystem>

namespace RawrXD {
namespace Agent {

class AgentMemory::Impl {
public:
    sqlite3* db;
    std::string db_path;

    Impl() : db(nullptr) {
        // Use SQLite for persistent storage
        db_path = "agent_memory.db";
        initialize_database();
    }

    ~Impl() {
        if (db) {
            sqlite3_close(db);
        }
    }

    void initialize_database() {
        int rc = sqlite3_open(db_path.c_str(), &db);
        if (rc) {
            std::cerr << "[AgentMemory] Can't open database: " << sqlite3_errmsg(db) << std::endl;
            return;
        }

        // Create tables
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS memory_entries (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                session_id TEXT NOT NULL,
                key TEXT NOT NULL,
                value TEXT NOT NULL,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                tags TEXT,
                importance REAL DEFAULT 1.0,
                UNIQUE(session_id, key)
            );

            CREATE TABLE IF NOT EXISTS agent_sessions (
                id TEXT PRIMARY KEY,
                start_time DATETIME DEFAULT CURRENT_TIMESTAMP,
                end_time DATETIME,
                agent_type TEXT,
                context TEXT
            );

            CREATE INDEX IF NOT EXISTS idx_memory_session_key ON memory_entries(session_id, key);
            CREATE INDEX IF NOT EXISTS idx_memory_timestamp ON memory_entries(timestamp);
        )";

        char* err_msg = nullptr;
        rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::cerr << "[AgentMemory] SQL error: " << err_msg << std::endl;
            sqlite3_free(err_msg);
        }
    }

    void store(const std::string& session_id, const std::string& key,
               const nlohmann::json& value, const std::vector<std::string>& tags,
               double importance) {
        nlohmann::json tags_json = tags;
        std::string value_str = value.dump();
        std::string tags_str = tags_json.dump();

        const char* sql = R"(
            INSERT OR REPLACE INTO memory_entries
            (session_id, key, value, tags, importance, timestamp)
            VALUES (?, ?, ?, ?, ?, CURRENT_TIMESTAMP)
        )";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, value_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, tags_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 5, importance);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    nlohmann::json retrieve(const std::string& session_id, const std::string& key) {
        const char* sql = "SELECT value FROM memory_entries WHERE session_id = ? AND key = ?";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);

        nlohmann::json result = nullptr;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* value_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (value_str) {
                result = nlohmann::json::parse(value_str);
            }
        }

        sqlite3_finalize(stmt);
        return result;
    }

    std::vector<MemoryEntry> search(const std::string& query, size_t limit) {
        std::string sql = R"(
            SELECT session_id, key, value, tags, importance, timestamp
            FROM memory_entries
            WHERE key LIKE ? OR value LIKE ?
            ORDER BY importance DESC, timestamp DESC
            LIMIT ?
        )";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

        std::string search_pattern = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, search_pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, search_pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, static_cast<int>(limit));

        std::vector<MemoryEntry> results;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            MemoryEntry entry;
            entry.session_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            entry.key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

            const char* value_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            if (value_str) {
                entry.value = nlohmann::json::parse(value_str);
            }

            const char* tags_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            if (tags_str) {
                auto tags_json = nlohmann::json::parse(tags_str);
                entry.tags = tags_json.get<std::vector<std::string>>();
            }

            entry.importance = sqlite3_column_double(stmt, 4);
            entry.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));

            results.push_back(entry);
        }

        sqlite3_finalize(stmt);
        return results;
    }

    void start_session(const std::string& session_id, const std::string& agent_type,
                      const nlohmann::json& context) {
        const char* sql = R"(
            INSERT OR REPLACE INTO agent_sessions
            (id, agent_type, context, start_time)
            VALUES (?, ?, ?, CURRENT_TIMESTAMP)
        )";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, agent_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, context.dump().c_str(), -1, SQLITE_TRANSIENT);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void end_session(const std::string& session_id) {
        const char* sql = "UPDATE agent_sessions SET end_time = CURRENT_TIMESTAMP WHERE id = ?";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
};

AgentMemory::AgentMemory() : pimpl(std::make_unique<Impl>()) {}

AgentMemory::~AgentMemory() = default;

AgentMemory& AgentMemory::instance() {
    static AgentMemory instance;
    return instance;
}

void AgentMemory::store(const std::string& session_id, const std::string& key,
                        const nlohmann::json& value, const std::vector<std::string>& tags,
                        double importance) {
    pimpl->store(session_id, key, value, tags, importance);
}

nlohmann::json AgentMemory::retrieve(const std::string& session_id, const std::string& key) {
    return pimpl->retrieve(session_id, key);
}

std::vector<AgentMemory::MemoryEntry> AgentMemory::search(const std::string& query, size_t limit) {
    return pimpl->search(query, limit);
}

void AgentMemory::start_session(const std::string& session_id, const std::string& agent_type,
                               const nlohmann::json& context) {
    pimpl->start_session(session_id, agent_type, context);
}

void AgentMemory::end_session(const std::string& session_id) {
    pimpl->end_session(session_id);
}

void AgentMemory::clear_session(const std::string& session_id) {
    // Delete all memory entries for this session
    const char* sql = "DELETE FROM memory_entries WHERE session_id = ?";

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(pimpl->db, sql, -1, &stmt, nullptr);

    sqlite3_bind_text(stmt, 1, session_id.c_str(), -1, SQLITE_TRANSIENT);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

}} // namespace RawrXD::Agent