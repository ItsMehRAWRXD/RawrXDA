// ============================================================================
// ExtensionUIState.hpp — Single source of truth for Extension UI
// ============================================================================
// Mirrors ExtensionInstaller lifecycle into a UI-friendly struct.
// Thread-safe: all reads/writes are guarded by a mutex.
//
// Usage:
//   ExtensionUIState::Instance().update("rust-analyzer", {
//       .status = ExtensionUIStatus::Downloading,
//       .progress = 0.63f,
//       .bytesDownloaded = 12*1024*1024,
//       .totalBytes = 19*1024*1024
//   });
// ============================================================================

#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace RawrXD {

enum class ExtensionUIStatus {
    NotInstalled,
    Downloading,
    Installed,
    Active,
    Failed
};

struct ExtensionUIEntry {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::string author;
    ExtensionUIStatus status = ExtensionUIStatus::NotInstalled;
    float progress = 0.0f;          // 0.0 → 1.0
    uint64_t bytesDownloaded = 0;
    uint64_t totalBytes = 0;
    double speedBps = 0.0;
    std::string lastError;
    std::string installPath;
    bool hasNativeModule = false;
    bool hasCommands = false;
};

using ExtensionUIChangeCallback = std::function<void()>;

class ExtensionUIState {
public:
    static ExtensionUIState& Instance();

    // Update or insert an entry (thread-safe)
    void update(const std::string& id, const ExtensionUIEntry& entry);

    // Partial update — only touches provided fields (thread-safe)
    void updateProgress(const std::string& id, float progress,
                        uint64_t bytesDone, uint64_t bytesTotal, double speed);
    void updateStatus(const std::string& id, ExtensionUIStatus status);
    void setError(const std::string& id, const std::string& error);

    // Remove an entry
    void remove(const std::string& id);

    // Snapshot copy for UI rendering (main thread)
    std::vector<ExtensionUIEntry> snapshot() const;

    // Get single entry (returns nullptr if not found)
    std::optional<ExtensionUIEntry> get(const std::string& id) const;

    // Register a callback fired on any change (from any thread).
    // Callback should PostMessage to panel HWND for UI refresh.
    void setChangeCallback(ExtensionUIChangeCallback cb);

    // Seed from ExtensionInstaller records (call after scan)
    void syncFromInstaller(class ExtensionInstaller* installer);

private:
    ExtensionUIState() = default;
    ExtensionUIState(const ExtensionUIState&) = delete;
    ExtensionUIState& operator=(const ExtensionUIState&) = delete;

    mutable std::mutex m_mutex;
    std::map<std::string, ExtensionUIEntry> m_entries;
    ExtensionUIChangeCallback m_cb;

    void notifyChange();
};

} // namespace RawrXD
