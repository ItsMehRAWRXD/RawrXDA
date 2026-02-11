#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <functional>

/**
 * @brief Code signing utility for Windows/macOS executables
 */
class CodeSigner {
public:
    static CodeSigner* instance();
    bool signWindowsExecutable(const std::string& exePath,
                              const std::string& certPath = "",
                              const std::string& certPassword = "");
    bool signMacOSBundle(const std::string& bundlePath, const std::string& identity = "");
    bool verifySignature(const std::string& exePath);
    bool notarizeMacOSApp(const std::string& bundlePath,
                         const std::string& appleId,
                         const std::string& password = "");

    // Callbacks (replace Qt signals)
    std::function<void(const std::string&, bool)> onSignatureCompleted;
    std::function<void(const std::string&, bool)> onNotarizationCompleted;

private:
    CodeSigner() = default;
    ~CodeSigner() = default;
    static CodeSigner* s_instance;
    bool executeCommand(const std::string& command, const std::vector<std::string>& args);
};
