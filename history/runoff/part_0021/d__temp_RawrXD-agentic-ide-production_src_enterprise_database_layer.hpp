#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class DatabaseLayer {
private:
    static DatabaseLayer* s_instance;
    static std::mutex s_mutex;
    std::string m_databasePath;
    bool m_isInitialized;
    std::map<std::string, std::string> m_connectionStrings;

    DatabaseLayer() : m_isInitialized(false) {}

public:
    static DatabaseLayer& instance();

    void initialize(const std::string& dbPath);
    void shutdown();
    bool isInitialized() const;

    // Basic CRUD operations
    bool executeQuery(const std::string& query);
    json executeSelectQuery(const std::string& query);
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // Connection management
    void registerConnectionString(const std::string& name, const std::string& connectionStr);
    std::string getConnectionString(const std::string& name) const;
};
