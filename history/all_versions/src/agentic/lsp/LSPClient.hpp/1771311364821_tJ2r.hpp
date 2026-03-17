#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <sstream>
#include <fstream>
#include <chrono>

namespace RawrXD::Agentic {

// ============================================================================
// LSP Protocol Types
// ============================================================================
struct LSPPosition {
    int line = 0;
    int character = 0;
};

struct LSPRange {
    LSPPosition start;
    LSPPosition end;
};

struct LSPLocation {
    std::string uri;
    LSPRange range;
};

struct LSPDiagnostic {
    LSPRange range;
    int severity = 1;  // 1=Error, 2=Warning, 3=Info, 4=Hint
    std::string code;
    std::string source;
    std::string message;
};

struct LSPCompletionItem {
    std::string label;
    int kind = 1;          // 1=Text, 2=Method, 3=Function, 4=Constructor, etc.
    std::string detail;
    std::string documentation;
    std::string insertText;
    int insertTextFormat = 1; // 1=PlainText, 2=Snippet
    std::string sortText;
    std::string filterText;
};

struct LSPHover {
    std::string contents;
    LSPRange range;
};

struct LSPSignatureHelp {
    struct Parameter {
        std::string label;
        std::string documentation;
    };
    struct Signature {
        std::string label;
        std::string documentation;
        std::vector<Parameter> parameters;
    };
    std::vector<Signature> signatures;
    int activeSignature = 0;
    int activeParameter = 0;
};

struct LSPSymbolInfo {
    std::string name;
    int kind = 0;
    LSPRange range;
    LSPRange selectionRange;
    std::string containerName;
    std::vector<LSPSymbolInfo> children;
};

// ============================================================================
// Simple JSON builder/parser for LSP messages
// ============================================================================
class JsonValue {
public:
    enum Type { Null, Bool, Number, String, Array, Object };

private:
    Type type_ = Null;
    bool boolVal_ = false;
    double numVal_ = 0.0;
    std::string strVal_;
    std::vector<JsonValue> arrVal_;
    std::vector<std::pair<std::string, JsonValue>> objVal_;

public:
    JsonValue() : type_(Null) {}
    JsonValue(bool v) : type_(Bool), boolVal_(v) {}
    JsonValue(int v) : type_(Number), numVal_(v) {}
    JsonValue(double v) : type_(Number), numVal_(v) {}
    JsonValue(const std::string& v) : type_(String), strVal_(v) {}
    JsonValue(const char* v) : type_(String), strVal_(v) {}

    static JsonValue object() { JsonValue v; v.type_ = Object; return v; }
    static JsonValue array() { JsonValue v; v.type_ = Array; return v; }
    static JsonValue null() { return JsonValue(); }

    // Object operations
    JsonValue& operator[](const std::string& key) {
        if (type_ != Object) { type_ = Object; objVal_.clear(); }
        for (auto& kv : objVal_) {
            if (kv.first == key) return kv.second;
        }
        objVal_.push_back({key, JsonValue()});
        return objVal_.back().second;
    }

    const JsonValue& get(const std::string& key) const {
        static JsonValue nullVal;
        for (auto& kv : objVal_) {
            if (kv.first == key) return kv.second;
        }
        return nullVal;
    }

    bool has(const std::string& key) const {
        for (auto& kv : objVal_) {
            if (kv.first == key) return true;
        }
        return false;
    }

    // Array operations
    void push(const JsonValue& v) {
        if (type_ != Array) { type_ = Array; arrVal_.clear(); }
        arrVal_.push_back(v);
    }

    size_t size() const {
        if (type_ == Array) return arrVal_.size();
        if (type_ == Object) return objVal_.size();
        return 0;
    }

    const JsonValue& at(size_t i) const {
        static JsonValue nullVal;
        if (type_ == Array && i < arrVal_.size()) return arrVal_[i];
        return nullVal;
    }

    // Getters
    Type type() const { return type_; }
    bool isNull() const { return type_ == Null; }
    bool asBool() const { return boolVal_; }
    int asInt() const { return (int)numVal_; }
    double asDouble() const { return numVal_; }
    const std::string& asString() const { return strVal_; }
    const std::vector<JsonValue>& asArray() const { return arrVal_; }

