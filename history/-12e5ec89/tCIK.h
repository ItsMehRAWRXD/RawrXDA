#pragma once

#include <QWidget>
#include <QString>

class QTextEdit;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QLabel;

class ChatInterface : public QWidget {
    Q_OBJECT
public:
    explicit ChatInterface(QWidget* parent = nullptr);
    
    void addMessage(const QString& sender, const QString& message);
    QString selectedModel() const;
    bool isMaxMode() const;
    
public slots:
    void displayResponse(const QString& response);
    void focusInput();
    void sendMessage();
    void refreshModels();
    void onModelChanged(int index);
    void onMaxModeToggled(bool enabled);
    
    // Display a response from the agent
    void displayResponse(const QString& response);
    // Add a generic message (system, planner, etc.)
    void addMessage(const QString& sender, const QString& message);
    // Focus the input line edit (used when chat is opened)
    void focusInput();
    
signals:
    void messageSent(const QString& message);
    void modelSelected(const QString& modelPath);
    void maxModeChanged(bool enabled);
    
private:
    void loadAvailableModels();
    
    QTextEdit* message_history_;
    QLineEdit* message_input_;
    QComboBox* modelSelector_;
    QCheckBox* maxModeToggle_;
    QLabel* statusLabel_;
    bool maxMode_;
};
