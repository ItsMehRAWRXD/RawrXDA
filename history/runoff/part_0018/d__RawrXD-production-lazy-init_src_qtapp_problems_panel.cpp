/**
 * @file problems_panel.cpp
 * @brief Problems panel implementation
 */

#include "problems_panel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QRegularExpression>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

ProblemsPanel::ProblemsPanel(QWidget* parent)
    : QWidget(parent), m_errorCount(0), m_warningCount(0), m_infoCount(0)
{
    qRegisterMetaType<DiagnosticIssue>("DiagnosticIssue");
    setupUI();
}

ProblemsPanel::~ProblemsPanel() = default;

void ProblemsPanel::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // ========== Filter Bar ==========
    QHBoxLayout* filterLayout = new QHBoxLayout();
    
    QLabel* filterLabel = new QLabel("Filter:", this);
    m_filterInput = new QLineEdit(this);
    m_filterInput->setPlaceholderText("Search issues (file, message, code)...");
    m_filterInput->setMaximumHeight(28);
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(m_filterInput, 1);
    
    QLabel* severityLabel = new QLabel("Severity:", this);
    m_severityFilter = new QComboBox(this);
    m_severityFilter->addItem("All");
    m_severityFilter->addItem("Errors");
    m_severityFilter->addItem("Warnings");
    m_severityFilter->addItem("Info");
    m_severityFilter->setMaximumWidth(120);
    filterLayout->addWidget(severityLabel);
    filterLayout->addWidget(m_severityFilter);
    
    QLabel* sourceLabel = new QLabel("Source:", this);
    m_sourceFilter = new QComboBox(this);
    m_sourceFilter->addItem("All");
    m_sourceFilter->addItem("MASM");
    m_sourceFilter->addItem("LSP");
    m_sourceFilter->addItem("Build");
    m_sourceFilter->addItem("Runtime");
    m_sourceFilter->setMaximumWidth(120);
    filterLayout->addWidget(sourceLabel);
    filterLayout->addWidget(m_sourceFilter);
    
    mainLayout->addLayout(filterLayout);

    // ========== Summary Label ==========
    m_summaryLabel = new QLabel("No issues", this);
    m_summaryLabel->setStyleSheet("QLabel { color: #00ff00; font-weight: bold; padding: 4px; }");
    mainLayout->addWidget(m_summaryLabel);

    // ========== Issue Tree ==========
    m_issueTree = new QTreeWidget(this);
    m_issueTree->setHeaderLabels(QStringList() << "Issue" << "File" << "Line" << "Code" << "Source");
    m_issueTree->setColumnCount(5);
    m_issueTree->setStyleSheet(
        "QTreeWidget { background-color: #1e1e1e; color: #e0e0e0; border: 1px solid #3e3e42; }"
        "QTreeWidget::item:selected { background-color: #007acc; }"
        "QHeaderView::section { background-color: #2d2d30; color: #e0e0e0; padding: 4px; border: none; }"
    );
    m_issueTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_issueTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_issueTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_issueTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_issueTree->setSelectionMode(QAbstractItemView::SingleSelection);
    mainLayout->addWidget(m_issueTree, 1);

    // ========== Connections ==========
    connect(m_filterInput, &QLineEdit::textChanged, this, &ProblemsPanel::onFilterTextChanged);
    connect(m_severityFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ProblemsPanel::onSeverityFilterChanged);
    connect(m_sourceFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProblemsPanel::onSourceFilterChanged);
    connect(m_issueTree, &QTreeWidget::itemDoubleClicked, this, &ProblemsPanel::onItemDoubleClicked);

    setLayout(mainLayout);
}

void ProblemsPanel::addIssue(const DiagnosticIssue& issue)
{
    // Skip duplicates
    for (const auto& existing : m_issues) {
        if (existing == issue) return;
    }

    m_issues.append(issue);

    // Update counts
    switch (issue.severity) {
        case DiagnosticIssue::Error:   m_errorCount++; break;
        case DiagnosticIssue::Warning: m_warningCount++; break;
        case DiagnosticIssue::Info:    m_infoCount++; break;
    }

    updateTree();
}

