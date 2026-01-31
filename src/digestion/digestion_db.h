#pragma once


class DigestionDatabase  {public:
    explicit DigestionDatabase();

    bool open(const std::string &path, std::string *error = nullptr);
    bool ensureSchema(const std::string &schemaPath, std::string *error = nullptr);
    bool insertRun(const std::string &rootDir, const DigestionMetrics &metrics, void* report, int *runId, std::string *error = nullptr);
    bool insertFileResult(int runId, const void* &fileObj, std::string *error = nullptr);
    bool insertTask(int fileId, const void* &taskObj, std::string *error = nullptr);

    std::vector<void*> fetchRecentRuns(int limit = 10, std::string *error = nullptr) const;
    bool deleteRun(int runId, std::string *error = nullptr);

    static std::string defaultSchema();

private:
    bool executeBatch(const std::stringList &statements, std::string *error);

    QSqlDatabase m_db;
};

