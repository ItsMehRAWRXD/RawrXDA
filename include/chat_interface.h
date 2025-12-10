#pragma once

#include <QWidget>
#include <QString>

class QTextEdit;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class QProgressBar;
class QTimer;
class AgenticEngine;

namespace RawrXD {
    class PlanOrchestrator;
}

class ChatInterface : public QWidget {
    Q_OBJECT
public:
    explicit ChatInterface(QWidget* parent = nullptr);
    
    // Two-phase initialization: call after QApplication exists
    void initialize();
    
    void setAgenticEngine(AgenticEngine* engine) { m_agenticEngine = engine; }
    void setPlanOrchestrator(RawrXD::PlanOrchestrator* orchestrator) { m_planOrchestrator = orchestrator; }
    
    void addMessage(const QString& sender, const QString& message);
    QString selectedModel() const;
    bool isMaxMode() const;
    void sendMessageProgrammatically(const QString& message);
    
    // Agent tool commands
    // Updated to accept optional arguments string for future extensions
    void executeAgentCommand(const QString& command, const QString& args = "");
    bool isAgentCommand(const QString& message) const;
    
public slots:
    void displayResponse(const QString& response);
    void focusInput();
    void sendMessage();
    void refreshModels();
    void onModelChanged(int index);
    void onModel2Changed(int index);
    void onMaxModeToggled(bool enabled);
    void setCanSendMessage(bool enabled);
    
    // Phase 2: Streaming token progress
    void onTokenGenerated(int delta);
    void hideProgress();
    
signals:
    void messageSent(const QString& message);
    void modelSelected(const QString& modelPath);
    void model2Selected(const QString& modelPath);
    void maxModeChanged(bool enabled);
    void messageReceived(const QString& reply);
    
private:
    void loadAvailableModels();
    void loadAvailableModelsForSecond();
    QString resolveGgufPath(const QString& modelName);  // Resolve Ollama model name to GGUF file
    
    QTextEdit* message_history_;
    QLineEdit* message_input_;
    QComboBox* modelSelector_;
    QComboBox* modelSelector2_;
    QCheckBox* maxModeToggle_;
    QLabel* statusLabel_;
    bool maxMode_;
    bool m_busy = false;  // Prevent re-entrancy during inference
    QString m_lastPrompt;  // Store last user message
    AgenticEngine* m_agenticEngine = nullptr;
    RawrXD::PlanOrchestrator* m_planOrchestrator = nullptr;
    
    // Phase 2: Streaming token progress bar
    QProgressBar* m_tokenProgress{nullptr};
    QTimer* m_hideTimer{nullptr};
};
