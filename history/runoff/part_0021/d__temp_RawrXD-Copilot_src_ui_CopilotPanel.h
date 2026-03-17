#ifndef COPILOTPANEL_H
#define COPILOTPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTabWidget>
#include <QJsonObject>
#include <QTimer>

class CopilotPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CopilotPanel(QWidget *parent = nullptr);
    ~CopilotPanel();

    // UI management
    void showPanel();
    void hidePanel();
    void togglePanel();
    
    // Content updates
    void updateSuggestions(const QList<QJsonObject>& suggestions);
    void updateChatMessages(const QString& message);
    void updateStatus(const QString& status);
    void updatePluginStatus(const QString& pluginName, const QString& status);

private slots:
    void onSendMessage();
    void onAcceptSuggestion();
    void onRejectSuggestion();
    void onSuggestionSelected();
    void onTabChanged(int index);
    void onPluginAction();

private:
    // UI components
    QTabWidget* m_tabWidget;
    
    // Chat tab
    QTextEdit* m_chatDisplay;
    QLineEdit* m_chatInput;
    QPushButton* m_sendButton;
    
    // Suggestions tab
    QListWidget* m_suggestionsList;
    QTextEdit* m_suggestionPreview;
    QPushButton* m_acceptButton;
    QPushButton* m_rejectButton;
    
    // Plugins tab
    QListWidget* m_pluginsList;
    QPushButton* m_pluginActionButton;
    QTextEdit* m_pluginOutput;
    
    // Status tab
    QTextEdit* m_statusDisplay;
    QPushButton* m_settingsButton;
    
    // Layout
    QVBoxLayout* m_mainLayout;
    
    void setupUI();
    void setupChatTab();
    void setupSuggestionsTab();
    void setupPluginsTab();
    void setupStatusTab();
    void connectSignals();
    QString formatSuggestionItem(const QJsonObject& suggestion);
};

#endif // COPILOTPANEL_H
