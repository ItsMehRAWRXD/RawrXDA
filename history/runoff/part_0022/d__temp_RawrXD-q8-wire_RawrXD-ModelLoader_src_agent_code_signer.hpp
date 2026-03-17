#pragma once

#include <string>
#include <vector>
#include <functional>

/**
 * @brief Code signing utility for Windows/macOS executables
 * 
 * PRODUCTION-READY: Ensures executable authenticity and security
 */
class CodeSigner
{
public:
    static CodeSigner* instance();
    
    /**
     * @brief Sign Windows executable with Authenticode
     */
    bool signWindowsExecutable(const std::string& exePath, 
                              const std::string& certPath = "",
                              const std::string& certPassword = "");
    
    /**
     * @brief Sign macOS application bundle
     */
    bool signMacOSBundle(const std::string& bundlePath, const std::string& identity = "");
    
    /**
     * @brief Verify executable signature
     */
    bool verifySignature(const std::string& exePath);
    
    // Callbacks
    using SignatureCompletedCallback = std::function<void(const std::string& filePath, bool success)>;
    using NotarizationCompletedCallback = std::function<void(const std::string& bundlePath, bool success)>;

    void setSignatureCompletedCallback(SignatureCompletedCallback cb) { m_onSignatureCompleted = cb; }
    void setNotarizationCompletedCallback(NotarizationCompletedCallback cb) { m_onNotarizationCompleted = cb; }

private:
    explicit CodeSigner();
    ~CodeSigner();
    
    static CodeSigner* s_instance;
    
    bool executeCommand(const std::string& command, const std::vector<std::string>& args);

    SignatureCompletedCallback m_onSignatureCompleted;
    NotarizationCompletedCallback m_onNotarizationCompleted;
};
