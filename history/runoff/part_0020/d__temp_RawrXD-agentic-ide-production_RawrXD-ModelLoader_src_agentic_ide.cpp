#include "agentic_ide.h"
#include "telemetry.h"
#include "settings.h"
#include "multi_tab_editor.h"
#include "chat_interface.h"
#include "agentic_engine.h"
#include "terminal_pool.h"
#include "plan_orchestrator.h"
#include "lsp_client.h"
#include "zero_day_agentic_engine.hpp"
#include "universal_model_router.h"
#include "tool_registry.hpp"
#include "logging/logger.h"
#include "metrics/metrics.h"
#include "ui/agentic_browser.h"
#include "ai_code_assistant.h"
#include "ai_code_assistant_panel.h"
#include "autonomous_feature_engine.h"
#include "autonomous_model_manager.h"
#include "autonomous_widgets.h"
#include "Memory/streaming_gguf_memory_manager.hpp"
#include <QTimer>
#include <QShowEvent>
#include <QDockWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QAction>
#include <QIcon>
#include <QDir>
#include <QDateTime>

// Static instance pointer
AgenticIDE* AgenticIDE::s_instance = nullptr;

// Lightweight constructor - no heavy initialization
AgenticIDE::AgenticIDE(QWidget *parent) : QMainWindow(parent) {
    // All initialization deferred to showEvent
    s_instance = this;
}

AgenticIDE* AgenticIDE::instance() {
    return s_instance;
}

AgenticIDE::~AgenticIDE() = default;

