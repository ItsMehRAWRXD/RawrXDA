#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <mutex>

using json = nlohmann::json;

struct DigestionMetrics {
    long long startMs = 0;
    long long endMs = 0;
    long long elapsedMs = 0;
    int totalFiles = 0;
    int scannedFiles = 0;
    int stubsFound = 0;
    int fixesApplied = 0;
    long long bytesProcessed = 0;
};

class DigestionDatabase {
public:
    explicit DigestionDatabase();

    bool open(const std::string &path, std::string *error = nullptr);
    bool ensureSchema(const std::string &schemaPath, std::string *error = nullptr);
    bool insertRun(const std::string &rootDir, const DigestionMetrics &metrics, const json& report, int *runId, std::string *error = nullptr);
    bool insertFileResult(int runId, const json& fileObj, std::string *error = nullptr);
    
    std::vector<json> fetchRecentRuns(int limit = 10, std::string *error = nullptr) const;
    bool deleteRun(int runId, std::string *error = nullptr);

    static std::string defaultSchema();

private:
    std::string m_dbPath;
    mutable std::mutex m_mutex;
    
    // Simple JSON-based storage helper
    json loadDb() const;
    bool saveDb(const json& db) const;
};
