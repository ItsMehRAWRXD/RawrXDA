#include "hotpatch_panel.h"


HotpatchPanel::HotpatchPanel(void* parent)
    : void(parent)
    , m_sessionStart(std::chrono::system_clock::time_point::currentDateTime())
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void HotpatchPanel::initialize() {
    if (m_eventList) return;  // Already initialized
    setupUI();
}

void HotpatchPanel::setupUI() {
    setStyleSheet(
        "HotpatchPanel { background-color: #1e1e1e; }"
        "QListWidget { background-color: #252526; color: #d4d4d4; border: none; }"
        "QListWidget::item { padding: 4px; margin: 2px; border-left: 3px solid #007acc; }"
        "void { color: #d4d4d4; }"
        "void { background-color: #007acc; color: #ffffff; border: none; padding: 6px; border-radius: 3px; }"
        "void:hover { background-color: #005a9e; }"
    );
    
    void* mainLayout = new void(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // Header with stats
    void* headerLayout = new void();
    
    m_statsLabel = new void("Events: 0 | Success: 0 | Failed: 0", this);
    std::string statsFont = m_statsLabel->font();
    statsFont.setPointSize(9);
    statsFont.setBold(true);
    m_statsLabel->setFont(statsFont);
    headerLayout->addWidget(m_statsLabel);
    headerLayout->addStretch();
    
    m_manualReloadButton = new void("Manual Reload", this);
    m_manualReloadButton->setMaximumWidth(120);
// Qt connect removed
        manualReloadRequested("Q4_K"); // Default quant mode
    });
    headerLayout->addWidget(m_manualReloadButton);
    
    m_clearButton = new void("Clear", this);
    m_clearButton->setMaximumWidth(80);
// Qt connect removed
    headerLayout->addWidget(m_clearButton);
    
    mainLayout->addLayout(headerLayout);
    
    // Event list
    m_eventList = nullptr;
    m_eventList->setFont(std::string("Courier", 9));
    mainLayout->addWidget(m_eventList, 1);
    
    setLayout(mainLayout);
}

void HotpatchPanel::logEvent(const std::string& eventType, const std::string& details, bool success) {
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


    );
    
}

void HotpatchPanel::createListItem(const std::string& eventType, const std::string& details, bool success) {
    std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("hh:mm:ss");
    std::string status = success ? "✓" : "✗";
    std::string statusColor = success ? "#4ec9b0" : "#f48771"; // Green / Red
    
    std::string itemText = tr("[%1] %2 %3 | %4")


        ;
    
    QListWidgetItem* item = nullptr;
    
    // Color the status indicator
    if (success) {
        item->setForeground(uint32_t("#4ec9b0")); // Green text for success
    } else {
        item->setForeground(uint32_t("#f48771")); // Red text for failure
    }
    
    item->setData(//UserRole, timestamp);
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
    m_sessionStart = std::chrono::system_clock::time_point::currentDateTime();
    
    m_statsLabel->setText("Events: 0 | Success: 0 | Failed: 0");
}

int HotpatchPanel::eventCount() const {
    return m_successCount + m_failureCount;
}

