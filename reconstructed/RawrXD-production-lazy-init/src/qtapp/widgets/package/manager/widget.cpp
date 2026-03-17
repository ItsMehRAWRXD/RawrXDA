/**
 * @file package_manager_widget.cpp
 * @brief Implementation of package manager widget
 */

#include "package_manager_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStyle>
#include <QCheckBox>
#include <QApplication>

// ============================================================================
// PackageInfo Implementation
// ============================================================================

QString PackageInfo::getSeverityColor() const {
    if (vulnerabilities.isEmpty()) return QString();
    
    // Find highest severity
    bool hasCritical = false, hasHigh = false, hasModerate = false;
    for (const auto& vuln : vulnerabilities) {
        if (vuln.severity == "critical") hasCritical = true;
        else if (vuln.severity == "high") hasHigh = true;
        else if (vuln.severity == "moderate") hasModerate = true;
    }
    
    if (hasCritical) return "#ff1744";
    if (hasHigh) return "#ff9100";
    if (hasModerate) return "#ffea00";
    return "#00e676";
}

// ============================================================================
// PackageManagerWidget Implementation
// ============================================================================

PackageManagerWidget::PackageManagerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();
}

PackageManagerWidget::~PackageManagerWidget() {
    if (m_currentProcess) {
        m_currentProcess->terminate();
        m_currentProcess->waitForFinished(3000);
    }
}

void PackageManagerWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    
    // Header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    m_ecosystemCombo = new QComboBox();
    m_ecosystemCombo->addItem("npm (Node.js)", static_cast<int>(PackageEcosystem::NPM));
    m_ecosystemCombo->addItem("pip (Python)", static_cast<int>(PackageEcosystem::Pip));
    m_ecosystemCombo->addItem("cargo (Rust)", static_cast<int>(PackageEcosystem::Cargo));
    m_ecosystemCombo->addItem("go mod (Go)", static_cast<int>(PackageEcosystem::Go));
    m_ecosystemCombo->addItem("NuGet (.NET)", static_cast<int>(PackageEcosystem::NuGet));
    m_ecosystemCombo->addItem("Composer (PHP)", static_cast<int>(PackageEcosystem::Composer));
    
    m_projectPathLabel = new QLabel(tr("No project loaded"));
    m_projectPathLabel->setStyleSheet("color: #888;");
    
    m_refreshBtn = new QPushButton(tr("Refresh"));
    m_refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    
    headerLayout->addWidget(new QLabel(tr("Package Manager:")));
    headerLayout->addWidget(m_ecosystemCombo);
    headerLayout->addWidget(m_projectPathLabel, 1);
    headerLayout->addWidget(m_refreshBtn);
    
    mainLayout->addLayout(headerLayout);
    
    // Tab widget
    m_tabWidget = new QTabWidget();
    
    setupInstalledTab();
    setupSearchTab();
    setupScriptsTab();
    setupAuditTab();
    
    mainLayout->addWidget(m_tabWidget, 1);
    
    // Output panel
    QGroupBox* outputGroup = new QGroupBox(tr("Output"));
    QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);
    
    m_outputPanel = new QTextEdit();
    m_outputPanel->setReadOnly(true);
    m_outputPanel->setMaximumHeight(120);
    m_outputPanel->setFont(QFont("Consolas", 9));
    m_outputPanel->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");
    
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 0);  // Indeterminate
    m_progressBar->hide();
    m_statusLabel = new QLabel(tr("Ready"));
    
    statusLayout->addWidget(m_statusLabel, 1);
    statusLayout->addWidget(m_progressBar);
    
    outputLayout->addWidget(m_outputPanel);
    outputLayout->addLayout(statusLayout);
    
    mainLayout->addWidget(outputGroup);
}

void PackageManagerWidget::setupInstalledTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    
    m_showOutdatedCheck = new QCheckBox(tr("Outdated only"));
    m_showDevCheck = new QCheckBox(tr("Show dev"));
    m_showDevCheck->setChecked(true);
    
    m_updateAllBtn = new QPushButton(tr("Update All"));
    m_updateBtn = new QPushButton(tr("Update"));
    m_updateBtn->setEnabled(false);
    m_uninstallBtn = new QPushButton(tr("Uninstall"));
    m_uninstallBtn->setEnabled(false);
    
    m_packageCountLabel = new QLabel();
    
    toolbar->addWidget(m_showOutdatedCheck);
    toolbar->addWidget(m_showDevCheck);
    toolbar->addStretch();
    toolbar->addWidget(m_packageCountLabel);
    toolbar->addWidget(m_updateAllBtn);
    toolbar->addWidget(m_updateBtn);
    toolbar->addWidget(m_uninstallBtn);
    
    layout->addLayout(toolbar);
    
    // Packages tree
    m_packagesTree = new QTreeWidget();
    m_packagesTree->setHeaderLabels({
        tr("Package"), tr("Version"), tr("Latest"), tr("Type"), tr("License")
    });
    m_packagesTree->setAlternatingRowColors(true);
    m_packagesTree->setRootIsDecorated(true);
    m_packagesTree->setSortingEnabled(true);
    m_packagesTree->header()->setStretchLastSection(false);
    m_packagesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    
    layout->addWidget(m_packagesTree, 1);
    
    m_tabWidget->addTab(tab, tr("Installed"));
}

