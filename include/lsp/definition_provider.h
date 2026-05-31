#pragma once
/**
 * @file definition_provider.h
 * @brief Go to definition and peek definition
 * Batch 3 - Item 41: Definition provider
 */

#include <string>
#include <vector>
#include <optional>

namespace RawrXD::LSP {

struct Location {
    std::string uri;
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
};

struct DefinitionLink {
    std::string originUri;
    uint32_t originStartLine;
    uint32_t originStartColumn;
    uint32_t originEndLine;
    uint32_t originEndColumn;
    std::string targetUri;
    uint32_t targetStartLine;
    uint32_t targetStartColumn;
    uint32_t targetEndLine;
    uint32_t targetEndColumn;
    std::optional<std::string> targetRange;
};

class DefinitionProvider {
public:
    DefinitionProvider();
    ~DefinitionProvider();

    // Definition
    std::vector<Location> provideDefinition(const std::string& uri,
                                              const std::string& content,
                                              uint32_t line,
                                              uint32_t column);

    // Declaration
    std::vector<Location> provideDeclaration(const std::string& uri,
                                               const std::string& content,
                                               uint32_t line,
                                               uint32_t column);

    // Type definition
    std::vector<Location> provideTypeDefinition(const std::string& uri,
                                                   const std::string& content,
                                                   uint32_t line,
                                                   uint32_t column);

    // Implementation
    std::vector<Location> provideImplementation(const std::string& uri,
                                                   const std::string& content,
                                                   uint32_t line,
                                                   uint32_t column);

    // Links
    std::vector<DefinitionLink> provideDefinitionLinks(const std::string& uri,
                                                         const std::string& content);

    // Indexing
    void indexDefinition(const std::string& symbol,
                         const Location& location);
    void indexDeclaration(const std::string& symbol,
                          const Location& location);
    void clearIndex();

    // C++ specific
    std::vector<Location> findCppDefinition(const std::string& uri,
                                              const std::string& content,
                                              const std::string& symbol);
    std::vector<Location> findCppDeclaration(const std::string& uri,
                                                const std::string& content,
                                                const std::string& symbol);

private:
    std::map<std::string, std::vector<Location>> m_definitions;
    std::map<std::string, std::vector<Location>> m_declarations;
    mutable std::mutex m_mutex;

    std::string getSymbolAtPosition(const std::string& content,
                                      uint32_t line,
                                      uint32_t column);
    std::vector<Location> searchInWorkspace(const std::string& symbol);
    bool isDefinition(const std::string& line, const std::string& symbol);
    bool isDeclaration(const std::string& line, const std::string& symbol);
};

// Global provider
DefinitionProvider& getDefinitionProvider();

} // namespace RawrXD::LSP
