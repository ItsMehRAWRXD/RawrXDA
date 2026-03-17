#pragma once

// ============================================================================
// RAWRXD LANGUAGE SERVER PROTOCOL (LSP) IMPLEMENTATION
// Complete LSP server for IDE integration
// ============================================================================

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <sstream>
#include <iostream>
#include <atomic>

namespace RawrXD {
namespace Compiler {
namespace LSP {

// ============================================================================
// JSON-RPC TYPES
// ============================================================================

using JsonValue = std::variant<
    std::nullptr_t,
    bool,
    int64_t,
    double,
    std::string,
    std::vector<struct JsonNode>,
    std::map<std::string, struct JsonNode>
>;

struct JsonNode {
    JsonValue value;
    
    JsonNode() : value(nullptr) {}
    JsonNode(std::nullptr_t) : value(nullptr) {}
    JsonNode(bool b) : value(b) {}
    JsonNode(int i) : value(static_cast<int64_t>(i)) {}
    JsonNode(int64_t i) : value(i) {}
    JsonNode(double d) : value(d) {}
    JsonNode(const char* s) : value(std::string(s)) {}
    JsonNode(const std::string& s) : value(s) {}
    JsonNode(std::string&& s) : value(std::move(s)) {}
    JsonNode(std::vector<JsonNode>&& arr) : value(std::move(arr)) {}
    JsonNode(std::map<std::string, JsonNode>&& obj) : value(std::move(obj)) {}
    
    bool isNull() const { return std::holds_alternative<std::nullptr_t>(value); }
    bool isBool() const { return std::holds_alternative<bool>(value); }
    bool isNumber() const { return std::holds_alternative<int64_t>(value) || std::holds_alternative<double>(value); }
    bool isString() const { return std::holds_alternative<std::string>(value); }
    bool isArray() const { return std::holds_alternative<std::vector<JsonNode>>(value); }
    bool isObject() const { return std::holds_alternative<std::map<std::string, JsonNode>>(value); }
    
    bool getBool() const { return std::get<bool>(value); }
    int64_t getInt() const { return std::get<int64_t>(value); }
    double getDouble() const { 
        if (std::holds_alternative<double>(value)) return std::get<double>(value);
        return static_cast<double>(std::get<int64_t>(value));
    }
    const std::string& getString() const { return std::get<std::string>(value); }
    const std::vector<JsonNode>& getArray() const { return std::get<std::vector<JsonNode>>(value); }
    const std::map<std::string, JsonNode>& getObject() const { return std::get<std::map<std::string, JsonNode>>(value); }
    
    bool has(const std::string& key) const {
        if (!isObject()) return false;
        return getObject().count(key) > 0;
    }
    
    const JsonNode& operator[](const std::string& key) const {
        static JsonNode null;
        if (!isObject()) return null;
        auto& obj = getObject();
        auto it = obj.find(key);
        return it != obj.end() ? it->second : null;
    }
    
    const JsonNode& operator[](size_t index) const {
        static JsonNode null;
        if (!isArray()) return null;
        auto& arr = getArray();
        return index < arr.size() ? arr[index] : null;
    }
};

/**
 * @brief Simple JSON parser
 */
class JsonParser {
public:
    static JsonNode parse(const std::string& json) {
        size_t pos = 0;
        return parseValue(json, pos);
    }
    
private:
    static void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || 
               json[pos] == '\n' || json[pos] == '\r')) {
            pos++;
        }
    }
    
    static JsonNode parseValue(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.size()) return JsonNode();
        
        char c = json[pos];
        if (c == 'n') return parseNull(json, pos);
        if (c == 't' || c == 'f') return parseBool(json, pos);
        if (c == '"') return parseString(json, pos);
        if (c == '[') return parseArray(json, pos);
        if (c == '{') return parseObject(json, pos);
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(json, pos);
        return JsonNode();
    }
    
    static JsonNode parseNull(const std::string& json, size_t& pos) {
        pos += 4;
        return JsonNode(nullptr);
    }
    
    static JsonNode parseBool(const std::string& json, size_t& pos) {
        if (json[pos] == 't') {
            pos += 4;
            return JsonNode(true);
        }
        pos += 5;
        return JsonNode(false);
    }
    
    static JsonNode parseString(const std::string& json, size_t& pos) {
        pos++; // skip opening quote
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                pos++;
                switch (json[pos]) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    default: result += json[pos];
                }
            } else {
                result += json[pos];
            }
            pos++;
        }
        pos++; // skip closing quote
        return JsonNode(std::move(result));
    }
    
    static JsonNode parseNumber(const std::string& json, size_t& pos) {
        size_t start = pos;
        bool isFloat = false;
        if (json[pos] == '-') pos++;
        while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') pos++;
        if (pos < json.size() && json[pos] == '.') {
            isFloat = true;
            pos++;
            while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') pos++;
        }
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            isFloat = true;
            pos++;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) pos++;
            while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') pos++;
        }
        std::string numStr = json.substr(start, pos - start);
        if (isFloat) {
            return JsonNode(std::stod(numStr));
        }
        return JsonNode(static_cast<int64_t>(std::stoll(numStr)));
    }
    
    static JsonNode parseArray(const std::string& json, size_t& pos) {
        pos++; // skip [
        std::vector<JsonNode> arr;
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ']') {
            pos++;
            return JsonNode(std::move(arr));
        }
        while (pos < json.size()) {
            arr.push_back(parseValue(json, pos));
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') {
                pos++;
                skipWhitespace(json, pos);
            } else {
                break;
            }
        }
        if (pos < json.size() && json[pos] == ']') pos++;
        return JsonNode(std::move(arr));
    }
    
    static JsonNode parseObject(const std::string& json, size_t& pos) {
        pos++; // skip {
        std::map<std::string, JsonNode> obj;
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') {
            pos++;
            return JsonNode(std::move(obj));
        }
        while (pos < json.size()) {
            skipWhitespace(json, pos);
            auto key = parseString(json, pos);
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ':') pos++;
            skipWhitespace(json, pos);
            obj[key.getString()] = parseValue(json, pos);
            skipWhitespace(json, pos);
            if (pos < json.size() && json[pos] == ',') {
                pos++;
            } else {
                break;
            }
        }
        if (pos < json.size() && json[pos] == '}') pos++;
        return JsonNode(std::move(obj));
    }
};