    // Serialize to JSON string
    std::string dump() const {
        std::ostringstream oss;
        serialize(oss);
        return oss.str();
    }

    void serialize(std::ostringstream& oss) const {
        switch (type_) {
            case Null:   oss << "null"; break;
            case Bool:   oss << (boolVal_ ? "true" : "false"); break;
            case Number: {
                if (numVal_ == (int)numVal_) oss << (int)numVal_;
                else oss << numVal_;
                break;
            }
            case String: {
                oss << "\"";
                for (char c : strVal_) {
                    if (c == '"') oss << "\\\"";
                    else if (c == '\\') oss << "\\\\";
                    else if (c == '\n') oss << "\\n";
                    else if (c == '\r') oss << "\\r";
                    else if (c == '\t') oss << "\\t";
                    else oss << c;
                }
                oss << "\"";
                break;
            }
            case Array: {
                oss << "[";
                for (size_t i = 0; i < arrVal_.size(); ++i) {
                    if (i > 0) oss << ",";
                    arrVal_[i].serialize(oss);
                }
                oss << "]";
                break;
            }
            case Object: {
                oss << "{";
                for (size_t i = 0; i < objVal_.size(); ++i) {
                    if (i > 0) oss << ",";
                    oss << "\"" << objVal_[i].first << "\":";
                    objVal_[i].second.serialize(oss);
                }
                oss << "}";
                break;
            }
        }
    }

    // Parse from JSON string (minimal parser)
    static JsonValue parse(const std::string& json) {
        size_t pos = 0;
        return parseValue(json, pos);
    }

private:
    static void skipWhitespace(const std::string& s, size_t& pos) {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
            pos++;
    }

    static JsonValue parseValue(const std::string& s, size_t& pos) {
        skipWhitespace(s, pos);
        if (pos >= s.size()) return JsonValue();

        char c = s[pos];
        if (c == '"') return parseString(s, pos);
        if (c == '{') return parseObject(s, pos);
        if (c == '[') return parseArray(s, pos);
        if (c == 't' || c == 'f') return parseBool(s, pos);
        if (c == 'n') return parseNull(s, pos);
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(s, pos);
        return JsonValue();
    }

    static JsonValue parseString(const std::string& s, size_t& pos) {
        pos++; // skip opening quote
        std::string result;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                pos++;
                switch (s[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += s[pos]; break;
                }
            } else {
                result += s[pos];
            }
            pos++;
        }
        if (pos < s.size()) pos++; // skip closing quote
        return JsonValue(result);
    }

    static JsonValue parseNumber(const std::string& s, size_t& pos) {
        size_t start = pos;
        if (s[pos] == '-') pos++;
        while (pos < s.size() && (s[pos] >= '0' && s[pos] <= '9')) pos++;
        if (pos < s.size() && s[pos] == '.') {
            pos++;
            while (pos < s.size() && (s[pos] >= '0' && s[pos] <= '9')) pos++;
        }
        return JsonValue(std::stod(s.substr(start, pos - start)));
    }

    static JsonValue parseBool(const std::string& s, size_t& pos) {
        if (s.substr(pos, 4) == "true") { pos += 4; return JsonValue(true); }
        if (s.substr(pos, 5) == "false") { pos += 5; return JsonValue(false); }
        return JsonValue();
    }

    static JsonValue parseNull(const std::string& s, size_t& pos) {
        if (s.substr(pos, 4) == "null") { pos += 4; }
        return JsonValue();
    }

    static JsonValue parseObject(const std::string& s, size_t& pos) {
        pos++; // skip {
        JsonValue obj = JsonValue::object();
        skipWhitespace(s, pos);
        if (pos < s.size() && s[pos] == '}') { pos++; return obj; }

        while (pos < s.size()) {
            skipWhitespace(s, pos);
            if (s[pos] != '"') break;
            JsonValue key = parseString(s, pos);
            skipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ':') pos++;
            obj[key.asString()] = parseValue(s, pos);
            skipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') pos++;
            else break;
        }
        if (pos < s.size() && s[pos] == '}') pos++;
        return obj;
    }

    static JsonValue parseArray(const std::string& s, size_t& pos) {
        pos++; // skip [
        JsonValue arr = JsonValue::array();
        skipWhitespace(s, pos);
        if (pos < s.size() && s[pos] == ']') { pos++; return arr; }

        while (pos < s.size()) {
            arr.push(parseValue(s, pos));
            skipWhitespace(s, pos);
            if (pos < s.size() && s[pos] == ',') pos++;
            else break;
        }
        if (pos < s.size() && s[pos] == ']') pos++;
        return arr;
    }
};

