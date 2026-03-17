#include "digestion_db.h"
#include <fstream>
#include <iostream>
#include <ctime>

DigestionDatabase::DigestionDatabase() {}

bool DigestionDatabase::open(const std::string &path, std::string *error) {
    m_dbPath = path;
    // ensure file exists
    std::ifstream f(m_dbPath);
    if (!f.good()) {
        json root = {{"runs", json::array()}};
        return saveDb(root);
    }
    return true;
}

bool DigestionDatabase::ensureSchema(const std::string &schemaPath, std::string *error) {
    // For JSON store, schema is implicit structure.
    // Use this to validate or migrate if needed.
    return true; 
}

bool DigestionDatabase::insertRun(const std::string &rootDir, const DigestionMetrics &metrics, const json& report, int *runId, std::string *error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        json db = loadDb();
        if (!db.contains("runs")) db["runs"] = json::array();

        int nextId = 1;
        if (!db["runs"].empty()) {
            nextId = db["runs"].back()["id"].get<int>() + 1;
        }

        json run = {
            {"id", nextId},
            {"rootDir", rootDir},
            {"timestamp", std::time(nullptr)},
            {"metrics", {
                {"startMs", metrics.startMs},
                {"endMs", metrics.endMs},
                {"elapsedMs", metrics.elapsedMs},
                {"totalFiles", metrics.totalFiles},
                {"scannedFiles", metrics.scannedFiles},
                {"stubsFound", metrics.stubsFound},
                {"fixesApplied", metrics.fixesApplied},
                {"bytesProcessed", metrics.bytesProcessed}
            }},
            {"report", report},
            {"file_results", json::array()}
        };

        db["runs"].push_back(run);
        if (runId) *runId = nextId;

        return saveDb(db);
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}

bool DigestionDatabase::insertFileResult(int runId, const json& fileObj, std::string *error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        json db = loadDb();
        for (auto& run : db["runs"]) {
            if (run["id"] == runId) {
                if (!run.contains("file_results")) run["file_results"] = json::array();
                run["file_results"].push_back(fileObj);
                return saveDb(db);
            }
        }
        if (error) *error = "Run ID not found";
        return false;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}

std::vector<json> DigestionDatabase::fetchRecentRuns(int limit, std::string *error) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        json db = loadDb();
        std::vector<json> results;
        if (!db.contains("runs")) return results;
        
        const auto& runs = db["runs"];
        // Iterate backwards for recent
        int count = 0;
        for (auto it = runs.rbegin(); it != runs.rend(); ++it) {
            results.push_back(*it);
            if (++count >= limit) break;
        }
        return results;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return {};
    }
}

bool DigestionDatabase::deleteRun(int runId, std::string *error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        json db = loadDb();
        if (!db.contains("runs")) return false;
        
        auto& runs = db["runs"];
        runs.erase(std::remove_if(runs.begin(), runs.end(), [runId](const json& j) {
            return j["id"] == runId;
        }), runs.end());
        
        return saveDb(db);
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}

std::string DigestionDatabase::defaultSchema() {
    return "{}"; // JSON, no SQL schema
}

json DigestionDatabase::loadDb() const {
    std::ifstream f(m_dbPath);
    if (!f.is_open()) return {{"runs", json::array()}};
    json j;
    f >> j;
    return j;
}

bool DigestionDatabase::saveDb(const json& db) const {
    std::ofstream f(m_dbPath);
    if (!f.is_open()) return false;
    f << db.dump(4);
    return true;
}
        "FROM digestion_runs ORDER BY id DESC LIMIT ?"));
    query.addBindValue(limit);

    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return runs;
    }

    while (query) {
        void* row;
        row.insert("id", query.value(0));
        row.insert("root_dir", query.value(1).toString());
        row.insert("start_ms", query.value(2).toLongLong());
        row.insert("end_ms", query.value(3).toLongLong());
        row.insert("elapsed_ms", query.value(4).toLongLong());
        row.insert("total_files", query.value(5));
        row.insert("scanned_files", query.value(6));
        row.insert("stubs_found", query.value(7));
        row.insert("fixes_applied", query.value(8));
        row.insert("bytes_processed", query.value(9).toLongLong());
        row.insert("created_at", query.value(10).toString());
        runs.append(row);
    }
    return runs;
}

bool DigestionDatabase::deleteRun(int runId, std::string *error) {
    if (!m_db.isValid()) {
        if (error) *error = std::stringLiteral("Database connection is not open");
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(std::stringLiteral("DELETE FROM digestion_runs WHERE id = ?"));
    query.addBindValue(runId);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool DigestionDatabase::insertFileResult(int runId, const void* &fileObj, std::string *error) {
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO digestion_files (run_id, path, language, size_bytes, stubs_found, hash) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(runId);
    query.addBindValue(fileObj.value("file").toString());
    query.addBindValue(fileObj.value("language").toString());
    query.addBindValue(fileObj.value("size_bytes").toVariant());
    query.addBindValue(fileObj.value("stubs_found"));
    query.addBindValue(fileObj.value("hash").toString());

    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }

    const int fileId = query.lastInsertId();
    const void* tasks = fileObj.value("tasks").toArray();
    for (const void* &taskVal : tasks) {
        if (!insertTask(fileId, taskVal.toObject(), error)) {
            return false;
        }
    }
    return true;
}

bool DigestionDatabase::insertTask(int fileId, const void* &taskObj, std::string *error) {
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO digestion_tasks (file_id, line_number, stub_type, context, suggested_fix, confidence, applied, backup_id) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(fileId);
    query.addBindValue(taskObj.value("line"));
    query.addBindValue(taskObj.value("type").toString());
    query.addBindValue(taskObj.value("context").toString());
    query.addBindValue(taskObj.value("suggested_fix").toString());
    query.addBindValue(taskObj.value("confidence").toString());
    query.addBindValue(taskObj.value("applied").toBool());
    query.addBindValue(taskObj.value("backup_id").toString());

    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

std::string DigestionDatabase::defaultSchema() {
    return std::string::fromUtf8(R"SQL(
CREATE TABLE IF NOT EXISTS digestion_runs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    root_dir TEXT NOT NULL,
    start_ms INTEGER,
    end_ms INTEGER,
    elapsed_ms INTEGER,
    total_files INTEGER,
    scanned_files INTEGER,
    stubs_found INTEGER,
    fixes_applied INTEGER,
    bytes_processed INTEGER,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS digestion_reports (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL,
    report_json TEXT,
    FOREIGN KEY(run_id) REFERENCES digestion_runs(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS digestion_files (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id INTEGER NOT NULL,
    path TEXT NOT NULL,
    language TEXT,
    size_bytes INTEGER,
    stubs_found INTEGER,
    hash TEXT,
    FOREIGN KEY(run_id) REFERENCES digestion_runs(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS digestion_tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_id INTEGER NOT NULL,
    line_number INTEGER,
    stub_type TEXT,
    context TEXT,
    suggested_fix TEXT,
    confidence TEXT,
    applied INTEGER,
    backup_id TEXT,
    FOREIGN KEY(file_id) REFERENCES digestion_files(id) ON DELETE CASCADE
);
)SQL");
}

bool DigestionDatabase::executeBatch(const std::stringList &statements, std::string *error) {
    QSqlQuery query(m_db);
    for (const std::string &stmt : statements) {
        if (!query.exec(stmt)) {
            if (error) *error = query.lastError().text();
            return false;
        }
    }
    return true;
}