void PackageManagerWidget::setupSearchTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // Search bar
    QHBoxLayout* searchLayout = new QHBoxLayout();
    
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search packages..."));
    m_searchEdit->setClearButtonEnabled(true);
    
    m_searchBtn = new QPushButton(tr("Search"));
    m_searchBtn->setIcon(style()->standardIcon(QStyle::SP_FileDialogContentsView));
    
    searchLayout->addWidget(m_searchEdit, 1);
    searchLayout->addWidget(m_searchBtn);
    
    layout->addLayout(searchLayout);
    
    // Splitter with results and details
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    
    // Search results
    m_searchResultsTable = new QTableWidget();
    m_searchResultsTable->setColumnCount(4);
    m_searchResultsTable->setHorizontalHeaderLabels({
        tr("Package"), tr("Version"), tr("Downloads"), tr("Updated")
    });
    m_searchResultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_searchResultsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_searchResultsTable->setAlternatingRowColors(true);
    m_searchResultsTable->horizontalHeader()->setStretchLastSection(false);
    m_searchResultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_searchResultsTable->verticalHeader()->setVisible(false);
    
    splitter->addWidget(m_searchResultsTable);
    
    // Package details
    m_packageDetailsGroup = new QGroupBox(tr("Package Details"));
    QVBoxLayout* detailsLayout = new QVBoxLayout(m_packageDetailsGroup);
    
    m_detailsNameLabel = new QLabel();
    m_detailsNameLabel->setStyleSheet("font-weight: bold; font-size: 16px;");
    
    m_detailsVersionLabel = new QLabel();
    m_detailsDescLabel = new QLabel();
    m_detailsDescLabel->setWordWrap(true);
    m_detailsAuthorLabel = new QLabel();
    m_detailsLicenseLabel = new QLabel();
    m_detailsDownloadsLabel = new QLabel();
    
    QHBoxLayout* installLayout = new QHBoxLayout();
    m_versionCombo = new QComboBox();
    m_versionCombo->setMinimumWidth(100);
    m_devDependencyCheck = new QCheckBox(tr("Dev dependency"));
    m_installBtn = new QPushButton(tr("Install"));
    m_installBtn->setEnabled(false);
    
    installLayout->addWidget(new QLabel(tr("Version:")));
    installLayout->addWidget(m_versionCombo);
    installLayout->addWidget(m_devDependencyCheck);
    installLayout->addStretch();
    installLayout->addWidget(m_installBtn);
    
    detailsLayout->addWidget(m_detailsNameLabel);
    detailsLayout->addWidget(m_detailsVersionLabel);
    detailsLayout->addWidget(m_detailsDescLabel);
    detailsLayout->addWidget(m_detailsAuthorLabel);
    detailsLayout->addWidget(m_detailsLicenseLabel);
    detailsLayout->addWidget(m_detailsDownloadsLabel);
    detailsLayout->addStretch();
    detailsLayout->addLayout(installLayout);
    
    splitter->addWidget(m_packageDetailsGroup);
    splitter->setSizes({400, 300});
    
    layout->addWidget(splitter, 1);
    
    m_tabWidget->addTab(tab, tr("Search"));
}

void PackageManagerWidget::setupScriptsTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    
    m_runScriptBtn = new QPushButton(tr("Run Script"));
    m_runScriptBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_runScriptBtn->setEnabled(false);
    
    toolbar->addStretch();
    toolbar->addWidget(m_runScriptBtn);
    
    layout->addLayout(toolbar);
    
    // Splitter
    QSplitter* splitter = new QSplitter(Qt::Vertical);
    
    // Scripts tree
    m_scriptsTree = new QTreeWidget();
    m_scriptsTree->setHeaderLabels({tr("Script"), tr("Command")});
    m_scriptsTree->setAlternatingRowColors(true);
    m_scriptsTree->setRootIsDecorated(false);
    m_scriptsTree->header()->setStretchLastSection(true);
    m_scriptsTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    
    splitter->addWidget(m_scriptsTree);
    
    // Script output
    m_scriptOutput = new QTextEdit();
    m_scriptOutput->setReadOnly(true);
    m_scriptOutput->setFont(QFont("Consolas", 10));
    m_scriptOutput->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");
    
    splitter->addWidget(m_scriptOutput);
    splitter->setSizes({200, 300});
    
    layout->addWidget(splitter, 1);
    
    m_tabWidget->addTab(tab, tr("Scripts"));
}

void PackageManagerWidget::setupAuditTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    
    m_auditBtn = new QPushButton(tr("Run Audit"));
    m_auditBtn->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    
    m_fixVulnsBtn = new QPushButton(tr("Fix Vulnerabilities"));
    m_fixVulnsBtn->setEnabled(false);
    
    m_auditSummaryLabel = new QLabel();
    
    toolbar->addWidget(m_auditBtn);
    toolbar->addWidget(m_fixVulnsBtn);
    toolbar->addStretch();
    toolbar->addWidget(m_auditSummaryLabel);
    
    layout->addLayout(toolbar);
    
    // Audit results tree
    m_auditTree = new QTreeWidget();
    m_auditTree->setHeaderLabels({
        tr("Package"), tr("Severity"), tr("Vulnerability"), tr("Recommendation")
    });
    m_auditTree->setAlternatingRowColors(true);
    m_auditTree->setRootIsDecorated(true);
    m_auditTree->header()->setStretchLastSection(true);
    m_auditTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_auditTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    
    layout->addWidget(m_auditTree, 1);
    
    m_tabWidget->addTab(tab, tr("Security"));
}

