/**
 * @file vscode_api_bridge.cpp
 * @brief VS Code Extension API Bridge Implementation
 */

#include "vscode_api_bridge.h"
#include <algorithm>
#include <fstream>

namespace RawrXD::Extensions
{

// ============================================================================
// VSCodeAPIBridge Implementation
// ============================================================================

VSCodeAPIBridge::VSCodeAPIBridge() = default;
VSCodeAPIBridge::~VSCodeAPIBridge()
{
    shutdown();
}

bool VSCodeAPIBridge::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized)
        return true;

    // Register built-in commands
    CommandHandler logCmd = [](const nlohmann::json& args) -> nlohmann::json {
        if (args.is_array() && !args.empty()) {
            std::ofstream log(".rawrxd_log.txt", std::ios::app);
            if (log.is_open()) {
                if (args.size() > 0) {
                    log << args.at(0).dump() << "\n";
                }
            }
        }
        return {{"success", true}};
    };
    m_commands.insert_or_assign(std::string("rawrxd.log"), logCmd);

    CommandHandler echoCmd = [](const nlohmann::json& args) -> nlohmann::json {
        if (args.is_object() && args.contains("message")) {
            return {{"message", args.at("message")}, {"success", true}};
        }
        return {{"success", false}, {"error", "message field required"}};
    };
    m_commands.insert_or_assign(std::string("rawrxd.echo"), echoCmd);

    m_initialized = true;
    return true;
}

void VSCodeAPIBridge::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = false;
    m_commands.clear();
    m_completionProviders.clear();
    m_diagnosticProviders.clear();
    m_diagnostics.clear();
    m_openDocuments.clear();
}

// --- workspace ---

std::vector<VSCodeUri> VSCodeAPIBridge::getWorkspaceFolders() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_workspaceFolders;
}

VSCodeTextDocument VSCodeAPIBridge::openTextDocument(const VSCodeUri& uri)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_openDocuments.find(uri.toString());
    if (it != m_openDocuments.end())
        return it->second;

    VSCodeTextDocument doc;
    doc.uri = uri;
    doc.version = 1;
    // Detect languageId from extension
    std::string ext;
    const size_t dot = uri.path.rfind('.');
    if (dot != std::string::npos)
        ext = uri.path.substr(dot);
    if (ext == ".cpp" || ext == ".hpp" || ext == ".h")
        doc.languageId = "cpp";
    else if (ext == ".c")
        doc.languageId = "c";
    else if (ext == ".js")
        doc.languageId = "javascript";
    else if (ext == ".ts")
        doc.languageId = "typescript";
    else if (ext == ".py")
        doc.languageId = "python";
    else if (ext == ".json")
        doc.languageId = "json";
    else if (ext == ".md")
        doc.languageId = "markdown";
    else
        doc.languageId = "plaintext";

    // Read file content if exists
    try
    {
        std::ifstream f(uri.path, std::ios::binary);
        if (f.is_open())
            doc.content = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    catch (...)
    {
    }

    m_openDocuments[uri.toString()] = doc;
    return doc;
}

bool VSCodeAPIBridge::saveTextDocument(const VSCodeTextDocument& doc)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    try
    {
        std::ofstream f(doc.uri.path, std::ios::binary);
        if (!f.is_open())
            return false;
        f.write(doc.content.data(), static_cast<std::streamsize>(doc.content.size()));
        m_openDocuments[doc.uri.toString()] = doc;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

nlohmann::json VSCodeAPIBridge::getConfiguration(const std::string& section) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json cfg = {{"editor", {{"tabSize", 4}, {"insertSpaces", true}}},
                          {"files", {{"autoSave", "afterDelay"}}},
                          {"rawrxd", {{"inferenceBackend", "cpu"}, {"maxTokens", 2048}}}};
    if (section.empty())
        return cfg;
    if (cfg.contains(section))
        return cfg[section];
    return nlohmann::json::object();
}

void VSCodeAPIBridge::onDidChangeWorkspaceFolders(std::function<void(const nlohmann::json&)> cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onWorkspaceChange = cb;
}

// --- window ---

VSCodeTextDocument VSCodeAPIBridge::getActiveTextEditor() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeEditor;
}

void VSCodeAPIBridge::showInformationMessage(const std::string& msg)
{
    (void)msg;
    // In production, route to Win32IDE message box or status bar
}

void VSCodeAPIBridge::showWarningMessage(const std::string& msg)
{
    (void)msg;
}

void VSCodeAPIBridge::showErrorMessage(const std::string& msg)
{
    (void)msg;
}

void VSCodeAPIBridge::onDidChangeActiveTextEditor(std::function<void(const VSCodeTextDocument&)> cb)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_onActiveEditorChange = cb;
}

// --- commands ---

bool VSCodeAPIBridge::registerCommand(const std::string& id, const CommandHandler& handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized)
        return false;
    m_commands[id] = handler;
    return true;
}

bool VSCodeAPIBridge::unregisterCommand(const std::string& id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_commands.erase(id) > 0;
}