void AgenticIDE::showEvent(QShowEvent *ev) {
    QMainWindow::showEvent(ev);
    
    // Defer heavy initialization until after Qt event loop is running
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        QTimer::singleShot(0, this, [this] {
            setWindowTitle("RawrXD Agentic IDE - Production Ready");
            resize(1200, 800);

            // Main toolbar for quick actions
            if (!m_mainToolbar) {
                m_mainToolbar = addToolBar("Main");
                QAction* actNewFile = m_mainToolbar->addAction("New File");
                QAction* actCloseFile = m_mainToolbar->addAction("Close File");
                m_mainToolbar->addSeparator();
                QAction* actNewChat = m_mainToolbar->addAction("New Chat");
                QAction* actCloseChat = m_mainToolbar->addAction("Close Chat");
                connect(actNewFile, &QAction::triggered, this, &AgenticIDE::onNewEditorTab);
                connect(actCloseFile, &QAction::triggered, this, &AgenticIDE::onCloseEditorTab);
                connect(actNewChat, &QAction::triggered, this, &AgenticIDE::onNewChatTab);
                connect(actCloseChat, &QAction::triggered, this, &AgenticIDE::onCloseChatTab);
            }
            
            // Initialize telemetry hardware monitoring (COM/PDH/WMI)
            // Safe to do now that QApplication is fully running
            static Telemetry telemetry;
            telemetry.initializeHardware();
            
            // Initialize settings (QSettings registry/file access)
            static Settings settings;
            settings.initialize();
            
            // Initialize multi-tab editor (Qt widgets creation)
            if (!m_multiTabEditor) {
                m_multiTabEditor = new MultiTabEditor(this);
                m_multiTabEditor->initialize();
                setCentralWidget(m_multiTabEditor);
            }
            
            // Initialize agentic engine (InferenceEngine + AI core)
            if (!m_agenticEngine) {
                m_agenticEngine = new AgenticEngine(this);
                m_agenticEngine->initialize();
                // Connect chat to engine
                if (m_chatInterface) {
                    m_chatInterface->setAgenticEngine(m_agenticEngine);
                }
            }
            
            // Initialize LSP client for smart operations
            if (!m_lspClient) {
                RawrXD::LSPServerConfig config;
                config.language = "cpp";
                config.command = "clangd";  // Assumes clangd in PATH
                config.arguments = QStringList{"--background-index", "--clang-tidy"};
                config.workspaceRoot = QDir::currentPath();  // TODO: Use actual project root
                config.autoStart = false;  // Don't auto-start until explicitly needed
                
                m_lspClient = new RawrXD::LSPClient(config, this);
                m_lspClient->initialize();
                if (m_multiTabEditor) {
                    m_multiTabEditor->setLSPClient(m_lspClient);
                }
            }
            
            // Initialize PlanOrchestrator (multi-file AI orchestration)
            if (!m_planOrchestrator) {
                m_planOrchestrator = new RawrXD::PlanOrchestrator(this);
                m_planOrchestrator->initialize();
                m_planOrchestrator->setLSPClient(m_lspClient);
                
                // Wire to chat interface for /refactor command
                if (m_chatInterface) {
                    m_chatInterface->setPlanOrchestrator(m_planOrchestrator);
                    
                    // Connect progress signals for real-time updates
                    connect(m_planOrchestrator, &RawrXD::PlanOrchestrator::planningStarted,
                            m_chatInterface, [this](const QString& prompt) {
                                m_chatInterface->addMessage("System", "📋 Planning: " + prompt);
                            });
                    
                    connect(m_planOrchestrator, &RawrXD::PlanOrchestrator::executionStarted,
                            m_chatInterface, [this](int taskCount) {
                                m_chatInterface->addMessage("System", 
                                    QString("🚀 Executing %1 tasks...").arg(taskCount));
                            });
                    
                    connect(m_planOrchestrator, &RawrXD::PlanOrchestrator::taskExecuted,
                            m_chatInterface, [this](int index, bool success, const QString& desc) {
                                QString status = success ? "✓" : "✗";
                                QString color = success ? "#4ec9b0" : "#f48771";
                                m_chatInterface->addMessage("Task", 
                                    QString("<span style='color:%1;'>%2 [%3] %4</span>")
                                        .arg(color).arg(status).arg(index + 1).arg(desc));
                            });
                }
                
                // Wire InferenceEngine from AgenticEngine
                if (m_agenticEngine) {
                    // Note: InferenceEngine is internal to AgenticEngine
                    // For now, PlanOrchestrator will use its own inference
                    // TODO: Expose InferenceEngine getter in AgenticEngine
                }
                // TODO: Set workspace root from current project
                // m_planOrchestrator->setWorkspaceRoot(projectRoot);
            }

            // Initialize tool registry and model router for Zero-Day engine
            if (!m_toolRegistry) {
                auto logger = std::make_shared<Logger>("tool_registry_ui");
                auto metrics = std::make_shared<Metrics>();
                m_toolRegistry = new ToolRegistry(logger, metrics);
            }

            if (!m_modelRouter) {
                m_modelRouter = new UniversalModelRouter(this);
                m_modelRouter->initializeLocalEngine(QString());
                m_modelRouter->initializeCloudClient();
            }

            if (!m_zeroDayAgent && m_modelRouter && m_toolRegistry && m_planOrchestrator) {
               m_zeroDayAgent = new ZeroDayAgenticEngine(m_modelRouter, m_toolRegistry, m_planOrchestrator, this);
               if (m_chatInterface) {
                   m_chatInterface->setZeroDayAgent(m_zeroDayAgent);
                   connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentStream,
                           m_chatInterface, [this](const QString& tok) {
                               m_chatInterface->addMessage("System", tok);
                           });
                    connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentComplete,
                            m_chatInterface, [this](const QString& summary) {
                                m_chatInterface->addMessage("System", summary);
                            });
                    connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentError,
                            m_chatInterface, [this](const QString& err) {
                                m_chatInterface->addMessage("System", "⚠ " + err);
                            });
                }
            }

            // Initialize chat interface (Qt widgets + model scanning)
            if (!m_chatInterface) {
                m_chatInterface = new ChatInterface(this);
                m_chatInterface->initialize();
                if (m_agenticEngine) {
                    m_chatInterface->setAgenticEngine(m_agenticEngine);
                }
                if (m_planOrchestrator) {
                    m_chatInterface->setPlanOrchestrator(m_planOrchestrator);
                }
                if (m_zeroDayAgent) {
                    m_chatInterface->setZeroDayAgent(m_zeroDayAgent);
                    connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentStream,
                            m_chatInterface, [this](const QString& tok) {
                                m_chatInterface->addMessage("System", tok);
                            });
                    connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentComplete,
                            m_chatInterface, [this](const QString& summary) {
                                m_chatInterface->addMessage("System", summary);
                            });
                    connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentError,
                            m_chatInterface, [this](const QString& err) {
                                m_chatInterface->addMessage("System", "⚠ " + err);
                            });
                }

                // Create chat tab dock and add first tab
                if (!m_chatTabs) {
                    m_chatTabs = new QTabWidget(this);
                    m_chatTabs->setTabsClosable(true);
                    m_chatTabs->setMovable(true);
                    connect(m_chatTabs, &QTabWidget::tabCloseRequested, this, [this](int index){
                        if (index < 0) return;
                        QWidget* w = m_chatTabs->widget(index);
                        m_chatTabs->removeTab(index);
                        if (w) w->deleteLater();
                        if (m_chatTabs->count() == 0) onNewChatTab();
                    });

                    // Add '+' button to create new chat tabs
                    auto* plus = new QToolButton(m_chatTabs);
                    plus->setText("+");
                    plus->setToolTip("New Chat Tab (Ctrl+Shift+N)");
                    connect(plus, &QToolButton::clicked, this, &AgenticIDE::onNewChatTab);
                    m_chatTabs->setCornerWidget(plus, Qt::TopRightCorner);

                    m_chatDock = new QDockWidget("Chat", this);
                    m_chatDock->setWidget(m_chatTabs);
                    addDockWidget(Qt::RightDockWidgetArea, m_chatDock);
                }

                // Add the initial chat tab
                int tabIndex = m_chatTabs->addTab(m_chatInterface, "Chat 1");
                m_chatTabs->setCurrentIndex(tabIndex);

                // Wire model selection to AgenticEngine small-model loader
                if (m_agenticEngine) {
                    connect(m_chatInterface, &ChatInterface::modelSelected,
                            m_agenticEngine, &AgenticEngine::setModel);
                }
            }
            
            // Initialize terminal pool (Qt widgets + QProcess spawning)
            if (!m_terminalPool) {
                m_terminalPool = new TerminalPool(2, this);  // 2 terminals by default
                m_terminalPool->initialize();
                
                // Add terminal as dock widget
                m_terminalDock = new QDockWidget("Terminal Pool", this);
                m_terminalDock->setWidget(m_terminalPool);
                addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
            }

            // ============================================================
            // Initialize AI Code Assistant (Phase 4 - Agentic IDE Integration)
            // ============================================================
            if (!m_aiCodeAssistant) {
                m_aiCodeAssistant = new AICodeAssistant(this);
                
                // Configure AI Code Assistant
                m_aiCodeAssistant->setOllamaServer("localhost", 11434);
                m_aiCodeAssistant->setWorkspaceRoot(QDir::currentPath());
                m_aiCodeAssistant->setModel("ministral-3");
                m_aiCodeAssistant->setTemperature(0.7f);
                m_aiCodeAssistant->setMaxTokens(2048);

                qDebug() << QString("[%1] AICodeAssistant initialized with workspace: %2")
                        .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                        .arg(QDir::currentPath());
            }

            // Initialize AI Code Assistant Panel (Phase 4 - UI Widget)
            if (!m_aiPanel) {
                m_aiPanel = new AICodeAssistantPanel(this);
                m_aiPanel->setAssistant(m_aiCodeAssistant);

                // Create dock widget for AI panel
                m_aiPanelDock = new QDockWidget("AI Code Assistant", this);
                m_aiPanelDock->setWidget(m_aiPanel);
                addDockWidget(Qt::RightDockWidgetArea, m_aiPanelDock);

                // Hide by default, show on demand
                m_aiPanelDock->hide();

                qDebug() << QString("[%1] AICodeAssistantPanel integrated into AgenticIDE")
                        .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
            }

            // Initialize agentic browser dock (sandboxed, no-JS)
            if (!m_browser) {
                m_browser = new AgenticBrowser(this);
                m_browserDock = new QDockWidget("Agentic Browser", this);
                m_browserDock->setWidget(m_browser);
                addDockWidget(Qt::RightDockWidgetArea, m_browserDock);
                
                // Wire browser to chat interface for feedback
                if (m_chatInterface) {
                    m_chatInterface->setBrowser(m_browser);
                    connect(m_browser, &AgenticBrowser::navigationFinished,
                            m_chatInterface, [this](const QUrl& url, bool success, int status, qint64 bytes) {
                                m_chatInterface->onBrowserNavigated(url, success, status);
                            });
                }
            }

            // Autonomous subsystems disabled for this build to unblock linker