void PackageManagerWidget::setupConnections() {
    connect(m_ecosystemCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PackageManagerWidget::onEcosystemChanged);
    connect(m_refreshBtn, &QPushButton::clicked, this, &PackageManagerWidget::refreshPackages);
    
    // Installed tab
    connect(m_packagesTree, &QTreeWidget::itemClicked,
            this, &PackageManagerWidget::onPackageSelected);
    connect(m_uninstallBtn, &QPushButton::clicked, this, &PackageManagerWidget::onUninstallClicked);
    connect(m_updateBtn, &QPushButton::clicked, this, &PackageManagerWidget::onUpdateClicked);
    connect(m_updateAllBtn, &QPushButton::clicked, this, &PackageManagerWidget::onUpdateAllClicked);
    connect(m_showOutdatedCheck, &QCheckBox::toggled,
            this, &PackageManagerWidget::onShowOutdatedToggled);
    connect(m_showDevCheck, &QCheckBox::toggled,
            this, &PackageManagerWidget::onShowDevToggled);
    
    // Search tab
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &PackageManagerWidget::onSearchClicked);
    connect(m_searchBtn, &QPushButton::clicked, this, &PackageManagerWidget::onSearchClicked);
    connect(m_searchResultsTable, &QTableWidget::itemClicked,
            this, &PackageManagerWidget::onSearchResultSelected);
    connect(m_searchResultsTable, &QTableWidget::itemDoubleClicked,
            this, &PackageManagerWidget::onSearchResultDoubleClicked);
    connect(m_installBtn, &QPushButton::clicked, this, &PackageManagerWidget::onInstallClicked);
    
    // Scripts tab
    connect(m_scriptsTree, &QTreeWidget::itemDoubleClicked,
            this, &PackageManagerWidget::onScriptDoubleClicked);
    connect(m_runScriptBtn, &QPushButton::clicked, [this]() {
        auto item = m_scriptsTree->currentItem();
        if (item) {
            runScript(item->text(0));
        }
    });
    connect(m_scriptsTree, &QTreeWidget::itemSelectionChanged, [this]() {
        m_runScriptBtn->setEnabled(m_scriptsTree->currentItem() != nullptr);
    });
    
    // Audit tab
    connect(m_auditBtn, &QPushButton::clicked, this, &PackageManagerWidget::onAuditClicked);
    connect(m_fixVulnsBtn, &QPushButton::clicked, this, &PackageManagerWidget::fixVulnerabilities);
}

void PackageManagerWidget::setProjectPath(const QString& path) {
    m_projectPath = path;
    m_projectPathLabel->setText(path);
    
    // Auto-detect ecosystem
    PackageEcosystem detected = detectEcosystem();
    int index = m_ecosystemCombo->findData(static_cast<int>(detected));
    if (index >= 0) {
        m_ecosystemCombo->setCurrentIndex(index);
    }
    
    refreshPackages();
}

PackageEcosystem PackageManagerWidget::detectEcosystem() const {
    QDir dir(m_projectPath);
    
    if (dir.exists("package.json")) return PackageEcosystem::NPM;
    if (dir.exists("requirements.txt") || dir.exists("Pipfile") || 
        dir.exists("pyproject.toml")) return PackageEcosystem::Pip;
    if (dir.exists("Cargo.toml")) return PackageEcosystem::Cargo;
    if (dir.exists("go.mod")) return PackageEcosystem::Go;
    if (dir.exists("*.csproj") || dir.exists("packages.config")) return PackageEcosystem::NuGet;
    if (dir.exists("composer.json")) return PackageEcosystem::Composer;
    
    return PackageEcosystem::NPM;  // Default
}

bool PackageManagerWidget::hasPackageFile() const {
    QDir dir(m_projectPath);
    
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM:
            return dir.exists("package.json");
        case PackageEcosystem::Pip:
            return dir.exists("requirements.txt") || dir.exists("Pipfile");
        case PackageEcosystem::Cargo:
            return dir.exists("Cargo.toml");
        case PackageEcosystem::Go:
            return dir.exists("go.mod");
        default:
            return false;
    }
}

void PackageManagerWidget::refreshPackages() {
    m_installedPackages.clear();
    m_packagesTree->clear();
    
    if (m_projectPath.isEmpty() || !hasPackageFile()) {
        m_packageCountLabel->setText(tr("No packages"));
        return;
    }
    
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM:
            parsePackageJson();
            break;
        case PackageEcosystem::Pip:
            parseRequirementsTxt();
            break;
        case PackageEcosystem::Cargo:
            parseCargoToml();
            break;
        default:
            break;
    }
    
    updatePackageList();
    loadScripts();
    emit packagesRefreshed();
}

