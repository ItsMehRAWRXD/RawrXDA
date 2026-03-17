/**
 * @file package_manager_widget.h
 * @brief Package manager UI for npm, pip, cargo, etc.
 * 
 * Production-ready package manager widget providing:
 * - Multi-ecosystem support (npm, pip, cargo, go, nuget, composer)
 * - Package search and installation
 * - Dependency tree visualization
 * - Version management and updates
 * - Vulnerability scanning
 * - Lock file handling
 * - Script runner for npm scripts
 * - Virtual environment support for Python
 */

#ifndef PACKAGE_MANAGER_WIDGET_H
#define PACKAGE_MANAGER_WIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QGroupBox>
#include <QTextEdit>
#include <QCheckBox>
#include <QProcess>
#include <QMap>
#include <QDateTime>

/**
 * @enum PackageEcosystem
 * @brief Supported package ecosystems
 */
enum class PackageEcosystem {
    NPM,        // Node.js
    Pip,        // Python
    Cargo,      // Rust
    Go,         // Go modules
    NuGet,      // .NET
    Composer,   // PHP
    Maven,      // Java
    Gradle,     // Java/Kotlin
    CocoaPods,  // iOS/macOS
    Vcpkg       // C/C++
};

/**
 * @struct PackageInfo
 * @brief Information about a package
 */
struct PackageInfo {
    QString name;
    QString version;
    QString latestVersion;
    QString description;
    QString author;
    QString license;
    QString homepage;
    QString repository;
    PackageEcosystem ecosystem;
    QStringList keywords;
    QDateTime publishedDate;
    qint64 downloads = 0;
    double rating = 0.0;
    bool isDev = false;
    bool isOutdated = false;
    
    // Dependency info
    QStringList dependencies;
    QStringList devDependencies;
    QStringList peerDependencies;
    
    // Vulnerability info
    struct Vulnerability {
        QString id;
        QString severity;  // low, moderate, high, critical
        QString title;
        QString description;
        QString recommendation;
    };
    QList<Vulnerability> vulnerabilities;
    
    bool hasVulnerabilities() const { return !vulnerabilities.isEmpty(); }
    QString getSeverityColor() const;
};

/**
 * @struct PackageScript
 * @brief npm/package script definition
 */
struct PackageScript {
    QString name;
    QString command;
    QString description;
};

/**
 * @class PackageSearchResult
 * @brief Search result from package registry
 */
struct PackageSearchResult {
    QString name;
    QString version;
    QString description;
    QString author;
    qint64 downloads = 0;
    QDateTime updated;
    QStringList keywords;
    bool isInstalled = false;
};

/**
 * @class PackageManagerWidget
 * @brief Main package manager widget
 */
class PackageManagerWidget : public QWidget {
    Q_OBJECT

public:
    explicit PackageManagerWidget(QWidget* parent = nullptr);
    ~PackageManagerWidget();
    
    // Project detection
    void setProjectPath(const QString& path);
    PackageEcosystem detectEcosystem() const;
    bool hasPackageFile() const;
    
    // Package operations
    void refreshPackages();
    void installPackage(const QString& name, const QString& version = QString(),
                       bool isDev = false);
    void uninstallPackage(const QString& name);
    void updatePackage(const QString& name);
    void updateAllPackages();
    
    // Search
    void searchPackages(const QString& query);
    
    // Scripts (npm)
    void loadScripts();
    void runScript(const QString& scriptName);
    
    // Audit/Security
    void runSecurityAudit();
    void fixVulnerabilities();
    
    // Version management
    void setPackageVersion(const QString& name, const QString& version);
    QStringList getAvailableVersions(const QString& name);

signals:
    void packageInstalled(const QString& name, const QString& version);
    void packageUninstalled(const QString& name);
    void packageUpdated(const QString& name, const QString& newVersion);
    void packagesRefreshed();
    void searchCompleted(const QList<PackageSearchResult>& results);
    void auditCompleted(int vulnerabilities);
    void scriptStarted(const QString& name);
    void scriptFinished(const QString& name, int exitCode);
    void errorOccurred(const QString& error);
    void outputReceived(const QString& output);

private slots:
    void onEcosystemChanged(int index);
    void onSearchTextChanged(const QString& text);
    void onSearchClicked();
    void onInstallClicked();
    void onUninstallClicked();
    void onUpdateClicked();
    void onUpdateAllClicked();
    void onAuditClicked();
    void onPackageSelected(QTreeWidgetItem* item, int column);
    void onSearchResultSelected(QTableWidgetItem* item);
    void onSearchResultDoubleClicked(QTableWidgetItem* item);
    void onScriptDoubleClicked(QTreeWidgetItem* item, int column);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessOutput();
    void onProcessError();
    void onShowOutdatedToggled(bool checked);
    void onShowDevToggled(bool checked);

private:
    void setupUI();
    void setupInstalledTab();
    void setupSearchTab();
    void setupScriptsTab();
    void setupAuditTab();
    void setupConnections();
    
