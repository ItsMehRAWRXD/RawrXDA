#include "extension_panel.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QFont>

namespace IDE {

ExtensionPanel::ExtensionPanel(QWidget* parent)
    : QWidget(parent),
      extManager_(GetExtensionManager())
{
    setupUI();
    refreshExtensionList();
}

ExtensionPanel::~ExtensionPanel() = default;

void ExtensionPanel::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);

    // Title
    auto* titleLabel = new QLabel("Extension Manager", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // Status label
    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setStyleSheet("color: green;");
    mainLayout->addWidget(statusLabel_);

    // Extension list
    auto* listGroup = new QGroupBox("Installed Extensions", this);
    auto* listLayout = new QVBoxLayout(listGroup);
    
    extensionList_ = new QListWidget(this);
    connect(extensionList_, &QListWidget::itemClicked, this, &ExtensionPanel::onExtensionSelected);
    listLayout->addWidget(extensionList_);
    
    mainLayout->addWidget(listGroup);

    // Details
    auto* detailsGroup = new QGroupBox("Extension Details", this);
    auto* detailsLayout = new QVBoxLayout(detailsGroup);
    
    detailsLabel_ = new QLabel("Select an extension to view details", this);
    detailsLabel_->setWordWrap(true);
    detailsLayout->addWidget(detailsLabel_);
    
    mainLayout->addWidget(detailsGroup);

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    
    createBtn_ = new QPushButton("Create", this);
    installBtn_ = new QPushButton("Install", this);
    enableBtn_ = new QPushButton("Enable", this);
    disableBtn_ = new QPushButton("Disable", this);
    uninstallBtn_ = new QPushButton("Uninstall", this);
    removeBtn_ = new QPushButton("Remove", this);
    refreshBtn_ = new QPushButton("Refresh", this);

    connect(createBtn_, &QPushButton::clicked, this, &ExtensionPanel::onCreateClicked);
    connect(installBtn_, &QPushButton::clicked, this, &ExtensionPanel::onInstallClicked);
    connect(enableBtn_, &QPushButton::clicked, this, &ExtensionPanel::onEnableClicked);
    connect(disableBtn_, &QPushButton::clicked, this, &ExtensionPanel::onDisableClicked);
    connect(uninstallBtn_, &QPushButton::clicked, this, &ExtensionPanel::onUninstallClicked);
    connect(removeBtn_, &QPushButton::clicked, this, &ExtensionPanel::onRemoveClicked);
    connect(refreshBtn_, &QPushButton::clicked, this, &ExtensionPanel::onRefreshClicked);

    btnLayout->addWidget(createBtn_);
    btnLayout->addWidget(installBtn_);
    btnLayout->addWidget(enableBtn_);
    btnLayout->addWidget(disableBtn_);
    btnLayout->addWidget(uninstallBtn_);
    btnLayout->addWidget(removeBtn_);
    btnLayout->addStretch();
    btnLayout->addWidget(refreshBtn_);

    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
}

void ExtensionPanel::refreshExtensionList() {
    extensionList_->clear();
    
    auto extensions = extManager_.listExtensions();
    for (const auto& ext : extensions) {
        QString icon;
        if (ext.enabled) {
            icon = "🟢 [ON]  ";
        } else if (ext.installed) {
            icon = "🟡 [OFF] ";
        } else {
            icon = "⚪ [NEW] ";
        }
        
        QString itemText = icon + QString::fromStdString(ext.name) + 
                          " (" + QString::fromStdString(ext.type) + ")";
        
        auto* item = new QListWidgetItem(itemText);
        item->setData(Qt::UserRole, QString::fromStdString(ext.name));
        extensionList_->addItem(item);
    }
    
    updateExtensionDetails();
}

void ExtensionPanel::onExtensionSelected(QListWidgetItem* item) {
    Q_UNUSED(item);
    updateExtensionDetails();
}