void PackageManagerWidget::parsePackageJson() {
    QString packageJsonPath = m_projectPath + "/package.json";
    QFile file(packageJsonPath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Could not read package.json"));
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();
    
    // Parse dependencies
    auto parseDeps = [this](const QJsonObject& deps, bool isDev) {
        for (auto it = deps.begin(); it != deps.end(); ++it) {
            PackageInfo pkg;
            pkg.name = it.key();
            pkg.version = it.value().toString();
            pkg.isDev = isDev;
            pkg.ecosystem = PackageEcosystem::NPM;
            m_installedPackages[pkg.name] = pkg;
        }
    };
    
    if (root.contains("dependencies")) {
        parseDeps(root["dependencies"].toObject(), false);
    }
    if (root.contains("devDependencies")) {
        parseDeps(root["devDependencies"].toObject(), true);
    }
    
    // Parse scripts
    m_scripts.clear();
    if (root.contains("scripts")) {
        QJsonObject scripts = root["scripts"].toObject();
        for (auto it = scripts.begin(); it != scripts.end(); ++it) {
            PackageScript script;
            script.name = it.key();
            script.command = it.value().toString();
            m_scripts.append(script);
        }
    }
    
    // Get outdated info
    executeCommand("npm", {"outdated", "--json"}, [this](int exitCode, const QString& output) {
        Q_UNUSED(exitCode);
        QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
        QJsonObject outdated = doc.object();
        
        for (auto it = outdated.begin(); it != outdated.end(); ++it) {
            if (m_installedPackages.contains(it.key())) {
                QJsonObject info = it.value().toObject();
                m_installedPackages[it.key()].latestVersion = info["latest"].toString();
                m_installedPackages[it.key()].isOutdated = true;
            }
        }
        
        updatePackageList();
    });
}

void PackageManagerWidget::parseRequirementsTxt() {
    QString reqPath = m_projectPath + "/requirements.txt";
    QFile file(reqPath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        // Try Pipfile
        parsePipfile();
        return;
    }
    
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        
        // Skip comments and empty lines
        if (line.isEmpty() || line.startsWith('#')) continue;
        
        // Parse package==version or package>=version etc.
        QRegularExpression re("^([a-zA-Z0-9_-]+)([<>=!]+)?(.+)?$");
        QRegularExpressionMatch match = re.match(line);
        
        if (match.hasMatch()) {
            PackageInfo pkg;
            pkg.name = match.captured(1);
            pkg.version = match.captured(3);
            pkg.ecosystem = PackageEcosystem::Pip;
            m_installedPackages[pkg.name] = pkg;
        }
    }
}

void PackageManagerWidget::parsePipfile() {
    QString pipfilePath = m_projectPath + "/Pipfile";
    QFile file(pipfilePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    // Simple TOML parsing for Pipfile
    QTextStream stream(&file);
    bool inPackages = false;
    bool inDevPackages = false;
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        
        if (line == "[packages]") {
            inPackages = true;
            inDevPackages = false;
            continue;
        } else if (line == "[dev-packages]") {
            inPackages = false;
            inDevPackages = true;
            continue;
        } else if (line.startsWith('[')) {
            inPackages = false;
            inDevPackages = false;
            continue;
        }
        
        if (inPackages || inDevPackages) {
            QRegularExpression re("^([a-zA-Z0-9_-]+)\\s*=\\s*\"?([^\"]+)\"?$");
            QRegularExpressionMatch match = re.match(line);
            
            if (match.hasMatch()) {
                PackageInfo pkg;
                pkg.name = match.captured(1);
                pkg.version = match.captured(2);
                pkg.isDev = inDevPackages;
                pkg.ecosystem = PackageEcosystem::Pip;
                m_installedPackages[pkg.name] = pkg;
            }
        }
    }
}

void PackageManagerWidget::parseCargoToml() {
    QString cargoPath = m_projectPath + "/Cargo.toml";
    QFile file(cargoPath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QTextStream stream(&file);
    bool inDependencies = false;
    bool inDevDependencies = false;
    
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        
        if (line == "[dependencies]") {
            inDependencies = true;
            inDevDependencies = false;
            continue;
        } else if (line == "[dev-dependencies]") {
            inDependencies = false;
            inDevDependencies = true;
            continue;
        } else if (line.startsWith('[')) {
            inDependencies = false;
            inDevDependencies = false;
            continue;
        }
        
        if (inDependencies || inDevDependencies) {
            // Handle both simple and table forms
            QRegularExpression re("^([a-zA-Z0-9_-]+)\\s*=\\s*(?:\"([^\"]+)\"|\\{[^}]*version\\s*=\\s*\"([^\"]+)\"[^}]*\\})$");
            QRegularExpressionMatch match = re.match(line);
            
            if (match.hasMatch()) {
                PackageInfo pkg;
                pkg.name = match.captured(1);
                pkg.version = match.captured(2).isEmpty() ? match.captured(3) : match.captured(2);
                pkg.isDev = inDevDependencies;
                pkg.ecosystem = PackageEcosystem::Cargo;
                m_installedPackages[pkg.name] = pkg;
            }
        }
    }
}

void PackageManagerWidget::updatePackageList() {
    m_packagesTree->clear();
    
    // Group by type
    QTreeWidgetItem* depsItem = new QTreeWidgetItem(m_packagesTree);
    depsItem->setText(0, tr("Dependencies"));
    depsItem->setExpanded(true);
    
    QTreeWidgetItem* devDepsItem = new QTreeWidgetItem(m_packagesTree);
    devDepsItem->setText(0, tr("Dev Dependencies"));
    devDepsItem->setExpanded(true);
    
    int totalCount = 0;
    int outdatedCount = 0;
    
    for (const auto& pkg : m_installedPackages) {
        // Apply filters
        if (m_showOutdatedCheck->isChecked() && !pkg.isOutdated) continue;
        if (!m_showDevCheck->isChecked() && pkg.isDev) continue;
        
        QTreeWidgetItem* parent = pkg.isDev ? devDepsItem : depsItem;
        QTreeWidgetItem* item = new QTreeWidgetItem(parent);
        
        item->setText(0, pkg.name);
        item->setText(1, pkg.version);
        item->setText(2, pkg.latestVersion.isEmpty() ? pkg.version : pkg.latestVersion);
        item->setText(3, pkg.isDev ? tr("dev") : tr("prod"));
        item->setText(4, pkg.license);
        item->setData(0, Qt::UserRole, pkg.name);
        
        // Highlight outdated
        if (pkg.isOutdated) {
            item->setForeground(2, QColor("#ff9100"));
            item->setIcon(0, style()->standardIcon(QStyle::SP_MessageBoxWarning));
            outdatedCount++;
        }
        
        // Show vulnerability indicator
        if (pkg.hasVulnerabilities()) {
            item->setBackground(0, QColor(pkg.getSeverityColor()).lighter(180));
        }
        
        totalCount++;
    }
    
    // Remove empty groups
    if (depsItem->childCount() == 0) {
        delete depsItem;
    }
    if (devDepsItem->childCount() == 0) {
        delete devDepsItem;
    }
    
    m_packageCountLabel->setText(tr("%1 packages (%2 outdated)")
        .arg(totalCount).arg(outdatedCount));
}