/**
 * @brief JSON serializer
 */
class JsonSerializer {
public:
    static std::string serialize(const JsonNode& node) {
        std::ostringstream oss;
        serializeValue(node, oss);
        return oss.str();
    }
    
private:
    static void serializeValue(const JsonNode& node, std::ostringstream& oss) {
        if (node.isNull()) {
            oss << "null";
        } else if (node.isBool()) {
            oss << (node.getBool() ? "true" : "false");
        } else if (std::holds_alternative<int64_t>(node.value)) {
            oss << node.getInt();
        } else if (std::holds_alternative<double>(node.value)) {
            oss << node.getDouble();
        } else if (node.isString()) {
            oss << '"';
            for (char c : node.getString()) {
                switch (c) {
                    case '"': oss << "\\\""; break;
                    case '\\': oss << "\\\\"; break;
                    case '\n': oss << "\\n"; break;
                    case '\r': oss << "\\r"; break;
                    case '\t': oss << "\\t"; break;
                    default: oss << c;
                }
            }
            oss << '"';
        } else if (node.isArray()) {
            oss << '[';
            bool first = true;
            for (const auto& elem : node.getArray()) {
                if (!first) oss << ',';
                first = false;
                serializeValue(elem, oss);
            }
            oss << ']';
        } else if (node.isObject()) {
            oss << '{';
            bool first = true;
            for (const auto& [key, val] : node.getObject()) {
                if (!first) oss << ',';
                first = false;
                oss << '"' << key << "\":";
                serializeValue(val, oss);
            }
            oss << '}';
        }
    }
};

// ============================================================================
// LSP TYPES
// ============================================================================

struct Position {
    int line = 0;      // 0-indexed
    int character = 0; // 0-indexed (UTF-16 code units)
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        obj["line"] = line;
        obj["character"] = character;
        return JsonNode(std::move(obj));
    }
    
    static Position fromJson(const JsonNode& node) {
        return Position{
            static_cast<int>(node["line"].getInt()),
            static_cast<int>(node["character"].getInt())
        };
    }
};

struct Range {
    Position start;
    Position end;
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        obj["start"] = start.toJson();
        obj["end"] = end.toJson();
        return JsonNode(std::move(obj));
    }
    
    static Range fromJson(const JsonNode& node) {
        return Range{
            Position::fromJson(node["start"]),
            Position::fromJson(node["end"])
        };
    }
};

struct Location {
    std::string uri;
    Range range;
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        obj["uri"] = uri;
        obj["range"] = range.toJson();
        return JsonNode(std::move(obj));
    }
};

struct TextDocumentIdentifier {
    std::string uri;
    
    static TextDocumentIdentifier fromJson(const JsonNode& node) {
        return TextDocumentIdentifier{node["uri"].getString()};
    }
};

struct TextDocumentPositionParams {
    TextDocumentIdentifier textDocument;
    Position position;
    
    static TextDocumentPositionParams fromJson(const JsonNode& node) {
        return TextDocumentPositionParams{
            TextDocumentIdentifier::fromJson(node["textDocument"]),
            Position::fromJson(node["position"])
        };
    }
};

enum class DiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

struct Diagnostic {
    Range range;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string code;
    std::string source;
    std::string message;
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        obj["range"] = range.toJson();
        obj["severity"] = static_cast<int>(severity);
        if (!code.empty()) obj["code"] = code;
        if (!source.empty()) obj["source"] = source;
        obj["message"] = message;
        return JsonNode(std::move(obj));
    }
};

enum class CompletionItemKind {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25
};

struct CompletionItem {
    std::string label;
    CompletionItemKind kind = CompletionItemKind::Text;
    std::string detail;
    std::string documentation;
    std::string insertText;
    bool deprecated = false;
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        obj["label"] = label;
        obj["kind"] = static_cast<int>(kind);
        if (!detail.empty()) obj["detail"] = detail;
        if (!documentation.empty()) obj["documentation"] = documentation;
        if (!insertText.empty()) obj["insertText"] = insertText;
        if (deprecated) obj["deprecated"] = true;
        return JsonNode(std::move(obj));
    }
};

struct Hover {
    std::string contents; // Markdown
    std::optional<Range> range;
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        std::map<std::string, JsonNode> contentsObj;
        contentsObj["kind"] = "markdown";
        contentsObj["value"] = contents;
        obj["contents"] = JsonNode(std::move(contentsObj));
        if (range.has_value()) {
            obj["range"] = range->toJson();
        }
        return JsonNode(std::move(obj));
    }
};

struct SignatureInformation {
    std::string label;
    std::string documentation;
    std::vector<std::pair<std::string, std::string>> parameters; // label, doc
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        obj["label"] = label;
        if (!documentation.empty()) obj["documentation"] = documentation;
        