#if 0
            // ... original autonomous initialization ...
#endif

            // Note: Zero-Day agent is now initialized and ready
            // User can trigger missions manually via ChatInterface
            // Auto-start is disabled to avoid blocking on model load
        });
    }
}

void AgenticIDE::onNewChatTab()
{
    // Create a fresh ChatInterface, wire it like the first one
    auto* chat = new ChatInterface(this);
    chat->initialize();
    if (m_agenticEngine) chat->setAgenticEngine(m_agenticEngine);
    if (m_planOrchestrator) chat->setPlanOrchestrator(m_planOrchestrator);
    if (m_zeroDayAgent) {
        chat->setZeroDayAgent(m_zeroDayAgent);
        connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentStream, chat, [chat](const QString& tok){ chat->addMessage("System", tok); });
        connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentComplete, chat, [chat](const QString& s){ chat->addMessage("System", s); });
        connect(m_zeroDayAgent, &ZeroDayAgenticEngine::agentError, chat, [chat](const QString& e){ chat->addMessage("System", "⚠ " + e); });
    }
    if (m_agenticEngine) connect(chat, &ChatInterface::modelSelected, m_agenticEngine, &AgenticEngine::setModel);
    int n = m_chatTabs ? m_chatTabs->count() + 1 : 1;
    if (m_chatTabs) {
        int idx = m_chatTabs->addTab(chat, QString("Chat %1").arg(n));
        m_chatTabs->setCurrentIndex(idx);
    }
}

