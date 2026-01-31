// ═══════════════════════════════════════════════════════════════════════════════
// LSPClient.hpp - Language Server Protocol Integration
// Pure C++20 implementation - No Qt dependencies
// ═══════════════════════════════════════════════════════════════════════════════

#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <future>
#include <variant>

namespace RawrXD {
namespace LSP {

using String = std::wstring;
template<typename T> using Vector = std::vector<T>;
template<typename T> using Optional = std::optional<T>;
template<typename T> using SharedPtr = std::shared_ptr<T>;

// ═══════════════════════════════════════════════════════════════════════════════
// LSP Types (Language Server Protocol specification)
// ═══════════════════════════════════════════════════════════════════════════════

struct Position {
    int line{0};
    int character{0};
};

struct Range {
    Position start;
    Position end;
};

struct Location {
    String uri;
    Range range;
};

struct Diagnostic {
    Range range;
    int severity{1}; // 1=Error, 2=Warning, 3=Info, 4=Hint
    String code;
    String source;
    String message;
};

struct TextEdit {
    Range range;
    String newText;
};

struct CompletionItem {
    String label;
    int kind{1}; // 1=Text, 2=Method, 3=Function, 4=Constructor, 5=Field, 6=Variable, 7=Class, 8=Interface, 9=Module, 10=Property
    String detail;
    String documentation;
    String insertText;
};

struct SymbolInformation {
    String name;
    int kind{1};
    Location location;
    String containerName;
};

struct Hover {
    String contents;
    Optional<Range> range;
};

// ═══════════════════════════════════════════════════════════════════════════════
// JSON-RPC Message Types
// ═══════════════════════════════════════════════════════════════════════════════

struct JsonRpcMessage {
    String jsonrpc{L"2.0"};
    Optional<int> id;
    String method;
    std::map<String, std::variant<std::nullptr_t, bool, int64_t, double, String, 
                                   std::vector<std::variant<std::nullptr_t, bool, int64_t, double, String>>,
                                   std::map<String, std::variant<std::nullptr_t, bool, int64_t, double, String>>>> params;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Process I/O Manager - Handles stdin/stdout with LSP server
// ═══════════════════════════════════════════════════════════════════════════════

class ProcessIO {
public:
    ProcessIO() = default;
    ~ProcessIO() { Stop(); }
    
    bool Start(const String& command, const Vector<String>& args = {}) {
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        
        // Create pipes for stdin/stdout
        if (!CreatePipe(&hStdoutRead_, &hStdoutWrite_, &sa, 0)) return false;
        SetHandleInformation(hStdoutRead_, HANDLE_FLAG_INHERIT, 0);
        
        if (!CreatePipe(&hStdinRead_, &hStdinWrite_, &sa, 0)) return false;
        SetHandleInformation(hStdinWrite_, HANDLE_FLAG_INHERIT, 0);
        
        // Build command line
        String cmdLine = command;
        for (const auto& arg : args) {
            cmdLine += L" " + arg;
        }
        
        STARTUPINFOW si{sizeof(si)};
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = hStdinRead_;
        si.hStdOutput = hStdoutWrite_;
        si.hStdError = hStdoutWrite_;
        
        std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(L'\0');
        
        if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi_)) {
            return false;
        }
        
        CloseHandle(hStdoutWrite_);
        CloseHandle(hStdinRead_);
        hStdoutWrite_ = nullptr;
        hStdinRead_ = nullptr;
        
        running_ = true;
        return true;
    }
    
    void Stop() {
        running_ = false;
        
        if (pi_.hProcess) {
            TerminateProcess(pi_.hProcess, 1);
            WaitForSingleObject(pi_.hProcess, 1000);
            CloseHandle(pi_.hProcess);
            CloseHandle(pi_.hThread);
            pi_ = {};
        }
        
        if (hStdoutRead_) { CloseHandle(hStdoutRead_); hStdoutRead_ = nullptr; }
        if (hStdinWrite_) { CloseHandle(hStdinWrite_); hStdinWrite_ = nullptr; }
    }
    
    bool Write(const std::string& data) {
        if (!hStdinWrite_) return false;
        
        DWORD written;
        return WriteFile(hStdinWrite_, data.c_str(), (DWORD)data.size(), &written, nullptr) &&
               written == data.size();
    }
    
