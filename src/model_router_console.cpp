#include "model_router_console.h"
#include "model_router_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QHeaderView>
#include <QDebug>

ModelRouterConsole::ModelRouterConsole(ModelRouterAdapter *adapter, QWidget *parent)
    : QWidget(parent), m_adapter(adapter)
{
    createUI();

    if (m_adapter) {
        connect(m_adapter, &ModelRouterAdapter::generationStarted,
                this, &ModelRouterConsole::onGenerationStarted);
        connect(m_adapter, &ModelRouterAdapter::generationComplete,
                this, &ModelRouterConsole::onGenerationComplete);
        connect(m_adapter, &ModelRouterAdapter::generationError,
                this, &ModelRouterConsole::onGenerationError);
        connect(m_adapter, &ModelRouterAdapter::modelChanged,
                this, &ModelRouterConsole::onModelChanged);
        connect(m_adapter, &ModelRouterAdapter::statusChanged,
                this, &ModelRouterConsole::onStatusChanged);
    }

    qDebug() << "[ModelRouterConsole] Constructed";
}

ModelRouterConsole::~ModelRouterConsole()
{
    qDebug() << "[ModelRouterConsole] Destroyed with" << m_log_entries.size() << "entries";
}

void ModelRouterConsole::createUI()
{
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // === Control Panel ===
    QHBoxLayout *control_layout = new QHBoxLayout();

    control_layout->addWidget(new QLabel("Search:", this));
    m_search_input = new QLineEdit(this);
    m_search_input->setPlaceholderText("Filter logs...");
    connect(m_search_input, &QLineEdit::textChanged, this, &ModelRouterConsole::onSearchTextChanged);
    control_layout->addWidget(m_search_input);

    control_layout->addWidget(new QLabel("Level:", this));
    m_filter_level_combo = new QComboBox(this);
    m_filter_level_combo->addItems({"All", "INFO", "WARNING", "ERROR"});
    connect(m_filter_level_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModelRouterConsole::onFilterLevelChanged);
    control_layout->addWidget(m_filter_level_combo);

    m_auto_scroll_checkbox = new QCheckBox("Auto-scroll", this);
    m_auto_scroll_checkbox->setChecked(true);
    connect(m_auto_scroll_checkbox, &QCheckBox::toggled, this, &ModelRouterConsole::onAutoScrollChanged);
    control_layout->addWidget(m_auto_scroll_checkbox);

    m_clear_button = new QPushButton("Clear Logs", this);
    connect(m_clear_button, &QPushButton::clicked, this, &ModelRouterConsole::onClearButtonClicked);
    control_layout->addWidget(m_clear_button);

    m_export_button = new QPushButton("Export Logs", this);
    connect(m_export_button, &QPushButton::clicked, this, &ModelRouterConsole::onExportButtonClicked);
    control_layout->addWidget(m_export_button);

    control_layout->addStretch();
    main_layout->addLayout(control_layout);

    // === Log Display (Plain Text) ===
    m_log_display = new QPlainTextEdit(this);
    m_log_display->setReadOnly(true);
    m_log_display->setMaximumBlockCount(m_max_log_entries);
    m_log_display->setStyleSheet(
        "QPlainTextEdit {"
        "  font-family: 'Consolas', 'Monaco', monospace;"
        "  font-size: 9pt;"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "}"
    );
    main_layout->addWidget(m_log_display);

    // === Log Table (Detailed View) ===
    m_log_table = new QTableWidget(0, 6, this);
    m_log_table->setHorizontalHeaderLabels({
        "Timestamp", "Level", "Model", "Message", "Latency", "Status"
    });
    m_log_table->horizontalHeader()->setStretchLastSection(true);
    m_log_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_log_table->setAlternatingRowColors(true);
    m_log_table->setMaximumHeight(200);
    main_layout->addWidget(m_log_table);

    setLayout(main_layout);
    setMinimumSize(700, 500);
}

void ModelRouterConsole::addLogEntry(const LogEntry& entry)
{
    m_log_entries.append(entry);

    // Limit log size
    if (m_log_entries.size() > m_max_log_entries) {
        m_log_entries.removeFirst();
    }

    addLogToDisplay(entry);
}

void ModelRouterConsole::addLogToDisplay(const LogEntry& entry)
{
    // Add to text display
    QString log_line = formatLogEntry(entry);
    
    // Color-code by level
    QString color = "#d4d4d4";
    if (entry.level == "WARNING") {
        color = "#dcdcaa";  // Yellow
    } else if (entry.level == "ERROR") {
        color = "#f48771";  // Red
    } else if (entry.level == "INFO") {
        color = "#4ec9b0";  // Cyan
    }
    
    m_log_display->appendHtml(QString("<span style='color:%1'>%2</span>").arg(color).arg(log_line));

    if (m_auto_scroll) {
        m_log_display->ensureCursorVisible();
    }

    // Add to table
    int row = m_log_table->rowCount();
    m_log_table->insertRow(row);
    m_log_table->setItem(row, 0, new QTableWidgetItem(entry.timestamp.toString("hh:mm:ss.zzz")));
    m_log_table->setItem(row, 1, new QTableWidgetItem(entry.level));
    m_log_table->setItem(row, 2, new QTableWidgetItem(entry.model));
    m_log_table->setItem(row, 3, new QTableWidgetItem(entry.message));
    m_log_table->setItem(row, 4, new QTableWidgetItem(QString::number(entry.latency_ms) + "ms"));
    m_log_table->setItem(row, 5, new QTableWidgetItem(entry.success ? "✓" : "✗"));

    // Color-code table row
    if (entry.level == "ERROR") {
        for (int col = 0; col < 6; ++col) {
            m_log_table->item(row, col)->setBackground(QColor(255, 230, 230));
        }
    } else if (entry.level == "WARNING") {
        for (int col = 0; col < 6; ++col) {
            m_log_table->item(row, col)->setBackground(QColor(255, 255, 230));
        }
    }
}

