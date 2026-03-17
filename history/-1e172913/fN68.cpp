#include "Backend.hpp"
#include "Metrics.hpp"
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QMutex>
#include <QDebug>

namespace mem {

/* ---------- SQLite Backend ---------- */
class SqliteBackend : public IBackend {
public:
    QSqlDatabase db;

    explicit SqliteBackend(const QString& path) {
        db = QSqlDatabase::addDatabase("QSQLITE", "memdb_sqlite");
        db.setDatabaseName(path);
        if (!db.open()) {
            qWarning("Failed to open SQLite database: %s", db.lastError().text().toStdString().c_str());
        }
    }

    void migrate() override {
        auto t0 = mem::Metrics::startTimer();
        QSqlQuery q(db);
        q.exec("CREATE TABLE IF NOT EXISTS user_fact("
               "  user_id TEXT,"
               "  key TEXT,"
               "  cipher BLOB,"
               "  tag BLOB,"
               "  iv BLOB,"
               "  updated DATETIME DEFAULT CURRENT_TIMESTAMP,"
               "  PRIMARY KEY(user_id, key)"
               ");");
        q.exec("CREATE TABLE IF NOT EXISTS repo_tensor("
               "  repo_id TEXT,"
               "  name TEXT,"
               "  cipher BLOB,"
               "  tag BLOB,"
               "  iv BLOB,"
               "  updated DATETIME DEFAULT CURRENT_TIMESTAMP,"
               "  PRIMARY KEY(repo_id, name)"
               ");");
        q.exec("CREATE TABLE IF NOT EXISTS user_role("
               "  user_id TEXT,"
               "  repo_id TEXT,"
               "  role TEXT,"
               "  PRIMARY KEY(user_id, repo_id)"
               ");");
        q.exec("CREATE TABLE IF NOT EXISTS corrections("
               "  user_id TEXT,"
               "  pattern TEXT,"
               "  intent INTEGER,"
               "  hits INTEGER DEFAULT 1,"
               "  PRIMARY KEY(user_id, pattern)"
               ");");
        mem::Metrics::recordLatency("memory_migrate_ms", mem::Metrics::elapsedMs(t0));
    }

    bool storeUserFact(const QString& userId,
                      const QString& key,
                      const EncryptedBlob& blob) override {
        auto t0 = mem::Metrics::startTimer();
        QSqlQuery q(db);
        q.prepare("INSERT OR REPLACE INTO user_fact(user_id,key,cipher,tag,iv) VALUES(?,?,?,?,?)");
        q.addBindValue(userId);
        q.addBindValue(key);
        q.addBindValue(blob.cipher);
        q.addBindValue(blob.tag);
        q.addBindValue(blob.iv);
        bool ok = q.exec();
        mem::Metrics::increment("memory_user_fact_write_total", ok ? 1 : 0);
        mem::Metrics::recordLatency("memory_user_fact_write_ms", mem::Metrics::elapsedMs(t0));
        mem::Metrics::recordBytes("memory_user_fact_bytes", blob.cipher.size());
        return ok;
    }

    std::optional<EncryptedBlob> userFact(const QString& userId,
                                         const QString& key) const override {
        auto t0 = mem::Metrics::startTimer();
        QSqlQuery q(db);
        q.prepare("SELECT cipher,tag,iv FROM user_fact WHERE user_id=? AND key=?");
        q.addBindValue(userId);
        q.addBindValue(key);
        if (!q.exec() || !q.next()) {
            mem::Metrics::increment("memory_user_fact_read_miss_total", 1);
            mem::Metrics::recordLatency("memory_user_fact_read_ms", mem::Metrics::elapsedMs(t0));
            return std::nullopt;
        }

        EncryptedBlob blob{
            .cipher = q.value(0).toByteArray(),
            .tag = q.value(1).toByteArray(),
            .iv = q.value(2).toByteArray()
        };
        mem::Metrics::increment("memory_user_fact_read_hit_total", 1);
        mem::Metrics::recordLatency("memory_user_fact_read_ms", mem::Metrics::elapsedMs(t0));
        mem::Metrics::recordBytes("memory_user_fact_bytes", blob.cipher.size());
        return blob;
    }

    bool storeRepoTensor(const QString& repoId,
                        const QString& tensorName,
                        const EncryptedBlob& blob) override {
        auto t0 = mem::Metrics::startTimer();
        QSqlQuery q(db);
        q.prepare("INSERT OR REPLACE INTO repo_tensor(repo_id,name,cipher,tag,iv) VALUES(?,?,?,?,?)");
        q.addBindValue(repoId);
        q.addBindValue(tensorName);
        q.addBindValue(blob.cipher);
        q.addBindValue(blob.tag);
        q.addBindValue(blob.iv);
        bool ok = q.exec();
        mem::Metrics::increment("memory_repo_tensor_write_total", ok ? 1 : 0);
        mem::Metrics::recordLatency("memory_repo_tensor_write_ms", mem::Metrics::elapsedMs(t0));
        mem::Metrics::recordBytes("memory_repo_tensor_bytes", blob.cipher.size());
        return ok;
    }