void PackageManagerWidget::installPackage(const QString& name, const QString& version, bool isDev) {
    m_statusLabel->setText(tr("Installing %1...").arg(name));
    m_progressBar->show();
    
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM:
            npmInstall(name, version, isDev);
            break;
        case PackageEcosystem::Pip:
            pipInstall(name, version);
            break;
        case PackageEcosystem::Cargo:
            cargoInstall(name, version);
            break;
        default:
            break;
    }
}

void PackageManagerWidget::uninstallPackage(const QString& name) {
    m_statusLabel->setText(tr("Uninstalling %1...").arg(name));
    m_progressBar->show();
    
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM:
            npmUninstall(name);
            break;
        case PackageEcosystem::Pip:
            pipUninstall(name);
            break;
        case PackageEcosystem::Cargo:
            cargoUninstall(name);
            break;
        default:
            break;
    }
}

void PackageManagerWidget::updatePackage(const QString& name) {
    m_statusLabel->setText(tr("Updating %1...").arg(name));
    m_progressBar->show();
    
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM:
            npmUpdate(name);
            break;
        case PackageEcosystem::Pip:
            pipUpdate(name);
            break;
        case PackageEcosystem::Cargo:
            cargoUpdate(name);
            break;
        default:
            break;
    }
}

void PackageManagerWidget::updateAllPackages() {
    m_statusLabel->setText(tr("Updating all packages..."));
    m_progressBar->show();
    
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM:
            executeCommand("npm", {"update"}, [this](int code, const QString& output) {
                Q_UNUSED(output);
                m_progressBar->hide();
                m_statusLabel->setText(code == 0 ? tr("All packages updated") : tr("Update failed"));
                refreshPackages();
            });
            break;
        case PackageEcosystem::Pip:
            // pip doesn't have a native update-all command
            break;
        case PackageEcosystem::Cargo:
            executeCommand("cargo", {"update"}, [this](int code, const QString& output) {
                Q_UNUSED(output);
                m_progressBar->hide();
                m_statusLabel->setText(code == 0 ? tr("All packages updated") : tr("Update failed"));
                refreshPackages();
            });
            break;
        default:
            break;
    }
}

void PackageManagerWidget::searchPackages(const QString& query) {
    if (query.isEmpty()) return;
    
    m_statusLabel->setText(tr("Searching for '%1'...").arg(query));
    m_progressBar->show();
    
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM:
            npmSearch(query);
            break;
        case PackageEcosystem::Pip:
            pipSearch(query);
            break;
        case PackageEcosystem::Cargo:
            cargoSearch(query);
            break;
        default:
            break;
    }
}

// npm operations
void PackageManagerWidget::npmInstall(const QString& name, const QString& version, bool isDev) {
    QStringList args = {"install"};
    
    QString pkg = version.isEmpty() ? name : QString("%1@%2").arg(name, version);
    args << pkg;
    
    if (isDev) {
        args << "--save-dev";
    }
    
    executeCommand("npm", args, [this, name](int code, const QString& output) {
        Q_UNUSED(output);
        m_progressBar->hide();
        if (code == 0) {
            m_statusLabel->setText(tr("Package installed successfully"));
            refreshPackages();
            emit packageInstalled(name, QString());
        } else {
            m_statusLabel->setText(tr("Installation failed"));
        }
    });
}

void PackageManagerWidget::npmUninstall(const QString& name) {
    executeCommand("npm", {"uninstall", name}, [this, name](int code, const QString& output) {
        Q_UNUSED(output);
        m_progressBar->hide();
        if (code == 0) {
            m_statusLabel->setText(tr("Package uninstalled successfully"));
            refreshPackages();
            emit packageUninstalled(name);
        } else {
            m_statusLabel->setText(tr("Uninstallation failed"));
        }
    });
}

void PackageManagerWidget::npmUpdate(const QString& name) {
    executeCommand("npm", {"update", name}, [this, name](int code, const QString& output) {
        Q_UNUSED(output);
        m_progressBar->hide();
        if (code == 0) {
            m_statusLabel->setText(tr("Package updated successfully"));
            refreshPackages();
            emit packageUpdated(name, QString());
        } else {
            m_statusLabel->setText(tr("Update failed"));
        }
    });
}

