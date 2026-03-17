#include "security_alert_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QTextEdit>
#include <QSplitter>

SecurityAlertWidget::SecurityAlertWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    updateMetrics();
}

SecurityAlertWidget::~SecurityAlertWidget() = default;

void SecurityAlertWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Title and metrics row
    auto titleLayout = new QHBoxLayout();
    auto titleLabel = new QTextEdit();
    titleLabel->setPlainText("Security Issues");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt; border: none; background: transparent;");
    titleLabel->setReadOnly(true);
    titleLabel->setMaximumHeight(24);
    titleLayout->addWidget(titleLabel);
    
    m_statsLabel = new QTextEdit();
    m_statsLabel->setPlainText("0 issues");
    m_statsLabel->setStyleSheet("color: #666; border: none; background: transparent;");
    m_statsLabel->setReadOnly(true);
    m_statsLabel->setMaximumHeight(24);
    titleLayout->addStretch();
    titleLayout->addWidget(m_statsLabel);
    
    mainLayout->addLayout(titleLayout);

    // Severity bar
    m_severityBar = new QProgressBar();
    m_severityBar->setMaximumHeight(6);
    m_severityBar->setStyleSheet(
        "QProgressBar { border: 1px solid #ccc; background: #eee; }"
        "QProgressBar::chunk { background: #ff6b6b; }" // Red for critical
    );
    mainLayout->addWidget(m_severityBar);

    // Splitter: list on left, details on right
    auto splitter = new QSplitter(Qt::Horizontal);
    
    // Issue list
    m_issueList = new QListWidget();
    m_issueList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_issueList->setIconSize(QSize(16, 16));
    connect(m_issueList, &QListWidget::itemClicked, this, &SecurityAlertWidget::onItemClicked);
    splitter->addWidget(m_issueList);
    splitter->setStretchFactor(0, 1);

    // Details panel
    m_detailsText = new QTextEdit();
    m_detailsText->setReadOnly(true);
    m_detailsText->setMaximumWidth(300);
    splitter->addWidget(m_detailsText);
    splitter->setStretchFactor(1, 0);
    
    mainLayout->addWidget(splitter);

    // Button row
    auto buttonLayout = new QHBoxLayout();
    
    m_fixButton = new QPushButton("🔧 Fix Issue");
    m_fixButton->setEnabled(false);
    connect(m_fixButton, &QPushButton::clicked, this, &SecurityAlertWidget::onFixClicked);
    buttonLayout->addWidget(m_fixButton);

    m_ignoreButton = new QPushButton("↻ Ignore");
    m_ignoreButton->setEnabled(false);
    connect(m_ignoreButton, &QPushButton::clicked, this, &SecurityAlertWidget::onIgnoreClicked);
    buttonLayout->addWidget(m_ignoreButton);

    m_detailsButton = new QPushButton("ℹ Details");
    m_detailsButton->setEnabled(false);
    connect(m_detailsButton, &QPushButton::clicked, this, &SecurityAlertWidget::onDetailsClicked);
    buttonLayout->addWidget(m_detailsButton);

    m_clearAllButton = new QPushButton("✕ Clear All");
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_clearAllButton);
    connect(m_clearAllButton, &QPushButton::clicked, this, [this]() {
        clearAllIssues();
    });

    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);
}

void SecurityAlertWidget::addIssue(const SecurityIssue& issue) {
    m_issues.append(issue);
    refreshIssueList();
    updateMetrics();
}

void SecurityAlertWidget::removeIssue(const QString& issueId) {
    m_issues.erase(std::remove_if(m_issues.begin(), m_issues.end(),
        [issueId](const SecurityIssue& i) { return i.id == issueId; }), m_issues.end());
    refreshIssueList();
    updateMetrics();
}

void SecurityAlertWidget::clearAllIssues() {
    m_issues.clear();
    m_issueList->clear();
    m_detailsText->clear();
    m_selectedIssueId.clear();
    updateMetrics();
    emit issuesCleared();
}

void SecurityAlertWidget::markAsFixed(const QString& issueId) {
    for (auto& issue : m_issues) {
        if (issue.id == issueId) {
            issue.isFixed = true;
            emit issueFixed(issueId);
            break;
        }
    }
    refreshIssueList();
    updateMetrics();
}

int SecurityAlertWidget::issueCount() const {
    return m_issues.size();
}

int SecurityAlertWidget::criticalCount() const {
    return std::count_if(m_issues.begin(), m_issues.end(),
        [](const SecurityIssue& i) { return i.severity == SeverityCritical && !i.isFixed; });
}

int SecurityAlertWidget::highCount() const {
    return std::count_if(m_issues.begin(), m_issues.end(),
        [](const SecurityIssue& i) { return i.severity == SeverityHigh && !i.isFixed; });
}

QList<SecurityAlertWidget::SecurityIssue> SecurityAlertWidget::getAllIssues() const {
    return m_issues;
}

QColor SecurityAlertWidget::getSeverityColor(Severity severity) {
    switch (severity) {
        case SeverityCritical:  return QColor(255, 59, 48);   // Red
        case SeverityHigh:      return QColor(255, 152, 0);   // Orange
        case SeverityMedium:    return QColor(255, 193, 7);   // Yellow
        case SeverityLow:       return QColor(76, 175, 80);   // Green
        default:                return QColor(128, 128, 128); // Gray
    }
}