        std::vector<JsonNode> params;
        for (const auto& [lbl, doc] : parameters) {
            std::map<std::string, JsonNode> param;
            param["label"] = lbl;
            if (!doc.empty()) param["documentation"] = doc;
            params.push_back(JsonNode(std::move(param)));
        }
        obj["parameters"] = JsonNode(std::move(params));
        return JsonNode(std::move(obj));
    }
};

struct SignatureHelp {
    std::vector<SignatureInformation> signatures;
    int activeSignature = 0;
    int activeParameter = 0;
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        std::vector<JsonNode> sigs;
        for (const auto& sig : signatures) {
            sigs.push_back(sig.toJson());
        }
        obj["signatures"] = JsonNode(std::move(sigs));
        obj["activeSignature"] = activeSignature;
        obj["activeParameter"] = activeParameter;
        return JsonNode(std::move(obj));
    }
};

struct TextEdit {
    Range range;
    std::string newText;
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        obj["range"] = range.toJson();
        obj["newText"] = newText;
        return JsonNode(std::move(obj));
    }
};

struct WorkspaceEdit {
    std::map<std::string, std::vector<TextEdit>> changes; // uri -> edits
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        std::map<std::string, JsonNode> changesObj;
        for (const auto& [uri, edits] : changes) {
            std::vector<JsonNode> editArr;
            for (const auto& edit : edits) {
                editArr.push_back(edit.toJson());
            }
            changesObj[uri] = JsonNode(std::move(editArr));
        }
        obj["changes"] = JsonNode(std::move(changesObj));
        return JsonNode(std::move(obj));
    }
};

struct SymbolInformation {
    std::string name;
    int kind; // SymbolKind
    Location location;
    std::string containerName;
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        obj["name"] = name;
        obj["kind"] = kind;
        obj["location"] = location.toJson();
        if (!containerName.empty()) obj["containerName"] = containerName;
        return JsonNode(std::move(obj));
    }
};

// ============================================================================
// DOCUMENT MANAGEMENT
// ============================================================================

/**
 * @brief Manages open text documents
 */
class DocumentManager {
public:
    struct Document {
        std::string uri;
        std::string languageId;
        int version;
        std::string content;
        std::vector<std::string> lines;
        
        void updateLines() {
            lines.clear();
            std::istringstream iss(content);
            std::string line;
            while (std::getline(iss, line)) {
                lines.push_back(line);
            }
        }
        
        std::string getLine(int lineNum) const {
            if (lineNum >= 0 && lineNum < static_cast<int>(lines.size())) {
                return lines[lineNum];
            }
            return "";
        }
        
        char getCharAt(const Position& pos) const {
            if (pos.line >= 0 && pos.line < static_cast<int>(lines.size())) {
                const auto& line = lines[pos.line];
                if (pos.character >= 0 && pos.character < static_cast<int>(line.size())) {
                    return line[pos.character];
                }
            }
            return '\0';
        }
        
        std::string getWordAt(const Position& pos) const {
            if (pos.line < 0 || pos.line >= static_cast<int>(lines.size())) return "";
            const auto& line = lines[pos.line];
            if (pos.character < 0 || pos.character >= static_cast<int>(line.size())) return "";
            
            // Find word boundaries
            int start = pos.character;
            int end = pos.character;
            
            while (start > 0 && isWordChar(line[start - 1])) start--;
            while (end < static_cast<int>(line.size()) && isWordChar(line[end])) end++;
            
            return line.substr(start, end - start);
        }
        
    private:
        static bool isWordChar(char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_';
        }
    };
    
    void openDocument(const std::string& uri, const std::string& languageId, 
                      int version, const std::string& content) {
        std::lock_guard<std::mutex> lock(mutex_);
        Document doc;
        doc.uri = uri;
        doc.languageId = languageId;
        doc.version = version;
        doc.content = content;
        doc.updateLines();
        documents_[uri] = std::move(doc);
    }
    
    void updateDocument(const std::string& uri, int version, const std::string& content) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = documents_.find(uri);
        if (it != documents_.end()) {
            it->second.version = version;
            it->second.content = content;
            it->second.updateLines();
        }
    }
    
    void closeDocument(const std::string& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        documents_.erase(uri);
    }
    
    std::optional<Document> getDocument(const std::string& uri) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = documents_.find(uri);
        if (it != documents_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    std::vector<std::string> getOpenDocumentUris() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> uris;
        for (const auto& [uri, _] : documents_) {
            uris.push_back(uri);
        }
        return uris;
    }
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Document> documents_;
};

// ============================================================================
// SYMBOL DATABASE
// ============================================================================

/**
 * @brief Stores symbols for go-to-definition and references
 */
class SymbolDatabase {
public:
    struct Symbol {
        std::string name;
        std::string kind; // "function", "variable", "class", etc.
        std::string uri;
        Range range;
        Range selectionRange;
        std::string documentation;
        std::string signature;
        std::vector<Location> references;
    };
    
    void addSymbol(const Symbol& symbol) {
        std::lock_guard<std::mutex> lock(mutex_);
        symbols_[symbol.name].push_back(symbol);
        symbolsByUri_[symbol.uri].push_back(symbol.name);
    }
    
