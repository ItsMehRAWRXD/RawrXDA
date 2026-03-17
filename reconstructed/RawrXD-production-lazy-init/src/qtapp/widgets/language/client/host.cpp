/**
 * @file language_client_host.cpp
 * @brief Implementation of LanguageClientHost - LSP client
 */

#include "language_client_host.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QComboBox>
#include <QMessageBox>

LanguageClientHost::LanguageClientHost(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    connectSignals();
    setWindowTitle("Language Client Host");
}

LanguageClientHost::~LanguageClientHost() = default;

void LanguageClientHost::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    QHBoxLayout* controlLayout = new QHBoxLayout();
    mStatusLabel = new QLabel("Disconnected", this);
    mStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    controlLayout->addWidget(mStatusLabel);
    
    controlLayout->addWidget(new QLabel("Language:", this));
    mLanguageCombo = new QComboBox(this);
    mLanguageCombo->addItems({"C++", "Python", "JavaScript", "Rust", "Go"});
    controlLayout->addWidget(mLanguageCombo);
    
    mConnectButton = new QPushButton("Connect", this);
    mConnectButton->setStyleSheet("background-color: #4CAF50; color: white; padding: 8px;");
    controlLayout->addWidget(mConnectButton);
    
    mDisconnectButton = new QPushButton("Disconnect", this);
    mDisconnectButton->setEnabled(false);
    controlLayout->addWidget(mDisconnectButton);
    
    mRestartButton = new QPushButton("Restart", this);
    mRestartButton->setEnabled(false);
    controlLayout->addWidget(mRestartButton);
    
    controlLayout->addStretch();
    mMainLayout->addLayout(controlLayout);
    
    mMainLayout->addWidget(new QLabel("Server Log:", this));
    mServerLog = new QListWidget(this);
    mMainLayout->addWidget(mServerLog);
}

void LanguageClientHost::connectSignals()
{
    connect(mConnectButton, &QPushButton::clicked, this, &LanguageClientHost::onConnectServer);
    connect(mDisconnectButton, &QPushButton::clicked, this, &LanguageClientHost::onDisconnectServer);
    connect(mRestartButton, &QPushButton::clicked, this, &LanguageClientHost::onRestartServer);
    connect(mLanguageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LanguageClientHost::onSelectLanguage);
}

void LanguageClientHost::onConnectServer()
{
    mStatusLabel->setText(QString("Connecting to %1 server...").arg(mLanguageCombo->currentText()));
    mStatusLabel->setStyleSheet("color: blue; font-weight: bold;");
    mConnectButton->setEnabled(false);
    mDisconnectButton->setEnabled(true);
    mRestartButton->setEnabled(true);
    mLanguageCombo->setEnabled(false);
    
    mServerLog->addItem("Connecting to language server...");
    emit serverConnected(mLanguageCombo->currentText());
}

void LanguageClientHost::onDisconnectServer()
{
    mStatusLabel->setText("Disconnected");
    mStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    mConnectButton->setEnabled(true);
    mDisconnectButton->setEnabled(false);
    mRestartButton->setEnabled(false);
    mLanguageCombo->setEnabled(true);
    
    mServerLog->addItem("Language server disconnected.");
    emit serverDisconnected();
}

void LanguageClientHost::onSelectLanguage(int index)
{
    if (mConnectButton->isEnabled()) {
        QString lang = mLanguageCombo->itemText(index);
        mServerLog->addItem(QString("Language changed to %1").arg(lang));
    }
}

void LanguageClientHost::onRestartServer()
{
    QString lang = mLanguageCombo->currentText();
    mServerLog->addItem(QString("Restarting %1 server...").arg(lang));
    QMessageBox::information(this, "Restarted", QString("%1 server restarted.").arg(lang));
}