QString SecurityAlertWidget::getSeverityString(Severity severity) {
    switch (severity) {
        case SeverityCritical: return "CRITICAL";
        case SeverityHigh:     return "HIGH";
        case SeverityMedium:   return "MEDIUM";
        case SeverityLow:      return "LOW";
        default:               return "UNKNOWN";
    }
}

QString SecurityAlertWidget::getTypeString(IssueType type) {
    switch (type) {
        case TypeInputValidation:       return "Input Validation";
        case TypeAuthorizationBypass:   return "Authorization Bypass";
        case TypeSQLInjection:          return "SQL Injection";
        case TypeXSS:                   return "Cross-Site Scripting";
        case TypeCryptoWeakness:        return "Cryptographic Weakness";
        case TypeDependencyVulnerability: return "Dependency Vulnerability";
        case TypeMemorySafety:          return "Memory Safety";
        case TypeOther:                 return "Other";
        default:                        return "Unknown";
    }
}

void SecurityAlertWidget::refreshIssueList() {
    m_issueList->clear();
    
    // Sort by severity (Critical first)
    auto sortedIssues = m_issues;
    std::sort(sortedIssues.begin(), sortedIssues.end(),
        [](const SecurityIssue& a, const SecurityIssue& b) {
            return a.severity < b.severity; // Lower enum = higher severity
        });

    for (const auto& issue : sortedIssues) {
        auto item = new QListWidgetItem();
        item->setText(formatIssueDisplay(issue));
        item->setData(Qt::UserRole, issue.id);
        item->setForeground(getSeverityColor(issue.severity));
        
        if (issue.isFixed) {
            item->setBackground(QColor(200, 255, 200)); // Light green
        }
        
        m_issueList->addItem(item);
    }
}

QString SecurityAlertWidget::formatIssueDisplay(const SecurityIssue& issue) const {
    return QString("[%1] %2 - %3 (%4)")
        .arg(getSeverityString(issue.severity))
        .arg(issue.title)
        .arg(getTypeString(issue.type))
        .arg(issue.location);
}

void SecurityAlertWidget::updateMetrics() {
    int total = m_issues.size();
    int critical = criticalCount();
    int high = highCount();
    
    QString stats = QString::number(total) + " issues";
    if (critical > 0) stats += QString(": %1 Critical").arg(critical);
    if (high > 0) stats += QString(", %1 High").arg(high);
    
    m_statsLabel->setText(stats);
    
    // Update severity bar color
    if (critical > 0) {
        m_severityBar->setStyleSheet(
            "QProgressBar { border: 1px solid #ccc; background: #eee; }"
            "QProgressBar::chunk { background: #ff3b30; }" // Red
        );
    } else if (high > 0) {
        m_severityBar->setStyleSheet(
            "QProgressBar { border: 1px solid #ccc; background: #eee; }"
            "QProgressBar::chunk { background: #ff9800; }" // Orange
        );
    } else if (total > 0) {
        m_severityBar->setStyleSheet(
            "QProgressBar { border: 1px solid #ccc; background: #eee; }"
            "QProgressBar::chunk { background: #4caf50; }" // Green
        );
    }
    
    int maxValue = total > 0 ? total : 1;
    m_severityBar->setMaximum(maxValue);
    m_severityBar->setValue(critical > 0 ? critical : (high > 0 ? high : 0));
}

void SecurityAlertWidget::onItemClicked(QListWidgetItem* item) {
    m_selectedIssueId = item->data(Qt::UserRole).toString();
    
    // Find and display issue details
    for (const auto& issue : m_issues) {
        if (issue.id == m_selectedIssueId) {
            QString details = QString(
                "<b>%1</b><br/>"
                "<b>Severity:</b> %2<br/>"
                "<b>Type:</b> %3<br/>"
                "<b>Location:</b> %4<br/><br/>"
                "<b>Description:</b><br/>%5<br/><br/>"
                "<b>Fix Suggestion:</b><br/>%6"
            ).arg(issue.title)
             .arg(getSeverityString(issue.severity))
             .arg(getTypeString(issue.type))
             .arg(issue.location)
             .arg(issue.description)
             .arg(issue.fixSuggestion);
            
            m_detailsText->setText(details);
            emit issueSelected(m_selectedIssueId);
            break;
        }
    }
    
    m_fixButton->setEnabled(true);
    m_ignoreButton->setEnabled(true);
    m_detailsButton->setEnabled(true);
}

void SecurityAlertWidget::onFixClicked() {
    if (!m_selectedIssueId.isEmpty()) {
        emit fixRequested(m_selectedIssueId);
        markAsFixed(m_selectedIssueId);
    }
}

void SecurityAlertWidget::onIgnoreClicked() {
    if (!m_selectedIssueId.isEmpty()) {
        emit ignoreRequested(m_selectedIssueId);
        removeIssue(m_selectedIssueId);
    }
}

void SecurityAlertWidget::onDetailsClicked() {
    if (!m_selectedIssueId.isEmpty()) {
        // Details already shown in panel; could expand to a dialog if needed
        QMessageBox::information(this, "Issue Details", m_detailsText->toPlainText(), QMessageBox::Ok);
    }
}
