#include "qtapp/backup_manager.hpp"

#include <QString>
#include <QList>

BackupManager& BackupManager::instance() {
    static BackupManager inst;
    return inst;
}

BackupManager::~BackupManager() = default;

void BackupManager::start(int intervalMinutes) {
    (void)intervalMinutes;
}

void BackupManager::stop() {}

QString BackupManager::createBackup(BackupType type) {
    (void)type;
    return QString();
}

bool BackupManager::restoreBackup(const QString& path) {
    (void)path;
    return false;
}

QList<BackupManager::BackupInfo> BackupManager::listBackups() const {
    return QList<BackupInfo>();
}

bool BackupManager::verifyBackup(const QString& path) {
    (void)path;
    return true;
}

void BackupManager::cleanOldBackups(int days) {
    (void)days;
}

void BackupManager::setBackupDirectory(const QString& path) {
    m_backupDirectory = path;
}

QString BackupManager::backupDirectory() const {
    return m_backupDirectory;
}
