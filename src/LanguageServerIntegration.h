#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "lsp_client.h"

namespace RawrXD {
namespace IDE {

struct HoverInfo {
    bool isAvailable;
    int line;
    int column;
    std::string contents;
};

struct Location {
    std::string filePath;
    int line;
    int column;
    bool found;
};

struct PrepareRenameResult {
    bool canRename;
    std::string placeholder;
};

// Diagnostics structure that might differ from LSPClient's internal on
// reusing struct from existing headers if available.
// For now, using LSPClient's Diagnostic via inclusion or mapping.

class LanguageServerIntegration {
public:
    LanguageServerIntegration();
    
    // Lifecycle
    void initializeRoot(const std::string& rootPath);
    void openFile(const std::string& filePath, const std::string& languageISO);
    void closeFile(const std::string& filePath);
    void changeFile(const std::string& filePath, const std::string& content);

    // Capabilities
    HoverInfo provideHoverInfo(const std::string& filePath, int line, int column, const std::string& language, const std::string& codeContext);
    Location goToDefinition(const std::string& filePath, int line, int column, const std::string& language);
    std::vector<Location> findReferences(const std::string& filePath, int line, int column, const std::string& language);
    std::vector<RawrXD::Diagnostic> getDiagnostics(const std::string& filePath, const std::string& code, const std::string& language);
    PrepareRenameResult prepareRename(const std::string& filePath, int line, int column, const std::string& language);
    
    // Helper helpers
    std::string extractTokenAtPosition(const std::string& code, int line, int column);
    
private:
    bool m_isInitialized;
    int m_serverCapabilities;
    std::string m_rootPath;
    
    // Map language ID to client
    std::map<std::string, std::shared_ptr<LSPClient>> m_clients;
    
    std::shared_ptr<LSPClient> getClient(const std::string& language);
    
    // Fallback generators
    std::string generateCppHoverInfo(const std::string& token, const std::string& file);
    std::string generatePythonHoverInfo(const std::string& token, const std::string& file);
    std::string generateJsHoverInfo(const std::string& token, const std::string& file);
    
    std::vector<RawrXD::Diagnostic> checkSyntax(const std::string& code, const std::string& language);
    std::vector<RawrXD::Diagnostic> checkSemantics(const std::string& code, const std::string& language);
    std::vector<RawrXD::Diagnostic> checkStyle(const std::string& code, const std::string& language);
};

}
}