void AgenticIDE::onCloseChatTab()
{
    if (!m_chatTabs) return;
    int idx = m_chatTabs->currentIndex();
    if (idx < 0) return;
    QWidget* w = m_chatTabs->widget(idx);
    m_chatTabs->removeTab(idx);
    if (w) w->deleteLater();
    if (m_chatTabs->count() == 0) onNewChatTab();
}

void AgenticIDE::onNewEditorTab()
{
    if (m_multiTabEditor) m_multiTabEditor->newFile();
}

void AgenticIDE::onCloseEditorTab()
{
    if (m_multiTabEditor) m_multiTabEditor->closeCurrentTab();
}

            // Autonomous subsystems temporarily disabled to resolve linker issues
            // (FeatureEngine, ModelManager, StreamingMemory, Suggestion/Security/Optimization docks).
void AgenticIDE::onAIGrepCompleted(const QJsonArray &results)
{
    qDebug() << QString("[%1] AgenticIDE::onAIGrepCompleted - Found %2 matches")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                    .arg(results.count());

    // Results are already displayed in AI panel via signals
}

void AgenticIDE::onAICommandCompleted(int exitCode)
{
    qDebug() << QString("[%1] AgenticIDE::onAICommandCompleted - Exit code: %2")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                    .arg(exitCode);

    // Results are already displayed in AI panel via signals
}

void AgenticIDE::onAIAnalysisCompleted(const QString &recommendation)
{
    qDebug() << QString("[%1] AgenticIDE::onAIAnalysisCompleted - Recommendation length: %2")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                    .arg(recommendation.length());

    // Results are already displayed in AI panel via signals
}

void AgenticIDE::onAIAutofixCompleted(const QString &fixedCode)
{
    qDebug() << QString("[%1] AgenticIDE::onAIAutofixCompleted - Fixed code length: %2")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                    .arg(fixedCode.length());

    // Results are already displayed in AI panel via signals
    // Optionally apply fix to editor if in focus
}

void AgenticIDE::onAIError(const QString &error)
{
    qWarning() << QString("[%1] AgenticIDE::onAIError - %2")
                    .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                    .arg(error);

    // Error is already displayed in AI panel via signals
}

// ============================================================
// Autonomous Feature Handlers
// ============================================================

void AgenticIDE::onAutonomousSuggestionAccepted(const QString& suggestionId)
{
    Q_UNUSED(suggestionId);
}

void AgenticIDE::onAutonomousSuggestionRejected(const QString& suggestionId)
{
    Q_UNUSED(suggestionId);
}

void AgenticIDE::onSecurityIssueFixed(const QString& issueId)
{
    Q_UNUSED(issueId);
}

void AgenticIDE::onSecurityIssueIgnored(const QString& issueId)
{
    Q_UNUSED(issueId);
}

void AgenticIDE::onOptimizationApplied(const QString& optimizationId)
{
    Q_UNUSED(optimizationId);
}

void AgenticIDE::onOptimizationDismissed(const QString& optimizationId)
{
    Q_UNUSED(optimizationId);
}
