// Enterprise Tools Panel - Implementation
#include "enterprise_tools_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDateTime>

namespace RawrXD {

EnterpriseToolsPanel::EnterpriseToolsPanel(QWidget* parent)
    : QWidget(parent)
    , m_initialized(false)
{
    // Lightweight constructor - defer Qt widget creation
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_configFilePath = appDataPath + "/tools_config.json";
}

EnterpriseToolsPanel::~EnterpriseToolsPanel() {
    if (m_initialized) {
        saveConfiguration();
    }
}

void EnterpriseToolsPanel::initialize() {
    if (m_initialized) return;
    
    setupUI();
    registerBuiltInTools();
    registerGitHubTools();
    loadConfiguration();
    
    m_initialized = true;
}

void EnterpriseToolsPanel::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    
    // Header with search and controls
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QLabel* titleLabel = new QLabel("<h2>🛠️ Enterprise Tools Management</h2>", this);
    headerLayout->addWidget(titleLabel);
    
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("🔍 Search tools...");
    m_searchBox->setMaximumWidth(300);
    connect(m_searchBox, &QLineEdit::textChanged, this, &EnterpriseToolsPanel::onSearchTextChanged);
    headerLayout->addWidget(m_searchBox);
    
    headerLayout->addStretch();
    m_mainLayout->addLayout(headerLayout);
    
    // Create splitter for tools and stats
    QSplitter* splitter = new QSplitter(Qt::Vertical, this);
    
    // Tools section (scrollable)
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    
    QWidget* toolsContainer = new QWidget(this);
    QVBoxLayout* toolsLayout = new QVBoxLayout(toolsContainer);
    
    createToolsSection();
    
    toolsLayout->addStretch();
    m_scrollArea->setWidget(toolsContainer);
    splitter->addWidget(m_scrollArea);
    
    // Stats section
    createStatsSection();
    QWidget* statsWidget = new QWidget(this);
    QVBoxLayout* statsLayout = new QVBoxLayout(statsWidget);
    statsLayout->addWidget(new QLabel("<b>Tool Usage Statistics</b>", this));
    statsLayout->addWidget(m_statsDisplay);
    splitter->addWidget(statsWidget);
    
    splitter->setSizes({600, 200});
    m_mainLayout->addWidget(splitter);
    
    // Controls section
    createControlsSection();
}

void EnterpriseToolsPanel::createToolsSection() {
    // This will be populated by registerBuiltInTools() and registerGitHubTools()
}

void EnterpriseToolsPanel::createStatsSection() {
    m_statsDisplay = new QTextEdit(this);
    m_statsDisplay->setReadOnly(true);
    m_statsDisplay->setMaximumHeight(200);
    m_statsDisplay->setStyleSheet(
        "QTextEdit { background-color: #2b2b2b; color: #d4d4d4; "
        "font-family: 'Consolas', 'Courier New', monospace; font-size: 10pt; }"
    );
}

void EnterpriseToolsPanel::createControlsSection() {
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    
    m_enableAllBtn = new QPushButton("✓ Enable All", this);
    m_disableAllBtn = new QPushButton("✗ Disable All", this);
    m_resetDefaultsBtn = new QPushButton("🔄 Reset Defaults", this);
    m_exportConfigBtn = new QPushButton("📤 Export Config", this);
    m_importConfigBtn = new QPushButton("📥 Import Config", this);
    m_refreshStatsBtn = new QPushButton("📊 Refresh Stats", this);
    
    connect(m_enableAllBtn, &QPushButton::clicked, this, &EnterpriseToolsPanel::onBulkEnableClicked);
    connect(m_disableAllBtn, &QPushButton::clicked, this, &EnterpriseToolsPanel::onBulkDisableClicked);
    connect(m_resetDefaultsBtn, &QPushButton::clicked, this, &EnterpriseToolsPanel::onResetDefaultsClicked);
    connect(m_exportConfigBtn, &QPushButton::clicked, this, &EnterpriseToolsPanel::onExportConfigClicked);
    connect(m_importConfigBtn, &QPushButton::clicked, this, &EnterpriseToolsPanel::onImportConfigClicked);
    connect(m_refreshStatsBtn, &QPushButton::clicked, this, &EnterpriseToolsPanel::onRefreshStatsClicked);
    
    controlsLayout->addWidget(m_enableAllBtn);
    controlsLayout->addWidget(m_disableAllBtn);
    controlsLayout->addWidget(m_resetDefaultsBtn);
    controlsLayout->addStretch();
    controlsLayout->addWidget(m_exportConfigBtn);
    controlsLayout->addWidget(m_importConfigBtn);
    controlsLayout->addWidget(m_refreshStatsBtn);
    
    m_mainLayout->addLayout(controlsLayout);
}