    void clearSymbolsForUri(const std::string& uri) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = symbolsByUri_.find(uri);
        if (it != symbolsByUri_.end()) {
            for (const auto& name : it->second) {
                auto& syms = symbols_[name];
                syms.erase(
                    std::remove_if(syms.begin(), syms.end(),
                        [&uri](const Symbol& s) { return s.uri == uri; }),
                    syms.end());
            }
            symbolsByUri_.erase(it);
        }
    }
    
    std::vector<Symbol> findSymbol(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = symbols_.find(name);
        if (it != symbols_.end()) {
            return it->second;
        }
        return {};
    }
    
    std::vector<Symbol> findSymbolsMatching(const std::string& prefix) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Symbol> result;
        for (const auto& [name, syms] : symbols_) {
            if (name.find(prefix) == 0 || name.find(prefix) != std::string::npos) {
                result.insert(result.end(), syms.begin(), syms.end());
            }
        }
        return result;
    }
    
    std::vector<Symbol> getSymbolsInUri(const std::string& uri) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Symbol> result;
        auto it = symbolsByUri_.find(uri);
        if (it != symbolsByUri_.end()) {
            for (const auto& name : it->second) {
                auto symIt = symbols_.find(name);
                if (symIt != symbols_.end()) {
                    for (const auto& sym : symIt->second) {
                        if (sym.uri == uri) {
                            result.push_back(sym);
                        }
                    }
                }
            }
        }
        return result;
    }
    
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<Symbol>> symbols_;
    std::unordered_map<std::string, std::vector<std::string>> symbolsByUri_;
};

// ============================================================================
// LSP MESSAGE HANDLING
// ============================================================================

/**
 * @brief JSON-RPC message types
 */
struct RequestMessage {
    std::string jsonrpc = "2.0";
    std::variant<int64_t, std::string> id;
    std::string method;
    JsonNode params;
};

struct ResponseMessage {
    std::string jsonrpc = "2.0";
    std::variant<int64_t, std::string> id;
    std::optional<JsonNode> result;
    std::optional<JsonNode> error;
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        obj["jsonrpc"] = jsonrpc;
        if (std::holds_alternative<int64_t>(id)) {
            obj["id"] = std::get<int64_t>(id);
        } else {
            obj["id"] = std::get<std::string>(id);
        }
        if (result.has_value()) {
            obj["result"] = *result;
        }
        if (error.has_value()) {
            obj["error"] = *error;
        }
        return JsonNode(std::move(obj));
    }
};

struct NotificationMessage {
    std::string jsonrpc = "2.0";
    std::string method;
    JsonNode params;
    
    JsonNode toJson() const {
        std::map<std::string, JsonNode> obj;
        obj["jsonrpc"] = jsonrpc;
        obj["method"] = method;
        obj["params"] = params;
        return JsonNode(std::move(obj));
    }
};

// ============================================================================
// LSP SERVER
// ============================================================================

/**
 * @brief Complete LSP server implementation
 */
class LanguageServer {
public:
    using RequestHandler = std::function<JsonNode(const JsonNode&)>;
    using NotificationHandler = std::function<void(const JsonNode&)>;
    
    LanguageServer()
        : running_(false)
        , initialized_(false)
    {
        registerDefaultHandlers();
    }
    
    ~LanguageServer() {
        stop();
    }
    
    // Start the server (reads from stdin, writes to stdout)
    void run() {
        running_ = true;
        
        while (running_) {
            auto message = readMessage();
            if (!message.empty()) {
                processMessage(message);
            }
        }
    }
    
    void stop() {
        running_ = false;
    }
    
    // Register custom handlers
    void registerRequestHandler(const std::string& method, RequestHandler handler) {
        requestHandlers_[method] = std::move(handler);
    }
    
    void registerNotificationHandler(const std::string& method, NotificationHandler handler) {
        notificationHandlers_[method] = std::move(handler);
    }
    
    // Send notification to client
    void sendNotification(const std::string& method, const JsonNode& params) {
        NotificationMessage msg;
        msg.method = method;
        msg.params = params;
        writeMessage(JsonSerializer::serialize(msg.toJson()));
    }
    
    // Publish diagnostics
    void publishDiagnostics(const std::string& uri, const std::vector<Diagnostic>& diagnostics) {
        std::map<std::string, JsonNode> params;
        params["uri"] = uri;
        std::vector<JsonNode> diagArr;
        for (const auto& diag : diagnostics) {
            diagArr.push_back(diag.toJson());
        }
        params["diagnostics"] = JsonNode(std::move(diagArr));
        sendNotification("textDocument/publishDiagnostics", JsonNode(std::move(params)));
    }
    
