#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QTextEdit>
#include <QWidget>
#include "streaming_token_manager.h"
#include "model_router.h"
#include "tool_registry.h"
#include "agentic_planner.h"
#include "command_palette.h"
#include "diff_viewer.h"
#include "ai_model_client.h"

/**
 * @brief MASM Port Integration - Main IDE Window Integration
 * 
 * Integrates all ported MASM components into a single cohesive interface.
 */
class MASMIntegrationManager : public QObject {
    Q_OBJECT

public:
    explicit MASMIntegrationManager(QMainWindow* mainWindow, QObject* parent = nullptr);
    ~MASMIntegrationManager();

    // Initialize all components
    void initialize();

    // Component accessors
    StreamingTokenManager* getStreamingManager() const { return m_streamingManager; }
    ModelRouter* getModelRouter() const { return m_modelRouter; }
    MasmToolRegistry* getToolRegistry() const { return m_toolRegistry; }
    AgenticPlanner* getAgenticPlanner() const { return m_agenticPlanner; }
    CommandPalette* getCommandPalette() const { return m_commandPalette; }
    DiffViewer* getDiffViewer() const { return m_diffViewer; }
    AIModelClient* getAIClient() const { return m_aiClient; }

    // Wiring helper
    void wireUIComponents();
    void setupMenus();
    void setupKeyboardShortcuts();

signals:
    void taskStarted(const QString& taskDescription);
    void taskFinished(const QString& result);
    void modeChanged(ModelRouter::ModeFlags newMode);

private slots:
    void onTaskFinished(const QString& result);
    void onStepFinished(int index, const QJsonObject& result);
    void onDiffAccepted(const QString& filePath, const QString& newContent);
    void onCmdKTriggered();

private:
    QMainWindow* m_mainWindow{nullptr};
    QPointer<StreamingTokenManager> m_streamingManager;
    QPointer<ModelRouter> m_modelRouter;
    QPointer<MasmToolRegistry> m_toolRegistry;
    QPointer<AgenticPlanner> m_agenticPlanner;
    QPointer<CommandPalette> m_commandPalette;
    QPointer<DiffViewer> m_diffViewer;
    QPointer<AIModelClient> m_aiClient;
    QTextEdit* m_chatPanel{nullptr};
};
