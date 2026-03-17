#include "update_checker_widget.h"

UpdateCheckerWidget::UpdateCheckerWidget(QWidget* parent) : QWidget(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    QLabel* title = new QLabel("Update Checker", this);
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    layout->addWidget(title);
    
    m_statusLabel = new QLabel("Checking for updates...", this);
    m_statusLabel->setStyleSheet("font-size: 14px; color: #cccccc;");
    layout->addWidget(m_statusLabel);
    
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    layout->addWidget(m_progressBar);
    
    m_checkButton = new QPushButton("Check Now", this);
    connect(m_checkButton, &QPushButton::clicked, this, &UpdateCheckerWidget::checkForUpdates);
    layout->addWidget(m_checkButton);
    
    m_checkTimer = new QTimer(this);
    connect(m_checkTimer, &QTimer::timeout, this, &UpdateCheckerWidget::checkForUpdates);
    m_checkTimer->start(30000); // Check every 30 seconds
    
    // Initial check
    checkForUpdates();
}

UpdateCheckerWidget::~UpdateCheckerWidget()
{
    // Cleanup if needed
}