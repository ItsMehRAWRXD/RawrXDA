#include "hotpatch_panel.h"
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFont>
#include <QDebug>

HotpatchPanel::HotpatchPanel(QWidget* parent)
    : QWidget(parent)
    , m_sessionStart(QDateTime::currentDateTime())
{
    setupUI();
}

void HotpatchPanel::setupUI() {
    setStyleSheet(
        "HotpatchPanel { background-color: #1e1e1e; }"
        "QListWidget { background-color: #252526; color: #d4d4d4; border: none; }"
        "QListWidget::item { padding: 4px; margin: 2px; border-left: 3px solid #007acc; }"
        "QLabel { color: #d4d4d4; }"
        "QPushButton { background-color: #007acc; color: #ffffff; border: none; padding: 6px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #005a9e; }"
    );
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // Header with stats
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    m_statsLabel = new QLabel("Events: 0 | Success: 0 | Failed: 0", this);
    QFont statsFont = m_statsLabel->font();
    statsFont.setPointSize(9);
    statsFont.setBold(true);
    m_statsLabel->setFont(statsFont);
    headerLayout->addWidget(m_statsLabel);
    headerLayout->addStretch();
    
    m_manualReloadButton = new QPushButton("Manual Reload", this);
    m_manualReloadButton->setMaximumWidth(120);
    connect(m_manualReloadButton, &QPushButton::clicked, this, [this]() {
        emit manualReloadRequested("Q4_K"); // Default quant mode
    });
    headerLayout->addWidget(m_manualReloadButton);
    
    m_clearButton = new QPushButton("Clear", this);
    m_clearButton->setMaximumWidth(80);
    connect(m_clearButton, &QPushButton::clicked, this, &HotpatchPanel::clearLog);
    headerLayout->addWidget(m_clearButton);
    
    mainLayout->addLayout(headerLayout);
    
    // Event list
    m_eventList = new QListWidget(this);
    m_eventList->setFont(QFont("Courier", 9));
    mainLayout->addWidget(m_eventList, 1);
    
    setLayout(mainLayout);
}

void HotpatchPanel::logEvent(const QString& eventType, const QString& details, bool success) {
    if (success) {
        m_successCount++;
    } else {
        m_failureCount++;
    }
    
    createListItem(eventType, details, success);
    
    // Update stats
    int total = m_successCount + m_failureCount;
    m_statsLabel->setText(
        tr("Events: %1 | Success: %2 | Failed: %3")
            .arg(total)
            .arg(m_successCount)
            .arg(m_failureCount)
    );
    
    qDebug() << "[HotpatchPanel]" << eventType << details << (success ? "✓" : "✗");
}

void HotpatchPanel::createListItem(const QString& eventType, const QString& details, bool success) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString status = success ? "✓" : "✗";
    QString statusColor = success ? "#4ec9b0" : "#f48771"; // Green / Red
    
    QString itemText = tr("[%1] %2 %3 | %4")
        .arg(timestamp)
        .arg(status)
        .arg(eventType)
        .arg(details);
    
    QListWidgetItem* item = new QListWidgetItem(itemText);
    
    // Color the status indicator
    if (success) {
        item->setForeground(QColor("#4ec9b0")); // Green text for success
    } else {
        item->setForeground(QColor("#f48771")); // Red text for failure
    }
    
    item->setData(Qt::UserRole, timestamp);
    m_eventList->insertItem(0, item); // Add to top
    
    // Keep only last 100 events to avoid memory issues
    while (m_eventList->count() > 100) {
        delete m_eventList->takeItem(m_eventList->count() - 1);
    }
}

void HotpatchPanel::clearLog() {
    m_eventList->clear();
    m_successCount = 0;
    m_failureCount = 0;
    m_sessionStart = QDateTime::currentDateTime();
    
    m_statsLabel->setText("Events: 0 | Success: 0 | Failed: 0");
    qDebug() << "[HotpatchPanel] Log cleared";
}

int HotpatchPanel::eventCount() const {
    return m_successCount + m_failureCount;
}
