#include "hotpatch_system.h"
#include "logging/structured_logger.h"
#include "error_handler.h"
#include <QDir>
#include <QCoreApplication>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>

namespace RawrXD {

HotpatchManager& HotpatchManager::instance() {
    static HotpatchManager instance;
    return instance;
}

void HotpatchManager::initialize(const QString& storagePath, int maxVersions) {
    QMutexLocker lock(&mutex_);
    
    if (initialized_) {
        return;
    }
    
    // Determine storage path
    if (storagePath.isEmpty()) {
        QDir appDir(QCoreApplication::applicationDirPath());
        storagePath_ = appDir.filePath("hotpatches");
    } else {
        storagePath_ = storagePath;
    }
    
    // Create storage directory
    QDir dir(storagePath_);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            ERROR_HANDLE("Failed to create hotpatch storage directory", ErrorContext()
                .setSeverity(ErrorSeverity::HIGH)
                .setCategory(ErrorCategory::FILE_SYSTEM)
                .setOperation("HotpatchManager initialization")
                .addMetadata("storage_path", storagePath_));
            return;
        }
    }
    
    maxVersions_ = maxVersions;
    healthCheckInterval_ = DEFAULT_HEALTH_CHECK_INTERVAL;
    
    // Load patch history
    if (!loadPatchHistory()) {
        LOG_WARN("Failed to load patch history, starting fresh");
    }
    
    // Start health check timer
    QObject::connect(&healthCheckTimer_, &QTimer::timeout, this, [this]() {
        runHealthChecks();
    });
    healthCheckTimer_.start(healthCheckInterval_ * 1000);
    
    initialized_ = true;
    
    LOG_INFO("Hotpatch manager initialized", {
        {"storage_path", storagePath_},
        {"max_versions", maxVersions_},
        {"health_check_interval", healthCheckInterval_}
    });
}

void HotpatchManager::shutdown() {
    QMutexLocker lock(&mutex_);
    
    if (initialized_) {
        healthCheckTimer_.stop();
        savePatchHistory();
        patches_.clear();
        initialized_ = false;
        
        LOG_INFO("Hotpatch manager shut down");
    }
}

bool HotpatchManager::applyHotpatch(const QString& patchFile, const QString& version, const QString& description) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        ERROR_HANDLE("Hotpatch manager not initialized", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::CONFIGURATION)
            .setOperation("HotpatchManager applyHotpatch"));
        return false;
    }
    
    if (!QFile::exists(patchFile)) {
        ERROR_HANDLE("Patch file does not exist", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("HotpatchManager applyHotpatch")
            .addMetadata("patch_file", patchFile));
        return false;
    }
    
    // Validate patch file
    if (!validatePatchFile(patchFile)) {
        ERROR_HANDLE("Patch file validation failed", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("HotpatchManager applyHotpatch")
            .addMetadata("patch_file", patchFile));
        return false;
    }
    
    // Create backup before applying
    QString backupPath = QDir(storagePath_).filePath("backup_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    if (!createBackup(backupPath)) {
        ERROR_HANDLE("Failed to create backup before hotpatch", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("HotpatchManager applyHotpatch"));
        return false;
    }
    
    // Execute patch script
    if (!executePatchScript(patchFile)) {
        ERROR_HANDLE("Patch script execution failed", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::EXECUTION)
            .setOperation("HotpatchManager applyHotpatch")
            .addMetadata("patch_file", patchFile));
        
        // Attempt rollback
        if (!restoreFromBackup(backupPath)) {
            ERROR_HANDLE("Failed to rollback after patch failure", ErrorContext()
                .setSeverity(ErrorSeverity::CRITICAL)
                .setCategory(ErrorCategory::FILE_SYSTEM)
                .setOperation("HotpatchManager applyHotpatch"));
        }
        
        return false;
    }
    
    // Create hotpatch record
    Hotpatch patch(version, description, patchFile);
    patch.checksum = calculateChecksum(patchFile);
    patch.validated = true;
    
    patches_.append(patch);
    
    // Clean up old patches
    cleanupOldPatches();
    
    // Save history
    savePatchHistory();
    
    LOG_INFO("Hotpatch applied successfully", {
        {"patch_id", patch.id},
        {"version", version},
        {"description", description}
    });
    
    return true;
}

