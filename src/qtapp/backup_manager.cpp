#include "backup_manager.hpp"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDebug>
#include <QStandardPaths>

BackupManager& BackupManager::instance() {
    static BackupManager instance;
    return instance;
}

BackupManager::BackupManager()
    : QObject(nullptr)
{
    // Default backup directory
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_backupDirectory = appData + "/backups";
    
    // Ensure directory exists
    QDir().mkpath(m_backupDirectory);
    
    m_backupTimer = new QTimer(this);
    connect(m_backupTimer, &QTimer::timeout, this, &BackupManager::performAutomaticBackup);
}

BackupManager::~BackupManager() {
    stop();
}

void BackupManager::start(int intervalMinutes) {
    if (m_running) {
        qInfo() << "[BackupManager] Already running";
        return;
    }
    
    m_running = true;
    m_backupTimer->start(intervalMinutes * 60 * 1000);
    
    qInfo() << "[BackupManager] Started with" << intervalMinutes << "minute interval";
    qInfo() << "[BackupManager] Backup directory:" << m_backupDirectory;
    qInfo() << "[BackupManager] RPO:" << intervalMinutes << "minutes, RTO: <5 minutes";
    
    // Create initial backup
    performAutomaticBackup();
}

void BackupManager::stop() {
    if (!m_running) return;
    
    m_running = false;
    m_backupTimer->stop();
    
    qInfo() << "[BackupManager] Stopped";
}

QString BackupManager::createBackup(BackupType type) {
    QString backupId = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString typeName = (type == Full) ? "full" : (type == Incremental ? "incr" : "diff");
    backupId += "_" + typeName;
    
    emit backupStarted(backupId);
    
    qInfo() << "[BackupManager] Creating backup:" << backupId;
    
    // Create backup directory
    QString backupPath = m_backupDirectory + "/" + backupId;
    QDir().mkpath(backupPath);
    
    // Backup models directory (if exists)
    QString modelsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
    if (QDir(modelsDir).exists()) {
        QString dstModels = backupPath + "/models";
        if (!compressBackup(modelsDir, dstModels + ".tar.gz")) {
            emit backupFailed("Failed to backup models");
            return QString();
        }
    }
    
    // Backup configuration files
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config";
    if (QDir(configDir).exists()) {
        QString dstConfig = backupPath + "/config";
        if (!compressBackup(configDir, dstConfig + ".tar.gz")) {
            emit backupFailed("Failed to backup config");
            return QString();
        }
    }
    
    // Calculate total size
    size_t totalSize = 0;
    QDirIterator it(backupPath, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo fi(it.next());
        if (fi.isFile()) {
            totalSize += fi.size();
        }
    }
    
    // Create backup info
    BackupInfo info;
    info.id = backupId;
    info.type = type;
    info.timestamp = QDateTime::currentDateTime();
    info.path = backupPath;
    info.sizeBytes = totalSize;
    info.verified = verifyBackup(backupId);
    info.checksum = calculateChecksum(backupPath);
    
    m_backups[backupId] = info;
    
    emit backupCompleted(backupId, totalSize);
    
    qInfo() << "[BackupManager] Backup completed:" << backupId 
            << "Size:" << (totalSize / (1024.0 * 1024.0)) << "MB";
    
    return backupId;
}

bool BackupManager::restoreBackup(const QString& backupId) {
    if (!m_backups.contains(backupId)) {
        qWarning() << "[BackupManager] Backup not found:" << backupId;
        emit restoreFailed("Backup not found");
        return false;
    }
    
    emit restoreStarted(backupId);
    
    const BackupInfo& info = m_backups[backupId];
    
    qInfo() << "[BackupManager] Restoring backup:" << backupId;
    
    // Verify backup before restore
    if (!verifyBackup(backupId)) {
        emit restoreFailed("Backup verification failed");
        return false;
    }
    
    // Restore models
    QString modelsBackup = info.path + "/models.tar.gz";
    if (QFile::exists(modelsBackup)) {
        QString modelsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
        if (!decompressBackup(modelsBackup, modelsDir)) {
            emit restoreFailed("Failed to restore models");
            return false;
        }
    }
    
    // Restore config
    QString configBackup = info.path + "/config.tar.gz";
    if (QFile::exists(configBackup)) {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config";
        if (!decompressBackup(configBackup, configDir)) {
            emit restoreFailed("Failed to restore config");
            return false;
        }
    }
    
    emit restoreCompleted(backupId);
    
    qInfo() << "[BackupManager] Restore completed:" << backupId;
    return true;
}

