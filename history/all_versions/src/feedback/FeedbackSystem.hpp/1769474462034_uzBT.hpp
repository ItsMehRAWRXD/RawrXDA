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

#include <QObject>
#include <QDialog>
#include <QWidget>
#include <QString>
#include <QVariantMap>
#include <QDateTime>
#include <QUuid>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <memory>
#include <vector>
#include <functional>
#include <optional>

// Forward declarations
class QTextEdit;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QTabWidget;
class QListWidget;
class QStackedWidget;
class QVBoxLayout;
class QHBoxLayout;
class QGroupBox;
class QFormLayout;
class QDialogButtonBox;
class QSpinBox;
class QSlider;

namespace rawrxd::feedback {

// ═══════════════════════════════════════════════════════════════════════════════
// Data Structures
// ═══════════════════════════════════════════════════════════════════════════════

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
    QString id;
    QString title;
    QString description;
    FeedbackCategory category;
    FeedbackPriority priority;
    SubmissionStatus status;
    
    // User info (optional, consent-based)
    QString userEmail;
    QString userName;
    bool consentToContact;
    
    // System info (optional, consent-based)
    QVariantMap systemInfo;
    bool includedSystemInfo;
    
    // Attachments
    QStringList attachmentPaths;
    QStringList screenshotPaths;
    
    // Thermal-specific data
    std::optional<double> currentTemperature;
    std::optional<double> averageTemperature;
    std::optional<int> throttleCount;
    QVariantMap thermalSnapshot;
    
    // Timestamps
    QDateTime created;
    QDateTime modified;
    QDateTime submitted;
    
    // Response
    QString responseText;
    QDateTime responseDate;
    
    /**
     * @brief Convert to JSON for submission
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["title"] = title;
        obj["description"] = description;
        obj["category"] = static_cast<int>(category);
        obj["priority"] = static_cast<int>(priority);
        obj["status"] = static_cast<int>(status);
        obj["created"] = created.toString(Qt::ISODate);
        obj["modified"] = modified.toString(Qt::ISODate);
        
        if (consentToContact) {
            obj["userEmail"] = userEmail;
            obj["userName"] = userName;
        }
        
        if (includedSystemInfo) {
            obj["systemInfo"] = QJsonObject::fromVariantMap(systemInfo);
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
        if (!thermalSnapshot.isEmpty()) {
            obj["thermalSnapshot"] = QJsonObject::fromVariantMap(thermalSnapshot);
        }
        
        return obj;
    }
    
    /**
     * @brief Create from JSON response
     */
    static FeedbackEntry fromJson(const QJsonObject& obj) {
        FeedbackEntry entry;
        entry.id = obj["id"].toString();
        entry.title = obj["title"].toString();
        entry.description = obj["description"].toString();
        entry.category = static_cast<FeedbackCategory>(obj["category"].toInt());
        entry.priority = static_cast<FeedbackPriority>(obj["priority"].toInt());
        entry.status = static_cast<SubmissionStatus>(obj["status"].toInt());
        entry.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
        entry.modified = QDateTime::fromString(obj["modified"].toString(), Qt::ISODate);
        
        if (obj.contains("responseText")) {
            entry.responseText = obj["responseText"].toString();
            entry.responseDate = QDateTime::fromString(obj["responseDate"].toString(), Qt::ISODate);
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
    
    QDateTime consentDate;
    QString consentVersion;
    
    bool hasAnyConsent() const {
        return basicTelemetry || performanceTelemetry || thermalTelemetry ||
               crashReporting || featureUsage || hardwareInfo;
    }
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["basicTelemetry"] = basicTelemetry;
        obj["performanceTelemetry"] = performanceTelemetry;
        obj["thermalTelemetry"] = thermalTelemetry;
        obj["crashReporting"] = crashReporting;
        obj["featureUsage"] = featureUsage;
        obj["hardwareInfo"] = hardwareInfo;
        obj["consentDate"] = consentDate.toString(Qt::ISODate);
        obj["consentVersion"] = consentVersion;
        return obj;
    }
    
    static TelemetryConsent fromJson(const QJsonObject& obj) {
        TelemetryConsent consent;
        consent.basicTelemetry = obj["basicTelemetry"].toBool();
        consent.performanceTelemetry = obj["performanceTelemetry"].toBool();
        consent.thermalTelemetry = obj["thermalTelemetry"].toBool();
        consent.crashReporting = obj["crashReporting"].toBool();
        consent.featureUsage = obj["featureUsage"].toBool();
        consent.hardwareInfo = obj["hardwareInfo"].toBool();
        consent.consentDate = QDateTime::fromString(obj["consentDate"].toString(), Qt::ISODate);
        consent.consentVersion = obj["consentVersion"].toString();
        return consent;
    }
};

/**
 * @brief Community contribution entry
 */
struct ContributionEntry {
    QString id;
    QString title;
    QString description;
    QString contributorName;
    QString contributorEmail;
    