bool HotpatchManager::rollbackHotpatch(const QString& patchId) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    // Find the patch
    int patchIndex = -1;
    for (int i = 0; i < patches_.size(); ++i) {
        if (patches_[i].id == patchId) {
            patchIndex = i;
            break;
        }
    }
    
    if (patchIndex == -1) {
        LOG_WARN("Patch not found for rollback", {{"patch_id", patchId}});
        return false;
    }
    
    Hotpatch& patch = patches_[patchIndex];
    
    // Create backup before rollback
    QString backupPath = QDir(storagePath_).filePath("backup_rollback_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    if (!createBackup(backupPath)) {
        ERROR_HANDLE("Failed to create backup before rollback", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::FILE_SYSTEM)
            .setOperation("HotpatchManager rollbackHotpatch"));
        return false;
    }
    
    // Execute rollback script
    QString rollbackScript = patch.patchFile + ".rollback";
    if (!executeRollbackScript(rollbackScript)) {
        ERROR_HANDLE("Rollback script execution failed", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::EXECUTION)
            .setOperation("HotpatchManager rollbackHotpatch")
            .addMetadata("patch_id", patchId));
        
        // Attempt restore from backup
        if (!restoreFromBackup(backupPath)) {
            ERROR_HANDLE("Failed to restore after rollback failure", ErrorContext()
                .setSeverity(ErrorSeverity::CRITICAL)
                .setCategory(ErrorCategory::FILE_SYSTEM)
                .setOperation("HotpatchManager rollbackHotpatch"));
        }
        
        return false;
    }
    
    // Mark patch as inactive
    patch.active = false;
    
    LOG_INFO("Hotpatch rolled back", {{"patch_id", patchId}});
    
    return true;
}

bool HotpatchManager::rollbackToVersion(const QString& version) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    // Find the most recent patch with the specified version
    int targetIndex = -1;
    for (int i = patches_.size() - 1; i >= 0; --i) {
        if (patches_[i].version == version && patches_[i].active) {
            targetIndex = i;
            break;
        }
    }
    
    if (targetIndex == -1) {
        LOG_WARN("No active patch found for version", {{"version", version}});
        return false;
    }
    
    // Rollback all patches applied after the target version
    bool success = true;
    for (int i = patches_.size() - 1; i > targetIndex; --i) {
        if (patches_[i].active) {
            if (!rollbackHotpatch(patches_[i].id)) {
                success = false;
                break;
            }
        }
    }
    
    return success;
}

bool HotpatchManager::validateHotpatch(const QString& patchId) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    // Find the patch
    Hotpatch* patch = nullptr;
    for (Hotpatch& p : patches_) {
        if (p.id == patchId) {
            patch = &p;
            break;
        }
    }
    
    if (!patch) {
        return false;
    }
    
    // Verify checksum
    if (!verifyChecksum(patchId)) {
        patch->validated = false;
        return false;
    }
    
    // Verify integrity
    if (!verifyIntegrity(patchId)) {
        patch->validated = false;
        return false;
    }
    
    patch->validated = true;
    
    LOG_INFO("Hotpatch validated", {{"patch_id", patchId}});
    
    return true;
}

bool HotpatchManager::runHealthChecks(const QString& patchId) {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    bool allHealthy = true;
    
    if (patchId.isEmpty()) {
        // Run health checks for all active patches
        for (const Hotpatch& patch : patches_) {
            if (patch.active) {
                if (!validateHotpatch(patch.id)) {
                    allHealthy = false;
                    LOG_WARN("Health check failed for patch", {{"patch_id", patch.id}});
                }
            }
        }
    } else {
        // Run health check for specific patch
        allHealthy = validateHotpatch(patchId);
    }
    
    if (allHealthy) {
        LOG_DEBUG("Health checks passed");
    }
    
    return allHealthy;
}