void PackageManagerWidget::npmSearch(const QString& query) {
    executeCommand("npm", {"search", "--json", query}, [this](int code, const QString& output) {
        m_progressBar->hide();
        
        if (code != 0) {
            m_statusLabel->setText(tr("Search failed"));
            return;
        }
        
        m_searchResults.clear();
        
        QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
        QJsonArray results = doc.array();
        
        for (const QJsonValue& val : results) {
            QJsonObject obj = val.toObject();
            
            PackageSearchResult result;
            result.name = obj["name"].toString();
            result.version = obj["version"].toString();
            result.description = obj["description"].toString();
            result.author = obj["author"].toObject()["name"].toString();
            result.isInstalled = m_installedPackages.contains(result.name);
            
            // Parse keywords
            QJsonArray keywords = obj["keywords"].toArray();
            for (const QJsonValue& kw : keywords) {
                result.keywords.append(kw.toString());
            }
            
            m_searchResults.append(result);
        }
        
        updateSearchResults(m_searchResults);
        m_statusLabel->setText(tr("Found %1 packages").arg(m_searchResults.size()));
        emit searchCompleted(m_searchResults);
    });
}

void PackageManagerWidget::npmAudit() {
    executeCommand("npm", {"audit", "--json"}, [this](int code, const QString& output) {
        Q_UNUSED(code);
        m_progressBar->hide();
        
        m_vulnerabilities.clear();
        m_auditTree->clear();
        
        QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
        QJsonObject root = doc.object();
        QJsonObject vulnerabilities = root["vulnerabilities"].toObject();
        
        int critical = 0, high = 0, moderate = 0, low = 0;
        
        for (auto it = vulnerabilities.begin(); it != vulnerabilities.end(); ++it) {
            QJsonObject vuln = it.value().toObject();
            
            PackageInfo::Vulnerability v;
            v.id = it.key();
            v.severity = vuln["severity"].toString();
            v.title = vuln["name"].toString();
            v.recommendation = vuln["fixAvailable"].toBool() ? "npm audit fix" : "Manual update required";
            
            m_vulnerabilities.append(v);
            
            // Count by severity
            if (v.severity == "critical") critical++;
            else if (v.severity == "high") high++;
            else if (v.severity == "moderate") moderate++;
            else low++;
            
            // Add to tree
            QTreeWidgetItem* item = new QTreeWidgetItem(m_auditTree);
            item->setText(0, it.key());
            item->setText(1, v.severity);
            item->setText(2, v.title);
            item->setText(3, v.recommendation);
            item->setIcon(1, getSeverityIcon(v.severity));
        }
        
        int total = m_vulnerabilities.size();
        m_auditSummaryLabel->setText(tr("%1 vulnerabilities (%2 critical, %3 high, %4 moderate, %5 low)")
            .arg(total).arg(critical).arg(high).arg(moderate).arg(low));
        
        m_fixVulnsBtn->setEnabled(total > 0);
        
        emit auditCompleted(total);
    });
}

void PackageManagerWidget::npmRunScript(const QString& script) {
    m_scriptOutput->clear();
    m_statusLabel->setText(tr("Running '%1'...").arg(script));
    emit scriptStarted(script);
    
    executeCommand("npm", {"run", script}, [this, script](int code, const QString& output) {
        Q_UNUSED(output);
        m_statusLabel->setText(code == 0 ? tr("Script completed") : tr("Script failed"));
        emit scriptFinished(script, code);
    });
}

// pip operations
void PackageManagerWidget::pipInstall(const QString& name, const QString& version) {
    QString pkg = version.isEmpty() ? name : QString("%1==%2").arg(name, version);
    
    executeCommand("pip", {"install", pkg}, [this, name](int code, const QString& output) {
        Q_UNUSED(output);
        m_progressBar->hide();
        if (code == 0) {
            m_statusLabel->setText(tr("Package installed successfully"));
            refreshPackages();
            emit packageInstalled(name, QString());
        }
    });
}

void PackageManagerWidget::pipUninstall(const QString& name) {
    executeCommand("pip", {"uninstall", "-y", name}, [this, name](int code, const QString& output) {
        Q_UNUSED(output);
        m_progressBar->hide();
        if (code == 0) {
            refreshPackages();
            emit packageUninstalled(name);
        }
    });
}

void PackageManagerWidget::pipUpdate(const QString& name) {
    executeCommand("pip", {"install", "--upgrade", name}, [this, name](int code, const QString& output) {
        Q_UNUSED(output);
        m_progressBar->hide();
        if (code == 0) {
            refreshPackages();
            emit packageUpdated(name, QString());
        }
    });
}

void PackageManagerWidget::pipSearch(const QString& query) {
    // pip search is deprecated, use PyPI API instead
    // For now, emit empty results
    m_progressBar->hide();
    m_searchResults.clear();
    m_statusLabel->setText(tr("pip search is not available"));
}

void PackageManagerWidget::pipAudit() {
    // Use pip-audit or safety
    executeCommand("pip-audit", {"--format", "json"}, [this](int code, const QString& output) {
        Q_UNUSED(code);
        m_progressBar->hide();
        
        // Parse pip-audit JSON output
        QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
        // Process vulnerabilities...
    });
}

// cargo operations
void PackageManagerWidget::cargoInstall(const QString& name, const QString& version) {
    QStringList args = {"add", name};
    if (!version.isEmpty()) {
        args << "--vers" << version;
    }
    
    executeCommand("cargo", args, [this, name](int code, const QString& output) {
        Q_UNUSED(output);
        m_progressBar->hide();
        if (code == 0) {
            refreshPackages();
            emit packageInstalled(name, QString());
        }
    });
}

void PackageManagerWidget::cargoUninstall(const QString& name) {
    executeCommand("cargo", {"remove", name}, [this, name](int code, const QString& output) {
        Q_UNUSED(output);
        m_progressBar->hide();
        if (code == 0) {
            refreshPackages();
            emit packageUninstalled(name);
        }
    });
}

