#include "masm_integration_manager.h"
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QShortcut>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QDebug>

MASMIntegrationManager::MASMIntegrationManager(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent), m_mainWindow(mainWindow)
{
}

MASMIntegrationManager::~MASMIntegrationManager() = default;

void MASMIntegrationManager::initialize()
{
    // Create all components
    m_streamingManager = new StreamingTokenManager(this);
    m_modelRouter = new ModelRouter(this);
    m_toolRegistry = new MasmToolRegistry(this);
    m_aiClient = new AIModelClient(this);
    m_agenticPlanner = new AgenticPlanner(m_toolRegistry, m_modelRouter, m_aiClient, this);
    m_commandPalette = new CommandPalette(m_mainWindow);
    m_diffViewer = new DiffViewer(m_mainWindow);

    // Register built-in tools
    m_toolRegistry->registerBuiltInTools();

    // Wire up UI components
    wireUIComponents();
    setupMenus();
    setupKeyboardShortcuts();

    // Connect signals
    connect(m_agenticPlanner, &AgenticPlanner::taskFinished,
            this, &MASMIntegrationManager::onTaskFinished);
    connect(m_agenticPlanner, &AgenticPlanner::stepFinished,
            this, &MASMIntegrationManager::onStepFinished);
    connect(m_diffViewer, &DiffViewer::accepted,
            this, &MASMIntegrationManager::onDiffAccepted);

    qDebug() << "MASM Integration Manager initialized successfully!";
}

void MASMIntegrationManager::wireUIComponents()
{
    if (!m_mainWindow) return;

    // Create central widget with layout if needed
    QWidget* centralWidget = m_mainWindow->centralWidget();
    if (!centralWidget) {
        centralWidget = new QWidget(m_mainWindow);
        m_mainWindow->setCentralWidget(centralWidget);
    }

    // Find or create chat panel
    m_chatPanel = m_mainWindow->findChild<QTextEdit*>("ChatPanel");
    if (!m_chatPanel) {
        m_chatPanel = new QTextEdit(centralWidget);
        m_chatPanel->setObjectName("ChatPanel");
        m_chatPanel->setReadOnly(false);

        QVBoxLayout* layout = new QVBoxLayout(centralWidget);
        layout->addWidget(m_chatPanel);
        centralWidget->setLayout(layout);
    }

    // Initialize streaming manager with chat panel
    m_streamingManager->initialize(m_mainWindow, m_chatPanel);
}

void MASMIntegrationManager::setupMenus()
{
    if (!m_mainWindow) return;

    QMenuBar* menuBar = m_mainWindow->menuBar();

    // Create "AI" menu
    QMenu* aiMenu = menuBar->addMenu("&AI");

    // Task execution
    QAction* executeTaskAction = aiMenu->addAction("Execute Agentic &Task");
    connect(executeTaskAction, &QAction::triggered, [this]() {
        m_agenticPlanner->executeTask("User-defined task");
    });

    // Model modes
    QMenu* modeMenu = aiMenu->addMenu("Model &Modes");

    QAction* maxModeAction = modeMenu->addAction("&Max Mode");
    maxModeAction->setCheckable(true);
    connect(maxModeAction, &QAction::triggered, [this, maxModeAction]() {
        if (maxModeAction->isChecked()) {
            m_modelRouter->toggleMode(ModelRouter::MODE_MAX);
        }
        emit modeChanged(m_modelRouter->getMode());
    });

    QAction* searchWebAction = modeMenu->addAction("Search &Web");
    searchWebAction->setCheckable(true);
    connect(searchWebAction, &QAction::triggered, [this, searchWebAction]() {
        if (searchWebAction->isChecked()) {
            m_modelRouter->toggleMode(ModelRouter::MODE_SEARCH_WEB);
        }
        emit modeChanged(m_modelRouter->getMode());
    });

    QAction* turboAction = modeMenu->addAction("&Turbo");
    turboAction->setCheckable(true);
    connect(turboAction, &QAction::triggered, [this, turboAction]() {
        if (turboAction->isChecked()) {
            m_modelRouter->toggleMode(ModelRouter::MODE_TURBO);
        }
        emit modeChanged(m_modelRouter->getMode());
    });

    // Thinking UI
    aiMenu->addSeparator();
    QAction* thinkingAction = aiMenu->addAction("Toggle &Thinking UI");
    thinkingAction->setShortcut(QKeySequence("Ctrl+T"));
    connect(thinkingAction, &QAction::triggered, [this]() {
        m_streamingManager->setThinkingEnabled(!m_streamingManager->isThinkingEnabled());
    });

    // Tools
    aiMenu->addSeparator();
    QMenu* toolsMenu = aiMenu->addMenu("&Tools");
    for (const auto& toolName : m_toolRegistry->getToolNames()) {
        toolsMenu->addAction(toolName);
    }
}

void MASMIntegrationManager::setupKeyboardShortcuts()
{
    // Cmd-K (Command Palette) - Ctrl+Shift+P
    new QShortcut(QKeySequence("Ctrl+Shift+P"), m_mainWindow, [this]() {
        onCmdKTriggered();
    });

    // Thinking UI - Ctrl+T
    new QShortcut(QKeySequence("Ctrl+T"), m_mainWindow, [this]() {
        m_streamingManager->setThinkingEnabled(!m_streamingManager->isThinkingEnabled());
    });

    // Quick execute - Ctrl+Enter
    new QShortcut(QKeySequence("Ctrl+Return"), m_mainWindow, [this]() {
        if (m_chatPanel) {
            QString selectedText = m_chatPanel->textCursor().selectedText();
            if (!selectedText.isEmpty()) {
                m_agenticPlanner->executeTask(selectedText);
            }
        }
    });
}

void MASMIntegrationManager::onTaskFinished(const QString& result)
{
    qDebug() << "Task finished with result:" << result;
    if (m_chatPanel) {
        m_chatPanel->append("\n[Task Complete]\n" + result);
    }
    emit taskFinished(result);
}

void MASMIntegrationManager::onStepFinished(int index, const QJsonObject& result)
{
    qDebug() << "Step" << index << "finished:" << result;
    if (m_chatPanel) {
        m_chatPanel->append(QString("[Step %1] %2").arg(index).arg(
            result.value("success").toBool() ? "Success" : "Failed"
        ));
    }
}

void MASMIntegrationManager::onDiffAccepted(const QString& filePath, const QString& newContent)
{
    qDebug() << "Diff accepted for:" << filePath;
    // Execute file write tool
    QJsonObject params;
    params["path"] = filePath;
    params["content"] = newContent;
    m_toolRegistry->executeTool("file_write", params);
}

void MASMIntegrationManager::onCmdKTriggered()
{
    m_commandPalette->showPalette();
}