void ExtensionPanel::updateExtensionDetails() {
    QString name = getCurrentExtensionName();
    if (name.isEmpty()) {
        detailsLabel_->setText("Select an extension to view details");
        installBtn_->setEnabled(false);
        enableBtn_->setEnabled(false);
        disableBtn_->setEnabled(false);
        uninstallBtn_->setEnabled(false);
        removeBtn_->setEnabled(false);
        return;
    }

    auto ext = extManager_.getExtension(name.toStdString());
    
    QString details = QString("<b>Name:</b> %1<br>").arg(name);
    details += QString("<b>Type:</b> %1<br>").arg(QString::fromStdString(ext.type));
    details += QString("<b>Status:</b> %1<br>")
        .arg(ext.enabled ? "Enabled" : (ext.installed ? "Installed" : "Created"));
    details += QString("<b>Path:</b> %1<br>").arg(QString::fromStdString(ext.path));
    
    detailsLabel_->setText(details);

    // Update button states
    installBtn_->setEnabled(!ext.installed);
    enableBtn_->setEnabled(ext.installed && !ext.enabled);
    disableBtn_->setEnabled(ext.enabled);
    uninstallBtn_->setEnabled(ext.installed);
    removeBtn_->setEnabled(true);
}

QString ExtensionPanel::getCurrentExtensionName() const {
    auto* item = extensionList_->currentItem();
    if (!item) return QString();
    
    return item->data(Qt::UserRole).toString();
}

void ExtensionPanel::showMessage(const QString& message, bool isError) {
    statusLabel_->setText(message);
    statusLabel_->setStyleSheet(isError ? "color: red;" : "color: green;");
    
    if (isError) {
        QMessageBox::warning(this, "Extension Manager", message);
    }
}

void ExtensionPanel::onCreateClicked() {
    bool ok;
    QString name = QInputDialog::getText(this, "Create Extension",
                                        "Extension name:", QLineEdit::Normal,
                                        "", &ok);
    if (!ok || name.isEmpty()) return;

    QStringList types = {"Custom", "Translator", "Analyzer", "Connector", "Generator", "Processor"};
    QString type = QInputDialog::getItem(this, "Create Extension",
                                        "Extension type:", types,
                                        0, false, &ok);
    if (!ok) return;

    showMessage("Creating extension...", false);
    
    if (extManager_.createExtension(name.toStdString(), type.toStdString())) {
        showMessage("Extension created successfully!", false);
        refreshExtensionList();
    } else {
        showMessage("Failed to create extension", true);
    }
}

void ExtensionPanel::onInstallClicked() {
    QString name = getCurrentExtensionName();
    if (name.isEmpty()) return;

    showMessage("Installing extension...", false);
    
    if (extManager_.installExtension(name.toStdString())) {
        showMessage("Extension installed successfully!", false);
        refreshExtensionList();
        emit extensionInstalled(name);
    } else {
        showMessage("Failed to install extension", true);
    }
}

void ExtensionPanel::onEnableClicked() {
    QString name = getCurrentExtensionName();
    if (name.isEmpty()) return;

    showMessage("Enabling extension...", false);
    
    if (extManager_.enableExtension(name.toStdString())) {
        showMessage("Extension enabled successfully!", false);
        refreshExtensionList();
        emit extensionEnabled(name);
    } else {
        showMessage("Failed to enable extension", true);
    }
}

void ExtensionPanel::onDisableClicked() {
    QString name = getCurrentExtensionName();
    if (name.isEmpty()) return;

    showMessage("Disabling extension...", false);
    
    if (extManager_.disableExtension(name.toStdString())) {
        showMessage("Extension disabled successfully!", false);
        refreshExtensionList();
        emit extensionDisabled(name);
    } else {
        showMessage("Failed to disable extension", true);
    }
}

void ExtensionPanel::onUninstallClicked() {
    QString name = getCurrentExtensionName();
    if (name.isEmpty()) return;

    showMessage("Uninstalling extension...", false);
    
    if (extManager_.uninstallExtension(name.toStdString())) {
        showMessage("Extension uninstalled successfully!", false);
        refreshExtensionList();
    } else {
        showMessage("Failed to uninstall extension", true);
    }
}

void ExtensionPanel::onRemoveClicked() {
    QString name = getCurrentExtensionName();
    if (name.isEmpty()) return;

    auto reply = QMessageBox::question(this, "Remove Extension",
                                      QString("Are you sure you want to remove '%1'?").arg(name),
                                      QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;

    showMessage("Removing extension...", false);
    
    if (extManager_.removeExtension(name.toStdString())) {
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
