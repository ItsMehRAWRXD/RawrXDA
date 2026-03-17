#pragma once

#include <string>

/**
 * @brief Code signing utility for Windows/macOS executables
 * 
 * PRODUCTION-READY: Ensures executable authenticity and security
 * - Windows: signtool.exe with SHA256 certificate
 * - macOS: codesign with Developer ID certificate
 * - Timestamping for long-term validity
 */
class CodeSigner
{
public:
    static CodeSigner* instance();
    
    /**
     * @brief Sign Windows executable with Authenticode
     * @param exePath Path to executable
     * @param certPath Path to PFX certificate (or use cert store)
     * @param certPassword Certificate password (from env: CODE_SIGN_PASSWORD)
     * @return true if signing successful
     */
    bool signWindowsExecutable(const std::string& exePath, 
                              const std::string& certPath = "",
                              const std::string& certPassword = "");
    
    /**
     * @brief Sign macOS application bundle
     * @param bundlePath Path to .app bundle
     * @param identity Developer ID identity (from keychain)
     * @return true if signing successful
     */
    bool signMacOSBundle(const std::string& bundlePath, const std::string& identity = "");
    
    /**
     * @brief Verify executable signature
     * @param exePath Path to executable
     * @return true if signature is valid
     */
    bool verifySignature(const std::string& exePath);
    
private:
    CodeSigner();
    static CodeSigner* s_instance;
};
