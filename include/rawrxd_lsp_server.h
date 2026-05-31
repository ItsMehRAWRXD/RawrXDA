// =============================================================================
// include/rawrxd_lsp_server.h - Language Server Protocol Implementation
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <future>
#include <optional>
#include <mutex>

namespace RawrXD {
namespace LSP {

// =============================================================================
// LSP TYPES
// =============================================================================

struct Position {
    uint32_t line = 0;
    uint32_t character = 0;
};

struct Range {
    Position start;
    Position end;
};

struct Location {
    std::string uri;
    Range range;
};

struct TextEdit {
    Range range;
    std::string newText;
};

struct Diagnostic {
    Range range;
    uint32_t severity = 1;
    uint32_t code = 0;
    std::string source;
    std::string message;
    std::vector<Location> related;
};

struct CompletionItem {
    std::string label;
    uint32_t kind = 0;
    std::string detail;
    std::string documentation;
    std::string insertText;
    std::string data;
};

struct CompletionList {
    bool isIncomplete = false;
    std::vector<CompletionItem> items;
};

struct Hover {
    std::string contents;
    std::optional<Range> range;
};

struct SignatureHelp {
    struct SignatureInfo {
        std::string label;
        std::string documentation;
        std::vector<std::string> parameters;
    };
    std::vector<SignatureInfo> signatures;
    uint32_t activeSignature = 0;
    uint32_t activeParameter = 0;
};

struct DocumentSymbol {
    std::string name;
    std::string detail;
    uint32_t kind = 0;
    Range range;
    Range selectionRange;
    std::vector<DocumentSymbol> children;
};

struct CodeAction {
    std::string title;
    std::string kind;
    std::vector<Diagnostic> diagnostics;
    std::vector<TextEdit> edits;
};

struct WorkspaceEdit {
    std::unordered_map<std::string, std::vector<TextEdit>> changes;
};

// =============================================================================
// SERVER CAPABILITIES
// =============================================================================

struct ServerCapabilities {
    bool definitionProvider = true;
    bool referencesProvider = true;
    bool documentSymbolProvider = true;
    bool hoverProvider = true;
    bool completionProvider = true;
    bool signatureHelpProvider = true;
    bool documentFormattingProvider = true;
    bool renameProvider = true;
    bool codeActionProvider = true;
    std::vector<std::string> completionTriggers = {".", ">", ":", "("};
    std::vector<std::string> signatureTriggers = {"(", ","};
};

// =============================================================================
// LSP SERVER
// =============================================================================

class LSPServer {
public:
    LSPServer();
    ~LSPServer();
    
    // Lifecycle
    bool initialize(const std::string& rootPath);
    void shutdown();
    bool isInitialized() const;
    
    // Message handling
    std::string handleMessage(const std::string& message);
    
    // Document lifecycle
    void didOpen(const std::string& uri, const std::string& languageId, const std::string& text);
    void didChange(const std::string& uri, const std::vector<TextEdit>& changes);
    void didClose(const std::string& uri);
    void didSave(const std::string& uri);
    
    // Features
    std::vector<Location> gotoDefinition(const std::string& uri, Position pos);
    std::vector<Location> findReferences(const std::string& uri, Position pos);
    std::vector<DocumentSymbol> documentSymbols(const std::string& uri);
    std::optional<Hover> getHover(const std::string& uri, Position pos);
    CompletionList getCompletions(const std::string& uri, Position pos, const std::string& trigger);
    std::optional<SignatureHelp> getSignatureHelp(const std::string& uri, Position pos);
    std::vector<TextEdit> formatDocument(const std::string& uri);
    WorkspaceEdit rename(const std::string& uri, Position pos, const std::string& newName);
    std::vector<CodeAction> getCodeActions(const std::string& uri, Range range);
    
    // Diagnostics
    void setDiagnosticHandler(std::function<void(const std::string&, std::vector<Diagnostic>)> handler);
    void publishDiagnostics(const std::string& uri);
    
private:
    struct Document {
        std::string uri;
        std::string languageId;
        std::string text;
        int32_t version = 0;
        std::vector<std::string> lines;
    };
    
    bool initialized_ = false;
    std::string rootPath_;
    ServerCapabilities capabilities_;
    std::unordered_map<std::string, Document> documents_;
    std::mutex mutex_;
    std::function<void(const std::string&, std::vector<Diagnostic>)> diagHandler_;
    
    // Internal helpers
    std::string processRequest(const std::string& method, const std::string& params);
    std::string makeResponse(const std::string& id, const std::string& result);
    std::string makeError(const std::string& id, int code, const std::string& message);
    std::string serializeDiagnostics(const std::vector<Diagnostic>& diags);
    std::string serializeLocations(const std::vector<Location>& locs);
    std::string serializeSymbols(const std::vector<DocumentSymbol>& syms);
    std::string wordAtPosition(const Document& doc, Position pos);
    void reparse(const std::string& uri);
};

// =============================================================================
// LSP CLIENT (connect to other servers)
// =============================================================================

class LSPClient {
public:
    LSPClient();
    ~LSPClient();
    
    bool start(const std::string& command, const std::vector<std::string>& args);
    void stop();
    bool isRunning() const;
    
    std::future<std::string> request(const std::string& method, const std::string& params);
    void notify(const std::string& method, const std::string& params);
    
    void setHandler(std::function<void(const std::string&, const std::string&)> handler);
    
private:
    void* process_ = nullptr;
    void* stdin_ = nullptr;
    void* stdout_ = nullptr;
    bool running_ = false;
    uint32_t nextId_ = 1;
    std::mutex mutex_;
    std::function<void(const std::string&, const std::string&)> handler_;
    
    void send(const std::string& message);
    void readerThread();
};

} // namespace LSP
} // namespace RawrXD
