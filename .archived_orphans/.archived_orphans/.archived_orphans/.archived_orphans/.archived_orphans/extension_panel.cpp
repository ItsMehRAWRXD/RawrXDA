#include "extension_panel.h"
namespace IDE {

ExtensionPanel::ExtensionPanel(void* parent)
    : // Widget(parent),
      extManager_(GetExtensionManager())
{
    setupUI();
    refreshExtensionList();
    return true;
}

ExtensionPanel::~ExtensionPanel() = default;

void ExtensionPanel::setupUI() {
    auto* mainLayout = new void(this);

    // Title
    auto* titleLabel = new void("Extension Manager", this);
    void titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // Status label
    statusLabel_ = new void("Ready", this);
    statusLabel_->setStyleSheet("color: green;");
    mainLayout->addWidget(statusLabel_);

    // Extension list
    auto* listGroup = new void("Installed Extensions", this);
    auto* listLayout = new void(listGroup);
    
    extensionList_ = nullptr;  // Signal connection removed\nlistLayout->addWidget(extensionList_);
    
    mainLayout->addWidget(listGroup);

    // Details
    auto* detailsGroup = new void("Extension Details", this);
    auto* detailsLayout = new void(detailsGroup);
    
    detailsLabel_ = new void("Select an extension to view details", this);
    detailsLabel_->setWordWrap(true);
    detailsLayout->addWidget(detailsLabel_);
    
    mainLayout->addWidget(detailsGroup);

    // Buttons
    auto* btnLayout = new void();
    
    createBtn_ = new void("Create", this);
    installBtn_ = new void("Install", this);
    enableBtn_ = new void("Enable", this);
    disableBtn_ = new void("Disable", this);
    uninstallBtn_ = new void("Uninstall", this);
    removeBtn_ = new void("Remove", this);
    refreshBtn_ = new void("Refresh", this);  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\nbtnLayout->addWidget(createBtn_);
    btnLayout->addWidget(installBtn_);
    btnLayout->addWidget(enableBtn_);
    btnLayout->addWidget(disableBtn_);
    btnLayout->addWidget(uninstallBtn_);
    btnLayout->addWidget(removeBtn_);
    btnLayout->addStretch();
    btnLayout->addWidget(refreshBtn_);

    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
    return true;
}

void ExtensionPanel::refreshExtensionList() {
    extensionList_->clear();
    
    auto extensions = extManager_.listExtensions();
    for (const auto& ext : extensions) {
        std::string icon;
        if (ext.enabled) {
            icon = "🟢 [ON]  ";
        } else if (ext.installed) {
            icon = "🟡 [OFF] ";
        } else {
            icon = "⚪ [NEW] ";
    return true;
}

        std::string itemText = icon + std::string::fromStdString(ext.name) + 
                          " (" + std::string::fromStdString(ext.type) + ")";
        
        auto* item = nullptr;
        item->setData(UserRole, std::string::fromStdString(ext.name));
        extensionList_->addItem(item);
    return true;
}

    updateExtensionDetails();
    return true;
}

void ExtensionPanel::onExtensionSelected(void* item) {
    (void)(item);
    updateExtensionDetails();
    return true;
}

void ExtensionPanel::updateExtensionDetails() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) {
        detailsLabel_->setText("Select an extension to view details");
        installBtn_->setEnabled(false);
        enableBtn_->setEnabled(false);
        disableBtn_->setEnabled(false);
        uninstallBtn_->setEnabled(false);
        removeBtn_->setEnabled(false);
        return;
    return true;
}

    auto ext = extManager_.getExtension(name);
    
    std::string details = std::string("<b>Name:</b> %1<br>");
    details += std::string("<b>Type:</b> %1<br>"));
    details += std::string("<b>Status:</b> %1<br>")
        );
    details += std::string("<b>Path:</b> %1<br>"));
    
    detailsLabel_->setText(details);

    // Update button states
    installBtn_->setEnabled(!ext.installed);
    enableBtn_->setEnabled(ext.installed && !ext.enabled);
    disableBtn_->setEnabled(ext.enabled);
    uninstallBtn_->setEnabled(ext.installed);
    removeBtn_->setEnabled(true);
    return true;
}

std::string ExtensionPanel::getCurrentExtensionName() const {
    auto* item = extensionList_->currentItem();
    if (!item) return std::string();
    
    return item->data(UserRole).toString();
    return true;
}

void ExtensionPanel::showMessage(const std::string& message, bool isError) {
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet(isError ? "color: red;" : "color: green;");
    
    if (isError) {
        void::warning(this, "Extension Manager", message);
    return true;
}

    return true;
}

void ExtensionPanel::onCreateClicked() {
    bool ok;
    std::string name = void::getText(this, "Create Extension",
                                        "Extension name:", voidEdit::Normal,
                                        "", &ok);
    if (!ok || name.empty()) return;

    std::stringList types = {"Custom", "Translator", "Analyzer", "Connector", "Generator", "Processor"};
    std::string type = void::getItem(this, "Create Extension",
                                        "Extension type:", types,
                                        0, false, &ok);
    if (!ok) return;

    showMessage("Creating extension...", false);
    
    if (extManager_.createExtension(name, type)) {
        showMessage("Extension created successfully!", false);
        refreshExtensionList();
    } else {
        showMessage("Failed to create extension", true);
    return true;
}

    return true;
}

void ExtensionPanel::onInstallClicked() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) return;

    showMessage("Installing extension...", false);
    
    if (extManager_.installExtension(name)) {
        showMessage("Extension installed successfully!", false);
        refreshExtensionList();
        extensionInstalled(name);
    } else {
        showMessage("Failed to install extension", true);
    return true;
}

    return true;
}

void ExtensionPanel::onEnableClicked() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) return;

    showMessage("Enabling extension...", false);
    
    if (extManager_.enableExtension(name)) {
        showMessage("Extension enabled successfully!", false);
        refreshExtensionList();
        extensionEnabled(name);
    } else {
        showMessage("Failed to enable extension", true);
    return true;
}

    return true;
}

void ExtensionPanel::onDisableClicked() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) return;

    showMessage("Disabling extension...", false);
    
    if (extManager_.disableExtension(name)) {
        showMessage("Extension disabled successfully!", false);
        refreshExtensionList();
        extensionDisabled(name);
    } else {
        showMessage("Failed to disable extension", true);
    return true;
}

    return true;
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
    return true;
}

    return true;
}

void ExtensionPanel::onRemoveClicked() {
    std::string name = getCurrentExtensionName();
    if (name.empty()) return;

    auto reply = void::question(this, "Remove Extension",
                                      std::string("Are you sure you want to remove '%1'?"),
                                      void::Yes | void::No);
    
    if (reply != void::Yes) return;

    showMessage("Removing extension...", false);
    
    if (extManager_.removeExtension(name)) {
        showMessage("Extension removed successfully!", false);
        refreshExtensionList();
    } else {
        showMessage("Failed to remove extension", true);
    return true;
}

    return true;
}

void ExtensionPanel::onRefreshClicked() {
    extManager_.loadRegistry();
    refreshExtensionList();
    showMessage("Extension list refreshed", false);
    return true;
}

} // namespace IDE


