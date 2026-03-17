#ifndef EXTENSION_PANEL_H
#define EXTENSION_PANEL_H

// ============================================================================
// ExtensionPanel — C++20, Win32. No Qt. (QWidget, QListWidget, signals removed)
// ============================================================================

#include <functional>
#include <string>
#include "extension_manager.h"

namespace IDE {

class ExtensionPanel {
public:
    using ExtensionEnabledFn = std::function<void(const std::string&)>;
    using ExtensionDisabledFn = std::function<void(const std::string&)>;
    using ExtensionInstalledFn = std::function<void(const std::string&)>;

    explicit ExtensionPanel(void* parent = nullptr);
    ~ExtensionPanel();

    void refreshExtensionList();

    void setOnExtensionEnabled(ExtensionEnabledFn fn) { m_onExtensionEnabled = std::move(fn); }
    void setOnExtensionDisabled(ExtensionDisabledFn fn) { m_onExtensionDisabled = std::move(fn); }
    void setOnExtensionInstalled(ExtensionInstalledFn fn) { m_onExtensionInstalled = std::move(fn); }

private:
    void onExtensionSelected(void* item);
    void onCreateClicked();
    void onInstallClicked();
    void onEnableClicked();
    void onDisableClicked();
    void onUninstallClicked();
    void onRemoveClicked();
    void onRefreshClicked();
    void setupUI();
    void updateExtensionDetails();
    std::string getCurrentExtensionName() const;
    void showMessage(const std::string& message, bool isError = false);

    void* extensionList_ = nullptr;
    void* statusLabel_ = nullptr;
    void* detailsLabel_ = nullptr;
    void* createBtn_ = nullptr;
    void* installBtn_ = nullptr;
    void* enableBtn_ = nullptr;
    void* disableBtn_ = nullptr;
    void* uninstallBtn_ = nullptr;
    void* removeBtn_ = nullptr;
    void* refreshBtn_ = nullptr;

    ExtensionManager& extManager_;
    ExtensionEnabledFn m_onExtensionEnabled;
    ExtensionDisabledFn m_onExtensionDisabled;
    ExtensionInstalledFn m_onExtensionInstalled;
};

} // namespace IDE

#endif // EXTENSION_PANEL_H