nlohmann::json VSCodeAPIBridge::executeCommand(const std::string& id, const nlohmann::json& args)
{
    // 1. Try built-in commands first.
    CommandHandler builtinHandler;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_commands.find(id);
        if (it != m_commands.end())
            builtinHandler = it->second;
    }
    if (builtinHandler)
    {
        try { return builtinHandler(args); }
        catch (const std::exception& e)
        {
            nlohmann::json err = nlohmann::json::object();
            err["error"] = std::string(e.what());
            err["code"] = -32603;
            return err;
        }
    }

    // 2. Fall through to extension-registered commands.
    return invokeExtensionCommand(id, args);
}

bool VSCodeAPIBridge::registerExtensionCommand(const std::string& id, int64_t ext_id,
                                                const CommandHandler& handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    ExtCommandEntry entry;
    entry.ext_id = ext_id;
    entry.handler = handler;
    m_extCommands.insert_or_assign(id, entry);
    return true;
}

bool VSCodeAPIBridge::unregisterExtensionCommands(int64_t ext_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_extCommands.begin(); it != m_extCommands.end(); )
    {
        if (it->second.ext_id == ext_id)
            it = m_extCommands.erase(it);
        else
            ++it;
    }
    return true;
}

nlohmann::json VSCodeAPIBridge::invokeExtensionCommand(const std::string& id, const nlohmann::json& args)
{
    CommandHandler extHandler;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_extCommands.find(id);
        if (it == m_extCommands.end())
        {
            nlohmann::json err = nlohmann::json::object();
            err["error"] = "Command not found: " + id;
            err["code"] = -32601;
            return err;
        }
        extHandler = it->second.handler;
    }
    try { return extHandler(args); }
    catch (const std::exception& e)
    {
        nlohmann::json err = nlohmann::json::object();
        err["error"] = std::string(e.what());
        err["code"] = -32603;
        return err;
    }
}

// --- languages ---

bool VSCodeAPIBridge::registerCompletionItemProvider(const std::string& language, const CompletionProvider& provider)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_completionProviders[language] = provider;
    return true;
}

bool VSCodeAPIBridge::registerDiagnosticProvider(const std::string& language, const DiagnosticProvider& provider)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diagnosticProviders[language] = provider;
    return true;
}

std::vector<VSCodeCompletionItem> VSCodeAPIBridge::provideCompletions(const VSCodeTextDocument& doc,
                                                                      const VSCodeRange& pos)
{
    CompletionProvider provider;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_completionProviders.find(doc.languageId);
        if (it == m_completionProviders.end())
        {
            return {};
        }
        provider = it->second;
    }

    try
    {
        return provider(doc, pos);
    }
    catch (...)
    {
    }
    return {};
}

std::vector<VSCodeDiagnostic> VSCodeAPIBridge::provideDiagnostics(const VSCodeTextDocument& doc)
{
    DiagnosticProvider provider;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_diagnosticProviders.find(doc.languageId);
        if (it == m_diagnosticProviders.end())
        {
            return {};
        }
        provider = it->second;
    }

    try
    {
        return provider(doc);
    }
    catch (...)
    {
    }
    return {};
}

// --- diagnostics ---

void VSCodeAPIBridge::setDiagnostics(const VSCodeUri& uri, const std::vector<VSCodeDiagnostic>& diags)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diagnostics[uri.toString()] = diags;
}

void VSCodeAPIBridge::clearDiagnostics(const VSCodeUri& uri)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diagnostics.erase(uri.toString());
}

std::vector<VSCodeDiagnostic> VSCodeAPIBridge::getDiagnostics(const VSCodeUri& uri) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_diagnostics.find(uri.toString());
    if (it != m_diagnostics.end())
        return it->second;
    return {};
}

// --- JSON-RPC ---

