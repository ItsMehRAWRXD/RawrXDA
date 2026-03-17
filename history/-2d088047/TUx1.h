#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>

// Forward declarations
class FileBrowser;
class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class AgenticEngine;
class InferenceEngine;
class Settings;
class Telemetry;
class PlanningAgent;
class TodoManager;
class TodoDock;
class AgenticCopilotBridge;
class AgenticExecutor;
// Forward declarations for ModelTrainer enhancements
class ModelTrainer;
class TrainingDialog;
class TrainingProgressDock;
class ModelRegistry;
class Profiler;
class ObservabilityDashboard;
class HardwareBackendSelector;
class DistributedTrainer;
class InterpretabilityPanel;
class CIPipelineSettings;
class TokenizerLanguageSelector;
class CheckpointManager;
// Forward declarations for Phase 4: AI Code Assistant
class AICodeAssistant;
class AICodeAssistantPanel;

class AgenticIDE : public QMainWindow
{
    Q_OBJECT

public:
    explicit AgenticIDE(QWidget *parent = nullptr);
    ~AgenticIDE();

private slots:
    // File operations
    void newFile();
    void openFile();
    void saveFile();
    
    // Agent operations
    void startChat();
    void analyzeCode();
    void generateCode();
    void createPlan();
    void hotPatchModel();
    void trainModel();
    
    // View operations
    void toggleFileBrowser();
    void toggleChat();
    void toggleTerminals();
    void toggleTodos();
    void showSettings();
    
    // Edit operations
    void undo();
    void redo();
    void find();
    void replace();

    // ==== ModelTrainer related UI actions ====
    // Training workflow
    void openTrainingDialog();          // Show dialog to configure and start training
    void viewTrainingProgress();        // Open a dock/widget showing live training metrics
    void viewModelRegistry();           // Manage trained model versions
    // Profiling & observability
    void startProfiling();              // Begin performance profiling session
    void stopProfiling();               // End profiling and show results
    void openObservabilityDashboard();  // Show metrics dashboard (e.g., Grafana embed)
    // Hardware & compatibility
    void configureHardwareBackend();    // Select CPU/GPU/Vulkan backend
    // Security & privacy
    void manageSecuritySettings();      // Encryption & authentication options
    // Distributed training
    void startDistributedTraining();    // Launch MPI/cluster training
    // Interpretability
    void viewInterpretabilityReport();  // Show attention/feature importance visualizations
    // CI/CD integration
    void openCIPipelineSettings();      // Configure automated build/test pipelines
    // Multilingual support
    void configureTokenizerLanguage();  // Choose language for tokenization
    // Checkpointing & resume
    void manageCheckpoints();           // Save/load training checkpoints
    // Phase 4: AI Code Assistant
    void toggleAICodeAssistant();       // Show/hide AI suggestions panel
    void requestCodeCompletion();       // Request AI code completion
    void requestRefactoring();          // Request code refactoring suggestion
    void requestExplanation();          // Request code explanation
    void onAISearchWorkspace();         // AI-powered file search
    void onAIGrepWorkspace();           // AI-powered grep search
    void onAIExecuteCommand();          // AI-powered PowerShell command execution
    void onAIAnalyzeCode();             // AI code analysis
    void onAIAutofixError();            // AI auto-fix error suggestion

private:
    void setupUI();
    void setupMenus();
    void setupToolbar();
    void setupConnections();
    void loadSettings();
    void saveSettings();
    void addToRecentFiles(const QString &filePath);

    // Core components
    FileBrowser *m_fileBrowser;
    MultiTabEditor *m_multiTabEditor;
    ChatInterface *m_chatInterface;
    TerminalPool *m_terminalPool;
    AgenticEngine *m_agenticEngine;
    InferenceEngine *m_inferenceEngine;
    PlanningAgent *m_planningAgent;
    TodoManager *m_todoManager;
    TodoDock *m_todoDock;
    Settings *m_settings;
    Telemetry *m_telemetry;
    AgenticCopilotBridge *m_copilotBridge;
    AgenticExecutor *m_agenticExecutor;
    ModelTrainer *m_modelTrainer;                         // Model training orchestrator

    // ==== New components for ModelTrainer enhancements ====
    class TrainingDialog *m_trainingDialog;               // UI for configuring training
    class TrainingProgressDock *m_trainingProgressDock;   // Live metrics view
    class ModelRegistry *m_modelRegistry;                // Registry of trained models
    class Profiler *m_profiler;                          // Performance profiling tool
    class ObservabilityDashboard *m_observabilityDashboard; // Metrics dashboard UI
    class HardwareBackendSelector *m_hardwareBackendSelector; // Backend chooser
    class SecurityManager *m_securityManager;            // Encryption/auth handling
    class DistributedTrainer *m_distributedTrainer;      // Distributed training manager
    class InterpretabilityPanel *m_interpretabilityPanel; // Visual explanations
    class CIPipelineSettings *m_ciPipelineSettings;     // CI/CD configuration UI
    class TokenizerLanguageSelector *m_tokenizerLanguageSelector; // Language selector
    class CheckpointManager *m_checkpointManager;        // Checkpoint handling
    // Phase 4: AI Code Assistant
    class AICodeAssistant *m_aiCodeAssistant;            // AI suggestion engine
    class AICodeAssistantPanel *m_aiCodeAssistantPanel;  // AI suggestions UI panel
    
    // Dock widgets for toggle functionality
    class QDockWidget *m_fileDock;
    class QDockWidget *m_chatDock;
    class QDockWidget *m_terminalDock;
    class QDockWidget *m_todoDockWidget;
    
    QStringList m_recentFiles;
    QString m_workspaceRoot;  // Current workspace root directory
};