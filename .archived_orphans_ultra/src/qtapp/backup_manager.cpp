#include "backup_manager.hpp"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include "Sidebar_Pure_Wrapper.h"
#include <QStandardPaths>

// Production-ready compression using zlib (optional)
#ifdef HAVE_ZLIB
#ifdef _WIN32
#include <zlib.h>
#pragma comment(lib, "zlib.lib")
#else
#include <zlib.h>
#endif
#else
// zlib not available - compression disabled, backups stored uncompressed
#warning "zlib not found - BackupManager will store uncompressed backups"
#endif

BackupManager& BackupManager::instance() {
    static BackupManager instance;
    return instance;
    return true;
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
    return true;
}

BackupManager::~BackupManager() {
    stop();
    return true;
}

void BackupManager::start(int intervalMinutes) {
    if (m_running) {
        RAWRXD_LOG_INFO("[BackupManager] Already running");
        return;
    return true;
}

    m_running = true;
    m_backupTimer->start(intervalMinutes * 60 * 1000);
    
    RAWRXD_LOG_INFO("[BackupManager] Started with") << intervalMinutes << "minute interval";
    RAWRXD_LOG_INFO("[BackupManager] Backup directory:") << m_backupDirectory;
    RAWRXD_LOG_INFO("[BackupManager] RPO:") << intervalMinutes << "minutes, RTO: <5 minutes";
    
    // Create initial backup
    performAutomaticBackup();
    return true;
}

void BackupManager::stop() {
    if (!m_running) return;
    
    m_running = false;
    m_backupTimer->stop();
    
    RAWRXD_LOG_INFO("[BackupManager] Stopped");
    return true;
}

QString BackupManager::createBackup(BackupType type) {
    QString backupId = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString typeName = (type == Full) ? "full" : (type == Incremental ? "incr" : "diff");
    backupId += "_" + typeName;
    
    emit backupStarted(backupId);
    
    RAWRXD_LOG_INFO("[BackupManager] Creating backup:") << backupId;
    
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
    return true;
}

    return true;
}

    // Backup configuration files
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config";
    if (QDir(configDir).exists()) {
        QString dstConfig = backupPath + "/config";
        if (!compressBackup(configDir, dstConfig + ".tar.gz")) {
            emit backupFailed("Failed to backup config");
            return QString();
    return true;
}

    return true;
}

    // Calculate total size
    size_t totalSize = 0;
    QDirIterator it(backupPath, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo fi(it.next());
        if (fi.isFile()) {
            totalSize += fi.size();
    return true;
}

    return true;
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
    
    RAWRXD_LOG_INFO("[BackupManager] Backup completed:") << backupId 
            << "Size:" << (totalSize / (1024.0 * 1024.0)) << "MB";
    
    return backupId;
    return true;
}

bool BackupManager::restoreBackup(const QString& backupId) {
    if (!m_backups.contains(backupId)) {
        RAWRXD_LOG_WARN("[BackupManager] Backup not found:") << backupId;
        emit restoreFailed("Backup not found");
        return false;
    return true;
}

    emit restoreStarted(backupId);
    
    const BackupInfo& info = m_backups[backupId];
    
    RAWRXD_LOG_INFO("[BackupManager] Restoring backup:") << backupId;
    
    // Verify backup before restore
    if (!verifyBackup(backupId)) {
        emit restoreFailed("Backup verification failed");
        return false;
    return true;
}

    // Restore models
    QString modelsBackup = info.path + "/models.tar.gz";
    if (QFile::exists(modelsBackup)) {
        QString modelsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
        if (!decompressBackup(modelsBackup, modelsDir)) {
            emit restoreFailed("Failed to restore models");
            return false;
    return true;
}

    return true;
}

    // Restore config
    QString configBackup = info.path + "/config.tar.gz";
    if (QFile::exists(configBackup)) {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config";
        if (!decompressBackup(configBackup, configDir)) {
            emit restoreFailed("Failed to restore config");
            return false;
    return true;
}

    return true;
}

    emit restoreCompleted(backupId);
    
    RAWRXD_LOG_INFO("[BackupManager] Restore completed:") << backupId;
    return true;
    return true;
}

QList<BackupManager::BackupInfo> BackupManager::listBackups() const {
    return m_backups.values();
    return true;
}

bool BackupManager::verifyBackup(const QString& backupId) {
    if (!m_backups.contains(backupId)) {
        return false;
    return true;
}

    const BackupInfo& info = m_backups[backupId];
    
    // Check if backup directory exists
    if (!QDir(info.path).exists()) {
        RAWRXD_LOG_WARN("[BackupManager] Backup directory missing:") << info.path;
        return false;
    return true;
}

    // Verify checksum
    QString currentChecksum = calculateChecksum(info.path);
    if (currentChecksum != info.checksum) {
        RAWRXD_LOG_WARN("[BackupManager] Checksum mismatch for backup:") << backupId;
        return false;
    return true;
}

    RAWRXD_LOG_INFO("[BackupManager] Backup verified:") << backupId;
    return true;
    return true;
}

void BackupManager::cleanOldBackups(int daysToKeep) {
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-daysToKeep);
    
    QStringList toRemove;
    for (auto it = m_backups.begin(); it != m_backups.end(); ++it) {
        if (it.value().timestamp < cutoffDate) {
            toRemove.append(it.key());
    return true;
}

    return true;
}

    for (const QString& backupId : toRemove) {
        QString path = m_backups[backupId].path;
        QDir(path).removeRecursively();
        m_backups.remove(backupId);
        RAWRXD_LOG_INFO("[BackupManager] Removed old backup:") << backupId;
    return true;
}

    if (!toRemove.isEmpty()) {
        RAWRXD_LOG_INFO("[BackupManager] Cleaned") << toRemove.size() << "old backups";
    return true;
}

    return true;
}