void PackageManagerWidget::cargoUpdate(const QString& name) {
    executeCommand("cargo", {"update", "-p", name}, [this, name](int code, const QString& output) {
        Q_UNUSED(output);
        m_progressBar->hide();
        if (code == 0) {
            refreshPackages();
            emit packageUpdated(name, QString());
        }
    });
}

void PackageManagerWidget::cargoSearch(const QString& query) {
    executeCommand("cargo", {"search", "--limit", "20", query}, [this](int code, const QString& output) {
        m_progressBar->hide();
        
        if (code != 0) return;
        
        m_searchResults.clear();
        
        // Parse cargo search output: "name = version    # description"
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            QRegularExpression re("^([a-zA-Z0-9_-]+)\\s*=\\s*\"([^\"]+)\"\\s*#\\s*(.*)$");
            QRegularExpressionMatch match = re.match(line);
            
            if (match.hasMatch()) {
                PackageSearchResult result;
                result.name = match.captured(1);
                result.version = match.captured(2);
                result.description = match.captured(3);
                result.isInstalled = m_installedPackages.contains(result.name);
                m_searchResults.append(result);
            }
        }
        
        updateSearchResults(m_searchResults);
        emit searchCompleted(m_searchResults);
    });
}

void PackageManagerWidget::cargoAudit() {
    executeCommand("cargo", {"audit", "--json"}, [this](int code, const QString& output) {
        Q_UNUSED(code);
        m_progressBar->hide();
        // Parse cargo-audit JSON output
    });
}

void PackageManagerWidget::loadScripts() {
    m_scriptsTree->clear();
    
    for (const auto& script : m_scripts) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_scriptsTree);
        item->setText(0, script.name);
        item->setText(1, script.command);
        item->setIcon(0, style()->standardIcon(QStyle::SP_MediaPlay));
    }
}

void PackageManagerWidget::runScript(const QString& scriptName) {
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM:
            npmRunScript(scriptName);
            break;
        default:
            break;
    }
}

void PackageManagerWidget::runSecurityAudit() {
    m_statusLabel->setText(tr("Running security audit..."));
    m_progressBar->show();
    
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM:
            npmAudit();
            break;
        case PackageEcosystem::Pip:
            pipAudit();
            break;
        case PackageEcosystem::Cargo:
            cargoAudit();
            break;
        default:
            break;
    }
}

void PackageManagerWidget::fixVulnerabilities() {
    m_statusLabel->setText(tr("Fixing vulnerabilities..."));
    m_progressBar->show();
    
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM:
            executeCommand("npm", {"audit", "fix"}, [this](int code, const QString& output) {
                Q_UNUSED(output);
                m_progressBar->hide();
                if (code == 0) {
                    m_statusLabel->setText(tr("Vulnerabilities fixed"));
                    runSecurityAudit();
                }
            });
            break;
        default:
            break;
    }
}

void PackageManagerWidget::executeCommand(const QString& program, const QStringList& args,
                                          std::function<void(int, const QString&)> callback) {
    if (m_currentProcess && m_currentProcess->state() != QProcess::NotRunning) {
        emit errorOccurred(tr("A command is already running"));
        return;
    }
    
    m_currentProcess = new QProcess(this);
    m_currentProcess->setWorkingDirectory(m_projectPath);
    m_processCallback = callback;
    m_processOutput.clear();
    
    connect(m_currentProcess, &QProcess::readyReadStandardOutput,
            this, &PackageManagerWidget::onProcessOutput);
    connect(m_currentProcess, &QProcess::readyReadStandardError,
            this, &PackageManagerWidget::onProcessError);
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PackageManagerWidget::onProcessFinished);
    
    m_outputPanel->append(QString("> %1 %2").arg(program, args.join(' ')));
    m_currentProcess->start(program, args);
}

void PackageManagerWidget::updateSearchResults(const QList<PackageSearchResult>& results) {
    m_searchResultsTable->setRowCount(0);
    
    for (const auto& result : results) {
        int row = m_searchResultsTable->rowCount();
        m_searchResultsTable->insertRow(row);
        
        QTableWidgetItem* nameItem = new QTableWidgetItem(result.name);
        if (result.isInstalled) {
            nameItem->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
        }
        nameItem->setData(Qt::UserRole, result.name);
        
        m_searchResultsTable->setItem(row, 0, nameItem);
        m_searchResultsTable->setItem(row, 1, new QTableWidgetItem(result.version));
        m_searchResultsTable->setItem(row, 2, new QTableWidgetItem(formatDownloads(result.downloads)));
        m_searchResultsTable->setItem(row, 3, new QTableWidgetItem(
            result.updated.toString("yyyy-MM-dd")));
    }
}

void PackageManagerWidget::updateScriptsList() {
    loadScripts();
}

void PackageManagerWidget::updateAuditResults() {
    // Called after audit completes
}

QString PackageManagerWidget::getPackageManagerCommand() const {
    switch (m_currentEcosystem) {
        case PackageEcosystem::NPM: return "npm";
        case PackageEcosystem::Pip: return "pip";
        case PackageEcosystem::Cargo: return "cargo";
        case PackageEcosystem::Go: return "go";
        case PackageEcosystem::NuGet: return "dotnet";
        case PackageEcosystem::Composer: return "composer";
        default: return QString();
    }
}

QString PackageManagerWidget::formatDownloads(qint64 downloads) const {
    if (downloads < 1000) return QString::number(downloads);
    if (downloads < 1000000) return QString("%1K").arg(downloads / 1000);
    return QString("%1M").arg(downloads / 1000000);
}