    // Access managers
    DocumentManager& getDocumentManager() { return documentManager_; }
    SymbolDatabase& getSymbolDatabase() { return symbolDatabase_; }
    
private:
    void registerDefaultHandlers() {
        // Initialize
        registerRequestHandler("initialize", [this](const JsonNode& params) {
            return handleInitialize(params);
        });
        
        // Initialized notification
        registerNotificationHandler("initialized", [this](const JsonNode&) {
            initialized_ = true;
        });
        
        // Shutdown
        registerRequestHandler("shutdown", [this](const JsonNode&) {
            return JsonNode(nullptr);
        });
        
        // Exit
        registerNotificationHandler("exit", [this](const JsonNode&) {
            running_ = false;
        });
        
        // Text document sync
        registerNotificationHandler("textDocument/didOpen", [this](const JsonNode& params) {
            handleDidOpen(params);
        });
        
        registerNotificationHandler("textDocument/didChange", [this](const JsonNode& params) {
            handleDidChange(params);
        });
        
        registerNotificationHandler("textDocument/didClose", [this](const JsonNode& params) {
            handleDidClose(params);
        });
        
        registerNotificationHandler("textDocument/didSave", [this](const JsonNode& params) {
            handleDidSave(params);
        });
        
        // Completion
        registerRequestHandler("textDocument/completion", [this](const JsonNode& params) {
            return handleCompletion(params);
        });
        
        // Hover
        registerRequestHandler("textDocument/hover", [this](const JsonNode& params) {
            return handleHover(params);
        });
        
        // Go to definition
        registerRequestHandler("textDocument/definition", [this](const JsonNode& params) {
            return handleDefinition(params);
        });
        
        // Find references
        registerRequestHandler("textDocument/references", [this](const JsonNode& params) {
            return handleReferences(params);
        });
        
        // Document symbols
        registerRequestHandler("textDocument/documentSymbol", [this](const JsonNode& params) {
            return handleDocumentSymbol(params);
        });
        
        // Workspace symbols
        registerRequestHandler("workspace/symbol", [this](const JsonNode& params) {
            return handleWorkspaceSymbol(params);
        });
        
        // Formatting
        registerRequestHandler("textDocument/formatting", [this](const JsonNode& params) {
            return handleFormatting(params);
        });
        
        // Signature help
        registerRequestHandler("textDocument/signatureHelp", [this](const JsonNode& params) {
            return handleSignatureHelp(params);
        });
        
        // Rename
        registerRequestHandler("textDocument/rename", [this](const JsonNode& params) {
            return handleRename(params);
        });
    }
    
    JsonNode handleInitialize(const JsonNode& params) {
        std::map<std::string, JsonNode> capabilities;
        
        // Text document sync
        std::map<std::string, JsonNode> textDocSync;
        textDocSync["openClose"] = true;
        textDocSync["change"] = 1; // Full sync
        textDocSync["save"] = true;
        capabilities["textDocumentSync"] = JsonNode(std::move(textDocSync));
        
        // Completion
        std::map<std::string, JsonNode> completionProvider;
        completionProvider["resolveProvider"] = true;
        std::vector<JsonNode> triggerChars;
        triggerChars.push_back(JsonNode("."));
        triggerChars.push_back(JsonNode(":"));
        completionProvider["triggerCharacters"] = JsonNode(std::move(triggerChars));
        capabilities["completionProvider"] = JsonNode(std::move(completionProvider));
        
        // Hover
        capabilities["hoverProvider"] = true;
        
        // Definition
        capabilities["definitionProvider"] = true;
        
        // References
        capabilities["referencesProvider"] = true;
        
        // Document symbols
        capabilities["documentSymbolProvider"] = true;
        
        // Workspace symbols
        capabilities["workspaceSymbolProvider"] = true;
        
        // Formatting
        capabilities["documentFormattingProvider"] = true;
        capabilities["documentRangeFormattingProvider"] = true;
        
        // Signature help
        std::map<std::string, JsonNode> sigHelpProvider;
        std::vector<JsonNode> sigTriggerChars;
        sigTriggerChars.push_back(JsonNode("("));
        sigTriggerChars.push_back(JsonNode(","));
        sigHelpProvider["triggerCharacters"] = JsonNode(std::move(sigTriggerChars));
        capabilities["signatureHelpProvider"] = JsonNode(std::move(sigHelpProvider));
        
        // Rename
        capabilities["renameProvider"] = true;
        
        std::map<std::string, JsonNode> result;
        result["capabilities"] = JsonNode(std::move(capabilities));
        
        std::map<std::string, JsonNode> serverInfo;
        serverInfo["name"] = "RawrXD Language Server";
        serverInfo["version"] = "1.0.0";
        result["serverInfo"] = JsonNode(std::move(serverInfo));
        
        // Store workspace info
        if (params.has("rootUri")) {
            workspaceRoot_ = params["rootUri"].getString();
        }
        
        return JsonNode(std::move(result));
    }
    
    void handleDidOpen(const JsonNode& params) {
        auto textDoc = params["textDocument"];
        documentManager_.openDocument(
            textDoc["uri"].getString(),
            textDoc["languageId"].getString(),
            static_cast<int>(textDoc["version"].getInt()),
            textDoc["text"].getString()
        );
        
        // Index the document
        indexDocument(textDoc["uri"].getString());
    }
    
    void handleDidChange(const JsonNode& params) {
        auto textDoc = params["textDocument"];
        auto contentChanges = params["contentChanges"].getArray();
        
        if (!contentChanges.empty()) {
            // Full sync - just use the new text
            documentManager_.updateDocument(
                textDoc["uri"].getString(),
                static_cast<int>(textDoc["version"].getInt()),
                contentChanges[0]["text"].getString()
            );
            
            // Re-index
            indexDocument(textDoc["uri"].getString());
        }
    }
    
    void handleDidClose(const JsonNode& params) {
        auto uri = params["textDocument"]["uri"].getString();
        documentManager_.closeDocument(uri);
        symbolDatabase_.clearSymbolsForUri(uri);
    }
    
    void handleDidSave(const JsonNode& params) {
        auto uri = params["textDocument"]["uri"].getString();
        // Could trigger full analysis here
        validateDocument(uri);
    }
    
