#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>

/**
 * @file Win32IDE_ModelLoaderBridge.cpp
 * @brief Batch 4 (30/118): Model Loader to UI Bridge.
 * Updates the Speciator UI (Batch 2) with loading progress.
 */

namespace RawrXD::Models::Bridge {

// Resolves: Loader_OnProgressUpdate
extern "C" void Loader_OnProgressUpdate(float percent) {
    // Clamp to valid range
    if (percent < 0.0f) percent = 0.0f;
    if (percent > 100.0f) percent = 100.0f;
    int pct = static_cast<int>(percent);
    LOG_INFO("[Loader] Progress: %d%%", pct);
    // Post to the main UI thread — WM_APP+50 is the model-load progress message
    HWND hwndMain = FindWindowA("RawrXDMainWindow", nullptr);
    if (hwndMain) {
        PostMessageA(hwndMain, WM_APP + 50, static_cast<WPARAM>(pct), 0);
    }
}

// Resolves: Loader_OnFatalError
extern "C" void Loader_OnFatalError(const char* error_msg) {
    LOG_ERROR("[Loader] FATAL: " + std::string(error_msg));
}

} // namespace RawrXD::Models::Bridge
