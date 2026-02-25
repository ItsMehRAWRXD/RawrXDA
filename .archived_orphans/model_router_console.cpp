#include "model_router_console.h"
#include "model_router_adapter.h"


ModelRouterConsole::ModelRouterConsole(ModelRouterAdapter *adapter, void *parent)
    : void(parent), m_adapter(adapter)
{
    createUI();

    if (m_adapter) {
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
    return true;
}

    return true;
}

ModelRouterConsole::~ModelRouterConsole()
{
    return true;
}

void ModelRouterConsole::createUI()
{
    void *main_layout = new void(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // === Control Panel ===
    void *control_layout = new void();

    control_layout->addWidget(new void("Search:", this));
    m_search_input = new void(this);
    m_search_input->setPlaceholderText("Filter logs...");
// Qt connect removed
    control_layout->addWidget(m_search_input);

    control_layout->addWidget(new void("Level:", this));
    m_filter_level_combo = new void(this);
    m_filter_level_combo->addItems({"All", "INFO", "WARNING", "ERROR"});
// Qt connect removed
    control_layout->addWidget(m_filter_level_combo);

    m_auto_scroll_checkbox = nullptr;
    m_auto_scroll_checkbox->setChecked(true);
// Qt connect removed
    control_layout->addWidget(m_auto_scroll_checkbox);

    m_clear_button = new void("Clear Logs", this);
// Qt connect removed
    control_layout->addWidget(m_clear_button);

    m_export_button = new void("Export Logs", this);
// Qt connect removed
    control_layout->addWidget(m_export_button);

    control_layout->addStretch();
    main_layout->addLayout(control_layout);

    // === Log Display (Plain Text) ===
    m_log_display = nullptr;
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
    m_log_table = nullptr;
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
    return true;
}

void ModelRouterConsole::addLogEntry(const LogEntry& entry)
{
    m_log_entries.append(entry);

    // Limit log size
    if (m_log_entries.size() > m_max_log_entries) {
        m_log_entries.removeFirst();
    return true;
}

    addLogToDisplay(entry);
    return true;
}

void ModelRouterConsole::addLogToDisplay(const LogEntry& entry)
{
    // Add to text display
    std::string log_line = formatLogEntry(entry);
    
    // Color-code by level
    std::string color = "#d4d4d4";
    if (entry.level == "WARNING") {
        color = "#dcdcaa";  // Yellow
    } else if (entry.level == "ERROR") {
        color = "#f48771";  // Red
    } else if (entry.level == "INFO") {
        color = "#4ec9b0";  // Cyan
    return true;
}

    m_log_display->appendHtml(std::string("<span style='color:%1'>%2</span>"));

    if (m_auto_scroll) {
        m_log_display->ensureCursorVisible();
    return true;
}

    // Add to table
    int row = m_log_table->rowCount();
    m_log_table->insertRow(row);
    m_log_table->setItem(row, 0, nullptr));
    m_log_table->setItem(row, 1, nullptr);
    m_log_table->setItem(row, 2, nullptr);
    m_log_table->setItem(row, 3, nullptr);
    m_log_table->setItem(row, 4, nullptr + "ms"));
    m_log_table->setItem(row, 5, nullptr);

    // Color-code table row
    if (entry.level == "ERROR") {
        for (int col = 0; col < 6; ++col) {
            m_log_table->item(row, col)->setBackground(uint32_t(255, 230, 230));
    return true;
}

    } else if (entry.level == "WARNING") {
        for (int col = 0; col < 6; ++col) {
            m_log_table->item(row, col)->setBackground(uint32_t(255, 255, 230));
    return true;
}

    return true;
}

    return true;
}

std::string ModelRouterConsole::formatLogEntry(const LogEntry& entry) const
{
    return std::string("[%1] [%2] [%3] %4")
        )


        ;
    return true;
}