QIcon PackageManagerWidget::getEcosystemIcon(PackageEcosystem ecosystem) const {
    Q_UNUSED(ecosystem);
    return style()->standardIcon(QStyle::SP_FileIcon);
}

QIcon PackageManagerWidget::getSeverityIcon(const QString& severity) const {
    if (severity == "critical" || severity == "high") {
        return style()->standardIcon(QStyle::SP_MessageBoxCritical);
    } else if (severity == "moderate") {
        return style()->standardIcon(QStyle::SP_MessageBoxWarning);
    }
    return style()->standardIcon(QStyle::SP_MessageBoxInformation);
}

// Slot implementations
void PackageManagerWidget::onEcosystemChanged(int index) {
    m_currentEcosystem = static_cast<PackageEcosystem>(m_ecosystemCombo->itemData(index).toInt());
    
    // Show/hide scripts tab based on ecosystem
    bool hasScripts = (m_currentEcosystem == PackageEcosystem::NPM);
    m_tabWidget->setTabEnabled(2, hasScripts);
    
    refreshPackages();
}

void PackageManagerWidget::onSearchTextChanged(const QString& text) {
    Q_UNUSED(text);
    // Could implement auto-search with delay
}

void PackageManagerWidget::onSearchClicked() {
    searchPackages(m_searchEdit->text().trimmed());
}

void PackageManagerWidget::onInstallClicked() {
    int row = m_searchResultsTable->currentRow();
    if (row < 0) return;
    
    QString name = m_searchResultsTable->item(row, 0)->data(Qt::UserRole).toString();
    QString version = m_versionCombo->currentText();
    bool isDev = m_devDependencyCheck->isChecked();
    
    installPackage(name, version, isDev);
}

void PackageManagerWidget::onUninstallClicked() {
    auto item = m_packagesTree->currentItem();
    if (!item || !item->parent()) return;
    
    QString name = item->data(0, Qt::UserRole).toString();
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Uninstall Package"),
        tr("Are you sure you want to uninstall '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        uninstallPackage(name);
    }
}

void PackageManagerWidget::onUpdateClicked() {
    auto item = m_packagesTree->currentItem();
    if (!item || !item->parent()) return;
    
    QString name = item->data(0, Qt::UserRole).toString();
    updatePackage(name);
}

void PackageManagerWidget::onUpdateAllClicked() {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Update All Packages"),
        tr("Are you sure you want to update all packages?"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        updateAllPackages();
    }
}

void PackageManagerWidget::onAuditClicked() {
    runSecurityAudit();
}

void PackageManagerWidget::onPackageSelected(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    
    bool hasSelection = item && item->parent();
    m_uninstallBtn->setEnabled(hasSelection);
    m_updateBtn->setEnabled(hasSelection && 
        m_installedPackages[item->data(0, Qt::UserRole).toString()].isOutdated);
}

void PackageManagerWidget::onSearchResultSelected(QTableWidgetItem* item) {
    if (!item) {
        m_installBtn->setEnabled(false);
        return;
    }
    
    int row = item->row();
    if (row < 0 || row >= m_searchResults.size()) return;
    
    const PackageSearchResult& result = m_searchResults[row];
    
    m_detailsNameLabel->setText(result.name);
    m_detailsVersionLabel->setText(tr("Version: %1").arg(result.version));
    m_detailsDescLabel->setText(result.description);
    m_detailsAuthorLabel->setText(tr("Author: %1").arg(result.author));
    m_detailsDownloadsLabel->setText(tr("Downloads: %1").arg(formatDownloads(result.downloads)));
    
    m_versionCombo->clear();
    m_versionCombo->addItem(result.version);
    m_versionCombo->addItem("latest");
    
    m_installBtn->setEnabled(!result.isInstalled);
    m_installBtn->setText(result.isInstalled ? tr("Installed") : tr("Install"));
}

void PackageManagerWidget::onSearchResultDoubleClicked(QTableWidgetItem* item) {
    onSearchResultSelected(item);
    if (m_installBtn->isEnabled()) {
        onInstallClicked();
    }
}

void PackageManagerWidget::onScriptDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    if (item) {
        runScript(item->text(0));
    }
}

void PackageManagerWidget::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status);
    
    if (m_processCallback) {
        m_processCallback(exitCode, m_processOutput);
        m_processCallback = nullptr;
    }
    
    m_currentProcess->deleteLater();
    m_currentProcess = nullptr;
}

void PackageManagerWidget::onProcessOutput() {
    QString output = m_currentProcess->readAllStandardOutput();
    m_processOutput += output;
    m_outputPanel->append(output);
    m_scriptOutput->append(output);
    emit outputReceived(output);
}

void PackageManagerWidget::onProcessError() {
    QString error = m_currentProcess->readAllStandardError();
    m_processOutput += error;
    m_outputPanel->append(QString("<span style='color: #ff6b6b;'>%1</span>").arg(error));
    m_scriptOutput->append(error);
}

void PackageManagerWidget::onShowOutdatedToggled(bool checked) {
    Q_UNUSED(checked);
    updatePackageList();
}

void PackageManagerWidget::onShowDevToggled(bool checked) {
    Q_UNUSED(checked);
    updatePackageList();
}

void PackageManagerWidget::setPackageVersion(const QString& name, const QString& version) {
    installPackage(name, version, m_installedPackages[name].isDev);
}

QStringList PackageManagerWidget::getAvailableVersions(const QString& name) {
    Q_UNUSED(name);
    // Would need to query registry for version list
    return QStringList();
}
