#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
<<<<<<< HEAD
#include <functional>
=======
>>>>>>> origin/main
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

<<<<<<< HEAD
struct TextEdit {
    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    std::string newText;
};

struct Diagnostic {
    int line = 0;
    int startChar = 0;
    int endChar = 0;
    std::string severity;
    std::string message;
    std::string source;
    int code = 0;
};

enum class ServerCapability {
    HoverProvider = 1,
    DefinitionProvider = 2,
    ReferencesProvider = 4,
    RenameProvider = 8,
    CompletionProvider = 16,
    DiagnosticProvider = 32,
    FormattingProvider = 64
};

=======
>>>>>>> origin/main
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
<<<<<<< HEAD
    std::vector<Diagnostic> getDiagnostics(const std::string& filePath, const std::string& code, const std::string& language);
    PrepareRenameResult prepareRename(const std::string& filePath, int line, int column, const std::string& language);
    
    // Additional LSP methods
    std::vector<TextEdit> rename(const std::string& filePath, int line, int column,
                                 const std::string& newName, const std::string& language);
    std::vector<std::string> getCompletionItems(const std::string& filePath, int line, int column,
                                               const std::string& language, const std::string& codeContext);
    std::vector<TextEdit> formatDocument(const std::string& code, const std::string& language);
    std::vector<TextEdit> formatRange(const std::string& code, int startLine, int startColumn,
                                     int endLine, int endColumn, const std::string& language);
    
    // Lifecycle
    bool initialize();
    bool shutdown();
    bool supportsLanguage(const std::string& language);
    void registerLanguageHandler(const std::string& language,
                                const std::function<std::string(const std::string&)>& handler);
    
    // Helper methods
=======
    std::vector<RawrXD::Diagnostic> getDiagnostics(const std::string& filePath, const std::string& code, const std::string& language);
    PrepareRenameResult prepareRename(const std::string& filePath, int line, int column, const std::string& language);
    
    // Helper helpers
>>>>>>> origin/main
    std::string extractTokenAtPosition(const std::string& code, int line, int column);
    
private:
    bool m_isInitialized;
    int m_serverCapabilities;
    std::string m_rootPath;
<<<<<<< HEAD
    std::map<std::string, std::function<std::string(const std::string&)>> m_languageHandlers;
=======
>>>>>>> origin/main
    
    // Map language ID to client
    std::map<std::string, std::shared_ptr<LSPClient>> m_clients;
    
    std::shared_ptr<LSPClient> getClient(const std::string& language);
    
    // Fallback generators
    std::string generateCppHoverInfo(const std::string& token, const std::string& file);
    std::string generatePythonHoverInfo(const std::string& token, const std::string& file);
    std::string generateJsHoverInfo(const std::string& token, const std::string& file);
    
<<<<<<< HEAD
    std::vector<Diagnostic> checkSyntax(const std::string& code, const std::string& language);
    std::vector<Diagnostic> checkSemantics(const std::string& code, const std::string& language);
    std::vector<Diagnostic> checkStyle(const std::string& code, const std::string& language);
    
    // Language-specific completions
    std::vector<std::string> getCppCompletions(const std::string& context);
    std::vector<std::string> getPythonCompletions(const std::string& context);
    std::vector<std::string> getJsCompletions(const std::string& context);
    
    // Language-specific formatting
    std::string formatCppCode(const std::string& code);
    std::string formatPythonCode(const std::string& code);
    std::string formatJsCode(const std::string& code);
    
    // Helper
    std::string extractCodeRange(const std::string& code, int startLine, int startColumn,
                                int endLine, int endColumn);
};

} // namespace IDE
} // namespace RawrXD
=======
    std::vector<RawrXD::Diagnostic> checkSyntax(const std::string& code, const std::string& language);
    std::vector<RawrXD::Diagnostic> checkSemantics(const std::string& code, const std::string& language);
    std::vector<RawrXD::Diagnostic> checkStyle(const std::string& code, const std::string& language);
};

}
}
>>>>>>> origin/main
