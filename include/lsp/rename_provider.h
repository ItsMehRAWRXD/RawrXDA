#pragma once
/**
 * @file rename_provider.h
 * @brief Symbol renaming across workspace
 * Batch 3 - Item 42: Rename provider
 */

#include <string>
#include <vector>
#include <map>
#include <optional>

namespace RawrXD::LSP {

struct TextEdit {
    std::string uri;
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
    std::string newText;
};

struct WorkspaceEdit {
    std::map<std::string, std::vector<TextEdit>> changes;
};

struct RenameOptions {
    bool prepareProvider = true;
    bool honorExcludes = true;
    bool honorGitignore = true;
};

class RenameProvider {
public:
    RenameProvider();
    ~RenameProvider();

    // Validation
    std::optional<std::string> validateRename(const std::string& uri,
                                                const std::string& content,
                                                uint32_t line,
                                                uint32_t column,
                                                const std::string& newName);

    // Prepare rename (for UI preview)
    std::optional<WorkspaceEdit> prepareRename(const std::string& uri,
                                                  const std::string& content,
                                                  uint32_t line,
                                                  uint32_t column);

    // Execute rename
    std::optional<WorkspaceEdit> provideRename(const std::string& uri,
                                                  const std::string& content,
                                                  uint32_t line,
                                                  uint32_t column,
                                                  const std::string& newName);

    // On type rename (linked editing)
    std::vector<std::pair<uint32_t, uint32_t>> getLinkedEditingRanges(const std::string& uri,
                                                                        const std::string& content,
                                                                        uint32_t line,
                                                                        uint32_t column);

    // Configuration
    void setOptions(const RenameOptions& options);
    RenameOptions getOptions() const;

    // C++ specific
    bool isValidCppIdentifier(const std::string& name);
    bool isReservedCppKeyword(const std::string& name);
    std::vector<TextEdit> findCppOccurrences(const std::string& uri,
                                               const std::string& content,
                                               const std::string& oldName);

private:
    RenameOptions m_options;
    mutable std::mutex m_mutex;

    std::string getSymbolAtPosition(const std::string& content,
                                    uint32_t line,
                                    uint32_t column);
    std::vector<TextEdit> findOccurrences(const std::string& uri,
                                           const std::string& content,
                                           const std::string& symbol);
    std::vector<TextEdit> findOccurrencesInWorkspace(const std::string& symbol);
    bool isSameSymbol(const std::string& candidate, const std::string& symbol);
    bool isScopeValid(const std::string& content,
                      uint32_t line,
                      const std::string& symbol);
};

// Global provider
RenameProvider& getRenameProvider();

} // namespace RawrXD::LSP
