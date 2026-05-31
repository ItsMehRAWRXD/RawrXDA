#include "lsp/code_action_provider.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace RawrXD::LSP {

CodeActionProvider::CodeActionProvider() = default;
CodeActionProvider::~CodeActionProvider() = default;

void CodeActionProvider::registerQuickFix(const std::string& diagnosticCode,
                                         std::function<CodeAction(const Diagnostic&)> provider) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_quickFixes.push_back({diagnosticCode, provider});
}

void CodeActionProvider::registerRefactoring(CodeActionKind kind,
                                              const std::string& name,
                                              std::function<CodeAction(const std::string& uri,
                                                                         uint32_t startLine,
                                                                         uint32_t startColumn,
                                                                         uint32_t endLine,
                                                                         uint32_t endColumn)> provider) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refactorings.push_back({kind, name, provider});
}

void CodeActionProvider::registerSourceAction(CodeActionKind kind,
                                               const std::string& name,
                                               std::function<CodeAction(const std::string& uri)> provider) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sourceActions.push_back({kind, name, provider});
}

std::vector<CodeAction> CodeActionProvider::provideCodeActions(const std::string& uri,
                                                                  uint32_t startLine,
                                                                  uint32_t startColumn,
                                                                  uint32_t endLine,
                                                                  uint32_t endColumn,
                                                                  const CodeActionContext& context) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CodeAction> actions;

    // Quick fixes from diagnostics
    for (const auto& diagnostic : context.diagnostics) {
        for (const auto& [code, provider] : m_quickFixes) {
            if (diagnostic.code == code) {
                actions.push_back(provider(diagnostic));
            }
        }
    }

    // Refactorings
    for (const auto& refactoring : m_refactorings) {
        if (context.only.empty() ||
            std::find(context.only.begin(), context.only.end(), refactoring.kind) != context.only.end()) {
            actions.push_back(refactoring.provider(uri, startLine, startColumn, endLine, endColumn));
        }
    }

    // Source actions
    for (const auto& sourceAction : m_sourceActions) {
        if (context.only.empty() ||
            std::find(context.only.begin(), context.only.end(), sourceAction.kind) != context.only.end()) {
            actions.push_back(sourceAction.provider(uri));
        }
    }

    // Built-in actions
    auto builtIn = getBuiltInActions(uri, startLine, startColumn);
    actions.insert(actions.end(), builtIn.begin(), builtIn.end());

    return actions;
}

bool CodeActionProvider::executeCodeAction(const CodeAction& action) {
    // Apply workspace edit
    for (const auto& edit : action.edit.changes) {
        std::string content = readFile(edit.uri);
        if (content.empty()) return false;

        std::vector<std::string> lines;
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            lines.push_back(line);
        }

        // Apply edit
        if (edit.startLine == edit.endLine) {
            // Single line edit
            if (edit.startLine < lines.size()) {
                std::string& targetLine = lines[edit.startLine];
                if (edit.endColumn <= targetLine.length()) {
                    targetLine = targetLine.substr(0, edit.startColumn) +
                                edit.newText +
                                targetLine.substr(edit.endColumn);
                }
            }
        } else {
            // Multi-line edit
            // TODO: Implement multi-line edits
        }

        // Write back
        std::string newContent;
        for (size_t i = 0; i < lines.size(); ++i) {
            newContent += lines[i];
            if (i < lines.size() - 1) newContent += "\n";
        }

        if (!writeFile(edit.uri, newContent)) {
            return false;
        }
    }

    return true;
}

bool CodeActionProvider::resolveCodeAction(CodeAction& action) {
    // Resolve any additional data needed for the action
    return true;
}

std::vector<CodeAction> CodeActionProvider::getBuiltInActions(const std::string& uri,
                                                                uint32_t line,
                                                                uint32_t column) {
    std::vector<CodeAction> actions;

    // Add missing semicolon
    actions.push_back(fixMissingSemicolon(uri, line));

    // Organize imports
    actions.push_back(organizeImports(uri));

    return actions;
}

CodeAction CodeActionProvider::extractMethod(const std::string& uri,
                                              uint32_t startLine,
                                              uint32_t startColumn,
                                              uint32_t endLine,
                                              uint32_t endColumn) {
    CodeAction action;
    action.title = "Extract method";
    action.kind = CodeActionKind::RefactorExtract;
    action.isPreferred = false;

    // Create edit
    TextEdit edit;
    edit.uri = uri;
    edit.startLine = startLine;
    edit.startColumn = startColumn;
    edit.endLine = endLine;
    edit.endColumn = endColumn;
    edit.newText = "extractedMethod()";

    action.edit.changes.push_back(edit);
    return action;
}

