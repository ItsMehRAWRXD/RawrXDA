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

#include <memory>
#include <vector>
#include <functional>
#include <optional>

// Forward declarations












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
    std::string keyId;
    std::string keyName;
    KeyAlgorithm algorithm;
    KeyStrength strength;
    KeyPurposes purposes;
    
    // DateTime created;
    // DateTime expires;
    // DateTime lastUsed;
    
    std::string hardwareFingerprint;
    std::string systemFingerprint;
    bool isBoundToHardware;
    
    int usageCount;
    int maxUsages;  // 0 = unlimited
    
    bool isRevoked;
    std::string revocationReason;
    // DateTime revocationDate;
    
    std::anyMap customMetadata;
    
    void* toJson() const;
    static KeyMetadata fromJson(const void*& obj);
};

/**
 * @brief Key generation result
 */
struct KeyGenerationResult {
    bool success;
    std::string errorMessage;
    
    KeyMetadata metadata;
    std::vector<uint8_t> publicKey;       // For sharing/verification
    std::vector<uint8_t> privateKeyHash;  // Hash only, never store actual private key
    
    // Entropy quality metrics
    double entropyBitsPerByte;
    int rdrandCycles;
    int rdseedCycles;
    bool hardwareEntropyVerified;
    
    // Timing
    int64_t generationTimeMs;
};

/**
 * @brief Key enrollment status
 */
struct EnrollmentStatus {
    bool isEnrolled;
    std::string keyId;
    std::string deviceId;
    // DateTime enrollmentDate;
    std::string enrollmentServer;
    
    bool isPendingSync;
    // DateTime lastSyncAttempt;
    std::string lastSyncError;
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
class EntropyVisualizer
{public:
    explicit EntropyVisualizer(void* parent = nullptr);
    ~EntropyVisualizer() override;

    void setEntropyLevel(double level);  // 0.0 - 1.0
    void addEntropySample(uint8_t sample);
    void reset();
    
    double getCurrentEntropy() const;
    int getSampleCount() const;
\npublic:\n    void entropyReady();

protected:
    void paintEvent(void* event) override;

private:
    std::vector<uint8_t> m_samples;
    double m_entropyLevel;
    int m_targetSamples;
    std::unique_ptr<void> m_animationTimer;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Key Generation Wizard Pages
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class IntroductionPage
 * @brief Wizard introduction and overview
 */
class IntroductionPage
{public:
    explicit IntroductionPage(void* parent = nullptr);
    
    void initializePage() override;
    bool isComplete() const override;

private:
    void* m_welcomeLabel;
    void* m_understandCheck;
    void* m_securityNote;
};

/**
 * @class AlgorithmSelectionPage
 * @brief Select key algorithm and strength
 */
class AlgorithmSelectionPage
{public:
    explicit AlgorithmSelectionPage(void* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    
    KeyAlgorithm getSelectedAlgorithm() const;
    KeyStrength getSelectedStrength() const;
\nprivate:\n    void onAlgorithmChanged();
    void onStrengthChanged();
    void updateDescription();
    void checkHardwareCapabilities();

private:
    void* m_rdrandAes;
    void* m_rdrandChaCha;
    void* m_rdseedAes;
    void* m_hybridQuantum;
    
    void* m_strengthCombo;
    
    void* m_descriptionLabel;
    void* m_hardwareStatusLabel;
    void* m_estimatedTimeLabel;
    
    bool m_hasRdrand;
    bool m_hasRdseed;
};

/**
 * @class KeyPurposePage
 * @brief Select key usage purposes
 */
class KeyPurposePage
{public:
    explicit KeyPurposePage(void* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    
    KeyPurposes getSelectedPurposes() const;

private:
    void* m_systemAuthCheck;
    void* m_thermalSigningCheck;
    void* m_configEncryptionCheck;
    void* m_ipcAuthCheck;
    void* m_driveBindingCheck;
    
    void* m_purposeDescription;
};

/**
 * @class KeyNamingPage
 * @brief Name and describe the key
 */
class KeyNamingPage
{public:
    explicit KeyNamingPage(void* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;
    
    std::string getKeyName() const;
    std::string getKeyDescription() const;
    // DateTime getExpirationDate() const;
\nprivate:\n    void onNameChanged(const std::string& text);
    void updatePreview();

private:
    voidEdit* m_nameEdit;
    void* m_descriptionEdit;
    void* m_expirationCombo;
    void* m_hardwareBindCheck;
    
    void* m_previewLabel;
};

/**
 * @class EntropyCollectionPage
 * @brief Collect entropy and generate key
 */
class EntropyCollectionPage
{public:
    explicit EntropyCollectionPage(void* parent = nullptr);
    
    void initializePage() override;
    bool isComplete() const override;
    
    KeyGenerationResult getResult() const;
\npublic:\n    void startGeneration();
    void cancelGeneration();
\npublic:\n    void generationComplete(bool success);
\nprivate:\n    void onEntropyTick();
    void onGenerationFinished();

private:
    void collectRdrandEntropy();
    void collectRdseedEntropy();
    void generateKey();
    double calculateEntropy(const std::vector<uint8_t>& data);
    