    std::string Read(size_t maxBytes = 4096) {
        if (!hStdoutRead_) return {};
        
        std::vector<char> buffer(maxBytes);
        DWORD bytesRead;
        
        if (!ReadFile(hStdoutRead_, buffer.data(), (DWORD)maxBytes, &bytesRead, nullptr)) {
            return {};
        }
        
        return std::string(buffer.data(), bytesRead);
    }
    
    bool IsRunning() const { return running_ && pi_.hProcess; }
    
private:
    PROCESS_INFORMATION pi_{};
    HANDLE hStdoutRead_{nullptr};
    HANDLE hStdoutWrite_{nullptr};
    HANDLE hStdinRead_{nullptr};
    HANDLE hStdinWrite_{nullptr};
    std::atomic<bool> running_{false};
};

// ═══════════════════════════════════════════════════════════════════════════════
// Simple JSON Builder for LSP messages
// ═══════════════════════════════════════════════════════════════════════════════

class JsonBuilder {
public:
    static std::string Object(const std::vector<std::pair<std::string, std::string>>& fields) {
        std::string result = "{";
        for (size_t i = 0; i < fields.size(); i++) {
            if (i > 0) result += ", ";
            result += "\"" + fields[i].first + "\": " + fields[i].second;
        }
        result += "}";
        return result;
    }
    
    static std::string Array(const std::vector<std::string>& items) {
        std::string result = "[";
        for (size_t i = 0; i < items.size(); i++) {
            if (i > 0) result += ", ";
            result += items[i];
        }
        result += "]";
        return result;
    }
    
    static std::string String(const std::string& s) {
        std::string result = "\"";
        for (char c : s) {
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else if (c == '\n') result += "\\n";
            else if (c == '\r') result += "\\r";
            else if (c == '\t') result += "\\t";
            else result += c;
        }
        result += "\"";
        return result;
    }
    
    static std::string Int(int64_t v) { return std::to_string(v); }
    static std::string Bool(bool v) { return v ? "true" : "false"; }
    static std::string Null() { return "null"; }
    
    static std::string Position(int line, int character) {
        return Object({{"line", Int(line)}, {"character", Int(character)}});
    }
    
    static std::string Range(int startLine, int startChar, int endLine, int endChar) {
        return Object({
            {"start", Position(startLine, startChar)},
            {"end", Position(endLine, endChar)}
        });
    }
    
    static std::string TextDocumentIdentifier(const std::string& uri) {
        return Object({{"uri", String(uri)}});
    }
    
