/**
 * @file FeedbackSystem.hpp
 * @brief Community Feedback and Contribution System
 * 
 * Provides user feedback collection, telemetry consent, and community
 * contribution framework for RawrXD IDE thermal management.
 * 
 * @copyright RawrXD IDE 2026
 */

#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <string>
#include <optional>
#include <unordered_map>
#include <any>

namespace rawrxd::feedback {

// Type aliases for replaced Qt types
using anyMap = std::unordered_map<std::string, std::string>; // Simplified for now
using stringList = std::vector<std::string>;

// ...existing code...

/**
 * @brief Feedback category enumeration
 */
enum class FeedbackCategory {
    BugReport,
    FeatureRequest,
    PerformanceIssue,
    ThermalIssue,
    UIFeedback,
    Documentation,
    Security,
    Other
};

/**
 * @brief Feedback priority level
 */
enum class FeedbackPriority {
    Low,
    Medium,
    High,
    Critical
};

/**
 * @brief Feedback submission status
 */
enum class SubmissionStatus {
    Draft,
    Pending,
    Submitted,
    Acknowledged,
    InProgress,
    Resolved,
    Closed
};

/**
 * @brief Single feedback entry
 */
struct FeedbackEntry {
    std::string id;
    std::string title;
    std::string description;
    FeedbackCategory category;
    FeedbackPriority priority;
    SubmissionStatus status;
    
    // User info (optional, consent-based)
    std::string userEmail;
    std::string userName;
    bool consentToContact;
    
    // System info (optional, consent-based)
    std::anyMap systemInfo;
    bool includedSystemInfo;
    
    // Attachments
    std::stringList attachmentPaths;
    std::stringList screenshotPaths;
    
    // Thermal-specific data
    std::optional<double> currentTemperature;
    std::optional<double> averageTemperature;
    std::optional<int> throttleCount;
    std::anyMap thermalSnapshot;
    
    // Timestamps
    int64_t created = 0;
    int64_t modified = 0;
    int64_t submitted = 0;
    
    // Response
    std::string responseText;
    // DateTime responseDate;
    
    /**
     * @brief Convert to JSON for submission
     */
    void* toJson() const {
        void* obj;
        obj["id"] = id;
        obj["title"] = title;
        obj["description"] = description;
        obj["category"] = static_cast<int>(category);
        obj["priority"] = static_cast<int>(priority);
        obj["status"] = static_cast<int>(status);
        obj["created"] = created.toString(ISODate);
        obj["modified"] = modified.toString(ISODate);
        
        if (consentToContact) {
            obj["userEmail"] = userEmail;
            obj["userName"] = userName;
        }
        
        if (includedSystemInfo) {
            obj["systemInfo"] = void*::fromVariantMap(systemInfo);
        }
        
        if (currentTemperature) {
            obj["currentTemperature"] = *currentTemperature;
        }
        if (averageTemperature) {
            obj["averageTemperature"] = *averageTemperature;
        }
        if (throttleCount) {
            obj["throttleCount"] = *throttleCount;
        }
        if (!thermalSnapshot.empty()) {
            obj["thermalSnapshot"] = void*::fromVariantMap(thermalSnapshot);
        }
        
        return obj;
    }
    
    /**
     * @brief Create from JSON response
     */
    static FeedbackEntry fromJson(const void*& obj) {
        FeedbackEntry entry;
        entry.id = obj["id"].toString();
        entry.title = obj["title"].toString();
        entry.description = obj["description"].toString();
        entry.category = static_cast<FeedbackCategory>(obj["category"]);
        entry.priority = static_cast<FeedbackPriority>(obj["priority"]);
        entry.status = static_cast<SubmissionStatus>(obj["status"]);
        entry.created = // DateTime::fromString(obj["created"].toString(), ISODate);
        entry.modified = // DateTime::fromString(obj["modified"].toString(), ISODate);
        
        if (obj.contains("responseText")) {
            entry.responseText = obj["responseText"].toString();
            entry.responseDate = // DateTime::fromString(obj["responseDate"].toString(), ISODate);
        }
        
        return entry;
    }
};

/**
 * @brief Telemetry consent settings
 */
struct TelemetryConsent {
    bool basicTelemetry = false;           // Anonymous usage stats
    bool performanceTelemetry = false;     // Performance metrics
    bool thermalTelemetry = false;         // Thermal data
    bool crashReporting = false;           // Crash dumps
    bool featureUsage = false;             // Feature usage tracking
    bool hardwareInfo = false;             // Hardware specs
    
    // DateTime consentDate;
    std::string consentVersion;
    
    bool hasAnyConsent() const {
        return basicTelemetry || performanceTelemetry || thermalTelemetry ||
               crashReporting || featureUsage || hardwareInfo;
    }
    
