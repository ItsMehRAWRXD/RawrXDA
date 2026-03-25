<<<<<<< HEAD
/**
 * \file lsp_client.h
 * \brief Language Server Protocol client — Qt-free C++20/Win32
 * \author RawrXD Team
 * \date 2025-12-07
 *
 * Pure STL/Win32 implementation. Use lsp_client_unified.cpp for full LSP.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

namespace RawrXD {

/**
 * \brief LSP server configuration
 */
struct LSPServerConfig {
    std::string language;              // "cpp", "python", "javascript", etc.
    std::string command;               // "clangd", "pylsp", etc.
    std::vector<std::string> arguments;
    std::string workspaceRoot;
    bool autoStart = true;
};

/**
 * \brief Completion item from LSP server
 */
struct CompletionItem {
    std::string label;
    std::string insertText;
    std::string detail;
    std::string documentation;
    int kind = 1;
    std::string sortText;
    std::string filterText;
    int score = 0;
};

/**
 * \brief LSP diagnostic message
 */
struct Diagnostic {
    int line = 0;
    int column = 0;
    int severity = 1;  // 1=Error, 2=Warning, 3=Info, 4=Hint
    std::string message;
    std::string source;
};

/**
 * \brief LSP client base — Qt-free, uses std types
 */
class LSPClient {
public:
    explicit LSPClient(const LSPServerConfig& config) : m_config(config) {}
    virtual ~LSPClient() = default;

    virtual void initialize() {}
    virtual bool startServer() { return false; }
    virtual void stopServer() {}
    virtual bool isRunning() const { return m_serverRunning; }

    virtual void openDocument(const std::string&, const std::string&, const std::string&) {}
    virtual void closeDocument(const std::string&) {}
    virtual void updateDocument(const std::string&, const std::string&, int) {}

    virtual void requestCompletions(const std::string&, int, int) {}
    virtual void requestHover(const std::string&, int, int) {}
    virtual void requestDefinition(const std::string&, int, int) {}
    virtual void formatDocument(const std::string&) {}
    virtual std::vector<Diagnostic> getDiagnostics(const std::string&) const { return {}; }

    // Default in lsp_client_default.cpp; full impl in lsp_client_incremental.cpp (Myers diff)
    virtual void sendIncrementalUpdate(const std::string& uri, int64_t version,
                                       const std::string& oldContent,
                                       const std::string& newContent);
    virtual void cancelRequest(const std::string& id);

    struct Position { int line; int character; };
    Position offsetToPosition(const std::string& text, int offset) {
        int line = 0, col = 0;
        for (int i = 0; i < offset && i < static_cast<int>(text.size()); ++i) {
            if (text[static_cast<size_t>(i)] == '\n') { ++line; col = 0; }
            else { ++col; }
        }
        return {line, col};
    }

protected:
    virtual void sendNotification(const std::string& method, const std::string& paramsJson) { (void)method; (void)paramsJson; }

    LSPServerConfig m_config;
    bool m_serverRunning = false;
    bool m_initialized = false;
    int m_nextRequestId = 1;
    std::map<std::string, int> m_documentVersions;
    std::map<std::string, std::vector<Diagnostic>> m_diagnostics;
    std::map<std::string, bool> m_pendingCancellations;
};

} // namespace RawrXD
=======
/**
 * \file lsp_client.h
 * \brief Language Server Protocol client — Qt-free C++20/Win32
 * \author RawrXD Team
 * \date 2025-12-07
 *
 * Pure STL/Win32 implementation. Use lsp_client_unified.cpp for full LSP.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

namespace RawrXD {

/**
 * \brief LSP server configuration
 */
struct LSPServerConfig {
    std::string language;              // "cpp", "python", "javascript", etc.
    std::string command;               // "clangd", "pylsp", etc.
    std::vector<std::string> arguments;
    std::string workspaceRoot;
    bool autoStart = true;
};

/**
 * \brief Completion item from LSP server
 */
struct CompletionItem {
    std::string label;
    std::string insertText;
    std::string detail;
    std::string documentation;
    int kind = 1;
    std::string sortText;
    std::string filterText;
    int score = 0;
};

/**
 * \brief LSP diagnostic message
 */
struct Diagnostic {
    int line = 0;
    int column = 0;
    int severity = 1;  // 1=Error, 2=Warning, 3=Info, 4=Hint
    std::string message;
    std::string source;
};

/**
 * \brief LSP client base — Qt-free, uses std types
 */
class LSPClient {
public:
    explicit LSPClient(const LSPServerConfig& config) : m_config(config) {}
    virtual ~LSPClient() = default;

    virtual void initialize() {}
    virtual bool startServer() { return false; }
    virtual void stopServer() {}
    virtual bool isRunning() const { return m_serverRunning; }

    virtual void openDocument(const std::string&, const std::string&, const std::string&) {}
    virtual void closeDocument(const std::string&) {}
    virtual void updateDocument(const std::string&, const std::string&, int) {}

    virtual void requestCompletions(const std::string&, int, int) {}
    virtual void requestHover(const std::string&, int, int) {}
    virtual void requestDefinition(const std::string&, int, int) {}
    virtual void formatDocument(const std::string&) {}
    virtual std::vector<Diagnostic> getDiagnostics(const std::string&) const { return {}; }

    // Default in lsp_client_default.cpp; full impl in lsp_client_incremental.cpp (Myers diff)
    virtual void sendIncrementalUpdate(const std::string& uri, int64_t version,
                                       const std::string& oldContent,
                                       const std::string& newContent);
    virtual void cancelRequest(const std::string& id);

    struct Position { int line; int character; };
    Position offsetToPosition(const std::string& text, int offset) {
        int line = 0, col = 0;
        for (int i = 0; i < offset && i < static_cast<int>(text.size()); ++i) {
            if (text[static_cast<size_t>(i)] == '\n') { ++line; col = 0; }
            else { ++col; }
        }
        return {line, col};
    }

protected:
    virtual void sendNotification(const std::string& method, const std::string& paramsJson) { (void)method; (void)paramsJson; }

    LSPServerConfig m_config;
    bool m_serverRunning = false;
    bool m_initialized = false;
    int m_nextRequestId = 1;
    std::map<std::string, int> m_documentVersions;
    std::map<std::string, std::vector<Diagnostic>> m_diagnostics;
    std::map<std::string, bool> m_pendingCancellations;
};

} // namespace RawrXD
>>>>>>> origin/main