// ============================================================================
// LSP Client - communicates with language servers via stdio
// ============================================================================
class LSPClient {
private:
    HANDLE hProcess_ = nullptr;
    HANDLE hStdinRead_ = nullptr;
    HANDLE hStdinWrite_ = nullptr;
    HANDLE hStdoutRead_ = nullptr;
    HANDLE hStdoutWrite_ = nullptr;

    std::unique_ptr<std::thread> readerThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};

    std::mutex sendMutex_;
    std::mutex responseMutex_;
    std::condition_variable responseCV_;

    int nextId_ = 1;
    std::unordered_map<int, std::function<void(const JsonValue&)>> pendingRequests_;
    std::queue<JsonValue> notifications_;

    std::string serverPath_;
    std::string workspacePath_;
    std::string languageId_;

    // Callbacks
    std::function<void(const std::string&, const std::vector<LSPDiagnostic>&)> onDiagnostics_;
    std::function<void(const std::string&)> onLog_;

public:
    LSPClient() = default;

    ~LSPClient() {
        shutdown();
    }

    // Start a language server process
    bool start(const std::string& serverPath, const std::string& workspacePath,
               const std::string& languageId = "cpp") {
        serverPath_ = serverPath;
        workspacePath_ = workspacePath;
        languageId_ = languageId;

        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

        // Create pipes for stdin/stdout
        if (!CreatePipe(&hStdinRead_, &hStdinWrite_, &sa, 0)) return false;
        if (!CreatePipe(&hStdoutRead_, &hStdoutWrite_, &sa, 0)) return false;

        SetHandleInformation(hStdinWrite_, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStdoutRead_, HANDLE_FLAG_INHERIT, 0);

        // Start the server process
        STARTUPINFOA si = { sizeof(STARTUPINFOA) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = hStdinRead_;
        si.hStdOutput = hStdoutWrite_;
        si.hStdError = hStdoutWrite_;

        PROCESS_INFORMATION pi;
        if (!CreateProcessA(
            nullptr,
            (LPSTR)serverPath.c_str(),
            nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi
        )) {
            log("Failed to start LSP server: " + serverPath);
            return false;
        }

        hProcess_ = pi.hProcess;
        CloseHandle(pi.hThread);
        running_ = true;

        // Start reader thread
        readerThread_ = std::make_unique<std::thread>([this]() { readerLoop(); });

        // Send initialize request
        return sendInitialize();
    }

    // Shutdown the server
    void shutdown() {
        if (!running_) return;

        // Send shutdown request
        auto shutdownReq = createRequest("shutdown", JsonValue::null());
        sendMessage(shutdownReq);

        // Send exit notification
        auto exitNotif = createNotification("exit", JsonValue::null());
        sendMessage(exitNotif);

        running_ = false;

        if (readerThread_ && readerThread_->joinable()) {
            readerThread_->join();
        }
        readerThread_.reset();

        if (hProcess_) {
            WaitForSingleObject(hProcess_, 3000);
            TerminateProcess(hProcess_, 0);
            CloseHandle(hProcess_);
            hProcess_ = nullptr;
        }

        closeHandles();
        initialized_ = false;
    }

    // ========================================================================
    // LSP Protocol methods
    // ========================================================================

    // Notify server that a document was opened
    void didOpen(const std::string& uri, const std::string& text, int version = 1) {
        if (!initialized_) return;

        auto params = JsonValue::object();
        auto textDoc = JsonValue::object();
        textDoc["uri"] = uri;
        textDoc["languageId"] = languageId_;
        textDoc["version"] = version;
        textDoc["text"] = text;
        params["textDocument"] = textDoc;

        sendNotification("textDocument/didOpen", params);
    }

    // Notify server that a document was changed
    void didChange(const std::string& uri, const std::string& text, int version) {
        if (!initialized_) return;

        auto params = JsonValue::object();
        auto textDoc = JsonValue::object();
        textDoc["uri"] = uri;
        textDoc["version"] = version;
        params["textDocument"] = textDoc;

        auto changes = JsonValue::array();
        auto change = JsonValue::object();
        change["text"] = text;
        changes.push(change);
        params["contentChanges"] = changes;

        sendNotification("textDocument/didChange", params);
    }

    // Notify server that a document was saved
    void didSave(const std::string& uri, const std::string& text = "") {
        if (!initialized_) return;

        auto params = JsonValue::object();
        auto textDoc = JsonValue::object();
        textDoc["uri"] = uri;
        params["textDocument"] = textDoc;

        if (!text.empty()) {
            params["text"] = text;
        }

        sendNotification("textDocument/didSave", params);
    }

    // Notify server that a document was closed
    void didClose(const std::string& uri) {
        if (!initialized_) return;

        auto params = JsonValue::object();
        auto textDoc = JsonValue::object();
        textDoc["uri"] = uri;
        params["textDocument"] = textDoc;

        sendNotification("textDocument/didClose", params);
    }

    // Request completions at a position
    void requestCompletion(const std::string& uri, int line, int character,
                          std::function<void(const std::vector<LSPCompletionItem>&)> callback) {
        if (!initialized_) return;

        auto params = createTextDocumentPositionParams(uri, line, character);

        sendRequest("textDocument/completion", params, [callback](const JsonValue& result) {
            std::vector<LSPCompletionItem> items;

            // Result can be CompletionList or CompletionItem[]
            const auto& itemsArr = result.has("items") ? result.get("items").asArray() : result.asArray();

            for (const auto& item : itemsArr) {
                LSPCompletionItem ci;
                ci.label = item.get("label").asString();
                ci.kind = item.get("kind").asInt();
                ci.detail = item.get("detail").asString();
                ci.documentation = item.get("documentation").asString();
                ci.insertText = item.has("insertText") ? item.get("insertText").asString() : ci.label;
                ci.sortText = item.get("sortText").asString();
                ci.filterText = item.get("filterText").asString();
                items.push_back(ci);
            }

            if (callback) callback(items);
        });
    }

    // Request hover info at a position
    void requestHover(const std::string& uri, int line, int character,
                     std::function<void(const LSPHover&)> callback) {
        if (!initialized_) return;

        auto params = createTextDocumentPositionParams(uri, line, character);

        sendRequest("textDocument/hover", params, [callback](const JsonValue& result) {
            LSPHover hover;
            if (!result.isNull()) {
                const auto& contents = result.get("contents");
                if (contents.type() == JsonValue::String) {
                    hover.contents = contents.asString();
                } else if (contents.type() == JsonValue::Object) {
                    hover.contents = contents.get("value").asString();
                }
            }
            if (callback) callback(hover);
        });
    }

    // Request go-to-definition
    void requestDefinition(const std::string& uri, int line, int character,
                          std::function<void(const std::vector<LSPLocation>&)> callback) {
        if (!initialized_) return;

        auto params = createTextDocumentPositionParams(uri, line, character);

        sendRequest("textDocument/definition", params, [callback](const JsonValue& result) {
            std::vector<LSPLocation> locations;
            if (result.type() == JsonValue::Array) {
                for (const auto& loc : result.asArray()) {
                    locations.push_back(parseLocation(loc));
                }
            } else if (result.type() == JsonValue::Object) {
                locations.push_back(parseLocation(result));
            }
            if (callback) callback(locations);
        });
    }

    // Request document symbols
    void requestDocumentSymbols(const std::string& uri,
                               std::function<void(const std::vector<LSPSymbolInfo>&)> callback) {
        if (!initialized_) return;

        auto params = JsonValue::object();
        auto textDoc = JsonValue::object();
        textDoc["uri"] = uri;
        params["textDocument"] = textDoc;

        sendRequest("textDocument/documentSymbol", params, [callback](const JsonValue& result) {
            std::vector<LSPSymbolInfo> symbols;
            for (const auto& sym : result.asArray()) {
                symbols.push_back(parseSymbolInfo(sym));
            }
            if (callback) callback(symbols);
        });
    }

    // Request references
    void requestReferences(const std::string& uri, int line, int character,
                          std::function<void(const std::vector<LSPLocation>&)> callback) {
        if (!initialized_) return;

        auto params = createTextDocumentPositionParams(uri, line, character);
        auto context = JsonValue::object();
        context["includeDeclaration"] = true;
        params["context"] = context;

        sendRequest("textDocument/references", params, [callback](const JsonValue& result) {
            std::vector<LSPLocation> locations;
            for (const auto& loc : result.asArray()) {
                locations.push_back(parseLocation(loc));
            }
            if (callback) callback(locations);
        });
    }

    // Request formatting
    void requestFormatting(const std::string& uri,
                          std::function<void(const std::string&)> callback) {
        if (!initialized_) return;

        auto params = JsonValue::object();
        auto textDoc = JsonValue::object();
        textDoc["uri"] = uri;
        params["textDocument"] = textDoc;

        auto options = JsonValue::object();
        options["tabSize"] = 4;
        options["insertSpaces"] = true;
        params["options"] = options;

        sendRequest("textDocument/formatting", params, [callback](const JsonValue& result) {
            // Apply text edits - simplified: just return formatted text
            if (callback) callback(result.dump());
        });
    }

    // Setters
    void setOnDiagnostics(std::function<void(const std::string&, const std::vector<LSPDiagnostic>&)> callback) {
        onDiagnostics_ = callback;
    }
    void setOnLog(std::function<void(const std::string&)> callback) {
        onLog_ = callback;
    }

    // State
    bool isRunning() const { return running_; }
    bool isInitialized() const { return initialized_; }

private:
    // Send the initialize request
    bool sendInitialize() {
        auto params = JsonValue::object();
        params["processId"] = (int)GetCurrentProcessId();

        auto clientInfo = JsonValue::object();
        clientInfo["name"] = "RawrXD-IDE";
        clientInfo["version"] = "1.0.0";
        params["clientInfo"] = clientInfo;

        params["rootUri"] = "file:///" + workspacePath_;

        auto capabilities = JsonValue::object();

        // Text document capabilities
        auto textDoc = JsonValue::object();

        auto completion = JsonValue::object();
        auto completionItem = JsonValue::object();
        completionItem["snippetSupport"] = true;
        completion["completionItem"] = completionItem;
        textDoc["completion"] = completion;

        auto hover = JsonValue::object();
        hover["contentFormat"] = JsonValue::array();
        textDoc["hover"] = hover;

        auto definition = JsonValue::object();
        definition["linkSupport"] = true;
        textDoc["definition"] = definition;

        auto diagnostics = JsonValue::object();
        diagnostics["relatedDocumentSupport"] = false;
        textDoc["publishDiagnostics"] = diagnostics;

        capabilities["textDocument"] = textDoc;
        params["capabilities"] = capabilities;

        sendRequest("initialize", params, [this](const JsonValue& result) {
            // Process server capabilities
            log("LSP server initialized");

            // Send initialized notification
            sendNotification("initialized", JsonValue::object());
            initialized_ = true;
        });

        return true;
    }

    // Create JSON-RPC request
    JsonValue createRequest(const std::string& method, const JsonValue& params) {
        auto msg = JsonValue::object();
        msg["jsonrpc"] = "2.0";
        msg["id"] = nextId_++;
        msg["method"] = method;
        msg["params"] = params;
        return msg;
    }

    // Create JSON-RPC notification
    JsonValue createNotification(const std::string& method, const JsonValue& params) {
        auto msg = JsonValue::object();
        msg["jsonrpc"] = "2.0";
        msg["method"] = method;
        msg["params"] = params;
        return msg;
    }

    // Send a request with callback
    void sendRequest(const std::string& method, const JsonValue& params,
                    std::function<void(const JsonValue&)> callback) {
        auto msg = createRequest(method, params);
        int id = msg.get("id").asInt();

        {
            std::lock_guard<std::mutex> lock(responseMutex_);
            pendingRequests_[id] = callback;
        }

        sendMessage(msg);
    }

    // Send a notification (no response expected)
    void sendNotification(const std::string& method, const JsonValue& params) {
        auto msg = createNotification(method, params);
        sendMessage(msg);
    }

    // Send a raw JSON-RPC message
    void sendMessage(const JsonValue& msg) {
        std::lock_guard<std::mutex> lock(sendMutex_);

        std::string content = msg.dump();
        std::string header = "Content-Length: " + std::to_string(content.length()) + "\r\n\r\n";
        std::string fullMessage = header + content;

        DWORD written;
        WriteFile(hStdinWrite_, fullMessage.c_str(), (DWORD)fullMessage.length(), &written, nullptr);
    }

    // Reader thread loop
    void readerLoop() {
        std::string buffer;
        char readBuf[4096];

        while (running_) {
            DWORD bytesRead;
            if (!ReadFile(hStdoutRead_, readBuf, sizeof(readBuf), &bytesRead, nullptr)) {
                break;
            }

            if (bytesRead == 0) continue;
            buffer.append(readBuf, bytesRead);

            // Parse LSP messages from buffer
            while (true) {
                // Find Content-Length header
                size_t headerEnd = buffer.find("\r\n\r\n");
                if (headerEnd == std::string::npos) break;

                // Parse Content-Length
                int contentLength = 0;
                size_t clPos = buffer.find("Content-Length: ");
                if (clPos != std::string::npos && clPos < headerEnd) {
                    contentLength = std::stoi(buffer.substr(clPos + 16));
                }

                // Check if we have the full message
                size_t messageStart = headerEnd + 4;
                if (buffer.length() < messageStart + contentLength) break;

                // Extract and parse the message
                std::string content = buffer.substr(messageStart, contentLength);
                buffer.erase(0, messageStart + contentLength);

                handleMessage(content);
            }
        }
    }

    // Handle an incoming JSON-RPC message
    void handleMessage(const std::string& content) {
        auto msg = JsonValue::parse(content);

        if (msg.has("id") && msg.has("result")) {
            // Response to our request
            int id = msg.get("id").asInt();

            std::lock_guard<std::mutex> lock(responseMutex_);
            auto it = pendingRequests_.find(id);
            if (it != pendingRequests_.end()) {
                it->second(msg.get("result"));
                pendingRequests_.erase(it);
            }
        } else if (msg.has("id") && msg.has("error")) {
            // Error response
            int id = msg.get("id").asInt();
            std::string errMsg = msg.get("error").get("message").asString();
            log("LSP error: " + errMsg);

            std::lock_guard<std::mutex> lock(responseMutex_);
            pendingRequests_.erase(id);
        } else if (msg.has("method") && !msg.has("id")) {
            // Notification from server
            handleNotification(msg.get("method").asString(), msg.get("params"));
        }
    }

    // Handle server notifications
    void handleNotification(const std::string& method, const JsonValue& params) {
        if (method == "textDocument/publishDiagnostics") {
            std::string uri = params.get("uri").asString();
            std::vector<LSPDiagnostic> diagnostics;

            for (const auto& diag : params.get("diagnostics").asArray()) {
                LSPDiagnostic d;
                d.range = parseRange(diag.get("range"));
                d.severity = diag.get("severity").asInt();
                d.message = diag.get("message").asString();
                d.source = diag.get("source").asString();
                d.code = diag.get("code").asString();
                diagnostics.push_back(d);
            }

            if (onDiagnostics_) {
                onDiagnostics_(uri, diagnostics);
            }
        } else if (method == "window/logMessage") {
            log("[Server] " + params.get("message").asString());
        }
    }

    // Helper: create textDocument/position params
    JsonValue createTextDocumentPositionParams(const std::string& uri, int line, int character) {
        auto params = JsonValue::object();
        auto textDoc = JsonValue::object();
        textDoc["uri"] = uri;
        params["textDocument"] = textDoc;

        auto position = JsonValue::object();
        position["line"] = line;
        position["character"] = character;
        params["position"] = position;

        return params;
    }

    // Parse helpers
    static LSPRange parseRange(const JsonValue& v) {
        LSPRange r;
        r.start.line = v.get("start").get("line").asInt();
        r.start.character = v.get("start").get("character").asInt();
        r.end.line = v.get("end").get("line").asInt();
        r.end.character = v.get("end").get("character").asInt();
        return r;
    }

    static LSPLocation parseLocation(const JsonValue& v) {
        LSPLocation loc;
        loc.uri = v.get("uri").asString();
        loc.range = parseRange(v.get("range"));
        return loc;
    }

    static LSPSymbolInfo parseSymbolInfo(const JsonValue& v) {
        LSPSymbolInfo sym;
        sym.name = v.get("name").asString();
        sym.kind = v.get("kind").asInt();
        sym.range = parseRange(v.get("range"));
        sym.selectionRange = parseRange(v.get("selectionRange"));
        if (v.has("children")) {
            for (const auto& child : v.get("children").asArray()) {
                sym.children.push_back(parseSymbolInfo(child));
            }
        }
        return sym;
    }

    void closeHandles() {
        if (hStdinRead_) { CloseHandle(hStdinRead_); hStdinRead_ = nullptr; }
        if (hStdinWrite_) { CloseHandle(hStdinWrite_); hStdinWrite_ = nullptr; }
        if (hStdoutRead_) { CloseHandle(hStdoutRead_); hStdoutRead_ = nullptr; }
        if (hStdoutWrite_) { CloseHandle(hStdoutWrite_); hStdoutWrite_ = nullptr; }
    }

    void log(const std::string& msg) {
        if (onLog_) onLog_(msg);
        OutputDebugStringA(("[LSP] " + msg + "\n").c_str());
    }
};

