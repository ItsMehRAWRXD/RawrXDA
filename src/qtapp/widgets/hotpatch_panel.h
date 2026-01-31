#pragma once


/**
 * \class HotpatchPanel
 * \brief Real-time hotpatch/hot-reload visualization panel
 * 
 * Displays all hotpatch/reload events with timestamps and status.
 * Allows manual triggering of reloads and provides visual feedback.
 */
class HotpatchPanel : public void {

public:
    explicit HotpatchPanel(void* parent = nullptr);
    
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
    void logEvent(const std::string& eventType, const std::string& details, bool success = true);
    
    /**
     * Clear all logged events
     */
    void clearLog();
    
    /**
     * Get the total count of hotpatch events
     */
    int eventCount() const;


    /**
     * Emitted when user requests a manual reload
     */
    void manualReloadRequested(const std::string& quantType);

private:
    void setupUI();
    void createListItem(const std::string& eventType, const std::string& details, bool success);
    
    QListWidget* m_eventList{};
    QLabel* m_statsLabel{};
    QPushButton* m_clearButton{};
    QPushButton* m_manualReloadButton{};
    
    int m_successCount{0};
    int m_failureCount{0};
    std::chrono::system_clock::time_point m_sessionStart{};
};

