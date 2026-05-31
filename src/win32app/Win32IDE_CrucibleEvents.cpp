#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>

/**
 * @file Win32IDE_CrucibleEvents.cpp
 * @brief Batch 2 (9/118): Crucible Panel to Agent Event Handlers.
 * Routes UI-driven high-velocity inference requests to the Cortex.
 */

namespace RawrXD::UI::Crucible {

// Resolves: Crucible_OnRequestInference
extern "C" void Crucible_OnRequestInference(const char* prompt, int priority) {
    if (!prompt || prompt[0] == '\0') {
        LOG_WARN("[Crucible] Empty inference request, ignoring.");
        return;
    }
    std::string p(prompt);
    LOG_INFO("[Crucible] UI requested inference (pri=%d): %s...", priority, p.substr(0, 32).c_str());

    // Queue to the Win32IDE inference dispatch via PostMessage
    HWND hwnd = FindWindowA("RawrXD_MainClass", nullptr);
    if (hwnd) {
        // WM_APP+200 = inference request; wParam=priority, lParam=allocated prompt copy
        char* copy = _strdup(prompt);
        if (copy) {
            PostMessageA(hwnd, WM_APP + 200, static_cast<WPARAM>(priority),
                         reinterpret_cast<LPARAM>(copy));
            LOG_INFO("[Crucible] Inference request posted to main thread.");
        }
    } else {
        LOG_WARN("[Crucible] Main window not found, inference request dropped.");
    }
}

// Resolves: Crucible_OnCancelInference
extern "C" void Crucible_OnCancelInference(const char* request_id) {
    if (!request_id || request_id[0] == '\0') {
        LOG_WARN("[Crucible] Cancel requested with empty ID, ignoring.");
        return;
    }
    LOG_WARN("[Crucible] UI requested cancellation for ID: %s", request_id);

    // Post cancellation message to main thread
    HWND hwnd = FindWindowA("RawrXD_MainClass", nullptr);
    if (hwnd) {
        // WM_APP+201 = inference cancel; lParam=allocated ID copy
        char* copy = _strdup(request_id);
        if (copy) {
            PostMessageA(hwnd, WM_APP + 201, 0, reinterpret_cast<LPARAM>(copy));
            LOG_INFO("[Crucible] Cancellation posted for ID: %s", request_id);
        }
    }
}

} // namespace RawrXD::UI::Crucible
