BackupManager& BackupManager::instance() {
    static BackupManager inst;
    return inst;
}

BackupManager::~BackupManager() = default;

void BackupManager::start(int intervalMinutes) {
    (void)intervalMinutes;
}

void BackupManager::stop() {}

std::string BackupManager::createBackup(BackupType type) {
    (void)type;
    return std::string();
}

bool BackupManager::restoreBackup(const std::string& path) {
    (void)path;
    return false;
}

std::vector<BackupManager::BackupInfo> BackupManager::listBackups() const {
    return std::vector<BackupInfo>();
}

bool BackupManager::verifyBackup(const std::string& path) {
    (void)path;
    return true;
}

void BackupManager::cleanOldBackups(int days) {
    (void)days;
}

void BackupManager::setBackupDirectory(const std::string& path) {
    m_backupDirectory = path;
}

std::string BackupManager::backupDirectory() const {
    return m_backupDirectory;
}