    // Ecosystem-specific operations
    void npmInstall(const QString& name, const QString& version, bool isDev);
    void npmUninstall(const QString& name);
    void npmUpdate(const QString& name);
    void npmSearch(const QString& query);
    void npmAudit();
    void npmRunScript(const QString& script);
    void parsePackageJson();
    void parsePackageLock();
    
    void pipInstall(const QString& name, const QString& version);
    void pipUninstall(const QString& name);
    void pipUpdate(const QString& name);
    void pipSearch(const QString& query);
    void pipAudit();
    void parseRequirementsTxt();
    void parsePipfile();
    
    void cargoInstall(const QString& name, const QString& version);
    void cargoUninstall(const QString& name);
    void cargoUpdate(const QString& name);
    void cargoSearch(const QString& query);
    void cargoAudit();
    void parseCargoToml();
    
    void executeCommand(const QString& program, const QStringList& args,
                       std::function<void(int, const QString&)> callback = nullptr);
    
    void updatePackageList();
    void updateSearchResults(const QList<PackageSearchResult>& results);
    void updateScriptsList();
    void updateAuditResults();
    
    QString getPackageManagerCommand() const;
    QString formatDownloads(qint64 downloads) const;
    QIcon getEcosystemIcon(PackageEcosystem ecosystem) const;
    QIcon getSeverityIcon(const QString& severity) const;
    
    // UI Components
    QComboBox* m_ecosystemCombo;
    QLabel* m_projectPathLabel;
    QPushButton* m_refreshBtn;
    QTabWidget* m_tabWidget;
    
    // Installed tab
    QTreeWidget* m_packagesTree;
    QPushButton* m_uninstallBtn;
    QPushButton* m_updateBtn;
    QPushButton* m_updateAllBtn;
    QCheckBox* m_showOutdatedCheck;
    QCheckBox* m_showDevCheck;
    QLabel* m_packageCountLabel;
    
    // Search tab
    QLineEdit* m_searchEdit;
    QPushButton* m_searchBtn;
    QTableWidget* m_searchResultsTable;
    QGroupBox* m_packageDetailsGroup;
    QLabel* m_detailsNameLabel;
    QLabel* m_detailsVersionLabel;
    QLabel* m_detailsDescLabel;
    QLabel* m_detailsAuthorLabel;
    QLabel* m_detailsLicenseLabel;
    QLabel* m_detailsDownloadsLabel;
    QPushButton* m_installBtn;
    QComboBox* m_versionCombo;
    QCheckBox* m_devDependencyCheck;
    
    // Scripts tab
    QTreeWidget* m_scriptsTree;
    QPushButton* m_runScriptBtn;
    QTextEdit* m_scriptOutput;
    
    // Audit tab
    QTreeWidget* m_auditTree;
    QPushButton* m_auditBtn;
    QPushButton* m_fixVulnsBtn;
    QLabel* m_auditSummaryLabel;
    
    // Output panel
    QTextEdit* m_outputPanel;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    
    // Data
    QString m_projectPath;
    PackageEcosystem m_currentEcosystem = PackageEcosystem::NPM;
    QMap<QString, PackageInfo> m_installedPackages;
    QList<PackageSearchResult> m_searchResults;
    QList<PackageScript> m_scripts;
    QList<PackageInfo::Vulnerability> m_vulnerabilities;
    
    QProcess* m_currentProcess = nullptr;
    std::function<void(int, const QString&)> m_processCallback;
    QString m_processOutput;
};

#endif // PACKAGE_MANAGER_WIDGET_H