// ============================================================================
// LSP Manager - manages multiple language server instances
// ============================================================================
class LSPManager {
private:
    std::unordered_map<std::string, std::unique_ptr<LSPClient>> clients_;
    std::mutex mutex_;

    // Known language server paths
    struct ServerConfig {
        std::string languageId;
        std::string serverPath;
        std::vector<std::string> fileExtensions;
    };

    std::vector<ServerConfig> knownServers_ = {
        {"cpp", "clangd", {".cpp", ".c", ".h", ".hpp", ".cc", ".cxx"}},
        {"python", "pylsp", {".py"}},
        {"javascript", "typescript-language-server --stdio", {".js", ".ts", ".jsx", ".tsx"}},
        {"rust", "rust-analyzer", {".rs"}},
        {"go", "gopls", {".go"}},
    };

public:
    LSPManager() = default;

    ~LSPManager() {
        shutdownAll();
    }

    // Get or create a client for a given language
    LSPClient* getClient(const std::string& languageId, const std::string& workspacePath) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = clients_.find(languageId);
        if (it != clients_.end() && it->second->isRunning()) {
            return it->second.get();
        }

        // Find server config
        for (const auto& config : knownServers_) {
            if (config.languageId == languageId) {
                auto client = std::make_unique<LSPClient>();
                if (client->start(config.serverPath, workspacePath, languageId)) {
                    auto* ptr = client.get();
                    clients_[languageId] = std::move(client);
                    return ptr;
                }
            }
        }

        return nullptr;
    }

    // Get the language ID for a file extension
    std::string getLanguageId(const std::string& filePath) const {
        size_t dotPos = filePath.find_last_of('.');
        if (dotPos == std::string::npos) return "";

        std::string ext = filePath.substr(dotPos);
        for (auto& c : ext) c = tolower(c);

        for (const auto& config : knownServers_) {
            for (const auto& configExt : config.fileExtensions) {
                if (ext == configExt) return config.languageId;
            }
        }
        return "";
    }

    // Shutdown all clients
    void shutdownAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [lang, client] : clients_) {
            client->shutdown();
        }
        clients_.clear();
    }

    // Add custom server configuration
    void addServerConfig(const std::string& languageId, const std::string& serverPath,
                        const std::vector<std::string>& extensions) {
        knownServers_.push_back({languageId, serverPath, extensions});
    }
};

} // namespace RawrXD::Agentic