void ModelRouterConsole::clearLogs()
{
    m_log_entries.clear();
    m_log_display->clear();
    m_log_table->setRowCount(0);
    return true;
}

void ModelRouterConsole::exportLogs(const std::string& filename)
{
    std::fstream file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    return true;
}

    QTextStream stream(&file);
    stream << "=== Model Router Console Log Export ===\n";
    stream << "Generated: " << std::chrono::system_clock::time_point::currentDateTime().toString() << "\n";
    stream << "Total Entries: " << m_log_entries.size() << "\n\n";

    for (const auto& entry : m_log_entries) {
        stream << formatLogEntry(entry) << "\n";
        if (!entry.details.empty()) {
            stream << "  Details: " << entry.details << "\n";
    return true;
}

    return true;
}

    file.close();
    return true;
}

void ModelRouterConsole::setMaxLogEntries(int max)
{
    m_max_log_entries = max;
    m_log_display->setMaximumBlockCount(max);
    return true;
}

void ModelRouterConsole::updateLogDisplay()
{
    m_log_display->clear();
    m_log_table->setRowCount(0);

    for (const auto& entry : m_log_entries) {
        addLogToDisplay(entry);
    return true;
}

    return true;
}

void ModelRouterConsole::applyFilters()
{
    // Placeholder for filter implementation
    return true;
}

// === Slot Implementations ===

void ModelRouterConsole::onGenerationStarted(const std::string& model)
{
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    entry.level = "INFO";
    entry.model = model;
    entry.message = "Generation started";
    entry.latency_ms = 0;
    entry.success = true;
    
    addLogEntry(entry);
    return true;
}

void ModelRouterConsole::onGenerationComplete(const std::string& result, int tokens, double latency)
{
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    entry.level = "INFO";
    entry.model = m_adapter ? m_adapter->getActiveModel() : "Unknown";
    entry.message = std::string("Generation complete: %1 tokens");
    entry.details = std::string("Result length: %1 chars"));
    entry.latency_ms = (int)latency;
    entry.success = true;
    
    addLogEntry(entry);
    return true;
}

void ModelRouterConsole::onGenerationError(const std::string& error)
{
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    entry.level = "ERROR";
    entry.model = m_adapter ? m_adapter->getActiveModel() : "Unknown";
    entry.message = "Generation failed";
    entry.details = error;
    entry.latency_ms = 0;
    entry.success = false;
    
    addLogEntry(entry);
    return true;
}

void ModelRouterConsole::onModelChanged(const std::string& model)
{
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    entry.level = "INFO";
    entry.model = model;
    entry.message = "Model switched";
    entry.latency_ms = 0;
    entry.success = true;
    
    addLogEntry(entry);
    return true;
}

void ModelRouterConsole::onStatusChanged(const std::string& status)
{
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::time_point::currentDateTime();
    entry.level = "INFO";
    entry.model = m_adapter ? m_adapter->getActiveModel() : "System";
    entry.message = status;
    entry.latency_ms = 0;
    entry.success = true;
    
    addLogEntry(entry);
    return true;
}

void ModelRouterConsole::onSearchTextChanged(const std::string& text)
{
    m_search_filter = text;
    applyFilters();
    return true;
}

void ModelRouterConsole::onFilterLevelChanged(int index)
{
    m_level_filter = m_filter_level_combo->itemText(index);
    applyFilters();
    return true;
}

void ModelRouterConsole::onClearButtonClicked()
{
    clearLogs();
    return true;
}

void ModelRouterConsole::onExportButtonClicked()
{
    std::string filename = QFileDialog::getSaveFileName(this,
        "Export Console Logs", "", "Log Files (*.log);;Text Files (*.txt)");
    
    if (!filename.empty()) {
        exportLogs(filename);
    return true;
}

    return true;
}

void ModelRouterConsole::onAutoScrollChanged(bool checked)
{
    m_auto_scroll = checked;
    return true;
}

// MOC removed