CodeAction CodeActionProvider::extractVariable(const std::string& uri,
                                                uint32_t startLine,
                                                uint32_t startColumn,
                                                uint32_t endLine,
                                                uint32_t endColumn) {
    CodeAction action;
    action.title = "Extract variable";
    action.kind = CodeActionKind::RefactorExtract;
    action.isPreferred = false;

    TextEdit edit;
    edit.uri = uri;
    edit.startLine = startLine;
    edit.startColumn = startColumn;
    edit.endLine = endLine;
    edit.endColumn = endColumn;
    edit.newText = "extractedVar";

    action.edit.changes.push_back(edit);
    return action;
}

CodeAction CodeActionProvider::inlineVariable(const std::string& uri,
                                             uint32_t line,
                                             uint32_t column) {
    CodeAction action;
    action.title = "Inline variable";
    action.kind = CodeActionKind::RefactorInline;
    action.isPreferred = false;
    return action;
}

CodeAction CodeActionProvider::renameSymbol(const std::string& uri,
                                             uint32_t line,
                                             uint32_t column,
                                             const std::string& newName) {
    CodeAction action;
    action.title = "Rename symbol";
    action.kind = CodeActionKind::RefactorRewrite;
    action.isPreferred = false;
    return action;
}

CodeAction CodeActionProvider::organizeImports(const std::string& uri) {
    CodeAction action;
    action.title = "Organize imports";
    action.kind = CodeActionKind::SourceOrganizeImports;
    action.isPreferred = true;
    return action;
}

CodeAction CodeActionProvider::removeUnusedImports(const std::string& uri) {
    CodeAction action;
    action.title = "Remove unused imports";
    action.kind = CodeActionKind::Source;
    action.isPreferred = false;
    return action;
}

CodeAction CodeActionProvider::addMissingIncludes(const std::string& uri) {
    CodeAction action;
    action.title = "Add missing includes";
    action.kind = CodeActionKind::QuickFix;
    action.isPreferred = true;
    return action;
}

CodeAction CodeActionProvider::sortMembers(const std::string& uri) {
    CodeAction action;
    action.title = "Sort members";
    action.kind = CodeActionKind::Source;
    action.isPreferred = false;
    return action;
}

CodeAction CodeActionProvider::fixMissingSemicolon(const std::string& uri, uint32_t line) {
    CodeAction action;
    action.title = "Insert missing semicolon";
    action.kind = CodeActionKind::QuickFix;
    action.isPreferred = true;

    TextEdit edit;
    edit.uri = uri;
    edit.startLine = line;
    edit.startColumn = 0;
    edit.endLine = line;
    edit.endColumn = 0;
    edit.newText = ";";

    action.edit.changes.push_back(edit);
    return action;
}

CodeAction CodeActionProvider::fixMissingInclude(const std::string& uri,
                                                uint32_t line,
                                                const std::string& missingSymbol) {
    CodeAction action;
    action.title = "Add missing include for " + missingSymbol;
    action.kind = CodeActionKind::QuickFix;
    action.isPreferred = true;
    return action;
}

CodeAction CodeActionProvider::fixUnusedVariable(const std::string& uri,
                                                uint32_t line,
                                                uint32_t column) {
    CodeAction action;
    action.title = "Remove unused variable";
    action.kind = CodeActionKind::QuickFix;
    action.isPreferred = true;
    return action;
}

CodeAction CodeActionProvider::fixShadowedVariable(const std::string& uri,
                                                  uint32_t line,
                                                  uint32_t column) {
    CodeAction action;
    action.title = "Rename shadowed variable";
    action.kind = CodeActionKind::QuickFix;
    action.isPreferred = false;
    return action;
}

std::string CodeActionProvider::readFile(const std::string& uri) {
    std::string path = uri;
    if (path.find("file://") == 0) {
        path = path.substr(7);
    }

    std::ifstream file(path);
    if (!file.is_open()) return "";

    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

bool CodeActionProvider::writeFile(const std::string& uri, const std::string& content) {
    std::string path = uri;
    if (path.find("file://") == 0) {
        path = path.substr(7);
    }

    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << content;
    return true;
}

// Global provider
CodeActionProvider& getCodeActionProvider() {
    static CodeActionProvider provider;
    return provider;
}

std::string codeActionKindToString(CodeActionKind kind) {
    switch (kind) {
        case CodeActionKind::QuickFix: return "quickfix";
        case CodeActionKind::Refactor: return "refactor";
        case CodeActionKind::RefactorExtract: return "refactor.extract";
        case CodeActionKind::RefactorInline: return "refactor.inline";
        case CodeActionKind::RefactorRewrite: return "refactor.rewrite";
        case CodeActionKind::Source: return "source";
        case CodeActionKind::SourceOrganizeImports: return "source.organizeImports";
        case CodeActionKind::SourceFixAll: return "source.fixAll";
        default: return "";
    }
}

} // namespace RawrXD::LSP
