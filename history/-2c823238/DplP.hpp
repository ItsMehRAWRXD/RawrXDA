/**
 * @file QuantumAuthUI.hpp
 * @brief User-Facing Quantum Authentication Key Generation UI
 * 
 * Provides a wizard-style interface for users to generate, manage,
 * and enroll quantum-resistant cryptographic keys for system authentication.
 * 
 * @copyright RawrXD IDE 2026
 */

#pragma once

#include <QObject>
#include <QDialog>
#include <QWizard>
#include <QWizardPage>
#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QVariantMap>

#include <memory>
#include <vector>
#include <functional>
#include <optional>

// Forward declarations
class QLabel;
class QLineEdit;
class QTextEdit;
class QProgressBar;
class QPushButton;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QRadioButton;
class QSpinBox;
class QListWidget;
class QTimer;

namespace rawrxd::auth {

// ═══════════════════════════════════════════════════════════════════════════════
// Data Structures
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Supported key algorithms
 */
enum class KeyAlgorithm {
    RDRAND_AES256,              // RDRAND + AES-256 (fast, hardware-accelerated)
    RDRAND_ChaCha20,            // RDRAND + ChaCha20 (fast, side-channel resistant)
    RDSEED_AES256,              // RDSEED + AES-256 (higher entropy, slower)
    Hybrid_Quantum_Classical,   // Combined quantum-resistant approach
    Custom                      // User-defined parameters
};

/**
 * @brief Key purpose/usage flags
 */
enum class KeyPurpose {
    SystemAuthentication = 0x01,
    ThermalDataSigning = 0x02,
    ConfigEncryption = 0x04,
    IPC_Authentication = 0x08,
    DriveBinding = 0x10,
    All = 0xFF
};

Q_DECLARE_FLAGS(KeyPurposes, KeyPurpose)

/**
 * @brief Key strength level
 */
enum class KeyStrength {
    Standard,       // 128-bit effective security
    High,           // 192-bit effective security  
    Maximum,        // 256-bit effective security
    Paranoid        // 512-bit with additional hardening
};

/**
 * @brief Generated key metadata
 */
struct KeyMetadata {
    QString keyId;
    QString keyName;
    KeyAlgorithm algorithm;
    KeyStrength strength;
    KeyPurposes purposes;
    
    QDateTime created;
    QDateTime expires;
    QDateTime lastUsed;
    
    QString hardwareFingerprint;
    QString systemFingerprint;
    bool isBoundToHardware;
    
    int usageCount;
    int maxUsages;  // 0 = unlimited
    
    bool isRevoked;
    QString revocationReason;
    QDateTime revocationDate;
    
    QVariantMap customMetadata;
    
    QJsonObject toJson() const;
    static KeyMetadata fromJson(const QJsonObject& obj);
};

/**
 * @brief Key generation result
 */
struct KeyGenerationResult {
    bool success;
    QString errorMessage;
    
    KeyMetadata metadata;
    QByteArray publicKey;       // For sharing/verification
    QByteArray privateKeyHash;  // Hash only, never store actual private key
    
    // Entropy quality metrics
    double entropyBitsPerByte;
    int rdrandCycles;
    int rdseedCycles;
    bool hardwareEntropyVerified;
    
    // Timing
    qint64 generationTimeMs;
};

/**
 * @brief Key enrollment status
 */
struct EnrollmentStatus {
    bool isEnrolled;
    QString keyId;
    QString deviceId;
    QDateTime enrollmentDate;
    QString enrollmentServer;
    
    bool isPendingSync;
    QDateTime lastSyncAttempt;
    QString lastSyncError;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

using KeyGeneratedCallback = std::function<void(const KeyGenerationResult& result)>;
using EnrollmentCallback = std::function<void(const EnrollmentStatus& status)>;
using EntropyCallback = std::function<void(double entropyBits, int samplesCollected)>;

// ═══════════════════════════════════════════════════════════════════════════════
// Entropy Visualizer Widget
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class EntropyVisualizer
 * @brief Visual representation of entropy collection
 */
class EntropyVisualizer : public QWidget
{
    Q_OBJECT

public:
    explicit EntropyVisualizer(QWidget* parent = nullptr);
    ~EntropyVisualizer() override;

    void setEntropyLevel(double level);  // 0.0 - 1.0
    void addEntropySample(uint8_t sample);
    void reset();
    
    double getCurrentEntropy() const;
    int getSampleCount() const;

signals:
    void entropyReady();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<uint8_t> m_samples;
    double m_entropyLevel;
    int m_targetSamples;
    std::unique_ptr<QTimer> m_animationTimer;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Key Generation Wizard Pages
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class IntroductionPage
 * @brief Wizard introduction and overview
 */
class IntroductionPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit IntroductionPage(QWidget* parent = nullptr);
    