    JsonNode handleCompletion(const JsonNode& params) {
        auto tdpp = TextDocumentPositionParams::fromJson(params);
        auto doc = documentManager_.getDocument(tdpp.textDocument.uri);
        
        std::vector<JsonNode> items;
        
        if (doc.has_value()) {
            // Get word at cursor for filtering
            std::string prefix = doc->getWordAt(tdpp.position);
            
            // Add symbols matching prefix
            auto symbols = symbolDatabase_.findSymbolsMatching(prefix);
            for (const auto& sym : symbols) {
                CompletionItem item;
                item.label = sym.name;
                item.detail = sym.signature;
                item.documentation = sym.documentation;
                
                if (sym.kind == "function") {
                    item.kind = CompletionItemKind::Function;
                } else if (sym.kind == "variable") {
                    item.kind = CompletionItemKind::Variable;
                } else if (sym.kind == "class") {
                    item.kind = CompletionItemKind::Class;
                } else if (sym.kind == "keyword") {
                    item.kind = CompletionItemKind::Keyword;
                }
                
                items.push_back(item.toJson());
            }
            
            // Add keywords
            static const std::vector<std::string> keywords = {
                "if", "else", "while", "for", "return", "break", "continue",
                "function", "var", "const", "let", "class", "struct", "enum",
                "public", "private", "protected", "static", "virtual", "override"
            };
            
            for (const auto& kw : keywords) {
                if (prefix.empty() || kw.find(prefix) == 0) {
                    CompletionItem item;
                    item.label = kw;
                    item.kind = CompletionItemKind::Keyword;
                    items.push_back(item.toJson());
                }
            }
        }
        
        return JsonNode(std::move(items));
    }
    
    JsonNode handleHover(const JsonNode& params) {
        auto tdpp = TextDocumentPositionParams::fromJson(params);
        auto doc = documentManager_.getDocument(tdpp.textDocument.uri);
        
        if (!doc.has_value()) {
            return JsonNode(nullptr);
        }
        
        std::string word = doc->getWordAt(tdpp.position);
        if (word.empty()) {
            return JsonNode(nullptr);
        }
        
        auto symbols = symbolDatabase_.findSymbol(word);
        if (symbols.empty()) {
            return JsonNode(nullptr);
        }
        
        const auto& sym = symbols[0];
        
        std::ostringstream oss;
        oss << "```\n" << sym.signature << "\n```\n\n";
        if (!sym.documentation.empty()) {
            oss << sym.documentation;
        }
        
        Hover hover;
        hover.contents = oss.str();
        return hover.toJson();
    }
    
    JsonNode handleDefinition(const JsonNode& params) {
        auto tdpp = TextDocumentPositionParams::fromJson(params);
        auto doc = documentManager_.getDocument(tdpp.textDocument.uri);
        
        if (!doc.has_value()) {
            return JsonNode(nullptr);
        }
        
        std::string word = doc->getWordAt(tdpp.position);
        auto symbols = symbolDatabase_.findSymbol(word);
        
        if (symbols.empty()) {
            return JsonNode(nullptr);
        }
        
        // Return first definition location
        const auto& sym = symbols[0];
        Location loc;
        loc.uri = sym.uri;
        loc.range = sym.range;
        return loc.toJson();
    }
    
    JsonNode handleReferences(const JsonNode& params) {
        auto tdpp = TextDocumentPositionParams::fromJson(params);
        auto doc = documentManager_.getDocument(tdpp.textDocument.uri);
        
        std::vector<JsonNode> locations;
        
        if (doc.has_value()) {
            std::string word = doc->getWordAt(tdpp.position);
            auto symbols = symbolDatabase_.findSymbol(word);
            
            for (const auto& sym : symbols) {
                // Add definition
                Location defLoc;
                defLoc.uri = sym.uri;
                defLoc.range = sym.range;
                locations.push_back(defLoc.toJson());
                
                // Add references
                for (const auto& ref : sym.references) {
                    locations.push_back(ref.toJson());
                }
            }
        }
        
        return JsonNode(std::move(locations));
    }
    
    JsonNode handleDocumentSymbol(const JsonNode& params) {
        auto uri = params["textDocument"]["uri"].getString();
        auto symbols = symbolDatabase_.getSymbolsInUri(uri);
        
        std::vector<JsonNode> result;
        for (const auto& sym : symbols) {
            SymbolInformation info;
            info.name = sym.name;
            info.location.uri = sym.uri;
            info.location.range = sym.range;
            
            // Map kind string to SymbolKind enum
            if (sym.kind == "function") info.kind = 12;  // Function
            else if (sym.kind == "variable") info.kind = 13;  // Variable
            else if (sym.kind == "class") info.kind = 5;  // Class
            else if (sym.kind == "struct") info.kind = 23;  // Struct
            else info.kind = 1;  // File
            
            result.push_back(info.toJson());
        }
        
        return JsonNode(std::move(result));
    }
    
    JsonNode handleWorkspaceSymbol(const JsonNode& params) {
        std::string query = params["query"].getString();
        auto symbols = symbolDatabase_.findSymbolsMatching(query);
        
        std::vector<JsonNode> result;
        for (const auto& sym : symbols) {
            SymbolInformation info;
            info.name = sym.name;
            info.location.uri = sym.uri;
            info.location.range = sym.range;
            
            if (sym.kind == "function") info.kind = 12;
            else if (sym.kind == "variable") info.kind = 13;
            else if (sym.kind == "class") info.kind = 5;
            else info.kind = 1;
            
            result.push_back(info.toJson());
        }
        
        return JsonNode(std::move(result));
    }
    