std::string VSCodeAPIBridge::handleJsonRpc(const std::string& request)
{
    auto req = JsonRpcMessage::parse(request);
    JsonRpcMessage resp;
    resp.id = req.id;
    resp.jsonrpc = "2.0";

    try
    {
        // Accept both dotted and slash-separated method names.
        std::string method = req.method;
        if (method.find('/') == std::string::npos && method.find('.') != std::string::npos)
        {
            std::replace(method.begin(), method.end(), '.', '/');
        }

        if (method == "workspace/getWorkspaceFolders")
        {
            nlohmann::json arr = nlohmann::json::array();
            const auto folders = getWorkspaceFolders();
            for (const auto& u : folders)
                arr.push_back(u.toString());
            resp.result = arr;
        }
        else if (method == "workspace/openTextDocument")
        {
            VSCodeUri uri = VSCodeUri::file(req.params.value("path", ""));
            auto doc = openTextDocument(uri);
            resp.result = doc.toJson();
        }
        else if (method == "workspace/saveTextDocument")
        {
            VSCodeTextDocument doc;
            doc.uri = VSCodeUri::file(req.params.value("path", ""));
            doc.content = req.params.value("content", "");
            resp.result = saveTextDocument(doc);
        }
        else if (method == "workspace/getConfiguration")
        {
            resp.result = getConfiguration(req.params.value("section", ""));
        }
        else if (method == "window/getActiveTextEditor")
        {
            resp.result = getActiveTextEditor().toJson();
        }
        else if (method == "window/showInformationMessage")
        {
            showInformationMessage(req.params.value("message", ""));
            resp.result = true;
        }
        else if (method == "commands/register")
        {
            // Extension-side command registration over RPC.
            // The extension provides a command id; the bridge stores a forwarding
            // handler that echoes back the registered result (real handler lives
            // in-process; external extensions use process broker IPC on top).
            const std::string cmd = req.params.value("command", "");
            const int64_t ext_id = req.params.value("ext_id", int64_t{-1});
            if (cmd.empty())
            {
                resp.error = {{"code", -32602}, {"message", "command name required"}};
            }
            else
            {
                // The test (and in-process extensions) supply a handler via
                // registerExtensionCommand() directly; RPC registration marks the
                // slot so remote callers can query membership.  If no prior handler
                // was registered, install a stub that signals "handler pending".
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (m_extCommands.find(cmd) == m_extCommands.end())
                    {
                        ExtCommandEntry stub;
                        stub.ext_id = ext_id;
                        stub.handler = [cmd](const nlohmann::json&) -> nlohmann::json {
                            return {{"error", "extension handler not yet attached: " + cmd}};
                        };
                        m_extCommands.insert_or_assign(cmd, stub);
                    }
                }
                resp.result = {{"status", "ok"}, {"command", cmd}};
            }
        }
        else if (method == "commands/executeCommand")
        {
            const std::string cmd = req.params.value("command", "");
            nlohmann::json args = nlohmann::json::object();
            if (req.params.contains("args"))
                args = req.params.at("args");
            resp.result = executeCommand(cmd, args);
        }
        else if (method == "languages/provideCompletions")
        {
            VSCodeTextDocument doc;
            doc.languageId = req.params.value("languageId", "plaintext");
            VSCodeRange range;
            auto j = req.params.value("position", nlohmann::json::object());
            range.startLine = j.value("line", 0);
            range.startChar = j.value("character", 0);
            auto items = provideCompletions(doc, range);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& i : items)
                arr.push_back(i.toJson());
            resp.result = arr;
        }
        else if (method == "languages/provideDiagnostics")
        {
            VSCodeTextDocument doc;
            doc.languageId = req.params.value("languageId", "plaintext");
            auto items = provideDiagnostics(doc);
            nlohmann::json arr = nlohmann::json::array();
            for (const auto& i : items)
                arr.push_back(i.toJson());
            resp.result = arr;
        }
        else if (method == "diagnostics/set")
        {
            VSCodeUri uri = VSCodeUri::file(req.params.value("path", ""));
            std::vector<VSCodeDiagnostic> diags;
            if (req.params.contains("diagnostics"))
            {
                for (const auto& d : req.params["diagnostics"])
                {
                    VSCodeDiagnostic vd;
                    vd.message = d.value("message", "");
                    vd.severity = d.value("severity", 1);
                    diags.push_back(vd);
                }
            }
            setDiagnostics(uri, diags);
            resp.result = true;
        }
        else
        {
            resp.error = {{"code", -32601}, {"message", "Method not found"}};
        }
    }
    catch (const std::exception& e)
    {
        resp.error = {{"code", -32603}, {"message", e.what()}};
    }

    return resp.serialize();
}

void VSCodeAPIBridge::sendEvent(const std::string& event, const nlohmann::json& payload)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    JsonRpcMessage msg;
    msg.method = event;
    msg.params = payload;
    // In production, push to event queue for broadcast to extensions
    (void)msg;
}

bool VSCodeAPIBridge::saveCommandRegistry()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    try
    {
        nlohmann::json registry = nlohmann::json::array();
        for (const auto& [cmd_id, entry] : m_extCommands)
        {
            nlohmann::json item = nlohmann::json::object();
            item["command"] = cmd_id;
            item["ext_id"] = entry.ext_id;
            registry.push_back(item);
        }
        std::ofstream file(m_registryPath);
        if (!file.is_open()) return false;
        file << registry.dump(2);
        return true;
    }
    catch (...) { return false; }
}

bool VSCodeAPIBridge::loadCommandRegistry()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    try
    {
        std::ifstream file(m_registryPath);
        if (!file.is_open()) return true;
        nlohmann::json reg = nlohmann::json::parse(file);
        if (!reg.is_array()) return false;
        for (const auto& item : reg)
        {
            if (!item.is_object() || !item.contains("command")) continue;
            std::string cmd = item["command"];
            int64_t ext_id = item.value("ext_id", int64_t{-1});
            ExtCommandEntry e;
            e.ext_id = ext_id;
            e.handler = [cmd](const nlohmann::json&) { nlohmann::json err; err["error"] = "ext not attached: " + cmd; return err; };
            m_extCommands.insert_or_assign(cmd, e);
        }
        return true;
    }
    catch (...) { return false; }
}

}  // namespace RawrXD::Extensions
