#include "digestion_db.h"
DigestionDatabase::DigestionDatabase()  {}

bool DigestionDatabase::open(const std::string &path, std::string *error) {
    if (m_db.isValid()) {
        const std::string previousName = m_db.connectionName();
        if (m_db.isOpen()) m_db.close();
        if (!previousName.empty()) {
            QSqlDatabase::removeDatabase(previousName);
        }
    }

    const std::string connectionName = std::stringLiteral("digestion_%1_%2")
        )
        );

    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        if (error) *error = m_db.lastError().text();
        return false;
    }
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA foreign_keys = ON");
    return true;
}

bool DigestionDatabase::ensureSchema(const std::string &schemaPath, std::string *error) {
    std::string schemaSql;
    if (!schemaPath.empty()) {
        // File operation removed;
        if (file.open(std::iostream::ReadOnly | std::iostream::Text)) {
            schemaSql = std::string::fromUtf8(file.readAll());
        }
    }
    if (schemaSql.empty()) schemaSql = defaultSchema();

    std::stringList statements;
    for (const std::string &chunk : schemaSql.split(';', SkipEmptyParts)) {
        const std::string stmt = chunk.trimmed();
        if (!stmt.empty()) statements.append(stmt);
    }
    if (!m_db.transaction()) {
        if (error) *error = m_db.lastError().text();
        return false;
    }
    const bool ok = executeBatch(statements, error);
    if (ok) {
        m_db.commit();
    } else {
        m_db.rollback();
    }
    return ok;
}

bool DigestionDatabase::insertRun(const std::string &rootDir, const DigestionMetrics &metrics, void* report, int *runId, std::string *error) {
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO digestion_runs (root_dir, start_ms, end_ms, elapsed_ms, total_files, scanned_files, stubs_found, fixes_applied, bytes_processed) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(rootDir);
    query.addBindValue(metrics.startMs);
    query.addBindValue(metrics.endMs);
    query.addBindValue(metrics.elapsedMs);
    query.addBindValue(metrics.totalFiles);
    query.addBindValue(metrics.scannedFiles);
    query.addBindValue(metrics.stubsFound);
    query.addBindValue(metrics.fixesApplied);
    query.addBindValue(metrics.bytesProcessed);

    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    const int id = query.lastInsertId();
    if (runId) *runId = id;

    if (!report.empty()) {
        QSqlQuery reportQuery(m_db);
        reportQuery.prepare("INSERT INTO digestion_reports (run_id, report_json) VALUES (?, ?)");
        reportQuery.addBindValue(id);
        reportQuery.addBindValue(std::string::fromUtf8(void*(report).toJson(void*::Compact)));
        if (!reportQuery.exec() && error) {
            *error = reportQuery.lastError().text();
        }
    }

    return true;
}

std::vector<void*> DigestionDatabase::fetchRecentRuns(int limit, std::string *error) const {
    std::vector<void*> runs;
    if (!m_db.isValid()) {
        if (error) *error = std::stringLiteral("Database connection is not open");
        return runs;
    }

    QSqlQuery query(m_db);
    query.prepare(std::stringLiteral(
        "SELECT id, root_dir, start_ms, end_ms, elapsed_ms, total_files, scanned_files, "
        "stubs_found, fixes_applied, bytes_processed, created_at "
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

