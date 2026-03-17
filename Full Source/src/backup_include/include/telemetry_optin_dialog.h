// Telemetry Opt-In Dialog - Privacy-first telemetry consent
// Production-ready with clear explanation and user control
#pragma once

/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */

namespace RawrXD {

class TelemetryOptInDialog : public QDialog {
    /* /* Q_OBJECT */ */

public:
    explicit TelemetryOptInDialog(void* parent = nullptr);
    ~TelemetryOptInDialog() override = default;

    // Returns true if user opted in to telemetry
    bool isTelemetryEnabled() const { return m_telemetryEnabled; }
    
    // Returns true if user wants to see this dialog again
    bool shouldRemindLater() const { return m_remindLater; }

/* signals */ public:
    void telemetryDecisionMade(bool enabled);

private /* slots */ public:
    void onAcceptClicked();
    void onDeclineClicked();
    void onLearnMoreClicked();

private:
    void setupUI();
    std::wstring getTelemetryExplanation() const;
    std::wstring getCollectedDataDetails() const;
    
    void* m_remindLaterCheckbox{nullptr};
    bool m_telemetryEnabled{false};
    bool m_remindLater{false};
};

// Helper function to check if user has already made a telemetry decision
bool hasTelemetryPreference();

// Helper function to save telemetry preference
void saveTelemetryPreference(bool enabled);

// Helper function to get saved telemetry preference
bool getTelemetryPreference();

} // namespace RawrXD