    static std::string TextDocumentPositionParams(const std::string& uri, int line, int character) {
        return Object({
            {"textDocument", TextDocumentIdentifier(uri)},
            {"position", Position(line, character)}
        });
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Simple JSON Parser for LSP responses
// ═══════════════════════════════════════════════════════════════════════════════

class JsonParser {
public:
    struct Value;
    using Object = std::map<std::string, Value>;
    using Array = std::vector<Value>;
    
    struct Value {
        std::variant<std::nullptr_t, bool, int64_t, double, std::string, Array, Object> data;
        
        bool isNull() const { return std::holds_alternative<std::nullptr_t>(data); }
        bool isBool() const { return std::holds_alternative<bool>(data); }
        bool isInt() const { return std::holds_alternative<int64_t>(data); }
        bool isDouble() const { return std::holds_alternative<double>(data); }
        bool isString() const { return std::holds_alternative<std::string>(data); }
        bool isArray() const { return std::holds_alternative<Array>(data); }
        bool isObject() const { return std::holds_alternative<Object>(data); }
        
        bool asBool() const { return std::get<bool>(data); }
        int64_t asInt() const { return std::get<int64_t>(data); }
        double asDouble() const { return std::get<double>(data); }
        const std::string& asString() const { return std::get<std::string>(data); }
        const Array& asArray() const { return std::get<Array>(data); }
        const Object& asObject() const { return std::get<Object>(data); }
        
        Value operator[](const std::string& key) const {
            if (isObject()) {
                auto& obj = asObject();
                auto it = obj.find(key);
                if (it != obj.end()) return it->second;
            }
            return Value{nullptr};
        }
        
        Value operator[](size_t idx) const {
            if (isArray() && idx < asArray().size()) {
                return asArray()[idx];
            }
            return Value{nullptr};
        }
    };
    
    static Optional<Value> Parse(const std::string& json) {
        size_t pos = 0;
        return parseValue(json, pos);
    }
    
private:
    static void skipWhitespace(const std::string& s, size_t& pos) {
        while (pos < s.size() && std::isspace(s[pos])) pos++;
    }
    
    static Optional<Value> parseValue(const std::string& s, size_t& pos) {
        skipWhitespace(s, pos);
        if (pos >= s.size()) return std::nullopt;
        
        char c = s[pos];
        if (c == 'n') return parseNull(s, pos);
        if (c == 't' || c == 'f') return parseBool(s, pos);
        if (c == '"') return parseString(s, pos);
        if (c == '[') return parseArray(s, pos);
        if (c == '{') return parseObject(s, pos);
        if (c == '-' || std::isdigit(c)) return parseNumber(s, pos);
        
        return std::nullopt;
    }
    
    static Optional<Value> parseNull(const std::string& s, size_t& pos) {
        if (s.substr(pos, 4) == "null") { pos += 4; return Value{nullptr}; }
        return std::nullopt;
    }
    
    static Optional<Value> parseBool(const std::string& s, size_t& pos) {
        if (s.substr(pos, 4) == "true") { pos += 4; return Value{true}; }
        if (s.substr(pos, 5) == "false") { pos += 5; return Value{false}; }
        return std::nullopt;
    }
    
    static Optional<Value> parseString(const std::string& s, size_t& pos) {
        if (pos >= s.size() || s[pos] != '"') return std::nullopt;
        pos++;
        
        std::string result;
        while (pos < s.size() && s[pos] != '"') {
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                pos++;
                switch (s[pos]) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    default: result += s[pos];
                }
            } else {
                result += s[pos];
            }
            pos++;
        }
        
        if (pos >= s.size()) return std::nullopt;
        pos++; // consume closing quote
        return Value{result};
    }
    
    static Optional<Value> parseNumber(const std::string& s, size_t& pos) {
        size_t start = pos;
        if (s[pos] == '-') pos++;
        while (pos < s.size() && std::isdigit(s[pos])) pos++;
        
        bool isFloat = false;
        if (pos < s.size() && s[pos] == '.') {
            isFloat = true;
            pos++;
            while (pos < s.size() && std::isdigit(s[pos])) pos++;
        }
        
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            isFloat = true;
            pos++;
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) pos++;
            while (pos < s.size() && std::isdigit(s[pos])) pos++;
        }
        
