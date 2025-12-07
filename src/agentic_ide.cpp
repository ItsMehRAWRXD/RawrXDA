#include "agentic_ide.h"
#include "telemetry.h"
#include "settings.h"
#include "multi_tab_editor.h"
#include "chat_interface.h"
#include "agentic_engine.h"
#include "terminal_pool.h"
#include "plan_orchestrator.h"
#include "lsp_client.h"
#include <QTimer>
#include <QShowEvent>
#include <QDockWidget>
#include <QDir>

// Lightweight constructor - no heavy initialization
AgenticIDE::AgenticIDE(QWidget *parent) : QMainWindow(parent) {
    // All initialization deferred to showEvent
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
            
            // Initialize chat interface (Qt widgets + model scanning)
            if (!m_chatInterface) {
                m_chatInterface = new ChatInterface(this);
                m_chatInterface->initialize();
                // Connect chat to engine and orchestrator
                if (m_agenticEngine) {
                    m_chatInterface->setAgenticEngine(m_agenticEngine);
                }
                if (m_planOrchestrator) {
                    m_chatInterface->setPlanOrchestrator(m_planOrchestrator);
                }
                // Chat will be added as dock widget later
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
            
            // Initialize terminal pool (Qt widgets + QProcess spawning)
            if (!m_terminalPool) {
                m_terminalPool = new TerminalPool(2, this);  // 2 terminals by default
                m_terminalPool->initialize();
                
                // Add terminal as dock widget
                m_terminalDock = new QDockWidget("Terminal Pool", this);
                m_terminalDock->setWidget(m_terminalPool);
                addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
            }
        });
    }
}
