#ifndef HOTPATCH_SYSTEM_H
#define HOTPATCH_SYSTEM_H

#include <QString>
#include <QDateTime>
#include <QHash>
#include <QMutex>
#include <QSharedPointer>
#include <QVector>
#include <QProcess>

namespace RawrXD {

struct Hotpatch {
    QString id;
    QString version;
    QString description;
    QString patchFile;
    QString checksum;
    QDateTime appliedAt;
    bool active;
    bool validated;
    QJsonObject metadata;
    
    Hotpatch() : active(false), validated(false) {}
    Hotpatch(const QString& ver, const QString& desc, const QString& file)
        : version(ver), description(desc), patchFile(file), appliedAt(QDateTime::currentDateTime())
        , active(true), validated(false) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    
    bool isValid() const {
        return !id.isEmpty() && !version.isEmpty() && !patchFile.isEmpty();
    }
};

class HotpatchManager {
public:
    static HotpatchManager& instance();
    
    void initialize(const QString& storagePath = QString(), int maxVersions = 10);
    void shutdown();
    
    // Hotpatch operations
    bool applyHotpatch(const QString& patchFile, const QString& version, const QString& description = QString());
    bool rollbackHotpatch(const QString& patchId);
    bool rollbackToVersion(const QString& version);
    bool validateHotpatch(const QString& patchId);
    
    // Health checks
    bool runHealthChecks(const QString& patchId = QString());
    void setHealthCheckInterval(int seconds);
    
    // History and status
    QVector<Hotpatch> getAppliedPatches() const;
    QVector<Hotpatch> getActivePatches() const;
    Hotpatch getCurrentPatch() const;
    QString getPatchHistory() const;
    
    // Backup and restore
    bool createBackup(const QString& backupPath = QString());
    bool restoreFromBackup(const QString& backupPath);
    
    // Validation
    bool verifyChecksum(const QString& patchId) const;
    bool verifyIntegrity(const QString& patchId) const;
    
private:
    HotpatchManager() = default;
    ~HotpatchManager();
    
    bool executePatchScript(const QString& scriptPath);
    bool executeRollbackScript(const QString& scriptPath);
    QString calculateChecksum(const QString& filePath) const;
    bool validatePatchFile(const QString& filePath) const;
    void savePatchHistory();
    bool loadPatchHistory();
    void cleanupOldPatches();
    
    QString storagePath_;
    QVector<Hotpatch> patches_;
    mutable QMutex mutex_;
    int maxVersions_;
    int healthCheckInterval_;
    QTimer healthCheckTimer_;
    bool initialized_ = false;
    
    static const int DEFAULT_HEALTH_CHECK_INTERVAL = 30; // 30 seconds
};

// Convenience macros
#define HOTPATCH_APPLY(file, version, desc) RawrXD::HotpatchManager::instance().applyHotpatch(file, version, desc)
#define HOTPATCH_ROLLBACK(id) RawrXD::HotpatchManager::instance().rollbackHotpatch(id)
#define HOTPATCH_VALIDATE(id) RawrXD::HotpatchManager::instance().validateHotpatch(id)
#define HOTPATCH_HEALTH_CHECK(id) RawrXD::HotpatchManager::instance().runHealthChecks(id)

} // namespace RawrXD

#endif // HOTPATCH_SYSTEM_H