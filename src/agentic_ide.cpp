#include "agentic_ide.h"
#include "telemetry.h"
#include "settings.h"
#include "multi_tab_editor.h"
#include "chat_interface.h"
#include "agentic_engine.h"
#include "terminal_pool.h"
#include <QTimer>
#include <QShowEvent>
#include <QDockWidget>

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