    JsonNode handleFormatting(const JsonNode& params) {
        auto uri = params["textDocument"]["uri"].getString();
        auto doc = documentManager_.getDocument(uri);
        
        std::vector<JsonNode> edits;
        
        if (doc.has_value()) {
            // Simple formatting: fix indentation
            std::string formatted = formatDocument(doc->content);
            
            TextEdit edit;
            edit.range.start = {0, 0};
            edit.range.end = {static_cast<int>(doc->lines.size()), 0};
            edit.newText = formatted;
            edits.push_back(edit.toJson());
        }
        
        return JsonNode(std::move(edits));
    }
    
    JsonNode handleSignatureHelp(const JsonNode& params) {
        auto tdpp = TextDocumentPositionParams::fromJson(params);
        auto doc = documentManager_.getDocument(tdpp.textDocument.uri);
        
        if (!doc.has_value()) {
            return JsonNode(nullptr);
        }
        
        // Find function name before cursor
        std::string line = doc->getLine(tdpp.position.line);
        int parenPos = -1;
        int parenCount = 0;
        
        for (int i = tdpp.position.character - 1; i >= 0; --i) {
            if (line[i] == ')') parenCount++;
            else if (line[i] == '(') {
                if (parenCount == 0) {
                    parenPos = i;
                    break;
                }
                parenCount--;
            }
        }
        
        if (parenPos < 0) {
            return JsonNode(nullptr);
        }
        
        // Get function name
        int nameEnd = parenPos;
        int nameStart = nameEnd - 1;
        while (nameStart >= 0 && (isalnum(line[nameStart]) || line[nameStart] == '_')) {
            nameStart--;
        }
        nameStart++;
        
        std::string funcName = line.substr(nameStart, nameEnd - nameStart);
        auto symbols = symbolDatabase_.findSymbol(funcName);
        
        if (symbols.empty()) {
            return JsonNode(nullptr);
        }
        
        // Count commas to determine active parameter
        int commaCount = 0;
        for (int i = parenPos + 1; i < tdpp.position.character; ++i) {
            if (line[i] == ',') commaCount++;
        }
        
        SignatureHelp help;
        for (const auto& sym : symbols) {
            if (sym.kind == "function") {
                SignatureInformation sig;
                sig.label = sym.signature;
                sig.documentation = sym.documentation;
                // Parse parameters from signature (simplified)
                help.signatures.push_back(sig);
            }
        }
        
        help.activeSignature = 0;
        help.activeParameter = commaCount;
        
        return help.toJson();
    }
    
    JsonNode handleRename(const JsonNode& params) {
        auto tdpp = TextDocumentPositionParams::fromJson(params);
        std::string newName = params["newName"].getString();
        auto doc = documentManager_.getDocument(tdpp.textDocument.uri);
        
        WorkspaceEdit edit;
        
        if (doc.has_value()) {
            std::string oldName = doc->getWordAt(tdpp.position);
            auto symbols = symbolDatabase_.findSymbol(oldName);
            
            for (const auto& sym : symbols) {
                // Add edit at definition
                TextEdit te;
                te.range = sym.selectionRange;
                te.newText = newName;
                edit.changes[sym.uri].push_back(te);
                
                // Add edits at references
                for (const auto& ref : sym.references) {
                    TextEdit refEdit;
                    refEdit.range = ref.range;
                    refEdit.newText = newName;
                    edit.changes[ref.uri].push_back(refEdit);
                }
            }
        }
        
        return edit.toJson();
    }
    
    // Helper methods
    void indexDocument(const std::string& uri) {
        auto doc = documentManager_.getDocument(uri);
        if (!doc.has_value()) return;
        
        symbolDatabase_.clearSymbolsForUri(uri);
        
        // Simple symbol extraction (in real implementation, use parser)
        // Look for function definitions, variable declarations, etc.
        for (int i = 0; i < static_cast<int>(doc->lines.size()); ++i) {
            const auto& line = doc->lines[i];
            
            // Look for function definitions: "function name(" or "name("
            size_t funcPos = line.find("function ");
            if (funcPos != std::string::npos) {
                size_t nameStart = funcPos + 9;
                while (nameStart < line.size() && line[nameStart] == ' ') nameStart++;
                size_t nameEnd = nameStart;
                while (nameEnd < line.size() && (isalnum(line[nameEnd]) || line[nameEnd] == '_')) nameEnd++;
                
                if (nameEnd > nameStart) {
                    SymbolDatabase::Symbol sym;
                    sym.name = line.substr(nameStart, nameEnd - nameStart);
                    sym.kind = "function";
                    sym.uri = uri;
                    sym.range = {{i, static_cast<int>(funcPos)}, {i, static_cast<int>(line.size())}};
                    sym.selectionRange = {{i, static_cast<int>(nameStart)}, {i, static_cast<int>(nameEnd)}};
                    
                    // Extract signature
                    size_t parenEnd = line.find(')', nameEnd);
                    if (parenEnd != std::string::npos) {
                        sym.signature = line.substr(funcPos, parenEnd - funcPos + 1);
                    }
                    
                    symbolDatabase_.addSymbol(sym);
                }
            }
            
            // Look for variable declarations: "var name", "let name", "const name"
            for (const char* keyword : {"var ", "let ", "const "}) {
                size_t varPos = line.find(keyword);
                if (varPos != std::string::npos) {
                    size_t nameStart = varPos + strlen(keyword);
                    while (nameStart < line.size() && line[nameStart] == ' ') nameStart++;
                    size_t nameEnd = nameStart;
                    while (nameEnd < line.size() && (isalnum(line[nameEnd]) || line[nameEnd] == '_')) nameEnd++;
                    
                    if (nameEnd > nameStart) {
                        SymbolDatabase::Symbol sym;
                        sym.name = line.substr(nameStart, nameEnd - nameStart);
                        sym.kind = "variable";
                        sym.uri = uri;
                        sym.range = {{i, static_cast<int>(varPos)}, {i, static_cast<int>(line.size())}};
                        sym.selectionRange = {{i, static_cast<int>(nameStart)}, {i, static_cast<int>(nameEnd)}};
                        symbolDatabase_.addSymbol(sym);
                    }
                }
            }
        }
    }
    
