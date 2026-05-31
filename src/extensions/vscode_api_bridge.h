/**
 * @file vscode_api_bridge.h
 * @brief VS Code Extension API Bridge
 *
 * Implements core VS Code extension APIs:
 *   workspace: file operations, configuration, events
 *   window: active editor, panels, messages
 *   commands: registerCommand, executeCommand
 *   languages: registerCompletionItemProvider, diagnostics
 *   diagnostics: collection, reporting
 *
 * Maps VS Code APIs to RawrXD internal APIs via JSON-RPC.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>

namespace RawrXD::Extensions {

// ============================================================================
// VS Code API Types
// ============================================================================

struct VSCodeUri {
    std::string scheme = "file";
    std::string path;
    std::string toString() const { return scheme + "://" + path; }
    static VSCodeUri file(const std::string& p) { VSCodeUri u; u.path = p; return u; }
};

struct VSCodeRange {
    int startLine = 0, startChar = 0;
    int endLine = 0, endChar = 0;
    nlohmann::json toJson() const {
        return {{"start",{{"line",startLine},{"character",startChar}}},
                {"end",{{"line",endLine},{"character",endChar}}}};
    }
};

struct VSCodeDiagnostic {
    VSCodeRange range;
    std::string message;
    int severity = 1; // 1=Error, 2=Warning, 3=Info, 4=Hint
    std::string source;
    nlohmann::json toJson() const {
        return {{"range",range.toJson()},{"message",message},
                {"severity",severity},{"source",source}};
    }
};

struct VSCodeCompletionItem {
    std::string label;
    std::string detail;
    std::string insertText;
    int kind = 1; // Text=1, Method=2, Function=3, etc.
    nlohmann::json toJson() const {
        return {{"label",label},{"detail",detail},
                {"insertText",insertText},{"kind",kind}};
    }
};

struct VSCodeTextDocument {
    VSCodeUri uri;
    std::string languageId;
    int version = 0;
    std::string content;
    nlohmann::json toJson() const {
        return {{"uri",uri.toString()},{"languageId",languageId},
                {"version",version}};
    }
};

// ============================================================================
// JSON-RPC Message
// ============================================================================

struct JsonRpcMessage {
    std::string jsonrpc = "2.0";
    std::string id;
    std::string method;
    nlohmann::json params;
    nlohmann::json result;
    nlohmann::json error;

    nlohmann::json toJson() const {
        nlohmann::json j = {{"jsonrpc", jsonrpc}};
        if (!id.empty()) j["id"] = id;
        if (!method.empty()) j["method"] = method;
        if (!params.is_null()) j["params"] = params;
        if (!result.is_null()) j["result"] = result;
        if (!error.is_null()) j["error"] = error;
        return j;
    }
    std::string serialize() const { return toJson().dump(); }
    static JsonRpcMessage parse(const std::string& s) {
        JsonRpcMessage m;
        auto j = nlohmann::json::parse(s, nullptr, false);
        if (j.is_discarded() || !j.is_object()) return m;
        if (j.contains("jsonrpc") && j["jsonrpc"].is_string()) {
            m.jsonrpc = j["jsonrpc"].get<std::string>();
        }
        if (j.contains("id")) {
            const auto& id = j["id"];
            if (id.is_string()) {
                m.id = id.get<std::string>();
            } else if (id.is_number_integer()) {
                m.id = std::to_string(id.get<long long>());
            } else if (id.is_number_float()) {
                m.id = std::to_string(id.get<double>());
            }
        }
        if (j.contains("method") && j["method"].is_string()) {
            m.method = j["method"].get<std::string>();
        }
        if (j.contains("params")) m.params = j["params"];
        if (j.contains("result")) m.result = j["result"];
        if (j.contains("error")) m.error = j["error"];
        return m;
    }
};

// ============================================================================
// Handler Types
// ============================================================================

using CommandHandler = std::function<nlohmann::json(const nlohmann::json&)>;
using CompletionProvider = std::function<std::vector<VSCodeCompletionItem>(
    const VSCodeTextDocument&, const VSCodeRange&)>;
using DiagnosticProvider = std::function<std::vector<VSCodeDiagnostic>(
    const VSCodeTextDocument&)>;

// ============================================================================
// VSCodeAPIBridge
// ============================================================================

class VSCodeAPIBridge {
public:
    VSCodeAPIBridge();
    ~VSCodeAPIBridge();

    bool initialize();
    void shutdown();

    // --- workspace ---
    std::vector<VSCodeUri> getWorkspaceFolders() const;
    VSCodeTextDocument openTextDocument(const VSCodeUri& uri);
    bool saveTextDocument(const VSCodeTextDocument& doc);
    nlohmann::json getConfiguration(const std::string& section) const;
    void onDidChangeWorkspaceFolders(std::function<void(const nlohmann::json&)> cb);

    // --- window ---
    VSCodeTextDocument getActiveTextEditor() const;
    void showInformationMessage(const std::string& msg);
    void showWarningMessage(const std::string& msg);
    void showErrorMessage(const std::string& msg);
    void onDidChangeActiveTextEditor(std::function<void(const VSCodeTextDocument&)> cb);

    // --- commands ---
    bool registerCommand(const std::string& id, const CommandHandler& handler);
    bool unregisterCommand(const std::string& id);
    nlohmann::json executeCommand(const std::string& id, const nlohmann::json& args);

    // --- extension command registry ---
    // Register a command owned by an extension identified by ext_id.
    // When executeCommand can't find a built-in, it falls through to these.
    bool registerExtensionCommand(const std::string& id, int64_t ext_id,
                                   const CommandHandler& handler);
    bool unregisterExtensionCommands(int64_t ext_id);
    nlohmann::json invokeExtensionCommand(const std::string& id, const nlohmann::json& args);

    // --- persistence ---
    // Save/restore registered commands to disk for lifecycle preservation.
    bool saveCommandRegistry();
    bool loadCommandRegistry();

    // --- languages ---
    bool registerCompletionItemProvider(const std::string& language,
                                        const CompletionProvider& provider);
    bool registerDiagnosticProvider(const std::string& language,
                                    const DiagnosticProvider& provider);
    std::vector<VSCodeCompletionItem> provideCompletions(
        const VSCodeTextDocument& doc, const VSCodeRange& pos);
    std::vector<VSCodeDiagnostic> provideDiagnostics(
        const VSCodeTextDocument& doc);

    // --- diagnostics ---
    void setDiagnostics(const VSCodeUri& uri,
                        const std::vector<VSCodeDiagnostic>& diags);
    void clearDiagnostics(const VSCodeUri& uri);
    std::vector<VSCodeDiagnostic> getDiagnostics(const VSCodeUri& uri) const;

    // --- JSON-RPC ---
    std::string handleJsonRpc(const std::string& request);
    void sendEvent(const std::string& event, const nlohmann::json& payload);

private:
    mutable std::mutex m_mutex;
    bool m_initialized = false;

    // workspace
    std::vector<VSCodeUri> m_workspaceFolders;
    std::map<std::string, VSCodeTextDocument> m_openDocuments;
    std::function<void(const nlohmann::json&)> m_onWorkspaceChange;

    // window
    VSCodeTextDocument m_activeEditor;
    std::function<void(const VSCodeTextDocument&)> m_onActiveEditorChange;

    // commands (built-in, registered by the host at initialize)
    std::map<std::string, CommandHandler> m_commands;

    // extension command registry: cmd_id -> {ext_id, handler}
    struct ExtCommandEntry {
        int64_t ext_id;
        CommandHandler handler;
    };
    std::map<std::string, ExtCommandEntry> m_extCommands;

    // persistence registry: cmd_id -> ext_id (for disk storage)
    struct PersistentEntry {
        std::string cmd_id;
        int64_t ext_id;
    };
    std::vector<PersistentEntry> m_persistedCommands;
    std::string m_registryPath = ".rawrxd_commands.json";

    // languages
    std::map<std::string, CompletionProvider> m_completionProviders;
    std::map<std::string, DiagnosticProvider> m_diagnosticProviders;

    // diagnostics
    std::map<std::string, std::vector<VSCodeDiagnostic>> m_diagnostics;
};

} // namespace RawrXD::Extensions