    void* toJson() const {
        void* obj;
        obj["basicTelemetry"] = basicTelemetry;
        obj["performanceTelemetry"] = performanceTelemetry;
        obj["thermalTelemetry"] = thermalTelemetry;
        obj["crashReporting"] = crashReporting;
        obj["featureUsage"] = featureUsage;
        obj["hardwareInfo"] = hardwareInfo;
        obj["consentDate"] = consentDate.toString(ISODate);
        obj["consentVersion"] = consentVersion;
        return obj;
    }
    
    static TelemetryConsent fromJson(const void*& obj) {
        TelemetryConsent consent;
        consent.basicTelemetry = obj["basicTelemetry"].toBool();
        consent.performanceTelemetry = obj["performanceTelemetry"].toBool();
        consent.thermalTelemetry = obj["thermalTelemetry"].toBool();
        consent.crashReporting = obj["crashReporting"].toBool();
        consent.featureUsage = obj["featureUsage"].toBool();
        consent.hardwareInfo = obj["hardwareInfo"].toBool();
        consent.consentDate = // DateTime::fromString(obj["consentDate"].toString(), ISODate);
        consent.consentVersion = obj["consentVersion"].toString();
        return consent;
    }
};

/**
 * @brief Community contribution entry
 */
struct ContributionEntry {
    std::string id;
    std::string title;
    std::string description;
    std::string contributorName;
    std::string contributorEmail;
    
    enum class Type {
        ThermalProfile,
        DriveConfiguration,
        Algorithm,
        Documentation,
        Translation,
        Other
    } type;
    
    std::vector<uint8_t> fileContent;
    std::string fileName;
    std::string fileChecksum;
    
    std::string license;
    bool agreedToTerms;
    
    // DateTime submitted;
    SubmissionStatus status;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

using FeedbackSubmittedCallback = std::function<void(const FeedbackEntry& entry, bool success)>;
using TelemetryConsentCallback = std::function<void(const TelemetryConsent& consent)>;
using ContributionCallback = std::function<void(const ContributionEntry& entry, bool success)>;

// ═══════════════════════════════════════════════════════════════════════════════
// Feedback Dialog
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class FeedbackDialog
 * @brief Main feedback collection dialog
 */
class FeedbackDialog
{public:
    explicit FeedbackDialog(void* parent = nullptr);
    ~FeedbackDialog() override;

    // Pre-fill data
    void setThermalData(double currentTemp, double avgTemp, int throttleCount);
    void setThermalSnapshot(const std::anyMap& snapshot);
    void setSystemInfo(const std::anyMap& sysInfo);
    
    // Get result
    FeedbackEntry getFeedback() const;
    
    // Callbacks
    void setSubmitCallback(FeedbackSubmittedCallback callback);

\npublic:\n    void onCategoryChanged(int index);
    void onPriorityChanged(int index);
    void onAttachFile();
    void onAttachScreenshot();
    void onPreviewSubmission();
    void onSubmit();
    void onSaveDraft();

\npublic:\n    void feedbackSubmitted(const FeedbackEntry& entry);
    void draftSaved(const FeedbackEntry& entry);

private:
    void setupUI();
    void setupValidation();
    void collectSystemInfo();
    void updatePreview();
    bool validateInput();
    
    // UI Elements
    void* m_tabWidget;
    
    // Feedback tab
    voidEdit* m_titleEdit;
    void* m_descriptionEdit;
    void* m_categoryCombo;
    void* m_priorityCombo;
    
    // Contact tab
    voidEdit* m_emailEdit;
    voidEdit* m_nameEdit;
    void* m_consentContact;
    
    // System info tab
    void* m_includeSystemInfo;
    void* m_includeThermalData;
    void* m_systemInfoPreview;
    
    // Attachments tab
    QListWidget* m_attachmentsList;
    void* m_attachFileBtn;
    void* m_attachScreenshotBtn;
    void* m_removeAttachmentBtn;
    
    // Preview tab
    void* m_previewText;
    
    // Buttons
    voidButtonBox* m_buttonBox;
    void* m_submitBtn;
    void* m_saveDraftBtn;
    
    // Progress
    void* m_progressBar;
    void* m_statusLabel;
    
    // Data
    FeedbackEntry m_entry;
    std::anyMap m_systemInfo;
    std::anyMap m_thermalSnapshot;
    FeedbackSubmittedCallback m_submitCallback;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Telemetry Consent Dialog
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class TelemetryConsentDialog
 * @brief GDPR-compliant telemetry consent dialog
 */
class TelemetryConsentDialog
{public:
    explicit TelemetryConsentDialog(void* parent = nullptr);
    ~TelemetryConsentDialog() override;

