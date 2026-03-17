#pragma once

#include <QWidget>
#include <QString>

class QTextEdit;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;
class AgenticEngine;
class WorkspaceManager;

class ChatInterface : public QWidget {
    Q_OBJECT
public:
    explicit ChatInterface(QWidget* parent = nullptr);
    
    void setAgenticEngine(AgenticEngine* engine) { m_agenticEngine = engine; }
    
    void addMessage(const QString& sender, const QString& message);
    QString selectedModel() const;
    bool isMaxMode() const;
    
    // Get current workspace context for message processing
    // Returns absolute path of current workspace
    QString getCurrentWorkspaceContext() const;
    
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
    void onWorkspaceChanged(const QString& oldPath, const QString& newPath);
    
signals:
    void messageSent(const QString& message);
    void modelSelected(const QString& modelPath);
    void model2Selected(const QString& modelPath);
    void maxModeChanged(bool enabled);
    void workspaceContextUpdated(const QString& workspacePath);
    
private:
    void loadAvailableModels();
    void loadAvailableModelsForSecond();
    void setupWorkspaceIntegration();
    void updateWorkspaceInfo();
    
    QTextEdit* message_history_;
    QLineEdit* message_input_;
    QComboBox* modelSelector_;
    QComboBox* modelSelector2_;
    QCheckBox* maxModeToggle_;
    QLabel* statusLabel_;
    QLabel* workspaceLabel_;  // Display current workspace context
    bool maxMode_;
    AgenticEngine* m_agenticEngine = nullptr;
    QString m_currentWorkspacePath;  // Cache current workspace path
};

