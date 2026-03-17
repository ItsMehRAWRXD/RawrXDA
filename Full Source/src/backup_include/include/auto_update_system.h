// ============================================================================
// auto_update_system.h — Automatic Update System (Phase 33)
// ============================================================================
//
// PURPOSE:
//   Check GitHub Releases API on startup for new versions. Prompts the user
//   to download if an update is available. Uses WinHTTP for network access.
//
// PATTERN:   PatchResult-compatible, no exceptions, no std::function
// THREADING: Runs on background thread, notifies main thread via callback
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {
namespace Update {

// ============================================================================
// Version Info
// ============================================================================

struct VersionInfo {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint32_t build;

    bool isNewerThan(const VersionInfo& other) const {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        if (patch != other.patch) return patch > other.patch;
        return build > other.build;
    }

    bool operator==(const VersionInfo& other) const {
        return major == other.major && minor == other.minor &&
               patch == other.patch && build == other.build;
    }
};

// ============================================================================
// Update Check Result
// ============================================================================

struct UpdateCheckResult {
    bool        success;            // HTTP request succeeded
    bool        updateAvailable;    // Newer version exists
    VersionInfo latestVersion;      // Latest version from GitHub
    char        downloadUrl[512];   // Direct download URL
    char        releaseNotes[2048]; // Release notes (truncated)
    char        tagName[64];        // Git tag (e.g., "v1.1.0")
    char        errorDetail[256];   // Error message if !success
    int         errorCode;          // WinHTTP error code

    static UpdateCheckResult ok() {
        UpdateCheckResult r{};
        r.success = true;
        r.updateAvailable = false;
        return r;
    }

    static UpdateCheckResult error(const char* detail, int code = 0) {
        UpdateCheckResult r{};
        r.success = false;
        r.updateAvailable = false;
        r.errorCode = code;
        if (detail) {
            strncpy_s(r.errorDetail, sizeof(r.errorDetail), detail, _TRUNCATE);
        }
        return r;
    }
};

// ============================================================================
// Update Callback — function pointer, NOT std::function
// ============================================================================

typedef void (*UpdateCheckCallback)(const UpdateCheckResult* result, void* userData);

// ============================================================================
// AutoUpdateSystem — Singleton
// ============================================================================

class AutoUpdateSystem {
public:
    static AutoUpdateSystem& instance();

    // Set the current application version
    void setCurrentVersion(uint32_t major, uint32_t minor, uint32_t patch, uint32_t build = 0);

    // Set the GitHub repository (e.g., "ItsMehRAWRXD/RawrXD")
    void setRepository(const char* owner, const char* repo);

    // Check for updates synchronously (blocks)
    UpdateCheckResult checkForUpdates();

    // Check for updates asynchronously (background thread)
    // The callback is invoked on the background thread — caller must
    // PostMessage or similar to marshal back to the UI thread.
    void checkForUpdatesAsync(UpdateCheckCallback callback, void* userData);

    // Parse a "vX.Y.Z" or "X.Y.Z" tag into VersionInfo
    static bool parseVersionTag(const char* tag, VersionInfo* out);

    // Get the current version
    VersionInfo getCurrentVersion() const { return m_currentVersion; }

    // Get the last check result (valid after checkForUpdates)
    const UpdateCheckResult& getLastResult() const { return m_lastResult; }

    // Was the check performed this session?
    bool wasChecked() const { return m_checked; }

    // Open the download URL in the default browser
    static void openDownloadUrl(const char* url);

private:
    AutoUpdateSystem();
    ~AutoUpdateSystem();
    AutoUpdateSystem(const AutoUpdateSystem&) = delete;
    AutoUpdateSystem& operator=(const AutoUpdateSystem&) = delete;

    // WinHTTP GET request
    UpdateCheckResult httpGet(const wchar_t* host, const wchar_t* path,
                               char* outBuffer, size_t outBufferSize);

    // Parse GitHub releases JSON response (manual parser — no nlohmann for this module)
    UpdateCheckResult parseReleasesJson(const char* json, size_t jsonLen);

    VersionInfo         m_currentVersion;
    char                m_repoOwner[64];
    char                m_repoName[64];
    UpdateCheckResult   m_lastResult;
    bool                m_checked;
    CRITICAL_SECTION    m_cs;
};

} // namespace Update
} // namespace RawrXD