void ProblemsPanel::clearIssues()
{
    m_issues.clear();
    m_errorCount = 0;
    m_warningCount = 0;
    m_infoCount = 0;
    m_issueTree->clear();
    m_summaryLabel->setText("No issues");
    m_summaryLabel->setStyleSheet("QLabel { color: #00ff00; font-weight: bold; padding: 4px; }");
}

void ProblemsPanel::setIssuesFromJSON(const QString& filePath, const QJsonArray& diagnostics)
{
    for (const QJsonValue& val : diagnostics) {
        if (!val.isObject()) continue;

        QJsonObject diag = val.toObject();
        DiagnosticIssue issue;
        issue.file = filePath;
        issue.line = diag.value("range").toObject().value("start").toObject().value("line").toInt() + 1;
        issue.column = diag.value("range").toObject().value("start").toObject().value("character").toInt() + 1;
        issue.message = diag.value("message").toString();
        issue.code = diag.value("code").toString();
        issue.source = "LSP";

        // Map LSP severity (1=error, 2=warning, 3=info, 4=hint)
        int severity = diag.value("severity").toInt(2);
        issue.severity = (severity == 1) ? DiagnosticIssue::Error :
                        (severity == 2) ? DiagnosticIssue::Warning :
                        DiagnosticIssue::Info;

        addIssue(issue);
    }
}

QVector<DiagnosticIssue> ProblemsPanel::parseMASMOutput(const QString& output)
{
    QVector<DiagnosticIssue> issues;

    // MASM error patterns:
    // filename.asm(123) : error ML2005: symbol already defined
    // filename.asm(123) : warning ML4001: module compiled with /W2, exited with /W0
    // filename.asm(123) : fatal error LNK1104: cannot open file
    QRegularExpression masmRegex(
        R"(([^\s(]+\.asm)\((\d+)\)\s*:\s*(error|warning|fatal error)\s+(ML\d+|LNK\d+):\s*(.+)$)",
        QRegularExpression::MultilineOption
    );

    for (const auto& match : masmRegex.globalMatch(output)) {
        DiagnosticIssue issue;
        issue.file = match.captured(1);
        issue.line = match.captured(2).toInt();
        issue.code = match.captured(4);
        issue.message = match.captured(5).trimmed();
        issue.source = "MASM";

        QString sevText = match.captured(3).toLower();
        if (sevText == "error" || sevText == "fatal error") {
            issue.severity = DiagnosticIssue::Error;
        } else {
            issue.severity = DiagnosticIssue::Warning;
        }

        issues.append(issue);
    }

    qDebug() << "[ProblemsPanel] Parsed" << issues.size() << "MASM issues from output";
    return issues;
}

void ProblemsPanel::updateTree()
{
    m_issueTree->clear();

    // Update summary
    QString summary = QString("Errors: %1 | Warnings: %2 | Info: %3 | Total: %4")
        .arg(m_errorCount)
        .arg(m_warningCount)
        .arg(m_infoCount)
        .arg(m_issues.size());
    m_summaryLabel->setText(summary);

    // Update color based on severity
    if (m_errorCount > 0) {
        m_summaryLabel->setStyleSheet("QLabel { color: #ff6666; font-weight: bold; padding: 4px; }");
    } else if (m_warningCount > 0) {
        m_summaryLabel->setStyleSheet("QLabel { color: #ffaa44; font-weight: bold; padding: 4px; }");
    } else {
        m_summaryLabel->setStyleSheet("QLabel { color: #00ff00; font-weight: bold; padding: 4px; }");
    }

    // Group by file
    QMap<QString, QVector<DiagnosticIssue>> byFile;
    for (const auto& issue : m_issues) {
        byFile[issue.file].append(issue);
    }

    // Add to tree
    for (auto it = byFile.begin(); it != byFile.end(); ++it) {
        QTreeWidgetItem* fileItem = new QTreeWidgetItem();
        fileItem->setText(0, QString("%1 (%2)").arg(it.key()).arg(it.value().size()));
        fileItem->setForeground(0, QColor("#e0e0e0"));

        for (const auto& issue : it.value()) {
            QTreeWidgetItem* issueItem = new QTreeWidgetItem(fileItem);
            issueItem->setText(0, issue.message);
            issueItem->setText(1, issue.file);
            issueItem->setText(2, QString::number(issue.line));
            issueItem->setText(3, issue.code);
            issueItem->setText(4, issue.source);

            // Color by severity
            QString color = severityColor(issue.severity);
            issueItem->setForeground(0, QColor(color));

            // Store issue data
            issueItem->setData(0, Qt::UserRole, QVariant::fromValue(issue));
        }

        m_issueTree->addTopLevelItem(fileItem);
    }

    m_issueTree->expandAll();
}