    std::optional<EncryptedBlob> repoTensor(const QString& repoId,
                                           const QString& tensorName) const override {
        auto t0 = mem::Metrics::startTimer();
        QSqlQuery q(db);
        q.prepare("SELECT cipher,tag,iv FROM repo_tensor WHERE repo_id=? AND name=?");
        q.addBindValue(repoId);
        q.addBindValue(tensorName);
        if (!q.exec() || !q.next()) {
            mem::Metrics::increment("memory_repo_tensor_read_miss_total", 1);
            mem::Metrics::recordLatency("memory_repo_tensor_read_ms", mem::Metrics::elapsedMs(t0));
            return std::nullopt;
        }

        EncryptedBlob blob{
            .cipher = q.value(0).toByteArray(),
            .tag = q.value(1).toByteArray(),
            .iv = q.value(2).toByteArray()
        };
        mem::Metrics::increment("memory_repo_tensor_read_hit_total", 1);
        mem::Metrics::recordLatency("memory_repo_tensor_read_ms", mem::Metrics::elapsedMs(t0));
        mem::Metrics::recordBytes("memory_repo_tensor_bytes", blob.cipher.size());
        return blob;
    }

    QString role(const QString& userId, const QString& repoId) const override {
        QSqlQuery q(db);
        q.prepare("SELECT role FROM user_role WHERE user_id=? AND repo_id=?");
        q.addBindValue(userId);
        q.addBindValue(repoId);
        if (q.exec() && q.next()) {
            return q.value(0).toString();
        }
        return QStringLiteral("READER");  // Default
    }

    bool setRole(const QString& userId,
                const QString& repoId,
                const QString& role) override {
        QSqlQuery q(db);
        q.prepare("INSERT OR REPLACE INTO user_role(user_id,repo_id,role) VALUES(?,?,?)");
        q.addBindValue(userId);
        q.addBindValue(repoId);
        q.addBindValue(role);
        return q.exec();
    }

    bool recordCorrection(const QString& userId,
                         const QString& pattern,
                         int intent) override {
        QSqlQuery q(db);
        q.prepare("INSERT INTO corrections(user_id,pattern,intent) VALUES(?,?,?) "
                 "ON CONFLICT(user_id,pattern) DO UPDATE SET hits=hits+1");
        q.addBindValue(userId);
        q.addBindValue(pattern);
        q.addBindValue(intent);
        return q.exec();
    }

    std::unordered_map<std::string, int> correctionHistory(const QString& userId) const override {
        std::unordered_map<std::string, int> history;
        QSqlQuery q(db);
        q.prepare("SELECT pattern,hits FROM corrections WHERE user_id=?");
        q.addBindValue(userId);
        if (q.exec()) {
            while (q.next()) {
                history[q.value(0).toString().toStdString()] = q.value(1).toInt();
            }
        }
        return history;
    }

    std::unordered_map<std::string, int64_t> getStats() const override {
        std::unordered_map<std::string, int64_t> stats;
        QSqlQuery q(db);

        q.exec("SELECT COUNT(*) FROM user_fact");
        if (q.next()) stats["user_facts"] = q.value(0).toLongLong();

        q.exec("SELECT COUNT(*) FROM repo_tensor");
        if (q.next()) stats["repo_tensors"] = q.value(0).toLongLong();

        q.exec("SELECT COUNT(*) FROM corrections");
        if (q.next()) stats["corrections"] = q.value(0).toLongLong();

        return stats;
    }

    void clear() override {
        auto t0 = mem::Metrics::startTimer();
        QSqlQuery q(db);
        q.exec("DELETE FROM user_fact");
        q.exec("DELETE FROM repo_tensor");
        q.exec("DELETE FROM corrections");
        q.exec("DELETE FROM user_role");
        mem::Metrics::recordLatency("memory_clear_ms", mem::Metrics::elapsedMs(t0));
        mem::Metrics::increment("memory_clear_total", 1);
    }

    bool isConnected() const override {
        mem::Metrics::record("memory_connected", db.isOpen() ? 1.0 : 0.0);
        return db.isOpen();
    }
};

/* ---------- Factory ---------- */
BackendPtr makeBackend(const QString& postgresUri) {
    if (postgresUri.isEmpty()) {
        QString dbPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath("memory/catalog.db");
        QDir().mkpath(QFileInfo(dbPath).absolutePath());
        return std::make_unique<SqliteBackend>(dbPath);
    }
    // TODO: Implement PostgresBackend with connection pooling
    qWarning("Postgres backend not yet implemented, falling back to SQLite");
    return makeBackend("");
}

} // namespace mem
