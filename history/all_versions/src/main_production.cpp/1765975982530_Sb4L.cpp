#include "enhanced_main_window.h"
#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QStyleFactory>

/**
 * @file main.cpp
 * @brief Entry point for RawrXD Agentic IDE with Production-Ready Features
 * 
 * This demonstrates:
 * - Initialization of enhanced main window
 * - Features panel integration
 * - Logging and telemetry setup
 * - Stylesheet application
 */

// Custom stylesheet for production-ready UI
const char* PRODUCTION_STYLESHEET = R"(
    * {
        color: #333333;
        background-color: #ffffff;
    }
    
    QMainWindow {
        background-color: #f5f5f5;
    }
    
    QMenuBar {
        background-color: #ffffff;
        border-bottom: 1px solid #e0e0e0;
        padding: 4px;
    }
    
    QMenuBar::item:selected {
        background-color: #e3f2fd;
    }
    
    QMenu {
        background-color: #ffffff;
        border: 1px solid #e0e0e0;
        border-radius: 4px;
    }
    
    QMenu::item:selected {
        background-color: #0078d4;
        color: #ffffff;
    }
    
    QToolBar {
        background-color: #ffffff;
        border: 1px solid #e0e0e0;
        border-radius: 0px;
        spacing: 3px;
        padding: 6px;
    }
    
    QToolBar::handle {
        background: #cccccc;
    }
    
    QDockWidget {
        background-color: #f5f5f5;
        border: 1px solid #e0e0e0;
    }
    
    QDockWidget::title {
        background-color: #eeeeee;
        padding: 4px;
        border: 1px solid #e0e0e0;
        font-weight: bold;
    }
    
    QTreeWidget {
        background-color: #ffffff;
        border: 1px solid #e0e0e0;
        border-radius: 4px;
        gridline-color: #f0f0f0;
    }
    
    QTreeWidget::item {
        padding: 4px;
        border: none;
    }
    
    QTreeWidget::item:hover {
        background-color: #f0f0f0;
    }
    
    QTreeWidget::item:selected {
        background-color: #0078d4;
        color: #ffffff;
    }
    
    QLineEdit, QSpinBox, QComboBox {
        background-color: #ffffff;
        border: 1px solid #cccccc;
        border-radius: 4px;
        padding: 4px;
        selection-background-color: #0078d4;
    }
    
    QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
        border: 2px solid #0078d4;
    }
    
    QPushButton {
        background-color: #f0f0f0;
        border: 1px solid #cccccc;
        border-radius: 4px;
        padding: 4px 12px;
        font-weight: 500;
    }
    
    QPushButton:hover {
        background-color: #e8e8e8;
    }
    
    QPushButton:pressed {
        background-color: #0078d4;
        color: #ffffff;
        border: 1px solid #0078d4;
    }
    
    QPushButton:default {
        background-color: #0078d4;
        color: #ffffff;
        border: 1px solid #0078d4;
    }
    
    QStatusBar {
        background-color: #ffffff;
        border-top: 1px solid #e0e0e0;
        padding: 4px;
    }
    
    QLabel {
        color: #333333;
    }
    
    QCheckBox, QRadioButton {
        spacing: 4px;
        margin: 2px;
    }
    
    QCheckBox:hover, QRadioButton:hover {
        background-color: #f0f0f0;
    }
    
    QScrollBar:vertical {
        background-color: #f5f5f5;
        width: 12px;
        border-radius: 6px;
    }
    
    QScrollBar::handle:vertical {
        background-color: #c0c0c0;
        border-radius: 6px;
        min-height: 20px;
    }
    
    QScrollBar::handle:vertical:hover {
        background-color: #a0a0a0;
    }
    
    QScrollBar:horizontal {
        background-color: #f5f5f5;
        height: 12px;
        border-radius: 6px;
    }
    
    QScrollBar::handle:horizontal {
        background-color: #c0c0c0;
        border-radius: 6px;
        min-width: 20px;
    }
    
    QScrollBar::handle:horizontal:hover {
        background-color: #a0a0a0;
    }
)";

void setupLogging() {
    // Install message handler for debug logging
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        QString formattedMsg;
        
        switch (type) {
            case QtDebugMsg:
                formattedMsg = QString("[DEBUG] %1").arg(msg);
                break;
            case QtInfoMsg:
                formattedMsg = QString("[INFO] %1").arg(msg);
                break;
            case QtWarningMsg:
                formattedMsg = QString("[WARNING] %1:%2 %3").arg(context.file).arg(context.line).arg(msg);
                break;
            case QtCriticalMsg:
                formattedMsg = QString("[CRITICAL] %1:%2 %3").arg(context.file).arg(context.line).arg(msg);
                break;
            case QtFatalMsg:
                formattedMsg = QString("[FATAL] %1:%2 %3").arg(context.file).arg(context.line).arg(msg);
                abort();
        }
        
        fprintf(stderr, "%s\n", formattedMsg.toStdString().c_str());
        fflush(stderr);
    });
}

void setupApplicationStyle(QApplication &app) {
    // Set application style
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // Apply custom stylesheet
    app.setStyleSheet(PRODUCTION_STYLESHEET);
    
    // Set application metadata
    app.setApplicationName("RawrXD");
    app.setApplicationVersion("3.0-Production");
    app.setApplicationAuthor("ItsMehRAWRXD");
    app.setApplicationOrganization("RawrXD");
    
    qDebug() << "[Main] Application style set to Fusion with custom theme";
}

int main(int argc, char *argv[]) {
    // High-DPI scaling support
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    QApplication app(argc, argv);
    
    // Setup logging system
    setupLogging();
    
    qDebug() << "=============================================================";
    qDebug() << "RawrXD Agentic IDE - Production Ready";
    qDebug() << "Version: 3.0";
    qDebug() << "Build Date:" << __DATE__ << __TIME__;
    qDebug() << "=============================================================";
    
    // Setup application style and theme
    setupApplicationStyle(app);
    
    try {
        // Create and show main window
        EnhancedMainWindow window;
        
        qDebug() << "[Main] Enhanced main window created successfully";
        qDebug() << "[Main] Features panel integrated";
        qDebug() << "[Main] Showing main window...";
        
        window.show();
        
        // Enter event loop
        return app.exec();
        
    } catch (const std::exception &e) {
        qCritical() << "[Main] Exception:" << e.what();
        return 1;
    } catch (...) {
        qCritical() << "[Main] Unknown exception occurred";
        return 1;
    }
}
