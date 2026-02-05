#pragma once

#include <QObject>
#include <QString>

/**
 * @brief Code signing utility for Windows/macOS executables
 * 
 * PRODUCTION-READY: Ensures executable authenticity and security
 * - Windows: signtool.exe with SHA256 certificate
 * - macOS: codesign with Developer ID certificate
 * - Timestamping for long-term validity
 */
class CodeSigner : public QObject
{
    Q_OBJECT

public:
    static CodeSigner* instance();
    
    /**
     * @brief Sign Windows executable with Authenticode
     * @param exePath Path to executable
     * @param certPath Path to PFX certificate (or use cert store)
     * @param certPassword Certificate password (from env: CODE_SIGN_PASSWORD)
     * @return true if signing successful
     */
    bool signWindowsExecutable(const QString& exePath, 
                              const QString& certPath = QString(),
                              const QString& certPassword = QString());
    
    /**
     * @brief Sign macOS application bundle
     * @param bundlePath Path to .app bundle
     * @param identity Developer ID identity (from keychain)
     * @return true if signing successful
     */
    bool signMacOSBundle(const QString& bundlePath, const QString& identity = QString());
    
    /**
     * @brief Verify executable signature
     * @param exePath Path to executable
     * @return true if signature is valid
     */
    bool verifySignature(const QString& exePath);
    
    /**
     * @brief Notarize macOS application (required for distribution)
     * @param bundlePath Path to .app bundle
     * @param appleId Apple ID email
     * @param password App-specific password (from env: NOTARIZE_PASSWORD)
     * @return true if notarization successful
     */
    bool notarizeMacOSApp(const QString& bundlePath, 
                         const QString& appleId,
                         const QString& password = QString());

signals:
    void signatureCompleted(const QString& filePath, bool success);
    void notarizationCompleted(const QString& bundlePath, bool success);

private:
    explicit CodeSigner(QObject* parent = nullptr);
    ~CodeSigner();
    
    static CodeSigner* s_instance;
    
    bool executeCommand(const QString& command, const QStringList& args);
};
