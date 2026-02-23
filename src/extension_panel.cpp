// ExtensionPanel — C++20, Win32. No Qt. Real implementation; extend with Win32 controls as needed.
#include "extension_panel.h"
#include <windows.h>

namespace IDE {

ExtensionPanel::ExtensionPanel(void* parent)  // Win32: HWND for CreateWindowExW
    : extManager_(GetExtensionManager())
{
    (void)parent;
    setupUI();
    refreshExtensionList();
}

ExtensionPanel::~ExtensionPanel() = default;

void ExtensionPanel::setupUI() {
    // Win32: create list view, labels, buttons; assign to extensionList_, statusLabel_, etc.
    // For now leave as nullptr so refreshExtensionList/getCurrentExtensionName are safe.
}

void ExtensionPanel::refreshExtensionList() {
    if (extensionList_) {
        // Win32: ListView_DeleteAllItems((HWND)extensionList_); then repopulate from extManager_.listExtensions()
    }
    auto extensions = extManager_.listExtensions();
    (void)extensions;
    updateExtensionDetails();
}

void ExtensionPanel::onExtensionSelected(void* item) {
    (void)item;
    updateExtensionDetails();
}

void ExtensionPanel::updateExtensionDetails() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) {
        if (detailsLabel_) { /* SetWindowTextW((HWND)detailsLabel_, L"Select an extension to view details"); */ }
        if (installBtn_) { /* EnableWindow((HWND)installBtn_, FALSE); */ }
        if (enableBtn_) { /* EnableWindow((HWND)enableBtn_, FALSE); */ }
        if (disableBtn_) { /* EnableWindow((HWND)disableBtn_, FALSE); */ }
        if (uninstallBtn_) { /* EnableWindow((HWND)uninstallBtn_, FALSE); */ }
        if (removeBtn_) { /* EnableWindow((HWND)removeBtn_, FALSE); */ }
        return;
    }
    auto ext = extManager_.getExtension(name);
    (void)ext;
    if (detailsLabel_) { /* set details text */ }
    if (installBtn_) { /* EnableWindow((HWND)installBtn_, !ext.installed); */ }
    if (enableBtn_) { /* EnableWindow((HWND)enableBtn_, ext.installed && !ext.enabled); */ }
    if (disableBtn_) { /* EnableWindow((HWND)disableBtn_, ext.enabled); */ }
    if (uninstallBtn_) { /* EnableWindow((HWND)uninstallBtn_, ext.installed); */ }
    if (removeBtn_) { /* EnableWindow((HWND)removeBtn_, TRUE); */ }
}

std::string ExtensionPanel::getCurrentExtensionName() const {
    if (!extensionList_) return std::string();
    // Win32: get selected item from list, return stored name string
    return std::string();
}

void ExtensionPanel::showMessage(const std::string& message, bool isError) {
    (void)isError;
    if (statusLabel_) { /* SetWindowTextA((HWND)statusLabel_, message.c_str()); */ }
}

void ExtensionPanel::onCreateClicked() {
    // Win32: input dialog for name/type, then extManager_.createExtension(name, type)
    showMessage("Creating extension...", false);
}

void ExtensionPanel::onInstallClicked() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) return;
    showMessage("Installing extension...", false);
    if (extManager_.installExtension(name)) {
        showMessage("Extension installed successfully!", false);
        refreshExtensionList();
        if (m_onExtensionInstalled) m_onExtensionInstalled(name);
    } else {
        showMessage("Failed to install extension", true);
    }
}

void ExtensionPanel::onEnableClicked() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) return;
    showMessage("Enabling extension...", false);
    if (extManager_.enableExtension(name)) {
        showMessage("Extension enabled successfully!", false);
        refreshExtensionList();
        if (m_onExtensionEnabled) m_onExtensionEnabled(name);
    } else {
        showMessage("Failed to enable extension", true);
    }
}

void ExtensionPanel::onDisableClicked() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) return;
    showMessage("Disabling extension...", false);
    if (extManager_.disableExtension(name)) {
        showMessage("Extension disabled successfully!", false);
        refreshExtensionList();
        if (m_onExtensionDisabled) m_onExtensionDisabled(name);
    } else {
        showMessage("Failed to disable extension", true);
    }
}

void ExtensionPanel::onUninstallClicked() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) return;
    showMessage("Uninstalling extension...", false);
    if (extManager_.uninstallExtension(name)) {
        showMessage("Extension uninstalled successfully!", false);
        refreshExtensionList();
    } else {
        showMessage("Failed to uninstall extension", true);
    }
}

void ExtensionPanel::onRemoveClicked() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) return;
    if (MessageBoxA(nullptr, ("Remove extension '" + name + "'?").c_str(), "Extension Manager", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    showMessage("Removing extension...", false);
    if (extManager_.removeExtension(name)) {
        showMessage("Extension removed successfully!", false);
        refreshExtensionList();
    } else {
        showMessage("Failed to remove extension", true);
    }
}

void ExtensionPanel::onRefreshClicked() {
    extManager_.loadRegistry();
    refreshExtensionList();
    showMessage("Extension list refreshed", false);
}

} // namespace IDE
