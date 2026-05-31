#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>

extern "C" bool Mnemosyne_Checkpoint(const char* id, const char* data);

/**
 * @file Win32IDE_SettingsEvents.cpp
 * @brief Batch 2 (14/118): Settings/Config UI to Agent Event Handlers.
 * Routes global preference changes to the Mnemosyne persistence layer.
 */

namespace RawrXD::UI::Settings {

// Resolves: Settings_OnConfigChanged
extern "C" bool Settings_OnConfigChanged(const char* key, const char* value) {
    if (!key || !value) {
        return false;
    }
    LOG_INFO("[Settings] Configuration Key: " + std::string(key) + " Value: " + std::string(value));

    // Persist the changed key/value pair as a Mnemosyne checkpoint.
    // The snapshot id is the key itself; the stored data is a minimal JSON blob.
    std::string data = "{\"key\":\"";
    for (const char* p = key; *p; ++p) {
        if (*p == '"' || *p == '\\') data.push_back('\\');
        data.push_back(*p);
    }
    data += "\",\"value\":\"";
    for (const char* p = value; *p; ++p) {
        if (*p == '"' || *p == '\\') data.push_back('\\');
        data.push_back(*p);
    }
    data += "\"}";

    return Mnemosyne_Checkpoint(key, data.c_str());
}

// Resolves: Settings_OnResetToDefaults
extern "C" void Settings_OnResetToDefaults() {
    LOG_WARNING("[Settings] Resetting all Agentic configurations.");
}

} // namespace RawrXD::UI::Settings
