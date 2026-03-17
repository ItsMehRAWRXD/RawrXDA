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
#include <string>
#include <map>
#include <unordered_map>
#include <cstdint>

#include <nlohmann/json.hpp>

namespace rawrxd::auth {

using json = nlohmann::json;

// Win32: void* parent in wizard/page/dialog classes is HWND (CreateWindowExW parent).
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

// Replaces Q_DECLARE_FLAGS(KeyPurposes, KeyPurpose)
using KeyPurposes = uint32_t;

inline constexpr KeyPurposes operator|(KeyPurpose a, KeyPurpose b) {
    return static_cast<KeyPurposes>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline constexpr KeyPurposes operator|(KeyPurposes a, KeyPurpose b) {
    return a | static_cast<uint32_t>(b);
}
inline constexpr KeyPurposes operator&(KeyPurposes a, KeyPurpose b) {
    return a & static_cast<uint32_t>(b);
}
inline constexpr bool hasFlag(KeyPurposes flags, KeyPurpose flag) {
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

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
 * @brief Generated key metadata (C++20 / no Qt)
 */
struct KeyMetadata {
    std::string keyId;
    std::string keyName;
    KeyAlgorithm algorithm;
    KeyStrength strength;
    KeyPurposes purposes;

    int64_t created = 0;
    int64_t expires = 0;
    int64_t lastUsed = 0;

    std::string hardwareFingerprint;
    std::string systemFingerprint;
    bool isBoundToHardware = false;

    int usageCount = 0;
    int maxUsages = 0;  // 0 = unlimited

    bool isRevoked = false;
    std::string revocationReason;
    int64_t revocationDate = 0;

    std::map<std::string, json> customMetadata;

    json toJson() const;
    static KeyMetadata fromJson(const json& obj);
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
// Quantum Auth Manager (headless logic manager)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class QuantumAuthManager
 * @brief Manages quantum-resistant key generation, storage, and enrollment
 */
class QuantumAuthManager
{
public:
    QuantumAuthManager();
    ~QuantumAuthManager();

    double measureEntropy();
    KeyGenerationResult generateKey(const std::string& name, KeyAlgorithm algo, KeyStrength strength);
    std::vector<KeyMetadata> listKeys() const;
    bool revokeKey(const std::string& keyId, const std::string& reason);
    std::optional<KeyMetadata> getKey(const std::string& keyId) const;

private:
    std::string m_keystorePath;
};

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
    ~EntropyVisualizer();

    void setEntropyLevel(double level);  // 0.0 - 1.0
    void addEntropySample(uint8_t sample);
    void reset();
    
    double getCurrentEntropy() const;
    int getSampleCount() const;

public:
    void entropyReady();

protected:
    void paintEvent(void* event);

private:
    std::vector<uint8_t> m_samples;
    double m_entropyLevel = 0.0;
    int m_targetSamples = 0;
    uintptr_t m_animationTimerId = 0;  // Win32 timer ID or 0
};

// ═══════════════════════════════════════════════════════════════════════════════
// Key Generation Wizard Pages
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @class IntroductionPage
 * @brief Wizard introduction and overview
 */
class IntroductionPage {
public:
    explicit IntroductionPage(void* parent = nullptr);

    virtual void initializePage() {}
    virtual bool isComplete() const { return true; }

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
    
    virtual void initializePage() {}
    virtual bool validatePage() { return true; }
    
    KeyAlgorithm getSelectedAlgorithm() const;
    KeyStrength getSelectedStrength() const;

private:
    void onAlgorithmChanged();
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
    
    virtual void initializePage() {}
    virtual bool validatePage() { return true; }
    
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
    
    virtual void initializePage() {}
    virtual bool validatePage() { return true; }
    virtual bool isComplete() const { return true; }
    
    std::string getKeyName() const;
    std::string getKeyDescription() const;
    // DateTime getExpirationDate() const;

private:
    void onNameChanged(const std::string& text);
    void updatePreview();

private:
    void* m_nameEdit;
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
    
    virtual void initializePage() {}
    virtual bool isComplete() const { return true; }
    
    KeyGenerationResult getResult() const;

public:
    void startGeneration();
    void cancelGeneration();

public:
    void generationComplete(bool success);

private:
    void onEntropyTick();
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
    
    uintptr_t m_entropyTimerId = 0;
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
    
    virtual void initializePage() {}
    virtual bool validatePage() { return true; }
    virtual bool isComplete() const { return true; }

private:
    void onExportKey();
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
    
    virtual void initializePage() {}
    virtual bool validatePage() { return true; }
    virtual bool isComplete() const { return true; }

public:
    void startEnrollment();

public:
    void enrollmentComplete(bool success);

private:
    void onEnrollmentFinished();

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
    
    virtual void initializePage() {}

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
    ~KeyGenerationWizard();

    // Callbacks
    void setKeyGeneratedCallback(KeyGeneratedCallback callback);
    void setEnrollmentCallback(EnrollmentCallback callback);
    
    // Results
    KeyGenerationResult getResult() const;
    EnrollmentStatus getEnrollmentStatus() const;

public:
    void keyGenerated(const KeyGenerationResult& result);
    void keyEnrolled(const EnrollmentStatus& status);

protected:
    void accept();

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
    ~KeyManagerDialog();

public:
    void refreshKeyList();
    void generateNewKey();
    void revokeSelectedKey();
    void exportSelectedKey();
    void viewKeyDetails();

public:
    void keyRevoked(const std::string& keyId);
    void keyExported(const std::string& keyId, const std::string& path);

private:
    void setupUI();
    void loadKeys();
    
    void* m_keyList;  // HWND list control (was QListWidget*)
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
    std::vector<KeyMetadata> getKeysByPurpose(KeyPurposes purposeMask);
    std::optional<KeyMetadata> getActiveKeyForPurpose(KeyPurpose purpose);
    
    // Lifecycle
    bool revokeKey(const std::string& keyId, const std::string& reason);
    bool renewKey(const std::string& keyId, int64_t newExpiration);
    
    // Verification
    bool verifyKeyIntegrity(const std::string& keyId);
    bool verifyHardwareBinding(const std::string& keyId);

    // Path accessor
    std::string getStoragePath() const;

public:
    void keyStored(const std::string& keyId);
    void keyDeleted(const std::string& keyId);
    void keyRevoked(const std::string& keyId);

private:
    KeyStorage();
    ~KeyStorage();
    
    void loadFromDisk();
    void saveToDisk();
    std::vector<uint8_t> encryptMetadata(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decryptMetadata(const std::vector<uint8_t>& data);
    
    std::map<std::string, KeyMetadata> m_keys;
    std::map<std::string, std::vector<uint8_t>> m_encryptedKeys;
    std::string m_storagePath;
    std::vector<uint8_t> m_masterKey;
    std::function<void(const std::string&)> m_callback;
};

} // namespace rawrxd::auth

// Q_DECLARE_OPERATORS_FOR_FLAGS removed — operators defined inline in namespace