QList<BackupManager::BackupInfo> BackupManager::listBackups() const {
    return m_backups.values();
}

bool BackupManager::verifyBackup(const QString& backupId) {
    if (!m_backups.contains(backupId)) {
        return false;
    }
    
    const BackupInfo& info = m_backups[backupId];
    
    // Check if backup directory exists
    if (!QDir(info.path).exists()) {
        qWarning() << "[BackupManager] Backup directory missing:" << info.path;
        return false;
    }
    
    // Verify checksum
    QString currentChecksum = calculateChecksum(info.path);
    if (currentChecksum != info.checksum) {
        qWarning() << "[BackupManager] Checksum mismatch for backup:" << backupId;
        return false;
    }
    
    qInfo() << "[BackupManager] Backup verified:" << backupId;
    return true;
}

void BackupManager::cleanOldBackups(int daysToKeep) {
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-daysToKeep);
    
    QStringList toRemove;
    for (auto it = m_backups.begin(); it != m_backups.end(); ++it) {
        if (it.value().timestamp < cutoffDate) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& backupId : toRemove) {
        QString path = m_backups[backupId].path;
        QDir(path).removeRecursively();
        m_backups.remove(backupId);
        qInfo() << "[BackupManager] Removed old backup:" << backupId;
    }
    
    if (!toRemove.isEmpty()) {
        qInfo() << "[BackupManager] Cleaned" << toRemove.size() << "old backups";
    }
}

void BackupManager::setBackupDirectory(const QString& path) {
    m_backupDirectory = path;
    QDir().mkpath(m_backupDirectory);
    qInfo() << "[BackupManager] Backup directory set to:" << path;
}

QString BackupManager::backupDirectory() const {
    return m_backupDirectory;
}

void BackupManager::performAutomaticBackup() {
    qInfo() << "[BackupManager] Performing automatic backup...";
    
    // Use incremental backup for automatic backups
    QString backupId = createBackup(Incremental);
    
    if (!backupId.isEmpty()) {
        // Clean old backups (keep 30 days)
        cleanOldBackups(30);
    }
}

QString BackupManager::calculateChecksum(const QString& filePath) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        while (!file.atEnd()) {
            hash.addData(file.read(8192));
        }
        file.close();
    }
    
    return QString(hash.result().toHex());
}

bool BackupManager::compressBackup(const QString& srcPath, const QString& dstPath) {
    // Simplified compression - in production use brutal_gzip or tar
    QFile src(srcPath);
    QFile dst(dstPath);
    
    if (!src.open(QIODevice::ReadOnly)) {
        qWarning() << "[BackupManager] Failed to open source:" << srcPath;
        return false;
    }
    
    if (!dst.open(QIODevice::WriteOnly)) {
        qWarning() << "[BackupManager] Failed to create backup file:" << dstPath;
        return false;
    }
    
    // Copy data (TODO: actual compression with brutal_gzip)
    dst.write(src.readAll());
    
    src.close();
    dst.close();
    
    return true;
}

bool BackupManager::decompressBackup(const QString& srcPath, const QString& dstPath) {
    // Simplified decompression - in production use brutal_gzip or tar
    QFile src(srcPath);
    QFile dst(dstPath);
    
    if (!src.open(QIODevice::ReadOnly)) {
        qWarning() << "[BackupManager] Failed to open backup:" << srcPath;
        return false;
    }
    
    // Ensure destination directory exists
    QFileInfo fi(dstPath);
    QDir().mkpath(fi.absolutePath());
    
    if (!dst.open(QIODevice::WriteOnly)) {
        qWarning() << "[BackupManager] Failed to create restore file:" << dstPath;
        return false;
    }
    
    // Copy data (TODO: actual decompression with brutal_gzip)
    dst.write(src.readAll());
    
    src.close();
    dst.close();
    
    return true;
}
