/**
 * @file language_client_host.h
 * @brief Header for LanguageClientHost - LSP (Language Server Protocol) client
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QLabel;
class QListWidget;
class QComboBox;

class LanguageClientHost : public QWidget {
    Q_OBJECT
    
public:
    explicit LanguageClientHost(QWidget* parent = nullptr);
    ~LanguageClientHost();
    
public slots:
    void onConnectServer();
    void onDisconnectServer();
    void onSelectLanguage(int index);
    void onRestartServer();
    
signals:
    void serverConnected(const QString& language);
    void serverDisconnected();
    
private:
    void setupUI();
    void connectSignals();
    
    QVBoxLayout* mMainLayout;
    QLabel* mStatusLabel;
    QPushButton* mConnectButton;
    QPushButton* mDisconnectButton;
    QPushButton* mRestartButton;
    QComboBox* mLanguageCombo;
    QListWidget* mServerLog;
};

#endif // LANGUAGE_CLIENT_HOST_H
