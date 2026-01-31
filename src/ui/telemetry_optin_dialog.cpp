// Telemetry Opt-In Dialog - Implementation
#include "telemetry_optin_dialog.h"


namespace RawrXD {

TelemetryOptInDialog::TelemetryOptInDialog(void* parent)
    : void(parent)
{
    setWindowTitle("Help Improve RawrXD IDE");
    setMinimumSize(600, 500);
    setModal(true);
    
    setupUI();
    
}

void TelemetryOptInDialog::setupUI() {
    void* mainLayout = new void(this);
    mainLayout->setSpacing(15);
    
    // Header
    void* headerLabel = new void(
        "<h2>📊 Help Us Improve RawrXD IDE</h2>"
        "<p>Your feedback helps make this IDE better for everyone!</p>",
        this);
    headerLabel->setWordWrap(true);
    headerLabel->setStyleSheet("color: #d4d4d4; margin-bottom: 10px;");
    mainLayout->addWidget(headerLabel);
    
    // Explanation section
    QTextBrowser* explanationBrowser = nullptr;
    explanationBrowser->setOpenExternalLinks(false);
    explanationBrowser->setHtml(getTelemetryExplanation());
    explanationBrowser->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  border-radius: 5px;"
        "  padding: 10px;"
        "}"
    );
    explanationBrowser->setMaximumHeight(250);
    mainLayout->addWidget(explanationBrowser);
    
    // Privacy emphasis
    void* privacyLabel = new void(
        "🔒 <b>Your Privacy Matters:</b> All data is anonymous and used solely for improving the IDE. "
        "No personal information, code, or file paths are collected.",
        this);
    privacyLabel->setWordWrap(true);
    privacyLabel->setStyleSheet(
        "background-color: #1e4620; color: #4ec9b0; padding: 10px; border-radius: 5px; border: 1px solid #16825d;");
    mainLayout->addWidget(privacyLabel);
    
    // Collected data details
    QTextBrowser* dataBrowser = nullptr;
    dataBrowser->setHtml(getCollectedDataDetails());
    dataBrowser->setStyleSheet(
        "QTextBrowser {"
        "  background-color: #252526;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  border-radius: 5px;"
        "  padding: 8px;"
        "}"
    );
    dataBrowser->setMaximumHeight(120);
    mainLayout->addWidget(dataBrowser);
    
    // Remind later checkbox
    m_remindLaterCheckbox = nullptr", this);
    m_remindLaterCheckbox->setStyleSheet("color: #888888;");
    mainLayout->addWidget(m_remindLaterCheckbox);
    
    mainLayout->addStretch();
    
    // Buttons
    void* buttonLayout = new void();
    buttonLayout->addStretch();
    
    void* learnMoreButton = new void("📖 Learn More", this);
    learnMoreButton->setStyleSheet(
        "void {"
        "  background-color: #3c3c3c;"
        "  color: #d4d4d4;"
        "  padding: 8px 16px;"
        "  border: none;"
        "  border-radius: 4px;"
        "}"
        "void:hover { background-color: #4c4c4c; }"
    );
// Qt connect removed
    buttonLayout->addWidget(learnMoreButton);
    
    void* declineButton = new void("✗ No Thanks", this);
    declineButton->setStyleSheet(
        "void {"
        "  background-color: #c5323d;"
        "  color: white;"
        "  padding: 8px 16px;"
        "  border: none;"
        "  border-radius: 4px;"
        "}"
        "void:hover { background-color: #e53e49; }"
    );
// Qt connect removed
    buttonLayout->addWidget(declineButton);
    
    void* acceptButton = new void("✓ Yes, Help Improve", this);
    acceptButton->setStyleSheet(
        "void {"
        "  background-color: #16825d;"
        "  color: white;"
        "  padding: 8px 16px;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "}"
        "void:hover { background-color: #1a9c6f; }"
    );
// Qt connect removed
    buttonLayout->addWidget(acceptButton);
    
    mainLayout->addLayout(buttonLayout);
    
    setStyleSheet("void { background-color: #252526; }");
}

