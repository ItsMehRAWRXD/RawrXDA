#include "UserMemory.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QStandardPaths>
#include <sqlite3.h>

namespace mem {

UserMemory::UserMemory(const QString& dbPath) {
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    int rc = sqlite3_open(dbPath.toStdString().c_str(), &db);
    if (rc) {
        qWarning("Failed to open user memory database: %s", sqlite3_errmsg(db));
        db = nullptr;
        return;
    }
    initializeSchema();
}

UserMemory::~UserMemory() {
    if (db) sqlite3_close(db);
}

void UserMemory::initializeSchema() {
    if (!db) return;

    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS facts(
            id        INTEGER PRIMARY KEY,
            key       TEXT UNIQUE,
            value     TEXT,
            updated   DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS corrections(
            pattern     TEXT PRIMARY KEY,
            intent      INTEGER,
            hits        INTEGER DEFAULT 1,
            updated     DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE TABLE IF NOT EXISTS settings(
            id          INTEGER PRIMARY KEY,
            key         TEXT UNIQUE,
            value       TEXT,
            updated     DATETIME DEFAULT CURRENT_TIMESTAMP
        );

        CREATE INDEX IF NOT EXISTS idx_facts_key ON facts(key);
        CREATE INDEX IF NOT EXISTS idx_corrections_pattern ON corrections(pattern);
    )";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, schema, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        qWarning("Failed to initialize schema: %s", errMsg);
        sqlite3_free(errMsg);
    }
}

bool UserMemory::storeFact(const QString& key, const QJsonValue& value) {
    QMutexLocker locker(&mtx);
    if (!db) return false;

    QJsonDocument doc(value.toObject());
    QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    const char* sql = "INSERT OR REPLACE INTO facts(key,value) VALUES(?,?)";
    sqlite3_stmt* stmt = nullptr;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        qWarning("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.toStdString().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, jsonStr.toStdString().c_str(), -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

std::optional<QJsonValue> UserMemory::fact(const QString& key) const {
    QMutexLocker locker(&mtx);
    if (!db) return std::nullopt;

    const char* sql = "SELECT value FROM facts WHERE key=?";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, key.toStdString().c_str(), -1, SQLITE_STATIC);

    std::optional<QJsonValue> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        QJsonDocument doc = QJsonDocument::fromJson(QString(text).toUtf8());
        result = doc.object();
    }
    sqlite3_finalize(stmt);
    return result;
}

bool UserMemory::recordCorrection(const QString& pattern, int correctIntent) {
    QMutexLocker locker(&mtx);
    if (!db) return false;

    const char* sql = "INSERT INTO corrections(pattern,intent) VALUES(?,?) "
                      "ON CONFLICT(pattern) DO UPDATE SET hits=hits+1";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    sqlite3_bind_text(stmt, 1, pattern.toStdString().c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, correctIntent);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

int UserMemory::getCorrectionCount(const QString& pattern) const {
    QMutexLocker locker(&mtx);
    if (!db) return 0;

    const char* sql = "SELECT hits FROM corrections WHERE pattern=?";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    sqlite3_bind_text(stmt, 1, pattern.toStdString().c_str(), -1, SQLITE_STATIC);

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

std::optional<int> UserMemory::getCorrectedIntent(const QString& pattern) const {
    QMutexLocker locker(&mtx);
    if (!db) return std::nullopt;

    const char* sql = "SELECT intent FROM corrections WHERE pattern=?";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, pattern.toStdString().c_str(), -1, SQLITE_STATIC);

    std::optional<int> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return result;
}

void UserMemory::clear() {
    QMutexLocker locker(&mtx);
    if (!db) return;

    sqlite3_exec(db, "DELETE FROM facts; DELETE FROM corrections; DELETE FROM settings;",
                 nullptr, nullptr, nullptr);
}

std::unordered_map<std::string, int> UserMemory::getStats() const {
    QMutexLocker locker(&mtx);
    std::unordered_map<std::string, int> stats;

    if (!db) return stats;

    const char* sql = "SELECT COUNT(*) FROM facts";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats["facts_count"] = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    sql = "SELECT COUNT(*) FROM corrections";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats["corrections_count"] = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    return stats;
}

bool UserMemory::isValid() const {
    QMutexLocker locker(&mtx);
    return db != nullptr;
}

bool UserMemory::execute(const char* sql, const std::function<void(sqlite3_stmt*)>& callback) {
    QMutexLocker locker(&mtx);
    if (!db) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }

    bool ok = true;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (callback) callback(stmt);
    }

    sqlite3_finalize(stmt);
    return ok;
}

std::vector<QJsonObject> UserMemory::query(const char* sql) {
    std::vector<QJsonObject> results;
    QMutexLocker locker(&mtx);
    if (!db) return results;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QJsonObject obj;
        int colCount = sqlite3_column_count(stmt);
        for (int i = 0; i < colCount; ++i) {
            const char* colName = sqlite3_column_name(stmt, i);
            const char* colValue = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            obj[colName] = colValue;
        }
        results.push_back(obj);
    }

    sqlite3_finalize(stmt);
    return results;
}

} // namespace mem
