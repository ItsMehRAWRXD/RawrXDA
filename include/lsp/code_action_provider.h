#pragma once
/**
 * @file code_action_provider.h
 * @brief Quick fixes, refactorings, and source code transformations
 * Batch 3 - Item 37: Code action provider
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace RawrXD::LSP {

enum class CodeActionKind {
    QuickFix,
    Refactor,
    RefactorExtract,
    RefactorInline,
    RefactorRewrite,
    Source,
    SourceOrganizeImports,
    SourceFixAll
};

struct TextEdit {
    std::string uri;
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
    std::string newText;
};

struct WorkspaceEdit {
    std::vector<TextEdit> changes;
    std::vector<std::string> documentChanges;
};

struct CodeAction {
    std::string title;
    CodeActionKind kind;
    std::optional<std::string> diagnostics;
    bool isPreferred;
    WorkspaceEdit edit;
    std::optional<std::string> command;
    std::optional<std::string> data;
};

struct Diagnostic {
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
    std::string severity;
    std::string code;
    std::string source;
    std::string message;
};

struct CodeActionContext {
    std::vector<Diagnostic> diagnostics;
    std::vector<CodeActionKind> only;
};

class CodeActionProvider {
public:
    CodeActionProvider();
    ~CodeActionProvider();

    // Registration
    void registerQuickFix(const std::string& diagnosticCode,
                          std::function<CodeAction(const Diagnostic&)> provider);
    void registerRefactoring(CodeActionKind kind,
                           const std::string& name,
                           std::function<CodeAction(const std::string& uri,
                                                      uint32_t startLine,
                                                      uint32_t startColumn,
                                                      uint32_t endLine,
                                                      uint32_t endColumn)> provider);
    void registerSourceAction(CodeActionKind kind,
                              const std::string& name,
                              std::function<CodeAction(const std::string& uri)> provider);

    // Query
    std::vector<CodeAction> provideCodeActions(const std::string& uri,
                                                  uint32_t startLine,
                                                  uint32_t startColumn,
                                                  uint32_t endLine,
                                                  uint32_t endColumn,
                                                  const CodeActionContext& context);

    // Execution
    bool executeCodeAction(const CodeAction& action);
    bool resolveCodeAction(CodeAction& action);

    // Built-in actions
    std::vector<CodeAction> getBuiltInActions(const std::string& uri,
                                               uint32_t line,
                                               uint32_t column);

    // Refactorings
    CodeAction extractMethod(const std::string& uri,
                            uint32_t startLine,
                            uint32_t startColumn,
                            uint32_t endLine,
                            uint32_t endColumn);
    CodeAction extractVariable(const std::string& uri,
                                uint32_t startLine,
                                uint32_t startColumn,
                                uint32_t endLine,
                                uint32_t endColumn);
    CodeAction inlineVariable(const std::string& uri,
                             uint32_t line,
                             uint32_t column);
    CodeAction renameSymbol(const std::string& uri,
                           uint32_t line,
                           uint32_t column,
                           const std::string& newName);

    // Source actions
    CodeAction organizeImports(const std::string& uri);
    CodeAction removeUnusedImports(const std::string& uri);
    CodeAction addMissingIncludes(const std::string& uri);
    CodeAction sortMembers(const std::string& uri);

    // Quick fixes
    CodeAction fixMissingSemicolon(const std::string& uri, uint32_t line);
    CodeAction fixMissingInclude(const std::string& uri,
                                uint32_t line,
                                const std::string& missingSymbol);
    CodeAction fixUnusedVariable(const std::string& uri,
                                uint32_t line,
                                uint32_t column);
    CodeAction fixShadowedVariable(const std::string& uri,
                                  uint32_t line,
                                  uint32_t column);

private:
    struct QuickFixProvider {
        std::string diagnosticCode;
        std::function<CodeAction(const Diagnostic&)> provider;
    };

    struct RefactoringProvider {
        CodeActionKind kind;
        std::string name;
        std::function<CodeAction(const std::string&, uint32_t, uint32_t, uint32_t, uint32_t)> provider;
    };

    struct SourceActionProvider {
        CodeActionKind kind;
        std::string name;
        std::function<CodeAction(const std::string&)> provider;
    };

    std::vector<QuickFixProvider> m_quickFixes;
    std::vector<RefactoringProvider> m_refactorings;
    std::vector<SourceActionProvider> m_sourceActions;
    mutable std::mutex m_mutex;

    bool isRangeValid(uint32_t startLine, uint32_t startColumn, uint32_t endLine, uint32_t endColumn);
    std::string readFile(const std::string& uri);
    bool writeFile(const std::string& uri, const std::string& content);
};

// Global provider
CodeActionProvider& getCodeActionProvider();

// Utility
std::string codeActionKindToString(CodeActionKind kind);
CodeActionKind stringToCodeActionKind(const std::string& str);

} // namespace RawrXD::LSP