QString ModelRouterConsole::formatLogEntry(const LogEntry& entry) const
{
    return QString("[%1] [%2] [%3] %4")
        .arg(entry.timestamp.toString("hh:mm:ss.zzz"))
        .arg(entry.level)
        .arg(entry.model)
        .arg(entry.message);
}

void ModelRouterConsole::clearLogs()
{
    m_log_entries.clear();
    m_log_display->clear();
    m_log_table->setRowCount(0);
    qDebug() << "[ModelRouterConsole] Logs cleared";
}

void ModelRouterConsole::exportLogs(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[ModelRouterConsole] Cannot open file for export:" << filename;
        return;
    }

    QTextStream stream(&file);
    stream << "=== Model Router Console Log Export ===\n";
    stream << "Generated: " << QDateTime::currentDateTime().toString() << "\n";
    stream << "Total Entries: " << m_log_entries.size() << "\n\n";

    for (const auto& entry : m_log_entries) {
        stream << formatLogEntry(entry) << "\n";
        if (!entry.details.isEmpty()) {
            stream << "  Details: " << entry.details << "\n";
        }
    }

    file.close();
    qDebug() << "[ModelRouterConsole] Exported" << m_log_entries.size() << "entries to" << filename;
}

void ModelRouterConsole::setMaxLogEntries(int max)
{
    m_max_log_entries = max;
    m_log_display->setMaximumBlockCount(max);
}

void ModelRouterConsole::updateLogDisplay()
{
    m_log_display->clear();
    m_log_table->setRowCount(0);

    for (const auto& entry : m_log_entries) {
        addLogToDisplay(entry);
    }
}

void ModelRouterConsole::applyFilters()
{
    // Get current filter settings
    QString searchText = m_search_input ? m_search_input->text().trimmed() : QString();
    QString levelFilter = m_filter_level_combo ? m_filter_level_combo->currentText() : "All";

    // Clear and rebuild display with filtered entries
    m_log_display->clear();
    m_log_table->setRowCount(0);

    for (const auto& entry : m_log_entries) {
        // Level filter: skip entries that don't match the selected level
        if (levelFilter != "All" && entry.level != levelFilter) {
            continue;
        }

        // Text search filter: check message, model, and details fields
        if (!searchText.isEmpty()) {
            bool match = entry.message.contains(searchText, Qt::CaseInsensitive)
                      || entry.model.contains(searchText, Qt::CaseInsensitive)
                      || entry.details.contains(searchText, Qt::CaseInsensitive);
            if (!match) continue;
        }

        addLogToDisplay(entry);
    }
}

// === Slot Implementations ===

void ModelRouterConsole::onGenerationStarted(const QString& model)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = "INFO";
    entry.model = model;
    entry.message = "Generation started";
    entry.latency_ms = 0;
    entry.success = true;
    
    addLogEntry(entry);
}

void ModelRouterConsole::onGenerationComplete(const QString& result, int tokens, double latency)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = "INFO";
    entry.model = m_adapter ? m_adapter->getActiveModel() : "Unknown";
    entry.message = QString("Generation complete: %1 tokens").arg(tokens);
    entry.details = QString("Result length: %1 chars").arg(result.length());
    entry.latency_ms = (int)latency;
    entry.success = true;
    
    addLogEntry(entry);
}

void ModelRouterConsole::onGenerationError(const QString& error)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = "ERROR";
    entry.model = m_adapter ? m_adapter->getActiveModel() : "Unknown";
    entry.message = "Generation failed";
    entry.details = error;
    entry.latency_ms = 0;
    entry.success = false;
    
    addLogEntry(entry);
}

void ModelRouterConsole::onModelChanged(const QString& model)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = "INFO";
    entry.model = model;
    entry.message = "Model switched";
    entry.latency_ms = 0;
    entry.success = true;
    
    addLogEntry(entry);
}

void ModelRouterConsole::onStatusChanged(const QString& status)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = "INFO";
    entry.model = m_adapter ? m_adapter->getActiveModel() : "System";
    entry.message = status;
    entry.latency_ms = 0;
    entry.success = true;
    
    addLogEntry(entry);
}

void ModelRouterConsole::onSearchTextChanged(const QString& text)
{
    m_search_filter = text;
    applyFilters();
}

void ModelRouterConsole::onFilterLevelChanged(int index)
{
    m_level_filter = m_filter_level_combo->itemText(index);
    applyFilters();
}

void ModelRouterConsole::onClearButtonClicked()
{
    clearLogs();
}

void ModelRouterConsole::onExportButtonClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        "Export Console Logs", "", "Log Files (*.log);;Text Files (*.txt)");
    
    if (!filename.isEmpty()) {
        exportLogs(filename);
    }
}

void ModelRouterConsole::onAutoScrollChanged(bool checked)
{
    m_auto_scroll = checked;
}

#include "model_router_console.moc"