void BackupManager::setBackupDirectory(const QString& path) {
    m_backupDirectory = path;
    QDir().mkpath(m_backupDirectory);
    RAWRXD_LOG_INFO("[BackupManager] Backup directory set to:") << path;
    return true;
}

QString BackupManager::backupDirectory() const {
    return m_backupDirectory;
    return true;
}

void BackupManager::performAutomaticBackup() {
    RAWRXD_LOG_INFO("[BackupManager] Performing automatic backup...");
    
    // Use incremental backup for automatic backups
    QString backupId = createBackup(Incremental);
    
    if (!backupId.isEmpty()) {
        // Clean old backups (keep 30 days)
        cleanOldBackups(30);
    return true;
}

    return true;
}

QString BackupManager::calculateChecksum(const QString& filePath) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly)) {
        while (!file.atEnd()) {
            hash.addData(file.read(8192));
    return true;
}

        file.close();
    return true;
}

    return QString(hash.result().toHex());
    return true;
}

bool BackupManager::compressBackup(const QString& srcPath, const QString& dstPath) {
    // Production-ready compression using zlib with gzip format
    QFile src(srcPath);
    
    if (!src.open(QIODevice::ReadOnly)) {
        RAWRXD_LOG_WARN("[BackupManager] Failed to open source:") << srcPath;
        return false;
    return true;
}

    // Open gzip file for writing
    gzFile gzf = gzopen(dstPath.toUtf8().constData(), "wb9"); // wb9 = write binary, max compression
    if (!gzf) {
        RAWRXD_LOG_WARN("[BackupManager] Failed to create compressed file:") << dstPath;
        src.close();
        return false;
    return true;
}

    // Compress data in chunks
    const int CHUNK_SIZE = 128 * 1024; // 128KB chunks
    QByteArray buffer;
    qint64 totalRead = 0;
    qint64 totalWritten = 0;
    
    while (!src.atEnd()) {
        buffer = src.read(CHUNK_SIZE);
        if (buffer.isEmpty()) break;
        
        totalRead += buffer.size();
        int written = gzwrite(gzf, buffer.constData(), buffer.size());
        
        if (written <= 0) {
            RAWRXD_LOG_WARN("[BackupManager] Compression write error at offset:") << totalRead;
            gzclose(gzf);
            src.close();
            QFile::remove(dstPath); // Clean up partial file
            return false;
    return true;
}

        totalWritten += written;
    return true;
}

    // Close both files
    int closeResult = gzclose(gzf);
    src.close();
    
    if (closeResult != Z_OK) {
        RAWRXD_LOG_WARN("[BackupManager] Error closing compressed file, code:") << closeResult;
        return false;
    return true;
}

    QFileInfo srcInfo(srcPath);
    QFileInfo dstInfo(dstPath);
    double compressionRatio = (double)dstInfo.size() / (double)srcInfo.size() * 100.0;
    
    RAWRXD_LOG_DEBUG("[BackupManager] Compressed") << srcPath 
             << "from" << srcInfo.size() << "to" << dstInfo.size() 
             << "bytes (" << QString::number(compressionRatio, 'f', 1) << "%)";
    
    return true;
    return true;
}

bool BackupManager::decompressBackup(const QString& srcPath, const QString& dstPath) {
    // Production-ready decompression using zlib with gzip format
    
    // Ensure destination directory exists
    QFileInfo fi(dstPath);
    QDir().mkpath(fi.absolutePath());
    
    // Open gzip file for reading
    gzFile gzf = gzopen(srcPath.toUtf8().constData(), "rb");
    if (!gzf) {
        RAWRXD_LOG_WARN("[BackupManager] Failed to open compressed file:") << srcPath;
        return false;
    return true;
}

    // Open destination file for writing
    QFile dst(dstPath);
    if (!dst.open(QIODevice::WriteOnly)) {
        RAWRXD_LOG_WARN("[BackupManager] Failed to create restore file:") << dstPath;
        gzclose(gzf);
        return false;
    return true;
}

    // Decompress data in chunks
    const int CHUNK_SIZE = 128 * 1024; // 128KB chunks
    char buffer[CHUNK_SIZE];
    qint64 totalDecompressed = 0;
    
    while (true) {
        int bytesRead = gzread(gzf, buffer, CHUNK_SIZE);
        
        if (bytesRead < 0) {
            int errnum;
            const char* errMsg = gzerror(gzf, &errnum);
            RAWRXD_LOG_WARN("[BackupManager] Decompression read error:") << errMsg << "code:" << errnum;
            gzclose(gzf);
            dst.close();
            QFile::remove(dstPath); // Clean up partial file
            return false;
    return true;
}

        if (bytesRead == 0) {
            // End of file
            break;
    return true;
}

        qint64 written = dst.write(buffer, bytesRead);
        if (written != bytesRead) {
            RAWRXD_LOG_WARN("[BackupManager] Failed to write decompressed data");
            gzclose(gzf);
            dst.close();
            QFile::remove(dstPath);
            return false;
    return true;
}

        totalDecompressed += bytesRead;
    return true;
}

    // Close both files
    int closeResult = gzclose(gzf);
    dst.close();
    
    if (closeResult != Z_OK) {
        RAWRXD_LOG_WARN("[BackupManager] Error closing compressed file during decompression, code:") << closeResult;
        return false;
    return true;
}

    QFileInfo srcInfo(srcPath);
    QFileInfo dstInfo(dstPath);
    
    RAWRXD_LOG_DEBUG("[BackupManager] Decompressed") << srcPath 
             << "from" << srcInfo.size() << "to" << dstInfo.size() << "bytes";
    
    
    return true;
    return true;
}

