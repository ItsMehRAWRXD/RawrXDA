#include "agentic_ide.h"
#include "telemetry.h"
#include "settings.h"
#include "multi_tab_editor.h"
#include "chat_interface.h"
#include <QTimer>
#include <QShowEvent>

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
        });
    }
}