    enum class Type {
        ThermalProfile,
        DriveConfiguration,
        Algorithm,
        Documentation,
        Translation,
        Other
    } type;
    
    QByteArray fileContent;
    QString fileName;
    QString fileChecksum;
    
    QString license;
    bool agreedToTerms;
    
    QDateTime submitted;
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
class FeedbackDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FeedbackDialog(QWidget* parent = nullptr);
    ~FeedbackDialog() override;

    // Pre-fill data
    void setThermalData(double currentTemp, double avgTemp, int throttleCount);
    void setThermalSnapshot(const QVariantMap& snapshot);
    void setSystemInfo(const QVariantMap& sysInfo);
    
    // Get result
    FeedbackEntry getFeedback() const;
    
    // Callbacks
    void setSubmitCallback(FeedbackSubmittedCallback callback);

public slots:
    void onCategoryChanged(int index);
    void onPriorityChanged(int index);
    void onAttachFile();
    void onAttachScreenshot();
    void onPreviewSubmission();
    void onSubmit();
    void onSaveDraft();

signals:
    void feedbackSubmitted(const FeedbackEntry& entry);
    void draftSaved(const FeedbackEntry& entry);

private:
    void setupUI();
    void setupValidation();
    void collectSystemInfo();
    void updatePreview();
    bool validateInput();
    
    // UI Elements
    QTabWidget* m_tabWidget;
    
    // Feedback tab
    QLineEdit* m_titleEdit;
    QTextEdit* m_descriptionEdit;
    QComboBox* m_categoryCombo;
    QComboBox* m_priorityCombo;
    
    // Contact tab
    QLineEdit* m_emailEdit;
    QLineEdit* m_nameEdit;
    QCheckBox* m_consentContact;
    
    // System info tab
    QCheckBox* m_includeSystemInfo;
    QCheckBox* m_includeThermalData;
    QTextEdit* m_systemInfoPreview;
    
    // Attachments tab
    QListWidget* m_attachmentsList;
    QPushButton* m_attachFileBtn;
    QPushButton* m_attachScreenshotBtn;
    QPushButton* m_removeAttachmentBtn;
    
    // Preview tab
    QTextEdit* m_previewText;
    
    // Buttons
    QDialogButtonBox* m_buttonBox;
    QPushButton* m_submitBtn;
    QPushButton* m_saveDraftBtn;
    
    // Progress
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    
    // Data
    FeedbackEntry m_entry;
    QVariantMap m_systemInfo;
    QVariantMap m_thermalSnapshot;
    FeedbackSubmittedCallback m_submitCallback;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Telemetry Consent Dialog
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class TelemetryConsentDialog
 * @brief GDPR-compliant telemetry consent dialog
 */
class TelemetryConsentDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TelemetryConsentDialog(QWidget* parent = nullptr);
    ~TelemetryConsentDialog() override;

    void setCurrentConsent(const TelemetryConsent& consent);
    TelemetryConsent getConsent() const;
    
    void setConsentCallback(TelemetryConsentCallback callback);

public slots:
    void onSelectAll();
    void onSelectNone();
    void onShowDetails(const QString& category);
    void onSaveConsent();

signals:
    void consentUpdated(const TelemetryConsent& consent);

private:
    void setupUI();
    void updateSummary();
    
