#pragma once

// <QWidget> removed (Qt-free build)
// <QListWidget> removed (Qt-free build)
// Qt include removed (Qt-free build)
// <QLabel> removed (Qt-free build)
// <QPushButton> removed (Qt-free build)
// Qt include removed (Qt-free build)
// <QVBoxLayout> removed (Qt-free build)

/**
 * \class HotpatchPanel
 * \brief Real-time hotpatch/hot-reload visualization panel
 * 
 * Displays all hotpatch/reload events with timestamps and status.
 * Allows manual triggering of reloads and provides visual feedback.
 */
class HotpatchPanel  {
    /* Q_OBJECT */

public:
    explicit HotpatchPanel(QWidget* parent = nullptr);
    
    /**
     * Two-phase initialization - call after QApplication is ready
     * Creates all Qt widgets and sets up UI
     */
    void initialize();
    
    /**
     * Log a hotpatch event
     * \param eventType Event type ("quantReloaded", "moduleReloaded", "reloadFailed", etc.)
     * \param details Event details/error message
     * \param success Whether the event was successful
     */
    void logEvent(const QString& eventType, const QString& details, bool success = true);
    
    /**
     * Clear all logged events
     */
    void clearLog();
    
    /**
     * Get the total count of hotpatch events
     */
    int eventCount() const;

signals:
    /**
     * Emitted when user requests a manual reload
     */
    void manualReloadRequested(const QString& quantType);

private:
    void setupUI();
    void createListItem(const QString& eventType, const QString& details, bool success);
    
    QListWidget* m_eventList{};
    QLabel* m_statsLabel{};
    QPushButton* m_clearButton{};
    QPushButton* m_manualReloadButton{};
    
    int m_successCount{0};
    int m_failureCount{0};
    QDateTime m_sessionStart{};
};
