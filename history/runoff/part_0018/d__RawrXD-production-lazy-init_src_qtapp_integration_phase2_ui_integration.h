#pragma once
/**
 * \file phase2_ui_integration.h
 * \brief Phase 2 UI Infrastructure Integration
 * \author RawrXD Team  
 * \date 2026-01-13
 * 
 * Integrates Phase 1 Foundation Systems with Phase 2 UI Components:
 * - FileSystemManager ↔ ProjectExplorer (File Browser)
 * - CommandDispatcher ↔ MultiTabEditor (Code Editor)
 * - SettingsSystem ↔ SettingsDialog (Settings UI)
 * - LoggingSystem ↔ Build/Debug widgets (Build System Integration)
 * - ErrorHandler ↔ Error Recovery UI (Error Handling)
 * - LoggingSystem ↔ Terminal widgets (Debug Console)
 */

#include <QObject>
#include <QString>
#include <QWidget>
#include <memory>

// Phase 1 Foundation Systems
#include "../core/file_system_manager.h"
#include "../core/command_dispatcher.h"
#include "../core/settings_system.h"
#include "../core/error_handler.h"
#include "../core/logging_system.h"
#include "../core/model_state_manager.h"

// Phase 2 UI Components
#include "../widgets/project_explorer.h"
#include "../multi_tab_editor.h"
#include "../widgets/settings_dialog.h"
#include "../widgets/build_system_widget.h"
#include "../widgets/terminal_emulator.h"
#include "../widgets/terminal_cluster_widget.h"

namespace RawrXD {
namespace Phase2Integration {

/**
 * \brief Phase 2 UI Infrastructure Integration Manager
 * 
 * Orchestrates the integration between Phase 1 foundation systems
 * and Phase 2 UI components to create a cohesive IDE experience.
 */
class Phase2UIManager : public QObject {
    Q_OBJECT

public:
    explicit Phase2UIManager(QObject* parent = nullptr);
    ~Phase2UIManager();

    // Two-phase initialization
    void initialize();
    void shutdown();

    // Access to integrated components
    FileSystemManager* fileSystemManager() const { return m_fileSystemManager; }
    CommandDispatcher* commandDispatcher() const { return m_commandDispatcher; }
    SettingsSystem* settingsSystem() const { return m_settingsSystem; }
    ErrorHandler* errorHandler() const { return m_errorHandler; }
    LoggingSystem* loggingSystem() const { return m_loggingSystem; }
    ModelStateManager* modelStateManager() const { return m_modelStateManager; }

    // UI Component Integration
    void integrateProjectExplorer(ProjectExplorerWidget* projectExplorer);
    void integrateProjectExplorer(void* enhancedProjectExplorer);  // For EnhancedProjectExplorer
    void integrateMultiTabEditor(MultiTabEditor* multiTabEditor);
    void integrateMultiTabEditor(void* enhancedMultiTabEditor);  // For EnhancedMultiTabEditor
    void integrateSettingsDialog(SettingsDialog* settingsDialog);
    void integrateBuildSystemWidget(BuildSystemWidget* buildWidget);
    void integrateTerminalWidgets(TerminalClusterWidget* terminalWidget);

signals:
    // High-level signals for UI coordination
    void fileOpened(const QString& filepath);
    void fileClosed(const QString& filepath);
    void buildStarted(const QString& target);
    void buildCompleted(bool success);
    void errorOccurred(const QString& errorMessage);
    void settingsChanged(const QString& category);

private:
    // Phase 1 Foundation Systems
    FileSystemManager* m_fileSystemManager;
    CommandDispatcher* m_commandDispatcher;
    SettingsSystem* m_settingsSystem;
    ErrorHandler* m_errorHandler;
    LoggingSystem* m_loggingSystem;
    ModelStateManager* m_modelStateManager;

    // Phase 2 UI Components
    ProjectExplorerWidget* m_projectExplorer;
    MultiTabEditor* m_multiTabEditor;
    SettingsDialog* m_settingsDialog;
    BuildSystemWidget* m_buildSystemWidget;
    TerminalClusterWidget* m_terminalWidget;

    // Integration state
    bool m_initialized;

    // Private integration methods
    void setupFileSystemIntegration();
    void setupCommandIntegration();
    void setupSettingsIntegration();
    void setupErrorHandlingIntegration();
    void setupLoggingIntegration();

    // Event handlers
    void onFileOpened(const QString& filepath);
    void onFileChanged(const QString& filepath);
    void onBuildStatusChanged(const QString& status);
    void onErrorReported(const QString& error);
    void onSettingsUpdated(const QString& key, const QVariant& value);
};

} // namespace Phase2Integration
} // namespace RawrXD