void HotpatchManager::setHealthCheckInterval(int seconds) {
    QMutexLocker lock(&mutex_);
    
    healthCheckInterval_ = seconds;
    healthCheckTimer_.stop();
    healthCheckTimer_.start(healthCheckInterval_ * 1000);
    
    LOG_INFO("Health check interval updated", {{"interval_seconds", seconds}});
}

QVector<Hotpatch> HotpatchManager::getAppliedPatches() const {
    QMutexLocker lock(&mutex_);
    return patches_;
}

QVector<Hotpatch> HotpatchManager::getActivePatches() const {
    QMutexLocker lock(&mutex_);
    
    QVector<Hotpatch> active;
    for (const Hotpatch& patch : patches_) {
        if (patch.active) {
            active.append(patch);
        }
    }
    
    return active;
}

Hotpatch HotpatchManager::getCurrentPatch() const {
    QMutexLocker lock(&mutex_);
    
    if (patches_.isEmpty()) {
        return Hotpatch();
    }
    
    // Return the most recently applied active patch
    for (int i = patches_.size() - 1; i >= 0; --i) {
        if (patches_[i].active) {
            return patches_[i];
        }
    }
    
    return Hotpatch();
}

QString HotpatchManager::getPatchHistory() const {
    QMutexLocker lock(&mutex_);
    
    QJsonArray history;
    for (const Hotpatch& patch : patches_) {
        QJsonObject patchObj;
        patchObj["id"] = patch.id;
        patchObj["version"] = patch.version;
        patchObj["description"] = patch.description;
        patchObj["applied_at"] = patch.appliedAt.toString(Qt::ISODate);
        patchObj["active"] = patch.active;
        patchObj["validated"] = patch.validated;
        
        history.append(patchObj);
    }
    
    QJsonDocument doc(history);
    return doc.toJson(QJsonDocument::Indented);
}