    void setCurrentConsent(const TelemetryConsent& consent);
    TelemetryConsent getConsent() const;
    
    void setConsentCallback(TelemetryConsentCallback callback);

\npublic:\n    void onSelectAll();
    void onSelectNone();
    void onShowDetails(const std::string& category);
    void onSaveConsent();

\npublic:\n    void consentUpdated(const TelemetryConsent& consent);

private:
    void setupUI();
    void updateSummary();
    
    // Checkboxes
    void* m_basicCheck;
    void* m_performanceCheck;
    void* m_thermalCheck;
    void* m_crashCheck;
    void* m_featureCheck;
    void* m_hardwareCheck;
    
    // Info
    void* m_summaryLabel;
    void* m_detailsText;
    void* m_selectAllBtn;
    void* m_selectNoneBtn;
    
    // Legal
    void* m_agreedToPrivacy;
    void* m_privacyLink;
    
    // Data
    TelemetryConsent m_consent;
    TelemetryConsentCallback m_consentCallback;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Contribution Dialog
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class ContributionDialog
 * @brief Community contribution submission dialog
 */
class ContributionDialog
{public:
    explicit ContributionDialog(void* parent = nullptr);
    ~ContributionDialog() override;

    void setContributionCallback(ContributionCallback callback);
    ContributionEntry getContribution() const;

\npublic:\n    void onTypeChanged(int index);
    void onSelectFile();
    void onPreview();
    void onSubmit();

\npublic:\n    void contributionSubmitted(const ContributionEntry& entry);

private:
    void setupUI();
    bool validateInput();
    std::string calculateChecksum(const std::vector<uint8_t>& data);
    
    // Form
    voidEdit* m_titleEdit;
    void* m_descriptionEdit;
    void* m_typeCombo;
    
    // Contributor
    voidEdit* m_nameEdit;
    voidEdit* m_emailEdit;
    
    // File
    voidEdit* m_filePathEdit;
    void* m_selectFileBtn;
    void* m_fileSizeLabel;
    void* m_checksumLabel;
    
    // License
    void* m_licenseCombo;
    void* m_agreedToTerms;
    void* m_licensePreview;
    
    // Preview
    void* m_previewText;
    
    // Data
    ContributionEntry m_entry;
    ContributionCallback m_contributionCallback;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Feedback Manager
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class FeedbackManager
 * @brief Central manager for feedback, telemetry, and contributions
 */
class FeedbackManager 
{public:
    static FeedbackManager& instance();
    
    // Dialogs
    void showFeedbackDialog(void* parent = nullptr);
    void showTelemetryConsentDialog(void* parent = nullptr);
    void showContributionDialog(void* parent = nullptr);
    
    // Quick feedback
    void submitQuickFeedback(const std::string& message, FeedbackCategory category);
    void reportBug(const std::string& title, const std::string& description);
    void requestFeature(const std::string& title, const std::string& description);
    void reportThermalIssue(const std::string& description, const std::anyMap& thermalData);
    
    // Telemetry
    void setTelemetryConsent(const TelemetryConsent& consent);
    TelemetryConsent getTelemetryConsent() const;
    bool hasTelemetryConsent() const;
    
    void sendTelemetry(const std::string& eventName, const std::anyMap& data);
    void sendPerformanceMetrics(const std::anyMap& metrics);
    void sendThermalData(const std::anyMap& thermalData);
    void sendCrashReport(const std::string& crashDump, const std::anyMap& context);
    
    // Draft management
    void saveDraft(const FeedbackEntry& entry);
    std::vector<FeedbackEntry> loadDrafts();
    void deleteDraft(const std::string& id);
    
    // History
    std::vector<FeedbackEntry> getSubmissionHistory();
    FeedbackEntry getSubmission(const std::string& id);
    
    // Configuration
    void setApiEndpoint(const std::string& endpoint);
    void setApiKey(const std::string& key);
    
\npublic:\n    void feedbackSubmitted(const std::string& id, bool success);
    void telemetryConsentChanged(const TelemetryConsent& consent);
    void contributionSubmitted(const std::string& id, bool success);

\nprivate:\n    void onNetworkReply(void** reply);

private:
    FeedbackManager();
    ~FeedbackManager() override;
    
    void loadSettings();
    void saveSettings();
    std::anyMap collectSystemInfo();
    
    std::unique_ptr<void*> m_networkManager;
    TelemetryConsent m_consent;
    std::string m_apiEndpoint;
    std::string m_apiKey;
    std::string m_settingsPath;
    
    std::vector<FeedbackEntry> m_drafts;
    std::vector<FeedbackEntry> m_history;
};

} // namespace rawrxd::feedback