    void validateDocument(const std::string& uri) {
        auto doc = documentManager_.getDocument(uri);
        if (!doc.has_value()) return;
        
        std::vector<Diagnostic> diagnostics;
        
        // Simple validation (in real implementation, use full parser/analyzer)
        for (int i = 0; i < static_cast<int>(doc->lines.size()); ++i) {
            const auto& line = doc->lines[i];
            
            // Check for unclosed strings
            int quoteCount = 0;
            for (char c : line) {
                if (c == '"') quoteCount++;
            }
            if (quoteCount % 2 != 0) {
                Diagnostic diag;
                diag.range = {{i, 0}, {i, static_cast<int>(line.size())}};
                diag.severity = DiagnosticSeverity::Error;
                diag.message = "Unclosed string literal";
                diag.source = "rawrxd";
                diagnostics.push_back(diag);
            }
            
            // Check for TODO comments
            size_t todoPos = line.find("TODO");
            if (todoPos != std::string::npos) {
                Diagnostic diag;
                diag.range = {{i, static_cast<int>(todoPos)}, {i, static_cast<int>(todoPos + 4)}};
                diag.severity = DiagnosticSeverity::Information;
                diag.message = "TODO comment found";
                diag.source = "rawrxd";
                diagnostics.push_back(diag);
            }
        }
        
        publishDiagnostics(uri, diagnostics);
    }
    
    std::string formatDocument(const std::string& content) {
        std::ostringstream oss;
        std::istringstream iss(content);
        std::string line;
        int indentLevel = 0;
        
        while (std::getline(iss, line)) {
            // Trim leading whitespace
            size_t start = 0;
            while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
                start++;
            }
            line = line.substr(start);
            
            // Adjust indent for closing braces
            if (!line.empty() && (line[0] == '}' || line[0] == ']' || line[0] == ')')) {
                indentLevel = std::max(0, indentLevel - 1);
            }
            
            // Write indented line
            for (int i = 0; i < indentLevel; ++i) {
                oss << "    ";
            }
            oss << line << "\n";
            
            // Adjust indent for opening braces
            for (char c : line) {
                if (c == '{' || c == '[' || c == '(') indentLevel++;
                else if (c == '}' || c == ']' || c == ')') indentLevel = std::max(0, indentLevel - 1);
            }
        }
        
        return oss.str();
    }
    
    std::string readMessage() {
        std::string line;
        int contentLength = -1;
        
        // Read headers
        while (std::getline(std::cin, line)) {
            if (line.empty() || line == "\r") {
                break;
            }
            
            if (line.back() == '\r') {
                line.pop_back();
            }
            
            if (line.find("Content-Length: ") == 0) {
                contentLength = std::stoi(line.substr(16));
            }
        }
        
        if (contentLength < 0) {
            return "";
        }
        
        // Read content
        std::string content(contentLength, '\0');
        std::cin.read(&content[0], contentLength);
        
        return content;
    }
    
    void writeMessage(const std::string& content) {
        std::lock_guard<std::mutex> lock(writeMutex_);
        std::cout << "Content-Length: " << content.size() << "\r\n\r\n" << content;
        std::cout.flush();
    }
    
    void processMessage(const std::string& content) {
        auto json = JsonParser::parse(content);
        
        if (json.has("id")) {
            // Request
            std::variant<int64_t, std::string> id;
            if (json["id"].isString()) {
                id = json["id"].getString();
            } else {
                id = json["id"].getInt();
            }
            
            std::string method = json["method"].getString();
            
            ResponseMessage response;
            response.id = id;
            
            auto it = requestHandlers_.find(method);
            if (it != requestHandlers_.end()) {
                try {
                    response.result = it->second(json["params"]);
                } catch (const std::exception& e) {
                    std::map<std::string, JsonNode> error;
                    error["code"] = -32603;
                    error["message"] = std::string("Internal error: ") + e.what();
                    response.error = JsonNode(std::move(error));
                }
            } else {
                std::map<std::string, JsonNode> error;
                error["code"] = -32601;
                error["message"] = "Method not found: " + method;
                response.error = JsonNode(std::move(error));
            }
            
            writeMessage(JsonSerializer::serialize(response.toJson()));
        } else {
            // Notification
            std::string method = json["method"].getString();
            
            auto it = notificationHandlers_.find(method);
            if (it != notificationHandlers_.end()) {
                try {
                    it->second(json["params"]);
                } catch (...) {
                    // Ignore notification errors
                }
            }
        }
    }
    
    std::atomic<bool> running_;
    bool initialized_;
    std::string workspaceRoot_;
    
    std::mutex writeMutex_;
    
    DocumentManager documentManager_;
    SymbolDatabase symbolDatabase_;
    
    std::unordered_map<std::string, RequestHandler> requestHandlers_;
    std::unordered_map<std::string, NotificationHandler> notificationHandlers_;
};

} // namespace LSP
} // namespace Compiler
} // namespace RawrXD
