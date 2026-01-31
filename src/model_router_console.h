#ifndef MODEL_ROUTER_CONSOLE_H
#define MODEL_ROUTER_CONSOLE_H


#include <memory>

class ModelRouterAdapter;

/**
 * @class ModelRouterConsole
 * @brief Diagnostic console and request log viewer
 * 
 * Features:
 * - Detailed request/response logs
 * - Model switching history
 * - Error tracking with stack traces
 * - Performance metrics per request
 * - Search and filter capabilities
 * - Export logs to file
 */
class ModelRouterConsole : public void {

public:
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        std::string level;        // INFO, WARNING, ERROR
        std::string model;
        std::string message;
        std::string details;
        int latency_ms;
        bool success;
    };

    explicit ModelRouterConsole(ModelRouterAdapter *adapter, void *parent = nullptr);
    ~ModelRouterConsole();

    /**
     * Add log entry to console
     */
    void addLogEntry(const LogEntry& entry);

    /**
     * Clear all logs
     */
    void clearLogs();

    /**
     * Export logs to file
     */
    void exportLogs(const std::string& filename);

    /**
     * Set maximum log entries (default 1000)
     */
    void setMaxLogEntries(int max);

    /**
     * Get log entry count
     */
    int getLogCount() const { return m_log_entries.size(); }

public:
    void onGenerationStarted(const std::string& model);
    void onGenerationComplete(const std::string& result, int tokens, double latency);
    void onGenerationError(const std::string& error);
    void onModelChanged(const std::string& model);
    void onStatusChanged(const std::string& status);

private:
    void onSearchTextChanged(const std::string& text);
    void onFilterLevelChanged(int index);
    void onClearButtonClicked();
    void onExportButtonClicked();
    void onAutoScrollChanged(bool checked);

private:
    void createUI();
    void updateLogDisplay();
    void addLogToDisplay(const LogEntry& entry);
    void applyFilters();
    std::string formatLogEntry(const LogEntry& entry) const;

    ModelRouterAdapter *m_adapter;

    // UI components
    QPlainTextEdit *m_log_display;
    QTableWidget *m_log_table;
    QLineEdit *m_search_input;
    QComboBox *m_filter_level_combo;
    QCheckBox *m_auto_scroll_checkbox;
    QPushButton *m_clear_button;
    QPushButton *m_export_button;

    // Log storage
    std::vector<LogEntry> m_log_entries;
    int m_max_log_entries = 1000;

    // Filter state
    std::string m_search_filter;
    std::string m_level_filter;
    bool m_auto_scroll = true;
};

#endif // MODEL_ROUTER_CONSOLE_H

