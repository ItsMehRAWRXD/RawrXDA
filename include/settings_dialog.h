<<<<<<< HEAD
#pragma once

// C++20 / Win32. Settings dialog; no Qt. Use HWND/Modal in impl.

#include <string>

#include "settings_manager.h"

class SettingsDialog
{
public:
    SettingsDialog() = default;
    void initialize();

    /** Show modal; returns when closed. Win32: DialogBox or similar. */
    void showModal(void* parent = nullptr);

    void* getDialogHandle() const { return m_dialogHandle; }

private:
    void saveSettings();
    void applySettings();
    void loadSettings();
    void manageEncryptionKeys();
    void configureTokenizer();
    void configureCIPipeline();
    void setupUI();

    void* m_dialogHandle = nullptr;
    SettingsManager* m_settings = nullptr;
};
=======
#pragma once

// C++20 / Win32. Settings dialog; no Qt. Use HWND/Modal in impl.

#include <string>

#include "settings_manager.h"

class SettingsDialog
{
public:
    SettingsDialog() = default;
    void initialize();

    /** Show modal; returns when closed. Win32: DialogBox or similar. */
    void showModal(void* parent = nullptr);

    void* getDialogHandle() const { return m_dialogHandle; }

private:
    void saveSettings();
    void applySettings();
    void loadSettings();
    void manageEncryptionKeys();
    void configureTokenizer();
    void configureCIPipeline();
    void setupUI();

    void* m_dialogHandle = nullptr;
    SettingsManager* m_settings = nullptr;
};
>>>>>>> origin/main