    void initializePage() override;
    bool isComplete() const override;

private:
    QLabel* m_welcomeLabel;
    QCheckBox* m_understandCheck;
    QLabel* m_securityNote;
};

/**
 * @class AlgorithmSelectionPage
 * @brief Select key algorithm and strength
 */
class AlgorithmSelectionPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit AlgorithmSelectionPage(QWidget* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    
    KeyAlgorithm getSelectedAlgorithm() const;
    KeyStrength getSelectedStrength() const;

private slots:
    void onAlgorithmChanged();
    void onStrengthChanged();
    void updateDescription();
    void checkHardwareCapabilities();

private:
    QRadioButton* m_rdrandAes;
    QRadioButton* m_rdrandChaCha;
    QRadioButton* m_rdseedAes;
    QRadioButton* m_hybridQuantum;
    
    QComboBox* m_strengthCombo;
    
    QLabel* m_descriptionLabel;
    QLabel* m_hardwareStatusLabel;
    QLabel* m_estimatedTimeLabel;
    
    bool m_hasRdrand;
    bool m_hasRdseed;
};

/**
 * @class KeyPurposePage
 * @brief Select key usage purposes
 */
class KeyPurposePage : public QWizardPage
{
    Q_OBJECT

public:
    explicit KeyPurposePage(QWidget* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    
    KeyPurposes getSelectedPurposes() const;

private:
    QCheckBox* m_systemAuthCheck;
    QCheckBox* m_thermalSigningCheck;
    QCheckBox* m_configEncryptionCheck;
    QCheckBox* m_ipcAuthCheck;
    QCheckBox* m_driveBindingCheck;
    
    QLabel* m_purposeDescription;
};

/**
 * @class KeyNamingPage
 * @brief Name and describe the key
 */
class KeyNamingPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit KeyNamingPage(QWidget* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;
    
    QString getKeyName() const;
    QString getKeyDescription() const;
    QDateTime getExpirationDate() const;

private slots:
    void onNameChanged(const QString& text);
    void updatePreview();

private:
    QLineEdit* m_nameEdit;
    QTextEdit* m_descriptionEdit;
    QComboBox* m_expirationCombo;
    QCheckBox* m_hardwareBindCheck;
    
    QLabel* m_previewLabel;
};

/**
 * @class EntropyCollectionPage
 * @brief Collect entropy and generate key
 */
class EntropyCollectionPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit EntropyCollectionPage(QWidget* parent = nullptr);
    
    void initializePage() override;
    bool isComplete() const override;
    
    KeyGenerationResult getResult() const;

public slots:
    void startGeneration();
    void cancelGeneration();

signals:
    void generationComplete(bool success);

private slots:
    void onEntropyTick();
    void onGenerationFinished();

private:
    void collectRdrandEntropy();
    void collectRdseedEntropy();
    void generateKey();
    double calculateEntropy(const std::vector<uint8_t>& data);
    
    EntropyVisualizer* m_visualizer;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QLabel* m_entropyLabel;
    QPushButton* m_startBtn;
    QPushButton* m_cancelBtn;
    
    std::unique_ptr<QTimer> m_entropyTimer;
    std::vector<uint8_t> m_entropyPool;
    KeyGenerationResult m_result;
    bool m_generating;
    bool m_complete;
};

/**
 * @class KeyVerificationPage
 * @brief Verify and backup generated key
 */
class KeyVerificationPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit KeyVerificationPage(QWidget* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;

private slots:
    void onExportKey();
    void onVerifyKey();
    void onCopyFingerprint();

private:
    QLabel* m_keyIdLabel;
    QLabel* m_fingerprintLabel;
    QTextEdit* m_publicKeyDisplay;
    
    QCheckBox* m_backedUpCheck;
    QCheckBox* m_verifiedCheck;
    
    QPushButton* m_exportBtn;
    QPushButton* m_verifyBtn;
    QPushButton* m_copyBtn;
};

/**
 * @class EnrollmentPage
 * @brief Enroll key with system
 */
class EnrollmentPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit EnrollmentPage(QWidget* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;

public slots:
    void startEnrollment();

signals:
    void enrollmentComplete(bool success);

private slots:
    void onEnrollmentFinished();

private:
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QTextEdit* m_logText;
    
    QCheckBox* m_autoRenewCheck;
    QCheckBox* m_syncToCloudCheck;
    
    bool m_enrolled;
    EnrollmentStatus m_status;
};

/**
 * @class CompletionPage
 * @brief Wizard completion summary
 */
class CompletionPage : public QWizardPage
{
    Q_OBJECT

public:
    explicit CompletionPage(QWidget* parent = nullptr);
    
