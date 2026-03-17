#ifndef MODEL_ROUTER_CONSOLE_H
#define MODEL_ROUTER_CONSOLE_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QDateTime>
#include <QVector>
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
class ModelRouterConsole : public QWidget {
    Q_OBJECT

public:
    struct LogEntry {
        QDateTime timestamp;
        QString level;        // INFO, WARNING, ERROR
        QString model;
        QString message;
        QString details;
        int latency_ms;
        bool success;
    };

    explicit ModelRouterConsole(ModelRouterAdapter *adapter, QWidget *parent = nullptr);
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
    void exportLogs(const QString& filename);

    /**
     * Set maximum log entries (default 1000)
     */
    void setMaxLogEntries(int max);

    /**
     * Get log entry count
     */
    int getLogCount() const { return m_log_entries.size(); }

public slots:
    void onGenerationStarted(const QString& model);
    void onGenerationComplete(const QString& result, int tokens, double latency);
    void onGenerationError(const QString& error);
    void onModelChanged(const QString& model);
    void onStatusChanged(const QString& status);

private slots:
    void onSearchTextChanged(const QString& text);
    void onFilterLevelChanged(int index);
    void onClearButtonClicked();
    void onExportButtonClicked();
    void onAutoScrollChanged(bool checked);

private:
    void createUI();
    void updateLogDisplay();
    void addLogToDisplay(const LogEntry& entry);
    void applyFilters();
    QString formatLogEntry(const LogEntry& entry) const;

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
    QVector<LogEntry> m_log_entries;
    int m_max_log_entries = 1000;

    // Filter state
    QString m_search_filter;
    QString m_level_filter;
    bool m_auto_scroll = true;
};

#endif // MODEL_ROUTER_CONSOLE_H
