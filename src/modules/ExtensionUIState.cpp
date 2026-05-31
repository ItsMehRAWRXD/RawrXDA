// ============================================================================
// ExtensionUIState.cpp — Implementation
// ============================================================================

#include "ExtensionUIState.hpp"
#include "ExtensionInstaller.hpp"

namespace RawrXD {

ExtensionUIState& ExtensionUIState::Instance() {
    static ExtensionUIState s_instance;
    return s_instance;
}

void ExtensionUIState::update(const std::string& id, const ExtensionUIEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries[id] = entry;
    m_entries[id].id = id;
    notifyChange();
}

void ExtensionUIState::updateProgress(const std::string& id, float progress,
                                       uint64_t bytesDone, uint64_t bytesTotal, double speed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(id);
    if (it == m_entries.end()) {
        ExtensionUIEntry e;
        e.id = id;
        e.name = id;
        e.status = ExtensionUIStatus::Downloading;
        e.progress = progress;
        e.bytesDownloaded = bytesDone;
        e.totalBytes = bytesTotal;
        e.speedBps = speed;
        m_entries[id] = std::move(e);
    } else {
        it->second.progress = progress;
        it->second.bytesDownloaded = bytesDone;
        it->second.totalBytes = bytesTotal;
        it->second.speedBps = speed;
        if (progress >= 1.0f && it->second.status == ExtensionUIStatus::Downloading) {
            it->second.status = ExtensionUIStatus::Installed;
        }
    }
    notifyChange();
}

void ExtensionUIState::updateStatus(const std::string& id, ExtensionUIStatus status) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(id);
    if (it != m_entries.end()) {
        it->second.status = status;
    } else {
        ExtensionUIEntry e;
        e.id = id;
        e.name = id;
        e.status = status;
        m_entries[id] = std::move(e);
    }
    notifyChange();
}

void ExtensionUIState::setError(const std::string& id, const std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(id);
    if (it != m_entries.end()) {
        it->second.lastError = error;
        it->second.status = ExtensionUIStatus::Failed;
    } else {
        ExtensionUIEntry e;
        e.id = id;
        e.name = id;
        e.lastError = error;
        e.status = ExtensionUIStatus::Failed;
        m_entries[id] = std::move(e);
    }
    notifyChange();
}

void ExtensionUIState::remove(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.erase(id);
    notifyChange();
}

std::vector<ExtensionUIEntry> ExtensionUIState::snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ExtensionUIEntry> result;
    result.reserve(m_entries.size());
    for (const auto& [_, e] : m_entries) {
        result.push_back(e);
    }
    return result;
}

std::optional<ExtensionUIEntry> ExtensionUIState::get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entries.find(id);
    if (it != m_entries.end()) return it->second;
    return std::nullopt;
}

void ExtensionUIState::setChangeCallback(ExtensionUIChangeCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cb = std::move(cb);
}

void ExtensionUIState::syncFromInstaller(ExtensionInstaller* installer) {
    if (!installer) return;
    auto records = installer->listInstalled();
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& rec : records) {
        ExtensionUIEntry e;
        e.id = rec.manifest.id;
        e.name = rec.manifest.name.empty() ? rec.manifest.id : rec.manifest.name;
        e.version = rec.manifest.version;
        e.description = rec.manifest.description;
        e.author = rec.manifest.author;
        e.installPath = rec.installPath;
        e.hasNativeModule = hasCapability(rec.manifest.capabilities, ExtensionCapability::NativeModule);
        e.hasCommands = !rec.manifest.tools.empty();

        switch (rec.state) {
            case ExtensionLifecycleState::NotInstalled:
            case ExtensionLifecycleState::DownloadFailed:
            case ExtensionLifecycleState::VerifyFailed:
            case ExtensionLifecycleState::InstallFailed:
                e.status = ExtensionUIStatus::Failed;
                break;
            case ExtensionLifecycleState::Downloading:
            case ExtensionLifecycleState::Verifying:
            case ExtensionLifecycleState::Installing:
                e.status = ExtensionUIStatus::Downloading;
                break;
            case ExtensionLifecycleState::Installed:
            case ExtensionLifecycleState::Inactive:
            case ExtensionLifecycleState::Deactivating:
            case ExtensionLifecycleState::Uninstalling:
                e.status = ExtensionUIStatus::Installed;
                break;
            case ExtensionLifecycleState::Activating:
            case ExtensionLifecycleState::Active:
                e.status = ExtensionUIStatus::Active;
                break;
            default:
                e.status = ExtensionUIStatus::NotInstalled;
                break;
        }
        m_entries[e.id] = std::move(e);
    }
    notifyChange();
}

void ExtensionUIState::notifyChange() {
    auto cb = m_cb;
    if (cb) cb();
}

} // namespace RawrXD