    void initializePage() override;

private:
    QLabel* m_summaryLabel;
    QTextEdit* m_detailsText;
    QCheckBox* m_openManagerCheck;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Key Generation Wizard
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class KeyGenerationWizard
 * @brief Main wizard for quantum key generation
 */
class KeyGenerationWizard : public QWizard
{
    Q_OBJECT

public:
    enum PageId {
        Page_Introduction,
        Page_Algorithm,
        Page_Purpose,
        Page_Naming,
        Page_Entropy,
        Page_Verification,
        Page_Enrollment,
        Page_Completion
    };

    explicit KeyGenerationWizard(QWidget* parent = nullptr);
    ~KeyGenerationWizard() override;

    // Callbacks
    void setKeyGeneratedCallback(KeyGeneratedCallback callback);
    void setEnrollmentCallback(EnrollmentCallback callback);
    
    // Results
    KeyGenerationResult getResult() const;
    EnrollmentStatus getEnrollmentStatus() const;

signals:
    void keyGenerated(const KeyGenerationResult& result);
    void keyEnrolled(const EnrollmentStatus& status);

protected:
    void accept() override;

private:
    void setupPages();
    void setupConnections();
    
    IntroductionPage* m_introPage;
    AlgorithmSelectionPage* m_algorithmPage;
    KeyPurposePage* m_purposePage;
    KeyNamingPage* m_namingPage;
    EntropyCollectionPage* m_entropyPage;
    KeyVerificationPage* m_verificationPage;
    EnrollmentPage* m_enrollmentPage;
    CompletionPage* m_completionPage;
    
    KeyGeneratedCallback m_keyCallback;
    EnrollmentCallback m_enrollmentCallback;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Key Manager Dialog
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class KeyManagerDialog
 * @brief Manage existing keys
 */
class KeyManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeyManagerDialog(QWidget* parent = nullptr);
    ~KeyManagerDialog() override;

public slots:
    void refreshKeyList();
    void generateNewKey();
    void revokeSelectedKey();
    void exportSelectedKey();
    void viewKeyDetails();

signals:
    void keyRevoked(const QString& keyId);
    void keyExported(const QString& keyId, const QString& path);

private:
    void setupUI();
    void loadKeys();
    
    QListWidget* m_keyList;
    QLabel* m_detailsLabel;
    
    QPushButton* m_newKeyBtn;
    QPushButton* m_revokeBtn;
    QPushButton* m_exportBtn;
    QPushButton* m_detailsBtn;
    
    std::vector<KeyMetadata> m_keys;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Key Storage Backend
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class KeyStorage
 * @brief Secure key storage and retrieval
 */
class KeyStorage : public QObject
{
    Q_OBJECT

public:
    static KeyStorage& instance();
    
    // Storage operations
    bool storeKey(const KeyMetadata& metadata, const QByteArray& encryptedKey);
    std::optional<KeyMetadata> getKeyMetadata(const QString& keyId);
    QByteArray getEncryptedKey(const QString& keyId);
    bool deleteKey(const QString& keyId);
    
    // Queries
    std::vector<KeyMetadata> getAllKeys();
    std::vector<KeyMetadata> getKeysByPurpose(KeyPurpose purpose);
    std::optional<KeyMetadata> getActiveKeyForPurpose(KeyPurpose purpose);
    
    // Lifecycle
    bool revokeKey(const QString& keyId, const QString& reason);
    bool renewKey(const QString& keyId, const QDateTime& newExpiration);
    
    // Verification
    bool verifyKeyIntegrity(const QString& keyId);
    bool verifyHardwareBinding(const QString& keyId);

signals:
    void keyStored(const QString& keyId);
    void keyDeleted(const QString& keyId);
    void keyRevoked(const QString& keyId);

private:
    KeyStorage();
    ~KeyStorage() override;
    
    void loadFromDisk();
    void saveToDisk();
    QString getStoragePath() const;
    QByteArray encryptMetadata(const QByteArray& data);
    QByteArray decryptMetadata(const QByteArray& data);
    
    std::map<QString, KeyMetadata> m_keys;
    std::map<QString, QByteArray> m_encryptedKeys;
    QString m_storagePath;
    QByteArray m_masterKey;
};

} // namespace rawrxd::auth

Q_DECLARE_OPERATORS_FOR_FLAGS(rawrxd::auth::KeyPurposes)
