#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace RawrXD {
namespace IDE {

// Language Server Protocol message types
enum class LSPMessageType {
    HOVER,
    DEFINITION,
    REFERENCES,
    COMPLETION,
    SIGNATURE,
    RENAME,
    FORMAT,
    DIAGNOSTICS,
};

// LSP hover information
struct HoverInfo {
    std::string contents;
    std::string language;
    std::vector<std::string> markedString;
};

// LSP location
struct Location {
    std::string uri;
    int line;
    int character;
};

// LSP diagnostic
struct Diagnostic {
    int line;
    int startChar;
    int endChar;
    std::string severity;  // "error", "warning", "information", "hint"
    std::string message;
    std::string source;
    int code;
};

// LSP text edit
struct TextEdit {
    int startLine;
    int startChar;
    int endLine;
    int endChar;
    std::string newText;
};

// Language server integration
class LanguageServerIntegration {
public:
    LanguageServerIntegration();
    ~LanguageServerIntegration() = default;

    // Initialize with language and workspace
    bool initialize(const std::string& language, const std::string& workspacePath);

    // Provide hover information
    HoverInfo provideHoverInfo(
        const std::string& filePath,
        int line,
        int character
    );

    // Go to definition
    Location goToDefinition(
        const std::string& filePath,
        int line,
        int character
    );

    // Find all references
    std::vector<Location> findReferences(
        const std::string& filePath,
        int line,
        int character,
        bool includeDeclaration = true
    );

    // Symbol rename
    std::vector<TextEdit> prepareRename(
        const std::string& filePath,
        int line,
        int character,
        const std::string& newName
    );

    // Format document
    std::vector<TextEdit> formatDocument(
        const std::string& filePath,
        const std::string& options = ""
    );

    // Format range
    std::vector<TextEdit> formatRange(
        const std::string& filePath,
        int startLine,
        int startChar,
        int endLine,
        int endChar
    );

    // Get diagnostics (errors/warnings)
    std::vector<Diagnostic> getDiagnostics(const std::string& filePath);

    // Get all diagnostics
    std::unordered_map<std::string, std::vector<Diagnostic>> getAllDiagnostics();

    // Code action (quick fixes)
    std::vector<std::string> getCodeActions(
        const std::string& filePath,
        int line
    );

    // Symbol information
    struct SymbolInfo {
        std::string name;
        std::string kind;
        Location location;
        std::vector<Location> children;
    };

    // Get document symbols
    std::vector<SymbolInfo> getDocumentSymbols(const std::string& filePath);

    // Get workspace symbols
    std::vector<SymbolInfo> getWorkspaceSymbols(const std::string& query);

    // Call hierarchy
    struct CallHierarchyItem {
        std::string name;
        Location location;
        std::vector<CallHierarchyItem> callers;
        std::vector<CallHierarchyItem> callees;
    };

    CallHierarchyItem getCallHierarchy(
        const std::string& filePath,
        int line,
        int character
    );

    // Type information
    struct TypeInfo {
        std::string name;
        std::string kind;
        std::string documentation;
        std::vector<std::pair<std::string, std::string>> members;  // name, type
    };

    TypeInfo getTypeInfo(
        const std::string& filePath,
        int line,
        int character
    );

private:
    // Language-specific handlers
    HoverInfo handleCppHover(
        const std::string& filePath,
        int line,
        int character
    );

    HoverInfo handlePythonHover(
        const std::string& filePath,
        int line,
        int character
    );

    // Caching
    struct CachedInfo {
        std::string content;
        std::chrono::system_clock::time_point timestamp;
    };

    std::string m_language;
    std::string m_workspacePath;
    std::unordered_map<std::string, CachedInfo> m_cache;
};

} // namespace IDE
} // namespace RawrXD