void ProblemsPanel::filterAndSort()
{
    QString filterText = m_filterInput->text().toLower();
    int severityIndex = m_severityFilter->currentIndex();
    int sourceIndex = m_sourceFilter->currentIndex();

    // Rebuild tree with filters
    m_issueTree->clear();

    QVector<DiagnosticIssue> filtered = m_issues;

    // Apply filters
    filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
        [this, filterText, severityIndex, sourceIndex](const DiagnosticIssue& issue) {
            // Severity filter
            if (severityIndex == 1 && issue.severity != DiagnosticIssue::Error) return true;
            if (severityIndex == 2 && issue.severity != DiagnosticIssue::Warning) return true;
            if (severityIndex == 3 && issue.severity != DiagnosticIssue::Info) return true;

            // Source filter
            if (sourceIndex > 0) {
                QString sourceList[] = {"", "MASM", "LSP", "Build", "Runtime"};
                if (issue.source != sourceList[sourceIndex]) return true;
            }

            // Text filter
            if (!filterText.isEmpty()) {
                if (!issue.file.toLower().contains(filterText) &&
                    !issue.message.toLower().contains(filterText) &&
                    !issue.code.toLower().contains(filterText)) {
                    return true;
                }
            }

            return false;
        }), filtered.end());

    // Rebuild tree
    QMap<QString, QVector<DiagnosticIssue>> byFile;
    for (const auto& issue : filtered) {
        byFile[issue.file].append(issue);
    }

    for (auto it = byFile.begin(); it != byFile.end(); ++it) {
        QTreeWidgetItem* fileItem = new QTreeWidgetItem();
        fileItem->setText(0, QString("%1 (%2)").arg(it.key()).arg(it.value().size()));

        for (const auto& issue : it.value()) {
            QTreeWidgetItem* issueItem = new QTreeWidgetItem(fileItem);
            issueItem->setText(0, issue.message);
            issueItem->setText(1, issue.file);
            issueItem->setText(2, QString::number(issue.line));
            issueItem->setText(3, issue.code);
            issueItem->setText(4, issue.source);
            issueItem->setForeground(0, QColor(severityColor(issue.severity)));
            issueItem->setData(0, Qt::UserRole, QVariant::fromValue(issue));
        }

        m_issueTree->addTopLevelItem(fileItem);
    }

    m_issueTree->expandAll();
}

QString ProblemsPanel::severityColor(DiagnosticIssue::Severity sev) const
{
    switch (sev) {
        case DiagnosticIssue::Error:   return "#ff6666";
        case DiagnosticIssue::Warning: return "#ffaa44";
        case DiagnosticIssue::Info:    return "#88ccff";
    }
    return "#e0e0e0";
}

QString ProblemsPanel::severityIcon(DiagnosticIssue::Severity sev) const
{
    switch (sev) {
        case DiagnosticIssue::Error:   return "❌";
        case DiagnosticIssue::Warning: return "⚠️";
        case DiagnosticIssue::Info:    return "ℹ️";
    }
    return "";
}

void ProblemsPanel::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (!item->parent()) return;  // Only react to leaf items

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid()) return;

    DiagnosticIssue issue = data.value<DiagnosticIssue>();
    emit navigateToIssue(issue.file, issue.line, issue.column);
    emit issueSelected(issue);
}

void ProblemsPanel::onFilterTextChanged(const QString&)
{
    filterAndSort();
}

void ProblemsPanel::onSeverityFilterChanged(int)
{
    filterAndSort();
}

void ProblemsPanel::onSourceFilterChanged(int)
{
    filterAndSort();
}