void EnterpriseToolsPanel::registerBuiltInTools() {
    // FILE SYSTEM TOOLS (8 tools)
    registerTool({
        "editFiles", "Edit Files", 
        "Allows direct file modifications with security validation",
        ToolCategory::FileSystem, {"file_write"}, true, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "readFiles", "Read Files",
        "Read complete file contents with path validation",
        ToolCategory::FileSystem, {"file_read"}, true, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "searchFiles", "Search Files",
        "Search for files by name or pattern recursively",
        ToolCategory::FileSystem, {"file_read"}, true, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "listFiles", "List Files",
        "List directory contents with recursive option",
        ToolCategory::FileSystem, {"file_read"}, true, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "createFile", "Create File",
        "Create new files with conflict detection",
        ToolCategory::FileSystem, {"file_write"}, true, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "deleteFile", "Delete File",
        "Safe file deletion with audit logging",
        ToolCategory::FileSystem, {"file_write"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "renameFile", "Rename File",
        "Atomic file rename operations",
        ToolCategory::FileSystem, {"file_write"}, true, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "getEditorContext", "Get Editor Context",
        "Capture visible code and selections from active editor",
        ToolCategory::Editor, {"editor_read"}, true, false, "", 0, 0, 0, 0.0
    });
    
    // CODE ANALYSIS TOOLS (2 tools)
    registerTool({
        "findSymbols", "Find Symbols",
        "Search for code symbols (functions, classes) across project",
        ToolCategory::CodeAnalysis, {"code_read"}, true, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "codeSearch", "Code Search",
        "Semantic search through indexed codebase",
        ToolCategory::CodeAnalysis, {"code_read"}, true, false, "", 0, 0, 0, 0.0
    });
    
    // TERMINAL TOOLS (2 tools)
    registerTool({
        "runCommands", "Run Commands",
        "Execute shell/terminal commands with sandboxing",
        ToolCategory::Terminal, {"command_execution"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "getTerminalContent", "Get Terminal Content",
        "Read recent history from active terminal",
        ToolCategory::Terminal, {"terminal_read"}, true, false, "", 0, 0, 0, 0.0
    });
    
    // TESTING TOOLS (2 tools)
    registerTool({
        "testRunner", "Test Runner",
        "Run existing unit tests and capture results",
        ToolCategory::Testing, {"test_execution"}, true, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "generateTests", "Generate Tests",
        "Automatically generate new test files with AI",
        ToolCategory::Testing, {"test_write", "ai_generation"}, true, true, "", 0, 0, 0, 0.0
    });
    
    // REFACTORING TOOLS (1 tool)
    registerTool({
        "refactorCode", "Refactor Code",
        "Apply specific refactoring patterns (extract, rename, inline)",
        ToolCategory::Refactoring, {"code_write"}, true, false, "", 0, 0, 0, 0.0
    });
    
    // CODE UNDERSTANDING TOOLS (2 tools)
    registerTool({
        "explainCode", "Explain Code",
        "Generate natural language explanations of code logic",
        ToolCategory::CodeUnderstanding, {"code_read", "ai_generation"}, true, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "fixCode", "Fix Code",
        "Propose fixes for detected bugs or errors using AI",
        ToolCategory::CodeUnderstanding, {"code_write", "ai_generation"}, true, false, "", 0, 0, 0, 0.0
    });
    
    // GIT TOOLS (2 tools)
    registerTool({
        "gitStatus", "Git Status",
        "Read current state of git repository",
        ToolCategory::Git, {"git_read"}, true, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "installDependencies", "Install Dependencies",
        "Suggest and run package manager installs (npm, pip)",
        ToolCategory::Git, {"package_manager"}, false, false, "", 0, 0, 0, 0.0
    });
    
    // WORKSPACE TOOLS (1 tool)
    registerTool({
        "workspaceSymbols", "Workspace Symbols",
        "Access high-level map of project symbols",
        ToolCategory::Workspace, {"workspace_read"}, true, false, "", 0, 0, 0, 0.0
    });
    
    // DOCUMENTATION TOOLS (1 tool)
    registerTool({
        "documentationLookUp", "Documentation Lookup",
        "Search local or indexed documentation",
        ToolCategory::Documentation, {"documentation_read"}, true, false, "", 0, 0, 0, 0.0
    });
    
    // DIAGNOSTICS TOOLS (1 tool)
    registerTool({
        "getDiagnostics", "Get Diagnostics",
        "Access linter errors and compiler warnings",
        ToolCategory::Diagnostics, {"diagnostics_read"}, true, false, "", 0, 0, 0, 0.0
    });
}

void EnterpriseToolsPanel::registerGitHubTools() {
    // GITHUB PR TOOLS (8 tools)
    registerTool({
        "createPullRequest", "Create Pull Request",
        "Generate new PR with AI-written title and description",
        ToolCategory::GitHubPR, {"github_write"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "summarizePR", "Summarize PR",
        "Provide AI-generated prose and bulleted summary of PR changes",
        ToolCategory::GitHubPR, {"github_read", "ai_generation"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "reviewPR", "Review PR",
        "Perform automated AI code review on open PR",
        ToolCategory::GitHubPR, {"github_read", "ai_generation"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "generatePRDescription", "Generate PR Description",
        "Fill description field based on commit history using AI",
        ToolCategory::GitHubPR, {"github_write", "ai_generation"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "checkoutPR", "Checkout PR",
        "Switch local branch to specific PR for testing",
        ToolCategory::GitHubPR, {"github_read", "git_write"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "compareBranches", "Compare Branches",
        "Highlight differences between source and target branches",
        ToolCategory::GitHubPR, {"github_read"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "resolveConversations", "Resolve Conversations",
        "Automatically resolve PR comments when fixed",
        ToolCategory::GitHubPR, {"github_write"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "syncDocumentation", "Sync Documentation",
        "Update documentation files based on PR changes",
        ToolCategory::GitHubPR, {"github_write", "ai_generation"}, false, true, "", 0, 0, 0, 0.0
    });
    
    // GITHUB ISSUES TOOLS (6 tools)
    registerTool({
        "listIssues", "List Issues",
        "Display open or assigned GitHub issues",
        ToolCategory::GitHubIssues, {"github_read"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "searchIssues", "Search Issues",
        "Search for specific issue numbers or keywords",
        ToolCategory::GitHubIssues, {"github_read"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "closeIssue", "Close Issue",
        "Close issue after fix verification",
        ToolCategory::GitHubIssues, {"github_write"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "createIssueComment", "Create Issue Comment",
        "Add comments to active issues or PRs",
        ToolCategory::GitHubIssues, {"github_write"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "assignUsers", "Assign Users",
        "Add or remove assignees from issues/PRs",
        ToolCategory::GitHubIssues, {"github_write"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "manageLabels", "Manage Labels",
        "Apply or remove GitHub labels (e.g., 'bug', 'v1.0')",
        ToolCategory::GitHubIssues, {"github_write"}, false, false, "", 0, 0, 0, 0.0
    });
    
    // GITHUB WORKFLOWS TOOLS (4 tools)
    registerTool({
        "viewWorkflowRuns", "View Workflow Runs",
        "Monitor GitHub Actions CI/CD status",
        ToolCategory::GitHubWorkflows, {"github_read"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "retryWorkflow", "Retry Workflow",
        "Trigger re-run of failed CI job",
        ToolCategory::GitHubWorkflows, {"github_write"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "listReleases", "List Releases",
        "Display historical release notes for repository",
        ToolCategory::GitHubWorkflows, {"github_read"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "fetchRepositoryData", "Fetch Repository Data",
        "Pull metadata like stars, forks, or branch names",
        ToolCategory::GitHubWorkflows, {"github_read"}, false, false, "", 0, 0, 0, 0.0
    });
    
    // GITHUB COLLABORATION TOOLS (4 tools)
    registerTool({
        "requestReviews", "Request Reviews",
        "Automatically suggest and ping reviewers",
        ToolCategory::GitHubCollaboration, {"github_write"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "listDiscussions", "List Discussions",
        "Search GitHub Discussions tab for context",
        ToolCategory::GitHubCollaboration, {"github_read"}, false, false, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "trackSessions", "Track Sessions",
        "Monitor multi-turn agentic coding sessions",
        ToolCategory::GitHubCollaboration, {"github_read"}, false, true, "", 0, 0, 0, 0.0
    });
    
    registerTool({
        "notificationsView", "Notifications View",
        "Display real-time GitHub notifications in VS Code",
        ToolCategory::GitHubCollaboration, {"github_read"}, false, false, "", 0, 0, 0, 0.0
    });
}

void EnterpriseToolsPanel::registerTool(const ToolDefinition& tool) {
    m_tools[tool.id] = tool;
    
    // Create UI for this tool
    ToolCategory category = tool.category;
    
    // Get or create category group
    if (!m_categoryGroups.contains(category)) {
        QString categoryTitle = getCategoryString(category);
        QGroupBox* group = createCategoryGroup(category, categoryTitle);
        m_categoryGroups[category] = group;
        
        // Add to scroll area
        QWidget* container = m_scrollArea->widget();
        if (container) {
            container->layout()->addWidget(group);
        }
    }
    
    // Create tool control widget
    QWidget* toolControl = createToolControl(tool);
    m_categoryGroups[category]->layout()->addWidget(toolControl);
}

QGroupBox* EnterpriseToolsPanel::createCategoryGroup(ToolCategory category, const QString& title) {
    QGroupBox* group = new QGroupBox(title, this);
    group->setCheckable(true);
    group->setChecked(true);
    
    QVBoxLayout* layout = new QVBoxLayout(group);
    layout->setSpacing(5);
    layout->setContentsMargins(10, 15, 10, 10);
    
    connect(group, &QGroupBox::toggled, this, &EnterpriseToolsPanel::onCategoryExpandToggled);
    
    return group;
}

QWidget* EnterpriseToolsPanel::createToolControl(const ToolDefinition& tool) {
    QWidget* widget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(5, 2, 5, 2);
    
    // Checkbox for enable/disable
    QCheckBox* checkbox = new QCheckBox(tool.name, widget);
    checkbox->setChecked(tool.enabled);
    checkbox->setProperty("toolId", tool.id);
    checkbox->setToolTip(tool.description + "\nPermissions: " + tool.requiredPermissions.join(", "));
    
    // Experimental badge
    if (tool.experimental) {
        checkbox->setText(tool.name + " 🧪");
        checkbox->setStyleSheet("QCheckBox { color: #f39c12; font-weight: bold; }");
    }
    
    connect(checkbox, &QCheckBox::toggled, this, &EnterpriseToolsPanel::onToolCheckboxToggled);
    m_toolCheckboxes[tool.id] = checkbox;
    
    layout->addWidget(checkbox);
    layout->addStretch();
    
    // Stats label
    QLabel* statsLabel = new QLabel("0 exec", widget);
    statsLabel->setStyleSheet("QLabel { color: #7f8c8d; font-size: 9pt; }");
    m_toolStatsLabels[tool.id] = statsLabel;
    layout->addWidget(statsLabel);
    
    return widget;
}

QString EnterpriseToolsPanel::getCategoryString(ToolCategory category) const {
    switch (category) {
        case ToolCategory::FileSystem: return "📁 File System Tools";
        case ToolCategory::CodeAnalysis: return "🔍 Code Analysis Tools";
        case ToolCategory::Terminal: return "💻 Terminal Tools";
        case ToolCategory::Editor: return "📝 Editor Tools";
        case ToolCategory::Testing: return "🧪 Testing Tools";
        case ToolCategory::Refactoring: return "🔧 Refactoring Tools";
        case ToolCategory::CodeUnderstanding: return "🧠 Code Understanding Tools";
        case ToolCategory::Git: return "📚 Git Tools";
        case ToolCategory::Workspace: return "🗂️ Workspace Tools";
        case ToolCategory::Documentation: return "📖 Documentation Tools";
        case ToolCategory::Diagnostics: return "🩺 Diagnostics Tools";
        case ToolCategory::GitHubPR: return "🔀 GitHub PR Tools";
        case ToolCategory::GitHubIssues: return "📋 GitHub Issues Tools";
        case ToolCategory::GitHubWorkflows: return "⚙️ GitHub Workflows Tools";
        case ToolCategory::GitHubCollaboration: return "👥 GitHub Collaboration Tools";
        default: return "🛠️ Other Tools";
    }
}

// Tool management methods
void EnterpriseToolsPanel::enableTool(const QString& toolId) {
    if (m_tools.contains(toolId)) {
        m_tools[toolId].enabled = true;
        if (m_toolCheckboxes.contains(toolId)) {
            m_toolCheckboxes[toolId]->setChecked(true);
        }
        emit toolToggled(toolId, true);
    }
}

void EnterpriseToolsPanel::disableTool(const QString& toolId) {
    if (m_tools.contains(toolId)) {
        m_tools[toolId].enabled = false;
        if (m_toolCheckboxes.contains(toolId)) {
            m_toolCheckboxes[toolId]->setChecked(false);
        }
        emit toolToggled(toolId, false);
    }
}

bool EnterpriseToolsPanel::isToolEnabled(const QString& toolId) const {
    return m_tools.value(toolId).enabled;
}

void EnterpriseToolsPanel::enableAllTools() {
    for (auto& tool : m_tools) {
        enableTool(tool.id);
    }
    emit configurationChanged();
}

void EnterpriseToolsPanel::disableAllTools() {
    for (auto& tool : m_tools) {
        disableTool(tool.id);
    }
    emit configurationChanged();
}

void EnterpriseToolsPanel::enableCategory(ToolCategory category) {
    for (auto& tool : m_tools) {
        if (tool.category == category) {
            enableTool(tool.id);
        }
    }
    emit configurationChanged();
}

void EnterpriseToolsPanel::disableCategory(ToolCategory category) {
    for (auto& tool : m_tools) {
        if (tool.category == category) {
            disableTool(tool.id);
        }
    }
    emit configurationChanged();
}

void EnterpriseToolsPanel::recordToolExecution(const QString& toolId, bool success, double executionTime) {
    if (!m_tools.contains(toolId)) return;
    
    ToolDefinition& tool = m_tools[toolId];
    tool.executionCount++;
    
    if (success) {
        tool.successCount++;
    } else {
        tool.failureCount++;
    }
    
    // Update average execution time
    tool.avgExecutionTime = (tool.avgExecutionTime * (tool.executionCount - 1) + executionTime) / tool.executionCount;
    
    updateToolStats(toolId);
    emit toolExecuted(toolId, success);
}

void EnterpriseToolsPanel::updateToolStats(const QString& toolId) {
    if (!m_toolStatsLabels.contains(toolId)) return;
    
    const ToolDefinition& tool = m_tools[toolId];
    QString statsText = QString("%1 exec (%2 ✓ / %3 ✗)")
        .arg(tool.executionCount)
        .arg(tool.successCount)
        .arg(tool.failureCount);
    
    m_toolStatsLabels[toolId]->setText(statsText);
}

void EnterpriseToolsPanel::filterTools(const QString& searchText) {
    for (auto it = m_tools.constBegin(); it != m_tools.constEnd(); ++it) {
        const QString& toolId = it.key();
        const ToolDefinition& tool = it.value();
        
        bool matches = searchText.isEmpty() ||
                      tool.name.contains(searchText, Qt::CaseInsensitive) ||
                      tool.description.contains(searchText, Qt::CaseInsensitive) ||
                      toolId.contains(searchText, Qt::CaseInsensitive);
        
        if (m_toolCheckboxes.contains(toolId)) {
            m_toolCheckboxes[toolId]->parentWidget()->setVisible(matches);
        }
    }
}

// Configuration management
void EnterpriseToolsPanel::loadConfiguration() {
    QFile file(m_configFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        // No config file yet, use defaults
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull() && doc.isObject()) {
        deserializeConfiguration(doc.object());
    }
}

void EnterpriseToolsPanel::saveConfiguration() {
    QJsonObject config = serializeConfiguration();
    
    QJsonDocument doc(config);
    QByteArray data = doc.toJson(QJsonDocument::Indented);
    
    QFile file(m_configFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
    }
}

void EnterpriseToolsPanel::resetToDefaults() {
    int reply = QMessageBox::question(this, "Reset Defaults",
        "This will reset all tool configurations to defaults. Continue?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // Re-register all tools with default settings
        m_tools.clear();
        registerBuiltInTools();
        registerGitHubTools();
        
        // Update UI
        for (auto it = m_toolCheckboxes.constBegin(); it != m_toolCheckboxes.constEnd(); ++it) {
            const QString& toolId = it.key();
            if (m_tools.contains(toolId)) {
                it.value()->setChecked(m_tools[toolId].enabled);
            }
        }
        
        saveConfiguration();
        emit configurationChanged();
        
        QMessageBox::information(this, "Reset Complete", "Tool configuration reset to defaults.");
    }
}

QJsonObject EnterpriseToolsPanel::serializeConfiguration() const {
    QJsonObject config;
    config["version"] = "1.0.0";
    config["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray tools;
    for (auto it = m_tools.constBegin(); it != m_tools.constEnd(); ++it) {
        const ToolDefinition& tool = it.value();
        
        QJsonObject toolObj;
        toolObj["id"] = tool.id;
        toolObj["enabled"] = tool.enabled;
        toolObj["executionCount"] = tool.executionCount;
        toolObj["successCount"] = tool.successCount;
        toolObj["failureCount"] = tool.failureCount;
        toolObj["avgExecutionTime"] = tool.avgExecutionTime;
        
        tools.append(toolObj);
    }
    
    config["tools"] = tools;
    return config;
}

void EnterpriseToolsPanel::deserializeConfiguration(const QJsonObject& config) {
    if (!config.contains("tools")) return;
    
    QJsonArray tools = config["tools"].toArray();
    for (const QJsonValue& value : tools) {
        QJsonObject toolObj = value.toObject();
        QString toolId = toolObj["id"].toString();
        
        if (m_tools.contains(toolId)) {
            ToolDefinition& tool = m_tools[toolId];
            tool.enabled = toolObj["enabled"].toBool();
            tool.executionCount = toolObj["executionCount"].toInt();
            tool.successCount = toolObj["successCount"].toInt();
            tool.failureCount = toolObj["failureCount"].toInt();
            tool.avgExecutionTime = toolObj["avgExecutionTime"].toDouble();
            
            // Update UI
            if (m_toolCheckboxes.contains(toolId)) {
                m_toolCheckboxes[toolId]->setChecked(tool.enabled);
                updateToolStats(toolId);
            }
        }
    }
}

ToolDefinition EnterpriseToolsPanel::getToolDefinition(const QString& toolId) const {
    return m_tools.value(toolId);
}

QStringList EnterpriseToolsPanel::getEnabledTools() const {
    QStringList enabled;
    for (auto it = m_tools.constBegin(); it != m_tools.constEnd(); ++it) {
        if (it.value().enabled) {
            enabled.append(it.key());
        }
    }
    return enabled;
}

QStringList EnterpriseToolsPanel::getDisabledTools() const {
    QStringList disabled;
    for (auto it = m_tools.constBegin(); it != m_tools.constEnd(); ++it) {
        if (!it.value().enabled) {
            disabled.append(it.key());
        }
    }
    return disabled;
}

// Slot implementations
void EnterpriseToolsPanel::onToolCheckboxToggled(bool checked) {
    QCheckBox* checkbox = qobject_cast<QCheckBox*>(sender());
    if (!checkbox) return;
    
    QString toolId = checkbox->property("toolId").toString();
    if (checked) {
        enableTool(toolId);
    } else {
        disableTool(toolId);
    }
    
    saveConfiguration();
}

void EnterpriseToolsPanel::onCategoryExpandToggled() {
    // Category groups are collapsible via QGroupBox::setChecked
    // No additional action needed
}

void EnterpriseToolsPanel::onSearchTextChanged(const QString& text) {
    filterTools(text);
}

void EnterpriseToolsPanel::onBulkEnableClicked() {
    enableAllTools();
    saveConfiguration();
    QMessageBox::information(this, "Tools Enabled", "All 44 tools have been enabled.");
}

void EnterpriseToolsPanel::onBulkDisableClicked() {
    int reply = QMessageBox::question(this, "Disable All Tools",
        "This will disable all 44 tools. Continue?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        disableAllTools();
        saveConfiguration();
        QMessageBox::information(this, "Tools Disabled", "All 44 tools have been disabled.");
    }
}

void EnterpriseToolsPanel::onResetDefaultsClicked() {
    resetToDefaults();
}

void EnterpriseToolsPanel::onExportConfigClicked() {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Tool Configuration",
        "tools_config.json",
        "JSON Files (*.json)");
    
    if (!fileName.isEmpty()) {
        QJsonObject config = serializeConfiguration();
        QJsonDocument doc(config);
        
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();
            QMessageBox::information(this, "Export Complete", 
                "Tool configuration exported successfully.");
        }
    }
}

void EnterpriseToolsPanel::onImportConfigClicked() {
    QString fileName = QFileDialog::getOpenFileName(this,
        "Import Tool Configuration",
        "",
        "JSON Files (*.json)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull() && doc.isObject()) {
                deserializeConfiguration(doc.object());
                saveConfiguration();
                emit configurationChanged();
                QMessageBox::information(this, "Import Complete",
                    "Tool configuration imported successfully.");
            }
        }
    }
}

void EnterpriseToolsPanel::onRefreshStatsClicked() {
    QString stats;
    stats += "═══ ENTERPRISE TOOLS USAGE STATISTICS ═══\n\n";
    
    // Overall stats
    int totalEnabled = getEnabledTools().count();
    int totalDisabled = getDisabledTools().count();
    int totalExecutions = 0;
    int totalSuccesses = 0;
    int totalFailures = 0;
    
    for (const auto& tool : m_tools) {
        totalExecutions += tool.executionCount;
        totalSuccesses += tool.successCount;
        totalFailures += tool.failureCount;
    }
    
    stats += QString("Total Tools: 44\n");
    stats += QString("Enabled: %1 (%.1f%%)\n").arg(totalEnabled).arg(totalEnabled * 100.0 / 44);
    stats += QString("Disabled: %1 (%.1f%%)\n\n").arg(totalDisabled).arg(totalDisabled * 100.0 / 44);
    stats += QString("Total Executions: %1\n").arg(totalExecutions);
    stats += QString("Success Rate: %.1f%% (%2/%3)\n").arg(totalSuccesses * 100.0 / qMax(1, totalExecutions)).arg(totalSuccesses).arg(totalExecutions);
    stats += QString("Failure Rate: %.1f%% (%2/%3)\n\n").arg(totalFailures * 100.0 / qMax(1, totalExecutions)).arg(totalFailures).arg(totalExecutions);
    
    // Top 5 most used tools
    QList<ToolDefinition> sortedTools = m_tools.values();
    std::sort(sortedTools.begin(), sortedTools.end(), [](const ToolDefinition& a, const ToolDefinition& b) {
        return a.executionCount > b.executionCount;
    });
    
    stats += "Top 5 Most Used Tools:\n";
    for (int i = 0; i < qMin(5, sortedTools.count()); ++i) {
        const ToolDefinition& tool = sortedTools[i];
        stats += QString("  %1. %2 - %3 executions (%.1f%% success)\n")
            .arg(i + 1)
            .arg(tool.name)
            .arg(tool.executionCount)
            .arg(tool.successCount * 100.0 / qMax(1, tool.executionCount));
    }
    
    m_statsDisplay->setText(stats);
}

} // namespace RawrXD