bool HotpatchManager::createBackup(const QString& backupPath) {
    QString actualPath = backupPath;
    if (actualPath.isEmpty()) {
        actualPath = QDir(storagePath_).filePath("backup_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    }
    
    QDir backupDir(actualPath);
    if (!backupDir.exists()) {
        if (!backupDir.mkpath(".")) {
            return false;
        }
    }
    
    // Copy patch history
    QString historyFile = QDir(storagePath_).filePath("patch_history.json");
    if (QFile::exists(historyFile)) {
        QString backupFile = backupDir.filePath("patch_history.json");
        if (!QFile::copy(historyFile, backupFile)) {
            return false;
        }
    }
    
    // Copy patch files
    for (const Hotpatch& patch : patches_) {
        if (QFile::exists(patch.patchFile)) {
            QString backupFile = backupDir.filePath(QFileInfo(patch.patchFile).fileName());
            if (!QFile::copy(patch.patchFile, backupFile)) {
                return false;
            }
        }
    }
    
    LOG_INFO("Backup created", {{"backup_path", actualPath}});
    
    return true;
}

bool HotpatchManager::restoreFromBackup(const QString& backupPath) {
    if (!QDir(backupPath).exists()) {
        return false;
    }
    
    // Restore patch history
    QString historyFile = QDir(backupPath).filePath("patch_history.json");
    if (QFile::exists(historyFile)) {
        QString targetFile = QDir(storagePath_).filePath("patch_history.json");
        if (QFile::exists(targetFile)) {
            QFile::remove(targetFile);
        }
        if (!QFile::copy(historyFile, targetFile)) {
            return false;
        }
    }
    
    // Reload patch history
    if (!loadPatchHistory()) {
        return false;
    }
    
    LOG_INFO("Backup restored", {{"backup_path", backupPath}});
    
    return true;
}

bool HotpatchManager::verifyChecksum(const QString& patchId) const {
    // Find the patch
    const Hotpatch* patch = nullptr;
    for (const Hotpatch& p : patches_) {
        if (p.id == patchId) {
            patch = &p;
            break;
        }
    }
    
    if (!patch) {
        return false;
    }
    
    QString currentChecksum = calculateChecksum(patch->patchFile);
    return currentChecksum == patch->checksum;
}

bool HotpatchManager::verifyIntegrity(const QString& patchId) const {
    // Basic integrity check - verify the patch file exists and is readable
    const Hotpatch* patch = nullptr;
    for (const Hotpatch& p : patches_) {
        if (p.id == patchId) {
            patch = &p;
            break;
        }
    }
    
    if (!patch) {
        return false;
    }
    
    QFile file(patch->patchFile);
    return file.exists() && file.open(QIODevice::ReadOnly);
}

bool HotpatchManager::executePatchScript(const QString& scriptPath) {
    QProcess process;
    process.start(scriptPath);
    
    if (!process.waitForStarted()) {
        return false;
    }
    
    if (!process.waitForFinished(30000)) { // 30 second timeout
        process.kill();
        return false;
    }
    
    return process.exitCode() == 0;
}

bool HotpatchManager::executeRollbackScript(const QString& scriptPath) {
    if (!QFile::exists(scriptPath)) {
        // No rollback script - this is acceptable
        return true;
    }
    
    return executePatchScript(scriptPath);
}

QString HotpatchManager::calculateChecksum(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return QString();
    }
    
    return QString(hash.result().toHex());
}

bool HotpatchManager::validatePatchFile(const QString& filePath) const {
    // Basic validation - check file exists and has reasonable size
    QFileInfo info(filePath);
    if (!info.exists()) {
        return false;
    }
    
    if (info.size() == 0) {
        return false;
    }
    
    // Check file extension
    QString ext = info.suffix().toLower();
    if (ext != "patch" && ext != "sh" && ext != "bat" && ext != "ps1") {
        return false;
    }
    
    return true;
}

void HotpatchManager::savePatchHistory() {
    QJsonArray history;
    
    for (const Hotpatch& patch : patches_) {
        QJsonObject patchObj;
        patchObj["id"] = patch.id;
        patchObj["version"] = patch.version;
        patchObj["description"] = patch.description;
        patchObj["patch_file"] = patch.patchFile;
        patchObj["checksum"] = patch.checksum;
        patchObj["applied_at"] = patch.appliedAt.toString(Qt::ISODate);
        patchObj["active"] = patch.active;
        patchObj["validated"] = patch.validated;
        patchObj["metadata"] = patch.metadata;
        
        history.append(patchObj);
    }
    
    QJsonDocument doc(history);
    QString historyFile = QDir(storagePath_).filePath("patch_history.json");
    
    QSaveFile file(historyFile);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    
    file.write(doc.toJson());
    file.commit();
}

bool HotpatchManager::loadPatchHistory() {
    QString historyFile = QDir(storagePath_).filePath("patch_history.json");
    
    if (!QFile::exists(historyFile)) {
        return true; // No history file is okay
    }
    
    QFile file(historyFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return false;
    }
    
    QJsonArray history = doc.array();
    patches_.clear();
    
    for (const QJsonValue& value : history) {
        QJsonObject patchObj = value.toObject();
        
        Hotpatch patch;
        patch.id = patchObj["id"].toString();
        patch.version = patchObj["version"].toString();
        patch.description = patchObj["description"].toString();
        patch.patchFile = patchObj["patch_file"].toString();
        patch.checksum = patchObj["checksum"].toString();
        patch.appliedAt = QDateTime::fromString(patchObj["applied_at"].toString(), Qt::ISODate);
        patch.active = patchObj["active"].toBool();
        patch.validated = patchObj["validated"].toBool();
        patch.metadata = patchObj["metadata"].toObject();
        
        patches_.append(patch);
    }
    
    return true;
}

void HotpatchManager::cleanupOldPatches() {
    if (patches_.size() <= maxVersions_) {
        return;
    }
    
    // Remove oldest patches
    int removeCount = patches_.size() - maxVersions_;
    for (int i = 0; i < removeCount; ++i) {
        if (!patches_[i].active) {
            // Remove patch file
            if (QFile::exists(patches_[i].patchFile)) {
                QFile::remove(patches_[i].patchFile);
            }
            patches_.remove(i);
            i--; // Adjust index after removal
            removeCount--;
        }
    }
}

HotpatchManager::~HotpatchManager() {
    shutdown();
}

} // namespace RawrXD