    EntropyVisualizer* m_visualizer;
    void* m_progressBar;
    void* m_statusLabel;
    void* m_entropyLabel;
    void* m_startBtn;
    void* m_cancelBtn;
    
    std::unique_ptr<void> m_entropyTimer;
    std::vector<uint8_t> m_entropyPool;
    KeyGenerationResult m_result;
    bool m_generating;
    bool m_complete;
};

/**
 * @class KeyVerificationPage
 * @brief Verify and backup generated key
 */
class KeyVerificationPage
{public:
    explicit KeyVerificationPage(void* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;
\nprivate:\n    void onExportKey();
    void onVerifyKey();
    void onCopyFingerprint();

private:
    void* m_keyIdLabel;
    void* m_fingerprintLabel;
    void* m_publicKeyDisplay;
    
    void* m_backedUpCheck;
    void* m_verifiedCheck;
    
    void* m_exportBtn;
    void* m_verifyBtn;
    void* m_copyBtn;
};

/**
 * @class EnrollmentPage
 * @brief Enroll key with system
 */
class EnrollmentPage
{public:
    explicit EnrollmentPage(void* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    bool isComplete() const override;
\npublic:\n    void startEnrollment();
\npublic:\n    void enrollmentComplete(bool success);
\nprivate:\n    void onEnrollmentFinished();

private:
    void* m_statusLabel;
    void* m_progressBar;
    void* m_logText;
    
    void* m_autoRenewCheck;
    void* m_syncToCloudCheck;
    
    bool m_enrolled;
    EnrollmentStatus m_status;
};

/**
 * @class CompletionPage
 * @brief Wizard completion summary
 */
class CompletionPage
{public:
    explicit CompletionPage(void* parent = nullptr);
    
    void initializePage() override;

private:
    void* m_summaryLabel;
    void* m_detailsText;
    void* m_openManagerCheck;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Key Generation Wizard
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class KeyGenerationWizard
 * @brief Main wizard for quantum key generation
 */
class KeyGenerationWizard
{public:
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

    explicit KeyGenerationWizard(void* parent = nullptr);
    ~KeyGenerationWizard() override;

    // Callbacks
    void setKeyGeneratedCallback(KeyGeneratedCallback callback);
    void setEnrollmentCallback(EnrollmentCallback callback);
    
    // Results
    KeyGenerationResult getResult() const;
    EnrollmentStatus getEnrollmentStatus() const;
\npublic:\n    void keyGenerated(const KeyGenerationResult& result);
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
class KeyManagerDialog
{public:
    explicit KeyManagerDialog(void* parent = nullptr);
    ~KeyManagerDialog() override;
\npublic:\n    void refreshKeyList();
    void generateNewKey();
    void revokeSelectedKey();
    void exportSelectedKey();
    void viewKeyDetails();
\npublic:\n    void keyRevoked(const std::string& keyId);
    void keyExported(const std::string& keyId, const std::string& path);

private:
    void setupUI();
    void loadKeys();
    
    QListWidget* m_keyList;
    void* m_detailsLabel;
    
    void* m_newKeyBtn;
    void* m_revokeBtn;
    void* m_exportBtn;
    void* m_detailsBtn;
    
    std::vector<KeyMetadata> m_keys;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Key Storage Backend
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class KeyStorage
 * @brief Secure key storage and retrieval
 */
class KeyStorage 
{public:
    static KeyStorage& instance();
    
    // Storage operations
    bool storeKey(const KeyMetadata& metadata, const std::vector<uint8_t>& encryptedKey);
    std::optional<KeyMetadata> getKeyMetadata(const std::string& keyId);
    std::vector<uint8_t> getEncryptedKey(const std::string& keyId);
    bool deleteKey(const std::string& keyId);
    
    // Queries
    std::vector<KeyMetadata> getAllKeys();
    std::vector<KeyMetadata> getKeysByPurpose(KeyPurpose purpose);
    std::optional<KeyMetadata> getActiveKeyForPurpose(KeyPurpose purpose);
    
    // Lifecycle
    bool revokeKey(const std::string& keyId, const std::string& reason);
    bool renewKey(const std::string& keyId, const // DateTime& newExpiration);
    
    // Verification
    bool verifyKeyIntegrity(const std::string& keyId);
    bool verifyHardwareBinding(const std::string& keyId);
\npublic:\n    void keyStored(const std::string& keyId);
    void keyDeleted(const std::string& keyId);
    void keyRevoked(const std::string& keyId);

private:
    KeyStorage();
    ~KeyStorage() override;
    
    void loadFromDisk();
    void saveToDisk();
    std::string getStoragePath() const;
    std::vector<uint8_t> encryptMetadata(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decryptMetadata(const std::vector<uint8_t>& data);
    
    std::map<std::string, KeyMetadata> m_keys;
    std::map<std::string, std::vector<uint8_t>> m_encryptedKeys;
    std::string m_storagePath;
    std::vector<uint8_t> m_masterKey;
};

} // namespace rawrxd::auth

Q_DECLARE_OPERATORS_FOR_FLAGS(rawrxd::auth::KeyPurposes)







