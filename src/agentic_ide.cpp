#include "agentic_ide.h"
#include "telemetry.h"
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
        });
    }
}
