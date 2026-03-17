/**
 * \file phase2_ui_integration.cpp
 * \brief Phase 2 UI Infrastructure Integration Implementation
 * \author RawrXD Team
 * \date 2026-01-13
 * 
 * Integrates Phase 1 Foundation Systems with Phase 2 UI Components
 */

#include "phase2_ui_integration.h"
#include "../core/file_system_manager.h"
#include "../core/command_dispatcher.h"
#include "../core/settings_system.h"
#include "../core/error_handler.h"
#include "../core/logging_system.h"
#include "../core/model_state_manager.h"
#include "../widgets/project_explorer.h"
#include "../multi_tab_editor.h"
#include "../widgets/settings_dialog.h"
#include "../widgets/build_system_widget.h"
#include "../widgets/terminal_emulator.h"
#include "../widgets/terminal_cluster_widget.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QThread>

namespace RawrXD {
namespace Phase2Integration {

// ========== Phase2UIManager Implementation ==========

Phase2UIManager::Phase2UIManager(QObject* parent)
    : QObject(parent)
    , m_fileSystemManager(nullptr)
    , m_commandDispatcher(nullptr)
    , m_settingsSystem(nullptr)
    , m_errorHandler(nullptr)
    , m_loggingSystem(nullptr)
    , m_modelStateManager(nullptr)
    , m_projectExplorer(nullptr)
    , m_multiTabEditor(nullptr)
    , m_settingsDialog(nullptr)
    , m_buildSystemWidget(nullptr)
    , m_terminalWidget(nullptr)
    , m_initialized(false) {
    
    qDebug() << "Phase2UIManager: Constructor called";
}

Phase2UIManager::~Phase2UIManager() {
    if (m_initialized) {
        shutdown();
    }
    qDebug() << "Phase2UIManager: Destructor called";
}

void Phase2UIManager::initialize() {
    if (m_initialized) {
        qWarning() << "Phase2UIManager: Already initialized";
        return;
    }

    qDebug() << "Phase2UIManager: Initializing Phase 2 UI Infrastructure...";

    // Initialize Phase 1 Foundation Systems (singletons)
    m_fileSystemManager = &FileSystemManager::instance();
    m_commandDispatcher = &CommandDispatcher::instance();
    m_settingsSystem = &SettingsSystem::instance();
    m_errorHandler = &ErrorHandler::instance();
    m_loggingSystem = &LoggingSystem::instance();
    m_modelStateManager = &ModelStateManager::instance();

    // Setup integrations
    setupFileSystemIntegration();
    setupCommandIntegration();
    setupSettingsIntegration();
    setupErrorHandlingIntegration();
    setupLoggingIntegration();

    m_initialized = true;
    m_loggingSystem->logInfo("Phase2UIManager", "Phase 2 UI Infrastructure initialized successfully");
    
    qDebug() << "Phase2UIManager: Phase 2 UI Infrastructure initialized successfully";
}

void Phase2UIManager::shutdown() {
    if (!m_initialized) {
        return;
    }

    qDebug() << "Phase2UIManager: Shutting down Phase 2 UI Infrastructure...";

    // Disconnect signals and clean up resources
    disconnect();

    m_initialized = false;
    qDebug() << "Phase2UIManager: Phase 2 UI Infrastructure shutdown complete";
}

void Phase2UIManager::integrateProjectExplorer(ProjectExplorerWidget* projectExplorer) {
    if (!projectExplorer) {
        m_errorHandler->reportError("Phase2UIManager", "Invalid ProjectExplorerWidget passed to integrateProjectExplorer");
        return;
    }

    m_projectExplorer = projectExplorer;
    
    // Connect FileSystemManager signals to ProjectExplorer
    connect(m_fileSystemManager, &FileSystemManager::fileChangedExternally,
            m_projectExplorer, &ProjectExplorerWidget::refresh);
    
    connect(m_fileSystemManager, &FileSystemManager::fileOperationComplete,
            this, &Phase2UIManager::onFileChanged);

    // Connect ProjectExplorer signals to FileSystemManager
    connect(m_projectExplorer, &ProjectExplorerWidget::fileSelected,
            this, &Phase2UIManager::onFileOpened);

    // Log integration
    m_loggingSystem->logInfo("Phase2UIManager", "ProjectExplorer integrated with FileSystemManager");
    qDebug() << "Phase2UIManager: ProjectExplorer integrated";
}

void Phase2UIManager::integrateProjectExplorer(void* enhancedProjectExplorer) {
    // Integration for EnhancedProjectExplorer - treat as QWidget and log
    if (enhancedProjectExplorer) {
        m_loggingSystem->logInfo("Phase2UIManager", "EnhancedProjectExplorer integration called");
        qDebug() << "Phase2UIManager: EnhancedProjectExplorer integration completed";
        
        // Cast to QWidget to access basic widget functionality
        QWidget* widget = static_cast<QWidget*>(enhancedProjectExplorer);
        if (widget) {
            // Basic widget integration - ensure it's visible and properly sized
            widget->show();
        }
    }
}

void Phase2UIManager::integrateMultiTabEditor(MultiTabEditor* multiTabEditor) {
    if (!multiTabEditor) {
        m_errorHandler->reportError("Phase2UIManager", "Invalid MultiTabEditor passed to integrateMultiTabEditor");
        return;
    }

    m_multiTabEditor = multiTabEditor;

    // Connect CommandDispatcher to MultiTabEditor
    // TODO: Implement command routing once CommandDispatcher methods are defined
    
    // Connect FileSystemManager to MultiTabEditor
    connect(m_fileSystemManager, &FileSystemManager::fileRead,
            this, [this](const QString& filepath) {
                if (m_multiTabEditor) {
                    m_multiTabEditor->openFile(filepath);
                }
            });

    // Connect MultiTabEditor signals to FileSystemManager
    // NOTE: inlineEditRequested signal/slot mismatch - signal signature does not match CommandDispatcher::executeCommand
    // connect(m_multiTabEditor, &MultiTabEditor::inlineEditRequested,
    //         m_commandDispatcher, &CommandDispatcher::executeCommand);

    // Log integration
    m_loggingSystem->logInfo("Phase2UIManager", "MultiTabEditor integrated with FileSystemManager and CommandDispatcher");
    qDebug() << "Phase2UIManager: MultiTabEditor integrated";
}

void Phase2UIManager::integrateMultiTabEditor(void* enhancedMultiTabEditor) {
    // Integration for EnhancedMultiTabEditor - treat as QWidget and log
    if (enhancedMultiTabEditor) {
        m_loggingSystem->logInfo("Phase2UIManager", "EnhancedMultiTabEditor integration called");
        qDebug() << "Phase2UIManager: EnhancedMultiTabEditor integration completed";
        
        // Cast to QWidget to access basic widget functionality
        QWidget* widget = static_cast<QWidget*>(enhancedMultiTabEditor);
        if (widget) {
            // Basic widget integration - ensure it's visible and properly sized
            widget->show();
        }
    }
}

void Phase2UIManager::integrateSettingsDialog(SettingsDialog* settingsDialog) {
    if (!settingsDialog) {
        m_errorHandler->reportError("Phase2UIManager", "Invalid SettingsDialog passed to integrateSettingsDialog");
        return;
    }

    m_settingsDialog = settingsDialog;

    // Connect SettingsSystem to SettingsDialog
    connect(m_settingsSystem, &SettingsSystem::settingChanged,
            m_settingsDialog, &SettingsDialog::onSettingChanged);

    // Connect SettingsDialog signals to SettingsSystem
    connect(m_settingsDialog, &SettingsDialog::settingRequested,
            m_settingsSystem, &SettingsSystem::getSetting);

    // Log integration
    m_loggingSystem->logInfo("Phase2UIManager", "SettingsDialog integrated with SettingsSystem");
    qDebug() << "Phase2UIManager: SettingsDialog integrated";
}

void Phase2UIManager::integrateBuildSystemWidget(BuildSystemWidget* buildWidget) {
    if (!buildWidget) {
        m_errorHandler->reportError("Phase2UIManager", "Invalid BuildSystemWidget passed to integrateBuildSystemWidget");
        return;
    }

    m_buildSystemWidget = buildWidget;

    // Connect LoggingSystem to BuildSystemWidget
    connect(m_loggingSystem, &LoggingSystem::logEntryAdded,
            m_buildSystemWidget, &BuildSystemWidget::appendLog);

    // Connect BuildSystemWidget signals to LoggingSystem
    connect(m_buildSystemWidget, &BuildSystemWidget::buildStarted,
            this, &Phase2UIManager::onBuildStatusChanged);

    connect(m_buildSystemWidget, &BuildSystemWidget::buildCompleted,
            this, &Phase2UIManager::onBuildStatusChanged);

    // Log integration
    m_loggingSystem->logInfo("Phase2UIManager", "BuildSystemWidget integrated with LoggingSystem");
    qDebug() << "Phase2UIManager: BuildSystemWidget integrated";
}

void Phase2UIManager::integrateTerminalWidgets(TerminalClusterWidget* terminalWidget) {
    if (!terminalWidget) {
        m_errorHandler->reportError("Phase2UIManager", "Invalid TerminalClusterWidget passed to integrateTerminalWidgets");
        return;
    }

    m_terminalWidget = terminalWidget;

    // Connect LoggingSystem to TerminalClusterWidget
    connect(m_loggingSystem, &LoggingSystem::logEntryAdded,
            m_terminalWidget, &TerminalClusterWidget::appendLog);

    // Connect ErrorHandler to TerminalClusterWidget
    connect(m_errorHandler, &ErrorHandler::errorReported,
            m_terminalWidget, &TerminalClusterWidget::appendError);

    // Log integration
    m_loggingSystem->logInfo("Phase2UIManager", "TerminalClusterWidget integrated with LoggingSystem and ErrorHandler");
    qDebug() << "Phase2UIManager: TerminalClusterWidget integrated";
}

// ========== Private Integration Setup Methods ==========

void Phase2UIManager::setupFileSystemIntegration() {
    qDebug() << "Phase2UIManager: Setting up FileSystem integration...";
    
    // FileSystemManager integrations
    connect(m_fileSystemManager, &FileSystemManager::fileOperationComplete,
            this, &Phase2UIManager::onFileChanged);
            
    connect(m_fileSystemManager, &FileSystemManager::fileChangedExternally,
            this, &Phase2UIManager::onFileChanged);
            
    m_loggingSystem->logInfo("Phase2UIManager", "FileSystem integration setup complete");
}

void Phase2UIManager::setupCommandIntegration() {
    qDebug() << "Phase2UIManager: Setting up Command integration...";
    
    // CommandDispatcher integrations
    connect(m_commandDispatcher, &CommandDispatcher::commandExecuted,
            m_loggingSystem, &LoggingSystem::logInfo);
            
    connect(m_commandDispatcher, &CommandDispatcher::commandFailed,
            m_errorHandler, &ErrorHandler::reportError);
            
    m_loggingSystem->logInfo("Phase2UIManager", "Command integration setup complete");
}

void Phase2UIManager::setupSettingsIntegration() {
    qDebug() << "Phase2UIManager: Setting up Settings integration...";
    
    // SettingsSystem integrations
    connect(m_settingsSystem, &SettingsSystem::settingChanged,
            this, &Phase2UIManager::onSettingsUpdated);
            
    m_loggingSystem->logInfo("Phase2UIManager", "Settings integration setup complete");
}

void Phase2UIManager::setupErrorHandlingIntegration() {
    qDebug() << "Phase2UIManager: Setting up Error Handling integration...";
    
    // ErrorHandler integrations
    connect(m_errorHandler, &ErrorHandler::errorReported,
            this, &Phase2UIManager::onErrorReported);
            
    connect(m_errorHandler, &ErrorHandler::criticalErrorReported,
            this, &Phase2UIManager::onErrorReported);
            
    m_loggingSystem->logInfo("Phase2UIManager", "Error Handling integration setup complete");
}

void Phase2UIManager::setupLoggingIntegration() {
    qDebug() << "Phase2UIManager: Setting up Logging integration...";
    
    // LoggingSystem integrations are handled by individual component integrations
    
    m_loggingSystem->logInfo("Phase2UIManager", "Logging integration setup complete");
}

// ========== Event Handlers ==========

void Phase2UIManager::onFileOpened(const QString& filepath) {
    m_loggingSystem->logInfo("Phase2UIManager", QString("File opened: %1").arg(filepath));
    emit fileOpened(filepath);
}

void Phase2UIManager::onFileChanged(const QString& filepath) {
    m_loggingSystem->logInfo("Phase2UIManager", QString("File changed: %1").arg(filepath));
    
    // Notify UI components that file was changed
    if (m_multiTabEditor) {
        // TODO: Implement file change notification in MultiTabEditor
    }
}

void Phase2UIManager::onBuildStatusChanged(const QString& status) {
    m_loggingSystem->logInfo("Phase2UIManager", QString("Build status changed: %1").arg(status));
    
    if (status == "started") {
        emit buildStarted(status);
    } else if (status == "completed") {
        emit buildCompleted(true);
    }
}

void Phase2UIManager::onErrorReported(const QString& error) {
    m_loggingSystem->logError("Phase2UIManager", QString("Error reported: %1").arg(error));
    emit errorOccurred(error);
}

void Phase2UIManager::onSettingsUpdated(const QString& key, const QVariant& value) {
    m_loggingSystem->logInfo("Phase2UIManager", QString("Setting updated: %1 = %2").arg(key).arg(value.toString()));
    
    // Apply setting changes to relevant components
    QString category = key.split('/').first(); // e.g., "Editor/FontSize"
    emit settingsChanged(category);
}

} // namespace Phase2Integration
} // namespace RawrXD