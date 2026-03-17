// Diagnostic Panel Implementation
#include "diagnostic_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QScrollBar>

namespace RawrXD {

DiagnosticPanel::DiagnosticPanel(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    
    // Log display
    m_logDisplay = new QTextEdit(this);
    m_logDisplay->setReadOnly(true);
    m_logDisplay->setFont(QFont("Consolas", 9));
    m_logDisplay->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #444; }"
    );
    m_layout->addWidget(m_logDisplay);
    
    // Button row
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_refreshBtn = new QPushButton("🔄 Refresh", this);
    m_clearBtn = new QPushButton("🗑️ Clear", this);
    m_copyBtn = new QPushButton("📋 Copy", this);
    
    connect(m_refreshBtn, &QPushButton::clicked, this, &DiagnosticPanel::refreshLogs);
    connect(m_clearBtn, &QPushButton::clicked, this, &DiagnosticPanel::clearLogs);
    connect(m_copyBtn, &QPushButton::clicked, this, &DiagnosticPanel::copyLogs);
    
    buttonLayout->addWidget(m_refreshBtn);
    buttonLayout->addWidget(m_clearBtn);
    buttonLayout->addWidget(m_copyBtn);
    buttonLayout->addStretch();
    
    m_layout->addLayout(buttonLayout);
    
    // Auto-refresh timer
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &DiagnosticPanel::refreshLogs);
    m_refreshTimer->start(1000); // Refresh every second
    
    refreshLogs();
}

void DiagnosticPanel::refreshLogs() {
    QString logs = DiagnosticLogger::instance().getRecentLogs(200);
    
    // Save scroll position
    int scrollPos = m_logDisplay->verticalScrollBar()->value();
    bool atBottom = (scrollPos == m_logDisplay->verticalScrollBar()->maximum());
    
    m_logDisplay->setPlainText(logs);
    
    // Restore scroll position or scroll to bottom
    if (atBottom) {
        m_logDisplay->verticalScrollBar()->setValue(m_logDisplay->verticalScrollBar()->maximum());
    } else {
        m_logDisplay->verticalScrollBar()->setValue(scrollPos);
    }
}

void DiagnosticPanel::clearLogs() {
    DiagnosticLogger::instance().clearLogs();
    refreshLogs();
}

void DiagnosticPanel::copyLogs() {
    QApplication::clipboard()->setText(m_logDisplay->toPlainText());
}

} // namespace RawrXD