        std::string numStr = s.substr(start, pos - start);
        if (isFloat) return Value{std::stod(numStr)};
        return Value{static_cast<int64_t>(std::stoll(numStr))};
    }
    
    static Optional<Value> parseArray(const std::string& s, size_t& pos) {
        if (pos >= s.size() || s[pos] != '[') return std::nullopt;
        pos++;
        
        Array arr;
        skipWhitespace(s, pos);
        
        if (pos < s.size() && s[pos] == ']') { pos++; return Value{arr}; }
        
        while (true) {
            auto val = parseValue(s, pos);
            if (!val) return std::nullopt;
            arr.push_back(*val);
            
            skipWhitespace(s, pos);
            if (pos >= s.size()) return std::nullopt;
            if (s[pos] == ']') { pos++; return Value{arr}; }
            if (s[pos] != ',') return std::nullopt;
            pos++;
        }
    }
    
    static Optional<Value> parseObject(const std::string& s, size_t& pos) {
        if (pos >= s.size() || s[pos] != '{') return std::nullopt;
        pos++;
        
        Object obj;
        skipWhitespace(s, pos);
        
        if (pos < s.size() && s[pos] == '}') { pos++; return Value{obj}; }
        
        while (true) {
            skipWhitespace(s, pos);
            auto key = parseString(s, pos);
            if (!key || !key->isString()) return std::nullopt;
            
            skipWhitespace(s, pos);
            if (pos >= s.size() || s[pos] != ':') return std::nullopt;
            pos++;
            
            auto val = parseValue(s, pos);
            if (!val) return std::nullopt;
            
            obj[key->asString()] = *val;
            
            skipWhitespace(s, pos);
            if (pos >= s.size()) return std::nullopt;
            if (s[pos] == '}') { pos++; return Value{obj}; }
            if (s[pos] != ',') return std::nullopt;
            pos++;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// LSP Client - Manages connection to language server
// ═══════════════════════════════════════════════════════════════════════════════

class LSPClient {
public:
    struct Config {
        String serverCommand;
        Vector<String> serverArgs;
        String workspaceRoot;
        std::chrono::milliseconds initTimeout{30000};
        std::chrono::milliseconds requestTimeout{10000};
    };
    
    using DiagnosticsCallback = std::function<void(const String& uri, const Vector<Diagnostic>& diagnostics)>;
    using LogCallback = std::function<void(const String& message)>;
    
    explicit LSPClient(const Config& config) : config_(config) {}
    ~LSPClient() { Shutdown(); }
    
    bool Initialize() {
        if (connected_) return true;
        
        // Start the language server process
        std::vector<String> args = config_.serverArgs;
        if (!process_.Start(config_.serverCommand, args)) {
            Log(L"Failed to start LSP server process");
            return false;
        }
        
        // Start reader thread
        running_ = true;
        readerThread_ = std::thread(&LSPClient::ReaderLoop, this);
        
        // Send initialize request
        std::string workspaceUri = "file:///";
        for (wchar_t c : config_.workspaceRoot) {
            if (c == L'\\') workspaceUri += '/';
            else if (c < 128) workspaceUri += static_cast<char>(c);
        }
        
        auto initParams = JsonBuilder::Object({
            {"processId", JsonBuilder::Int(GetCurrentProcessId())},
            {"rootUri", JsonBuilder::String(workspaceUri)},
            {"capabilities", JsonBuilder::Object({
                {"textDocument", JsonBuilder::Object({
                    {"completion", JsonBuilder::Object({
                        {"completionItem", JsonBuilder::Object({
                            {"snippetSupport", JsonBuilder::Bool(true)}
                        })}
                    })},
                    {"hover", JsonBuilder::Object({})},
                    {"definition", JsonBuilder::Object({})},
                    {"references", JsonBuilder::Object({})},
                    {"documentSymbol", JsonBuilder::Object({})},
                    {"formatting", JsonBuilder::Object({})},
                    {"rename", JsonBuilder::Object({})}
                })},
                {"workspace", JsonBuilder::Object({
                    {"workspaceFolders", JsonBuilder::Bool(true)}
                })}
            })}
        });
        
        auto response = SendRequest("initialize", initParams);
        
        if (response && response->isObject()) {
            // Send initialized notification
            SendNotification("initialized", "{}");
            connected_ = true;
            Log(L"LSP client initialized successfully");
            return true;
        }
        
        Log(L"LSP initialization failed");
        Shutdown();
        return false;
    }
    
    void Shutdown() {
        if (!connected_) return;
        
        running_ = false;
        
        // Send shutdown request
        SendRequest("shutdown", "null");
        SendNotification("exit", "null");
        
        process_.Stop();
        
        if (readerThread_.joinable()) {
            readerThread_.join();
        }
        
        connected_ = false;
    }
    
    bool IsConnected() const { return connected_; }
    
    // ─────────────────────────────────────────────────────────────────────────────
    // Document Synchronization
    // ─────────────────────────────────────────────────────────────────────────────
    
    bool DidOpen(const String& uri, const String& languageId, const String& text) {
        std::string uriUtf8 = WideToUtf8(uri);
        std::string langUtf8 = WideToUtf8(languageId);
        std::string textUtf8 = WideToUtf8(text);
        
        auto params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uriUtf8)},
                {"languageId", JsonBuilder::String(langUtf8)},
                {"version", JsonBuilder::Int(1)},
                {"text", JsonBuilder::String(textUtf8)}
            })}
        });
        
        SendNotification("textDocument/didOpen", params);
        
        std::lock_guard<std::mutex> lock(documentsMutex_);
        openDocuments_[uri] = 1;
        return true;
    }
    
    bool DidChange(const String& uri, const String& newText, int version = -1) {
        std::string uriUtf8 = WideToUtf8(uri);
        std::string textUtf8 = WideToUtf8(newText);
        
        int ver = version;
        if (ver < 0) {
            std::lock_guard<std::mutex> lock(documentsMutex_);
            ver = ++openDocuments_[uri];
        }
        
        auto params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uriUtf8)},
                {"version", JsonBuilder::Int(ver)}
            })},
            {"contentChanges", JsonBuilder::Array({
                JsonBuilder::Object({{"text", JsonBuilder::String(textUtf8)}})
            })}
        });
        
        SendNotification("textDocument/didChange", params);
        return true;
    }
    
    bool DidSave(const String& uri) {
        std::string uriUtf8 = WideToUtf8(uri);
        
        auto params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uriUtf8)}
            })}
        });
        
        SendNotification("textDocument/didSave", params);
        return true;
    }
    
    bool DidClose(const String& uri) {
        std::string uriUtf8 = WideToUtf8(uri);
        
        auto params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({
                {"uri", JsonBuilder::String(uriUtf8)}
            })}
        });
        
        SendNotification("textDocument/didClose", params);
        
        std::lock_guard<std::mutex> lock(documentsMutex_);
        openDocuments_.erase(uri);
        return true;
    }
    
    // ─────────────────────────────────────────────────────────────────────────────
    // Language Features
    // ─────────────────────────────────────────────────────────────────────────────
    
    Vector<CompletionItem> Completion(const String& uri, int line, int character) {
        std::string uriUtf8 = WideToUtf8(uri);
        
        auto params = JsonBuilder::TextDocumentPositionParams(uriUtf8, line, character);
        auto response = SendRequest("textDocument/completion", params);
        
        Vector<CompletionItem> items;
        
        if (response && response->isObject()) {
            auto itemsJson = (*response)["items"];
            if (itemsJson.isArray()) {
                for (const auto& item : itemsJson.asArray()) {
                    CompletionItem ci;
                    if (item["label"].isString()) {
                        ci.label = Utf8ToWide(item["label"].asString());
                    }
                    if (item["kind"].isInt()) {
                        ci.kind = static_cast<int>(item["kind"].asInt());
                    }
                    if (item["detail"].isString()) {
                        ci.detail = Utf8ToWide(item["detail"].asString());
                    }
                    if (item["insertText"].isString()) {
                        ci.insertText = Utf8ToWide(item["insertText"].asString());
                    }
                    items.push_back(ci);
                }
            }
        }
        
        return items;
    }
    
    Optional<Location> Definition(const String& uri, int line, int character) {
        std::string uriUtf8 = WideToUtf8(uri);
        
        auto params = JsonBuilder::TextDocumentPositionParams(uriUtf8, line, character);
        auto response = SendRequest("textDocument/definition", params);
        
        if (response) {
            // Can be Location or Location[]
            if (response->isObject()) {
                return ParseLocation(*response);
            } else if (response->isArray() && !response->asArray().empty()) {
                return ParseLocation(response->asArray()[0]);
            }
        }
        
        return std::nullopt;
    }
    
    Vector<Location> References(const String& uri, int line, int character, bool includeDeclaration = true) {
        std::string uriUtf8 = WideToUtf8(uri);
        
        auto params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({{"uri", JsonBuilder::String(uriUtf8)}})},
            {"position", JsonBuilder::Position(line, character)},
            {"context", JsonBuilder::Object({
                {"includeDeclaration", JsonBuilder::Bool(includeDeclaration)}
            })}
        });
        
        auto response = SendRequest("textDocument/references", params);
        
        Vector<Location> locations;
        if (response && response->isArray()) {
            for (const auto& item : response->asArray()) {
                auto loc = ParseLocation(item);
                if (loc) locations.push_back(*loc);
            }
        }
        
        return locations;
    }
    
    Optional<Hover> GetHover(const String& uri, int line, int character) {
        std::string uriUtf8 = WideToUtf8(uri);
        
        auto params = JsonBuilder::TextDocumentPositionParams(uriUtf8, line, character);
        auto response = SendRequest("textDocument/hover", params);
        
        if (response && response->isObject()) {
            Hover hover;
            auto contents = (*response)["contents"];
            if (contents.isString()) {
                hover.contents = Utf8ToWide(contents.asString());
            } else if (contents.isObject() && contents["value"].isString()) {
                hover.contents = Utf8ToWide(contents["value"].asString());
            }
            return hover;
        }
        
        return std::nullopt;
    }
    
    Vector<SymbolInformation> DocumentSymbol(const String& uri) {
        std::string uriUtf8 = WideToUtf8(uri);
        
        auto params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({{"uri", JsonBuilder::String(uriUtf8)}})}
        });
        
        auto response = SendRequest("textDocument/documentSymbol", params);
        
        Vector<SymbolInformation> symbols;
        if (response && response->isArray()) {
            for (const auto& item : response->asArray()) {
                SymbolInformation sym;
                if (item["name"].isString()) {
                    sym.name = Utf8ToWide(item["name"].asString());
                }
                if (item["kind"].isInt()) {
                    sym.kind = static_cast<int>(item["kind"].asInt());
                }
                auto loc = ParseLocation(item["location"]);
                if (loc) sym.location = *loc;
                symbols.push_back(sym);
            }
        }
        
        return symbols;
    }
    
    Vector<TextEdit> Formatting(const String& uri) {
        std::string uriUtf8 = WideToUtf8(uri);
        
        auto params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({{"uri", JsonBuilder::String(uriUtf8)}})},
            {"options", JsonBuilder::Object({
                {"tabSize", JsonBuilder::Int(4)},
                {"insertSpaces", JsonBuilder::Bool(true)}
            })}
        });
        
        auto response = SendRequest("textDocument/formatting", params);
        
        Vector<TextEdit> edits;
        if (response && response->isArray()) {
            for (const auto& item : response->asArray()) {
                TextEdit edit;
                auto range = item["range"];
                if (range.isObject()) {
                    edit.range = ParseRange(range);
                }
                if (item["newText"].isString()) {
                    edit.newText = Utf8ToWide(item["newText"].asString());
                }
                edits.push_back(edit);
            }
        }
        
        return edits;
    }
    
    Vector<TextEdit> Rename(const String& uri, int line, int character, const String& newName) {
        std::string uriUtf8 = WideToUtf8(uri);
        std::string nameUtf8 = WideToUtf8(newName);
        
        auto params = JsonBuilder::Object({
            {"textDocument", JsonBuilder::Object({{"uri", JsonBuilder::String(uriUtf8)}})},
            {"position", JsonBuilder::Position(line, character)},
            {"newName", JsonBuilder::String(nameUtf8)}
        });
        
        auto response = SendRequest("textDocument/rename", params);
        
        Vector<TextEdit> edits;
        // Parse WorkspaceEdit response
        if (response && response->isObject()) {
            auto changes = (*response)["changes"];
            if (changes.isObject()) {
                for (const auto& [fileUri, fileEdits] : changes.asObject()) {
                    if (fileEdits.isArray()) {
                        for (const auto& editJson : fileEdits.asArray()) {
                            TextEdit edit;
                            auto range = editJson["range"];
                            if (range.isObject()) {
                                edit.range = ParseRange(range);
                            }
                            if (editJson["newText"].isString()) {
                                edit.newText = Utf8ToWide(editJson["newText"].asString());
                            }
                            edits.push_back(edit);
                        }
                    }
                }
            }
        }
        
        return edits;
    }
    
    Vector<Diagnostic> GetDiagnostics(const String& uri) {
        std::lock_guard<std::mutex> lock(diagnosticsMutex_);
        auto it = diagnostics_.find(uri);
        if (it != diagnostics_.end()) {
            return it->second;
        }
        return {};
    }
    
    // Callbacks
    void SetDiagnosticsCallback(DiagnosticsCallback cb) { diagnosticsCallback_ = cb; }
    void SetLogCallback(LogCallback cb) { logCallback_ = cb; }