    // Checkboxes
    QCheckBox* m_basicCheck;
    QCheckBox* m_performanceCheck;
    QCheckBox* m_thermalCheck;
    QCheckBox* m_crashCheck;
    QCheckBox* m_featureCheck;
    QCheckBox* m_hardwareCheck;
    
    // Info
    QLabel* m_summaryLabel;
    QTextEdit* m_detailsText;
    QPushButton* m_selectAllBtn;
    QPushButton* m_selectNoneBtn;
    
    // Legal
    QCheckBox* m_agreedToPrivacy;
    QLabel* m_privacyLink;
    
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
class ContributionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ContributionDialog(QWidget* parent = nullptr);
    ~ContributionDialog() override;

    void setContributionCallback(ContributionCallback callback);
    ContributionEntry getContribution() const;

public slots:
    void onTypeChanged(int index);
    void onSelectFile();
    void onPreview();
    void onSubmit();

signals:
    void contributionSubmitted(const ContributionEntry& entry);

private:
    void setupUI();
    bool validateInput();
    QString calculateChecksum(const QByteArray& data);
    
    // Form
    QLineEdit* m_titleEdit;
    QTextEdit* m_descriptionEdit;
    QComboBox* m_typeCombo;
    
    // Contributor
    QLineEdit* m_nameEdit;
    QLineEdit* m_emailEdit;
    
    // File
    QLineEdit* m_filePathEdit;
    QPushButton* m_selectFileBtn;
    QLabel* m_fileSizeLabel;
    QLabel* m_checksumLabel;
    
    // License
    QComboBox* m_licenseCombo;
    QCheckBox* m_agreedToTerms;
    QTextEdit* m_licensePreview;
    
    // Preview
    QTextEdit* m_previewText;
    
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
class FeedbackManager : public QObject
{
    Q_OBJECT

public:
    static FeedbackManager& instance();
    
    // Dialogs
    void showFeedbackDialog(QWidget* parent = nullptr);
    void showTelemetryConsentDialog(QWidget* parent = nullptr);
    void showContributionDialog(QWidget* parent = nullptr);
    
    // Quick feedback
    void submitQuickFeedback(const QString& message, FeedbackCategory category);
    void reportBug(const QString& title, const QString& description);
    void requestFeature(const QString& title, const QString& description);
    void reportThermalIssue(const QString& description, const QVariantMap& thermalData);
    
    // Telemetry
    void setTelemetryConsent(const TelemetryConsent& consent);
    TelemetryConsent getTelemetryConsent() const;
    bool hasTelemetryConsent() const;
    
    void sendTelemetry(const QString& eventName, const QVariantMap& data);
    void sendPerformanceMetrics(const QVariantMap& metrics);
    void sendThermalData(const QVariantMap& thermalData);
    void sendCrashReport(const QString& crashDump, const QVariantMap& context);
    
    // Draft management
    void saveDraft(const FeedbackEntry& entry);
    std::vector<FeedbackEntry> loadDrafts();
    void deleteDraft(const QString& id);
    
    // History
    std::vector<FeedbackEntry> getSubmissionHistory();
    FeedbackEntry getSubmission(const QString& id);
    
    // Configuration
    void setApiEndpoint(const QString& endpoint);
    void setApiKey(const QString& key);
    
signals:
    void feedbackSubmitted(const QString& id, bool success);
    void telemetryConsentChanged(const TelemetryConsent& consent);
    void contributionSubmitted(const QString& id, bool success);

private slots:
    void onNetworkReply(QNetworkReply* reply);

private:
    FeedbackManager();
    ~FeedbackManager() override;
    
    void loadSettings();
    void saveSettings();
    QVariantMap collectSystemInfo();
    
    std::unique_ptr<QNetworkAccessManager> m_networkManager;
    TelemetryConsent m_consent;
    QString m_apiEndpoint;
    QString m_apiKey;
    QString m_settingsPath;
    
    std::vector<FeedbackEntry> m_drafts;
    std::vector<FeedbackEntry> m_history;
};

} // namespace rawrxd::feedback
