#pragma once

#include <string>
#include <vector>
#include <memory>

#include "logging/logger.h"
#include "metrics/metrics.h"

struct Position {
    int line;
    int character;
};

struct Range {
    Position start;
    Position end;
};

struct Location {
    std::string uri;
    Range range;
};

struct SymbolInformation {
    std::string name;
    std::string kind;
    Location location;
    std::string detail;
};

struct HoverInfo {
    std::string contents;
    Range range;
    bool isMarkdown;
};

struct Diagnostic {
    Range range;
    std::string severity;
    std::string message;
    std::string source;
    std::vector<std::string> tags;
};

struct CompletionItem {
    std::string label;
    std::string kind;
    std::string detail;
    std::string documentation;
    std::string insertText;
};

class LanguageServerIntegrationImpl {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;

public:
    LanguageServerIntegrationImpl(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics
    );

    // Core LSP methods
    HoverInfo hover(const std::string& filePath, Position position);

    std::vector<Location> gotoDefinition(
        const std::string& filePath,
        Position position
    );

    std::vector<Location> findReferences(
        const std::string& filePath,
        Position position,
        bool includeDeclaration = true
    );

    std::vector<SymbolInformation> documentSymbols(const std::string& filePath);

    std::vector<CompletionItem> completion(
        const std::string& filePath,
        Position position,
        const std::string& triggerCharacter = ""
    );

    std::vector<Diagnostic> diagnostics(const std::string& filePath);

    // Advanced AI-powered features
    std::vector<Diagnostic> aiDiagnostics(
        const std::string& filePath,
        const std::string& code
    );

    std::string aiDocumentation(
        const std::string& symbol,
        const std::string& context
    );

    // Symbol indexing
    void indexDocument(const std::string& filePath, const std::string& content);
    void updateIndex(const std::string& filePath, const std::string& content);
    void removeFromIndex(const std::string& filePath);
};
