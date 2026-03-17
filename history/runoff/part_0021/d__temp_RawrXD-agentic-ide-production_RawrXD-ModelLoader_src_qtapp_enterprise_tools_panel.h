// Enterprise Tools Panel - GitHub-Style Tool Management for 44 Tools
#pragma once

#include <QWidget>
#include <QMap>
#include <QJsonObject>
#include <memory>

class QVBoxLayout;
class QScrollArea;
class QGroupBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QLineEdit;
class QTextEdit;

namespace RawrXD {

// Tool category enumeration
enum class ToolCategory {
    FileSystem,
    CodeAnalysis,
    Terminal,
    Editor,
    Testing,
    Refactoring,
    CodeUnderstanding,
    Git,
    Workspace,
    Documentation,
    Diagnostics,
    GitHubPR,
    GitHubIssues,
    GitHubWorkflows,
    GitHubCollaboration
};

// Tool definition structure
struct ToolDefinition {
    QString id;
    QString name;
    QString description;
    ToolCategory category;
    QStringList requiredPermissions;
    bool enabled;
    bool experimental;
    QString iconPath;
    int executionCount;
    int successCount;
    int failureCount;
    double avgExecutionTime;
};

/**
 * @class EnterpriseToolsPanel
 * @brief GitHub-style tool management panel with toggles for all 44 VS Code Copilot tools
 * 
 * Features:
 * - Individual enable/disable toggles for each tool
 * - Category-based organization (Built-in vs GitHub)
 * - Real-time usage statistics
 * - Bulk enable/disable operations
 * - Tool permission management
 * - Integration with existing security framework
 */
class EnterpriseToolsPanel : public QWidget {
    Q_OBJECT

public:
    explicit EnterpriseToolsPanel(QWidget* parent = nullptr);
    ~EnterpriseToolsPanel() override;

    // Initialization
    void initialize();

    // Tool management
    void enableTool(const QString& toolId);
    void disableTool(const QString& toolId);
    bool isToolEnabled(const QString& toolId) const;
    
    // Bulk operations
    void enableAllTools();
    void disableAllTools();
    void enableCategory(ToolCategory category);
    void disableCategory(ToolCategory category);
    
    // Tool execution tracking
    void recordToolExecution(const QString& toolId, bool success, double executionTime);
    
    // Configuration
    void loadConfiguration();
    void saveConfiguration();
    void resetToDefaults();
    
    // Get tool info
    ToolDefinition getToolDefinition(const QString& toolId) const;
    QStringList getEnabledTools() const;
    QStringList getDisabledTools() const;

signals:
    void toolToggled(const QString& toolId, bool enabled);
    void configurationChanged();
    void toolExecuted(const QString& toolId, bool success);

private slots:
    void onToolCheckboxToggled(bool checked);
    void onCategoryExpandToggled();
    void onSearchTextChanged(const QString& text);
    void onBulkEnableClicked();
    void onBulkDisableClicked();
    void onResetDefaultsClicked();
    void onExportConfigClicked();
    void onImportConfigClicked();
    void onRefreshStatsClicked();

private:
    // UI setup
    void setupUI();
    void createToolsSection();
    void createStatsSection();
    void createControlsSection();
    
    // Tool registry
    void registerBuiltInTools();
    void registerGitHubTools();
    void registerTool(const ToolDefinition& tool);
    
    // UI creation helpers
    QGroupBox* createCategoryGroup(ToolCategory category, const QString& title);
    QWidget* createToolControl(const ToolDefinition& tool);
    void updateToolStats(const QString& toolId);
    void filterTools(const QString& searchText);
    
    // Configuration helpers
    QJsonObject serializeConfiguration() const;
    void deserializeConfiguration(const QJsonObject& config);
    QString getCategoryString(ToolCategory category) const;
    
    // Member variables
    QMap<QString, ToolDefinition> m_tools;
    QMap<QString, QCheckBox*> m_toolCheckboxes;
    QMap<QString, QLabel*> m_toolStatsLabels;
    QMap<ToolCategory, QGroupBox*> m_categoryGroups;
    
    // UI components
    QScrollArea* m_scrollArea;
    QVBoxLayout* m_mainLayout;
    QLineEdit* m_searchBox;
    QTextEdit* m_statsDisplay;
    QPushButton* m_enableAllBtn;
    QPushButton* m_disableAllBtn;
    QPushButton* m_resetDefaultsBtn;
    QPushButton* m_exportConfigBtn;
    QPushButton* m_importConfigBtn;
    QPushButton* m_refreshStatsBtn;
    
    // State tracking
    bool m_initialized;
    QString m_configFilePath;
};

} // namespace RawrXD
