#pragma once
#include <string>
#include <vector>
#include <map>
#include "agentic/lsp/LSPClient.hpp"

namespace rawrxd::lsp {

using namespace RawrXD::Agentic;

struct Location {
    std::string uri;
    struct {
        uint32_t line;
        uint32_t character;
    } start, end;
};

struct WorkspaceEdit {
    std::map<std::string, std::vector<struct TextEdit>> changes;
};

struct TextEdit {
    struct Range {
        Location start, end;
    } range;
    std::string newText;
};

// Type aliases for LSP protocol types from base class
using Diagnostic = RawrXD::Agentic::LSPDiagnostic;
using CompletionItem = RawrXD::Agentic::LSPCompletionItem;

class LSPClientWired : public RawrXD::Agentic::LSPClient {
public:
    static LSPClientWired& instance() {
        static LSPClientWired instance;
        return instance;
    }
    
    // Full initialization with project detection
    bool initializeForProject(const std::string& project_root);
    
    // Real-time diagnostics to UI
    void publishDiagnostics(const std::string& file_path,
                           std::vector<Diagnostic> diagnostics);
    
    // Hover info for editor
    std::string getHoverInfo(const std::string& file, 
                             uint32_t line, 
                             uint32_t character);
    
    // Go-to-definition
    Location getDefinition(const std::string& file,
                          uint32_t line,
                          uint32_t character);
    
    // Auto-complete (ghost-text support)
    std::vector<CompletionItem> getCompletions(const std::string& file,
                                                uint32_t line,
                                                uint32_t character,
                                                const std::string& prefix);
    
    // Symbol rename (refactor tool wiring)
    WorkspaceEdit renameSymbol(const std::string& file,
                              uint32_t line,
                              uint32_t character,
                              const std::string& new_name);
};

} // namespace rawrxd::lsp