private:
    Config config_;
    ProcessIO process_;
    
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    
    std::thread readerThread_;
    
    std::atomic<int> nextId_{1};
    std::map<int, std::promise<Optional<JsonParser::Value>>> pendingRequests_;
    std::mutex pendingMutex_;
    
    std::map<String, int> openDocuments_;
    std::mutex documentsMutex_;
    
    std::map<String, Vector<Diagnostic>> diagnostics_;
    std::mutex diagnosticsMutex_;
    
    DiagnosticsCallback diagnosticsCallback_;
    LogCallback logCallback_;
    
    // ─────────────────────────────────────────────────────────────────────────────
    // Message I/O
    // ─────────────────────────────────────────────────────────────────────────────
    
    Optional<JsonParser::Value> SendRequest(const std::string& method, const std::string& params) {
        int id = nextId_++;
        
        // Create promise for response
        std::promise<Optional<JsonParser::Value>> promise;
        std::future<Optional<JsonParser::Value>> future = promise.get_future();
        
        {
            std::lock_guard<std::mutex> lock(pendingMutex_);
            pendingRequests_[id] = std::move(promise);
        }
        
        // Build JSON-RPC message
        auto message = JsonBuilder::Object({
            {"jsonrpc", JsonBuilder::String("2.0")},
            {"id", JsonBuilder::Int(id)},
            {"method", JsonBuilder::String(method)},
            {"params", params}
        });
        
        SendMessage(message);
        
        // Wait for response
        auto status = future.wait_for(config_.requestTimeout);
        if (status == std::future_status::timeout) {
            std::lock_guard<std::mutex> lock(pendingMutex_);
            pendingRequests_.erase(id);
            return std::nullopt;
        }
        
        return future.get();
    }
    
    void SendNotification(const std::string& method, const std::string& params) {
        auto message = JsonBuilder::Object({
            {"jsonrpc", JsonBuilder::String("2.0")},
            {"method", JsonBuilder::String(method)},
            {"params", params}
        });
        
        SendMessage(message);
    }
    
    void SendMessage(const std::string& content) {
        std::string header = "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n";
        process_.Write(header + content);
    }
    
    // ─────────────────────────────────────────────────────────────────────────────
    // Reader Loop
    // ─────────────────────────────────────────────────────────────────────────────
    
    void ReaderLoop() {
        std::string buffer;
        
        while (running_ && process_.IsRunning()) {
            std::string data = process_.Read(4096);
            if (data.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            buffer += data;
            
            // Parse LSP messages
            while (true) {
                // Find Content-Length header
                size_t headerEnd = buffer.find("\r\n\r\n");
                if (headerEnd == std::string::npos) break;
                
                std::string header = buffer.substr(0, headerEnd);
                size_t lenPos = header.find("Content-Length: ");
                if (lenPos == std::string::npos) {
                    buffer = buffer.substr(headerEnd + 4);
                    continue;
                }
                
                size_t contentLength = std::stoul(header.substr(lenPos + 16));
                size_t contentStart = headerEnd + 4;
                
                if (buffer.size() < contentStart + contentLength) break;
                
                std::string content = buffer.substr(contentStart, contentLength);
                buffer = buffer.substr(contentStart + contentLength);
                
                ProcessMessage(content);
            }
        }
    }
    
    void ProcessMessage(const std::string& content) {
        auto json = JsonParser::Parse(content);
        if (!json || !json->isObject()) return;
        
        // Check if it's a response
        auto id = (*json)["id"];
        if (!id.isNull()) {
            int reqId = 0;
            if (id.isInt()) reqId = static_cast<int>(id.asInt());
            else if (id.isString()) reqId = std::stoi(id.asString());
            
            std::lock_guard<std::mutex> lock(pendingMutex_);
            auto it = pendingRequests_.find(reqId);
            if (it != pendingRequests_.end()) {
                auto result = (*json)["result"];
                if (!result.isNull()) {
                    it->second.set_value(result);
                } else {
                    it->second.set_value(std::nullopt);
                }
                pendingRequests_.erase(it);
            }
            return;
        }
        
        // Check if it's a notification
        auto method = (*json)["method"];
        if (method.isString()) {
            std::string methodName = method.asString();
            
            if (methodName == "textDocument/publishDiagnostics") {
                HandleDiagnostics((*json)["params"]);
            }
        }
    }
    
    void HandleDiagnostics(const JsonParser::Value& params) {
        if (!params.isObject()) return;
        
        String uri;
        if (params["uri"].isString()) {
            uri = Utf8ToWide(params["uri"].asString());
        }
        
        Vector<Diagnostic> diags;
        auto diagsJson = params["diagnostics"];
        if (diagsJson.isArray()) {
            for (const auto& d : diagsJson.asArray()) {
                Diagnostic diag;
                auto range = d["range"];
                if (range.isObject()) {
                    diag.range = ParseRange(range);
                }
                if (d["severity"].isInt()) {
                    diag.severity = static_cast<int>(d["severity"].asInt());
                }
                if (d["message"].isString()) {
                    diag.message = Utf8ToWide(d["message"].asString());
                }
                if (d["source"].isString()) {
                    diag.source = Utf8ToWide(d["source"].asString());
                }
                diags.push_back(diag);
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(diagnosticsMutex_);
            diagnostics_[uri] = diags;
        }
        
        if (diagnosticsCallback_) {
            diagnosticsCallback_(uri, diags);
        }
    }
    
    // ─────────────────────────────────────────────────────────────────────────────
    // Helpers
    // ─────────────────────────────────────────────────────────────────────────────
    
    Optional<Location> ParseLocation(const JsonParser::Value& json) {
        if (!json.isObject()) return std::nullopt;
        
        Location loc;
        if (json["uri"].isString()) {
            loc.uri = Utf8ToWide(json["uri"].asString());
        }
        auto range = json["range"];
        if (range.isObject()) {
            loc.range = ParseRange(range);
        }
        return loc;
    }
    
    Range ParseRange(const JsonParser::Value& json) {
        Range r;
        auto start = json["start"];
        auto end = json["end"];
        
        if (start.isObject()) {
            if (start["line"].isInt()) r.start.line = static_cast<int>(start["line"].asInt());
            if (start["character"].isInt()) r.start.character = static_cast<int>(start["character"].asInt());
        }
        if (end.isObject()) {
            if (end["line"].isInt()) r.end.line = static_cast<int>(end["line"].asInt());
            if (end["character"].isInt()) r.end.character = static_cast<int>(end["character"].asInt());
        }
        
        return r;
    }
    
    void Log(const String& message) {
        if (logCallback_) {
            logCallback_(message);
        }
    }
    
    static std::string WideToUtf8(const String& wide) {
        if (wide.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
        return result;
    }
    
    static String Utf8ToWide(const std::string& utf8) {
        if (utf8.empty()) return {};
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
        String result(size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, result.data(), size);
        return result;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// LSP Server Manager - Manages multiple LSP servers for different languages
// ═══════════════════════════════════════════════════════════════════════════════

class LSPServerManager {
public:
    void RegisterServer(const String& languageId, const LSPClient::Config& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto client = std::make_shared<LSPClient>(config);
        if (client->Initialize()) {
            servers_[languageId] = client;
        }
    }
    
    void UnregisterServer(const String& languageId) {
        std::lock_guard<std::mutex> lock(mutex_);
        servers_.erase(languageId);
    }
    
    SharedPtr<LSPClient> GetClient(const String& languageId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = servers_.find(languageId);
        if (it != servers_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    SharedPtr<LSPClient> GetClientForFile(const String& filePath) {
        String language = DetectLanguage(filePath);
        return GetClient(language);
    }
    
    static String DetectLanguage(const String& filePath) {
        size_t dotPos = filePath.rfind(L'.');
        if (dotPos == String::npos) return L"plaintext";
        
        String ext = filePath.substr(dotPos);
        
        // Common extensions
        if (ext == L".cpp" || ext == L".cc" || ext == L".cxx" || ext == L".hpp" || ext == L".h") return L"cpp";
        if (ext == L".c") return L"c";
        if (ext == L".py" || ext == L".pyw") return L"python";
        if (ext == L".js" || ext == L".jsx") return L"javascript";
        if (ext == L".ts" || ext == L".tsx") return L"typescript";
        if (ext == L".json") return L"json";
        if (ext == L".html" || ext == L".htm") return L"html";
        if (ext == L".css") return L"css";
        if (ext == L".rs") return L"rust";
        if (ext == L".go") return L"go";
        if (ext == L".java") return L"java";
        if (ext == L".cs") return L"csharp";
        if (ext == L".asm") return L"asm";
        
        return L"plaintext";
    }

private:
    std::map<String, SharedPtr<LSPClient>> servers_;
    mutable std::mutex mutex_;
};

} // namespace LSP
} // namespace RawrXD
