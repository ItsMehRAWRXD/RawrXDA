// ============================================================================
// digestion_db.h — Digestion Database (SQLite3 C API, no Qt)
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>

// Forward declaration — SQLite3 C API
struct sqlite3;
struct sqlite3_stmt;

struct DigestionMetrics {
    int64_t startMs = 0;
    int64_t endMs = 0;
    int64_t elapsedMs = 0;
    int totalFiles = 0;
    int scannedFiles = 0;
    int stubsFound = 0;
    int fixesApplied = 0;
    int64_t bytesProcessed = 0;
};

struct DigestionRunRow {
    int id = 0;
    std::string rootDir;
    int64_t startMs = 0;
    int64_t endMs = 0;
    int64_t elapsedMs = 0;
    int totalFiles = 0;
    int scannedFiles = 0;
    int stubsFound = 0;
    int fixesApplied = 0;
    int64_t bytesProcessed = 0;
    std::string createdAt;
};

struct DigestionFileObj {
    std::string file;
    std::string language;
    int64_t sizeBytes = 0;
    int stubsFound = 0;
    std::string hash;
    // Nested tasks stored separately
};

struct DigestionTaskObj {
    int line = 0;
    std::string type;
    std::string context;
    std::string suggestedFix;
    std::string confidence;
    bool applied = false;
    std::string backupId;
};

class DigestionDatabase {
public:
    explicit DigestionDatabase();
    ~DigestionDatabase();

    bool open(const std::string &path, std::string *error = nullptr);
    void close();
    bool isOpen() const;

    bool ensureSchema(const std::string &schemaPath = std::string(), std::string *error = nullptr);

    bool insertRun(const std::string &rootDir, const DigestionMetrics &metrics,
                   const std::string &reportJson, int *runId, std::string *error = nullptr);
    bool insertFileResult(int runId, const DigestionFileObj &fileObj, std::string *error = nullptr);
    bool insertTask(int fileId, const DigestionTaskObj &taskObj, std::string *error = nullptr);

    std::vector<DigestionRunRow> fetchRecentRuns(int limit = 10, std::string *error = nullptr) const;
    bool deleteRun(int runId, std::string *error = nullptr);

    static std::string defaultSchema();

private:
    bool executeBatch(const std::vector<std::string> &statements, std::string *error);

    sqlite3* m_db = nullptr;
    mutable std::mutex m_mutex;
};