std::string TelemetryOptInDialog::getTelemetryExplanation() const {
    return R"(
<div style='font-size: 12px; line-height: 1.6;'>
<h3 style='color: #4ec9b0;'>What is Telemetry?</h3>
<p>
Telemetry helps us understand how RawrXD IDE is used in the real world. This data enables us to:
</p>
<ul style='margin-left: 20px;'>
<li><b>Fix bugs faster</b> - Identify which features crash or have errors</li>
<li><b>Improve performance</b> - See which operations are slow or use too much memory</li>
<li><b>Guide development</b> - Focus on features you actually use</li>
<li><b>Test hardware compatibility</b> - Ensure the IDE works on diverse systems</li>
</ul>

<h3 style='color: #4ec9b0;'>Your Control</h3>
<p>
✓ Telemetry is <b>completely optional</b><br>
✓ You can enable/disable it anytime in Settings → Telemetry<br>
✓ You can review all collected data before it's sent<br>
✓ All data is anonymous and encrypted<br>
</p>
</div>
)";
}

std::string TelemetryOptInDialog::getCollectedDataDetails() const {
    return R"(
<div style='font-size: 11px; color: #d4d4d4;'>
<h4 style='color: #569cd6;'>📋 What We Collect:</h4>
<ul style='margin-left: 15px; line-height: 1.4;'>
<li><b>Usage metrics:</b> Feature usage counts, session duration, action timestamps</li>
<li><b>Performance data:</b> Inference speed, memory usage, GPU utilization</li>
<li><b>Error reports:</b> Crash logs, error types (no stack traces with personal data)</li>
<li><b>System info:</b> OS version, GPU model, RAM size, CPU type</li>
</ul>

<h4 style='color: #569cd6;'>🚫 What We DON'T Collect:</h4>
<ul style='margin-left: 15px; line-height: 1.4;'>
<li>❌ Your source code or file contents</li>
<li>❌ File paths or project names</li>
<li>❌ Personal information (email, username, etc.)</li>
<li>❌ Network activity or IP addresses</li>
</ul>
</div>
)";
}

void TelemetryOptInDialog::onAcceptClicked() {
    m_telemetryEnabled = true;
    m_remindLater = false;


    saveTelemetryPreference(true);
    telemetryDecisionMade(true);
    
    accept();
}

void TelemetryOptInDialog::onDeclineClicked() {
    m_telemetryEnabled = false;
    m_remindLater = m_remindLaterCheckbox->isChecked();
    
    if (m_remindLater) {
        // Don't save preference, will ask again later
    } else {
        saveTelemetryPreference(false);
    }
    
    telemetryDecisionMade(false);
    
    reject();
}

void TelemetryOptInDialog::onLearnMoreClicked() {
    
    // Open privacy policy or documentation
    // For now, show a detailed message box
    std::string detailedInfo = 
        "<h3>RawrXD IDE Telemetry Details</h3>"
        "<p><b>Data Storage:</b></p>"
        "<ul>"
        "<li>Telemetry data is stored locally before being sent</li>"
        "<li>You can review it at: <code>%APPDATA%/RawrXD/telemetry/</code></li>"
        "<li>Data is sent encrypted over HTTPS</li>"
        "</ul>"
        "<p><b>Data Retention:</b></p>"
        "<ul>"
        "<li>Aggregate metrics: 90 days</li>"
        "<li>Individual events: 30 days</li>"
        "<li>Crash reports: 60 days</li>"
        "</ul>"
        "<p><b>Open Source Commitment:</b></p>"
        "<ul>"
        "<li>Telemetry code is open source - you can audit it!</li>"
        "<li>Check <code>src/telemetry/</code> in the repository</li>"
        "</ul>";
    
    void* detailDialog = new void(this);
    detailDialog->setWindowTitle("Telemetry Technical Details");
    detailDialog->setMinimumSize(500, 400);
    
    void* layout = new void(detailDialog);
    QTextBrowser* browser = nullptr;
    browser->setHtml(detailedInfo);
    browser->setOpenExternalLinks(true);
    browser->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; border: none;");
    layout->addWidget(browser);
    
    void* closeBtn = new void("Close", detailDialog);
// Qt connect removed
    layout->addWidget(closeBtn);
    
    detailDialog->setStyleSheet("void { background-color: #252526; }");
    detailDialog->exec();
}

// Helper functions implementation

bool hasTelemetryPreference() {
    void* settings("RawrXD", "AgenticIDE");
    return settings.contains("telemetry/decision_made");
}

void saveTelemetryPreference(bool enabled) {
    void* settings("RawrXD", "AgenticIDE");
    settings.setValue("telemetry/enabled", enabled);
    settings.setValue("telemetry/decision_made", true);
    settings.setValue("telemetry/decision_date", std::chrono::system_clock::time_point::currentDateTime());
    settings.sync();
    
}

bool getTelemetryPreference() {
    void* settings("RawrXD", "AgenticIDE");
    return settings.value("telemetry/enabled", false).toBool();
}

} // namespace RawrXD

