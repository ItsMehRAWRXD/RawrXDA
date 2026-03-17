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
    
    int64_t getConsentDate() const { return 0; }
    
    // Serialization removed for now or should use json
    // ...
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
// Feedback Dialog (Abstract/Headless Representation)
// ═══════════════════════════════════════════════════════════════════════════════

class FeedbackDialog {
public:
    explicit FeedbackDialog(void* parent = nullptr);
    virtual ~FeedbackDialog();

    void setThermalData(double currentTemp, double avgTemp, int throttleCount);
    void setThermalSnapshot(const anyMap& snapshot);
    void setSystemInfo(const anyMap& sysInfo);
    
    FeedbackEntry getFeedback() const;
    void setSubmitCallback(FeedbackSubmittedCallback callback);

    void show(); // Logic to show native dialog or log

private:
    FeedbackEntry m_entry;
    anyMap m_systemInfo;
    anyMap m_thermalSnapshot;
    FeedbackSubmittedCallback m_submitCallback;
    void* m_nativeHandle = nullptr;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Telemetry Consent Dialog
// ═══════════════════════════════════════════════════════════════════════════════

class TelemetryConsentDialog {
public:
    explicit TelemetryConsentDialog(void* parent = nullptr);
    virtual ~TelemetryConsentDialog();

    void setCurrentConsent(const TelemetryConsent& consent);
    TelemetryConsent getConsent() const;
    void setConsentCallback(TelemetryConsentCallback callback);
    void show(); 

private:
    TelemetryConsent m_consent;
    TelemetryConsentCallback m_consentCallback;
    void* m_nativeHandle = nullptr;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Contribution Dialog
// ═══════════════════════════════════════════════════════════════════════════════

class ContributionDialog {
public:
    explicit ContributionDialog(void* parent = nullptr);
    virtual ~ContributionDialog();

    void setContributionCallback(ContributionCallback callback);
    ContributionEntry getContribution() const;
    void show();

private:
    ContributionEntry m_entry;
    ContributionCallback m_contributionCallback;
    void* m_nativeHandle = nullptr;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Feedback Manager
// ═══════════════════════════════════════════════════════════════════════════════

class FeedbackManager {
public:
    static FeedbackManager& instance();
    
    void showFeedbackDialog(void* parent = nullptr);
    void showTelemetryConsentDialog(void* parent = nullptr);
    void showContributionDialog(void* parent = nullptr);
    
    void submitQuickFeedback(const std::string& message, FeedbackCategory category);
    void reportBug(const std::string& title, const std::string& description);
    void requestFeature(const std::string& title, const std::string& description);
    void reportThermalIssue(const std::string& description, const anyMap& thermalData);
    
    void setTelemetryConsent(const TelemetryConsent& consent);
    TelemetryConsent getTelemetryConsent() const;
    bool hasTelemetryConsent() const;
    
    void sendTelemetry(const std::string& eventName, const anyMap& data);
    void sendPerformanceMetrics(const anyMap& metrics);
    void sendThermalData(const anyMap& thermalData);
    void sendCrashReport(const std::string& crashDump, const anyMap& context);
    
    void saveDraft(const FeedbackEntry& entry);
    std::vector<FeedbackEntry> loadDrafts();
    void deleteDraft(const std::string& id);
    
    std::vector<FeedbackEntry> getSubmissionHistory();
    FeedbackEntry getSubmission(const std::string& id);
    
    void setApiEndpoint(const std::string& endpoint);
    void setApiKey(const std::string& key);

private:
    FeedbackManager();
    ~FeedbackManager();
    FeedbackManager(const FeedbackManager&) = delete;
    FeedbackManager& operator=(const FeedbackManager&) = delete;
    
    void loadSettings();
    void saveSettings();
    anyMap collectSystemInfo();
    
    TelemetryConsent m_consent;
    std::string m_apiEndpoint;
    std::string m_apiKey;
    std::string m_settingsPath;
    
    std::vector<FeedbackEntry> m_drafts;
    std::vector<FeedbackEntry> m_history;
};

} // namespace rawrxd::feedback

