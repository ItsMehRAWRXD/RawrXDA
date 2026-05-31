/**
 * RawrXD Native AI Integration Implementation
 * 
 * Complete implementation of AI integration layer for CLI and GUI modes.
 * Zero external dependencies - pure Win32/WinSock2.
 */

#include "native_ai_integration.hpp"
#include <cctype>
#include <cstring>
#include <direct.h>
#include <io.h>
#include <filesystem>
#include <regex>

namespace rawrxd {

// ============================================================================
// JSON Implementation
// ============================================================================
Json::Json(std::initializer_list<std::pair<const char*, Json>> list) {
    type = OBJ;
    for (auto& kv : list) o[kv.first] = std::move(kv.second);
}

Json& Json::operator[](const std::string& k) {
    if (type != OBJ) { type = OBJ; o.clear(); }
    return o[k];
}

const Json& Json::operator[](const std::string& k) const {
    static Json nullVal;
    if (type != OBJ) return nullVal;
    auto it = o.find(k);
    return it != o.end() ? it->second : nullVal;
}

Json& Json::operator[](size_t i) {
    if (type != ARR) { type = ARR; a.clear(); }
    if (i >= a.size()) a.resize(i + 1);
    return a[i];
}

bool Json::has(const std::string& k) const {
    return type == OBJ && o.find(k) != o.end();
}

void Json::skip(const char*& p) {
    while (*p && strchr(" \t\r\n", *p)) ++p;
}

std::string Json::parse_str(const char*& p) {
    if (*p != '"') return "";
    ++p;
    std::string r;
    while (*p && *p != '"') {
        if (*p == '\\') {
            ++p;
            switch (*p) {
                case 'n': r += '\n'; break;
                case 't': r += '\t'; break;
                case 'r': r += '\r'; break;
                case '\\': r += '\\'; break;
                case '"': r += '"'; break;
                case '/': r += '/'; break;
                case 'b': r += '\b'; break;
                case 'f': r += '\f'; break;
                case 'u': {
                    // Unicode escape
                    ++p;
                    if (*p && *(p+1) && *(p+2) && *(p+3)) {
                        char hex[5] = { *p, *(p+1), *(p+2), *(p+3), 0 };
                        int codepoint = strtol(hex, nullptr, 16);
                        if (codepoint < 0x80) {
                            r += (char)codepoint;
                        } else if (codepoint < 0x800) {
                            r += (char)(0xC0 | (codepoint >> 6));
                            r += (char)(0x80 | (codepoint & 0x3F));
                        } else {
                            r += (char)(0xE0 | (codepoint >> 12));
                            r += (char)(0x80 | ((codepoint >> 6) & 0x3F));
                            r += (char)(0x80 | (codepoint & 0x3F));
                        }
                        p += 3;
                    }
                    break;
                }
                default: r += *p;
            }
        } else r += *p;
        ++p;
    }
    if (*p == '"') ++p;
    return r;
}

double Json::parse_num(const char*& p) {
    bool neg = *p == '-';
    if (neg) ++p;
    double v = 0;
    while (*p && isdigit(*p)) v = v * 10 + (*p++ - '0');
    if (*p == '.') {
        ++p;
        double f = 0.1;
        while (*p && isdigit(*p)) { v += (*p++ - '0') * f; f *= 0.1; }
    }
    if (*p == 'e' || *p == 'E') {
        ++p;
        bool expNeg = *p == '-';
        if (expNeg || *p == '+') ++p;
        int exp = 0;
        while (*p && isdigit(*p)) exp = exp * 10 + (*p++ - '0');
        v *= pow(10.0, expNeg ? -exp : exp);
    }
    return neg ? -v : v;
}

Json Json::parse_val(const char*& p) {
    skip(p);
    if (*p == '{') {
        ++p;
        Json obj;
        obj.type = OBJ;
        skip(p);
        while (*p && *p != '}') {
            std::string k = parse_str(p);
            skip(p);
            if (*p == ':') ++p;
            obj[k] = parse_val(p);
            skip(p);
            if (*p == ',') ++p;
            skip(p);
        }
        if (*p == '}') ++p;
        return obj;
    } else if (*p == '[') {
        ++p;
        Json arr;
        arr.type = ARR;
        skip(p);
        size_t i = 0;
        while (*p && *p != ']') {
            arr[i++] = parse_val(p);
            skip(p);
            if (*p == ',') ++p;
            skip(p);
        }
        if (*p == ']') ++p;
        return arr;
    } else if (*p == '"') {
        return Json(parse_str(p));
    } else if (*p == 't' && strncmp(p, "true", 4) == 0) {
        p += 4;
        return Json(true);
    } else if (*p == 'f' && strncmp(p, "false", 5) == 0) {
        p += 5;
        return Json(false);
    } else if (*p == 'n' && strncmp(p, "null", 4) == 0) {
        p += 4;
        return Json();
    } else if (*p == '-' || isdigit(*p)) {
        return Json(parse_num(p));
    }
    return Json();
}

Json Json::parse(const std::string& text) {
    const char* p = text.c_str();
    return parse_val(p);
}

std::string Json::dump(int indent) const {
    std::string pad(indent, ' ');
    switch (type) {
        case NUL: return "null";
        case STR: {
            std::string r = "\"";
            for (char c : s) {
                switch (c) {
                    case '"': r += "\\\""; break;
                    case '\\': r += "\\\\"; break;
                    case '\n': r += "\\n"; break;
                    case '\t': r += "\\t"; break;
                    case '\r': r += "\\r"; break;
                    case '\b': r += "\\b"; break;
                    case '\f': r += "\\f"; break;
                    default:
                        if ((unsigned char)c < 0x20) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                            r += buf;
                        } else {
                            r += c;
                        }
                }
            }
            return r + "\"";
        }
        case NUM: {
            char b[64];
            snprintf(b, sizeof(b), "%.15g", n);
            return b;
        }
        case BOOL: return b ? "true" : "false";
        case ARR: {
            if (a.empty()) return "[]";
            std::string r = "[\n";
            for (size_t i = 0; i < a.size(); ++i) {
                if (i) r += ",\n";
                r += pad + "  " + a[i].dump(indent + 2);
            }
            return r + "\n" + pad + "]";
        }
        case OBJ: {
            if (o.empty()) return "{}";
            std::string r = "{\n";
            bool first = true;
            for (auto& kv : o) {
                if (!first) r += ",\n";
                first = false;
                r += pad + "  \"" + kv.first + "\": " + kv.second.dump(indent + 2);
            }
            return r + "\n" + pad + "}";
        }
    }
    return "null";
}

// ============================================================================
// LLMBridge Implementation
// ============================================================================
std::string LLMBridge::httpPost(const std::string& path, const std::string& body) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";

    // Set timeout
    DWORD timeout = 30000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port_);
    inet_pton(AF_INET, endpoint_.c_str(), &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return "";
    }

    // Build HTTP request
    std::string req = "POST " + path + " HTTP/1.1\r\n";
    req += "Host: " + endpoint_ + "\r\n";
    req += "Content-Type: application/json\r\n";
    req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    req += "Connection: close\r\n\r\n";
    req += body;

    send(sock, req.c_str(), (int)req.size(), 0);

    // Read response
    std::string response;
    char buf[8192];
    int r;
    while ((r = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[r] = 0;
        response += buf;
    }
    closesocket(sock);

    // Extract body
    size_t bodyStart = response.find("\r\n\r\n");
    return bodyStart != std::string::npos ? response.substr(bodyStart + 4) : response;
}

std::string LLMBridge::httpGet(const std::string& path) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port_);
    inet_pton(AF_INET, endpoint_.c_str(), &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return "";
    }

    std::string req = "GET " + path + " HTTP/1.1\r\n";
    req += "Host: " + endpoint_ + "\r\n";
    req += "Connection: close\r\n\r\n";

    send(sock, req.c_str(), (int)req.size(), 0);

    std::string response;
    char buf[8192];
    int r;
    while ((r = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[r] = 0;
        response += buf;
    }
    closesocket(sock);

    size_t bodyStart = response.find("\r\n\r\n");
    return bodyStart != std::string::npos ? response.substr(bodyStart + 4) : response;
}

bool LLMBridge::isAvailable() {
    std::string result = httpGet("/api/tags");
    return !result.empty() && result.find("error") == std::string::npos;
}

std::vector<std::string> LLMBridge::listModels() {
    std::string result = httpGet("/api/tags");
    std::vector<std::string> models;

    // Parse JSON response
    size_t pos = 0;
    while ((pos = result.find("\"name\":\"", pos)) != std::string::npos) {
        pos += 8;
        size_t end = result.find("\"", pos);
        if (end != std::string::npos) {
            models.push_back(result.substr(pos, end - pos));
        }
    }
    return models;
}

LLMResponse LLMBridge::generate(const LLMRequest& req, StreamCallback callback) {
    LLMResponse resp;
    auto start = std::chrono::high_resolution_clock::now();

    // Build request body
    std::string body = "{\"model\":\"" + req.model + "\",\"prompt\":\"";
    for (const auto& msg : req.messages) {
        // Escape JSON string
        for (char c : msg.content) {
            switch (c) {
                case '"': body += "\\\""; break;
                case '\\': body += "\\\\"; break;
                case '\n': body += "\\n"; break;
                case '\t': body += "\\t"; break;
                case '\r': body += "\\r"; break;
                default: body += c;
            }
        }
        body += "\\n";
    }
    body += "\",\"stream\":" + std::string(req.stream ? "true" : "false");
    body += ",\"options\":{\"temperature\":" + std::to_string(req.temperature);
    body += ",\"num_predict\":" + std::to_string(req.maxTokens) + "}}";

    std::string result = httpPost("/api/generate", body);

    // Parse streaming response
    std::istringstream ss(result);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty() || line[0] != '{') continue;

        // Extract "response" field
        size_t rpos = line.find("\"response\":\"");
        if (rpos != std::string::npos) {
            rpos += 12;
            size_t rend = line.find("\"", rpos);
            if (rend != std::string::npos) {
                std::string chunk = line.substr(rpos, rend - rpos);
                // Unescape
                for (size_t i = 0; i < chunk.size(); ++i) {
                    if (chunk[i] == '\\' && i + 1 < chunk.size()) {
                        switch (chunk[i + 1]) {
                            case 'n': chunk.replace(i, 2, "\n"); break;
                            case 't': chunk.replace(i, 2, "\t"); break;
                            case 'r': chunk.replace(i, 2, "\r"); break;
                            case '"': chunk.replace(i, 2, "\""); break;
                            case '\\': chunk.replace(i, 2, "\\"); break;
                        }
                    }
                }
                resp.content += chunk;
                if (callback) callback(chunk, false);
            }
        }

        // Check for done
        if (line.find("\"done\":true") != std::string::npos) {
            resp.done = true;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    resp.latencyMs = std::chrono::duration<double, std::milli>(end - start).count();
    resp.tokensGenerated = (int)std::count(resp.content.begin(), resp.content.end(), ' ') + 1;

    if (callback) callback("", true);
    return resp;
}

LLMResponse LLMBridge::chat(const std::vector<LLMMessage>& messages, const std::string& model, StreamCallback cb) {
    LLMRequest req;
    req.messages = messages;
    req.model = model.empty() ? "local" : model;
    req.stream = cb != nullptr;
    return generate(req, cb);
}

bool LLMBridge::loadModel(const std::string& model) {
    std::string body = "{\"name\":\"" + model + "\"}";
    std::string result = httpPost("/api/pull", body);
    return result.find("error") == std::string::npos;
}

bool LLMBridge::unloadModel(const std::string& model) {
    // Ollama doesn't have explicit unload, but we can try
    return true;
}

// ============================================================================
// ToolRegistry Implementation
// ============================================================================
void ToolRegistry::registerTool(const Tool& tool) {
    std::lock_guard<std::mutex> lock(mtx_);
    tools_[tool.name] = tool;
}

void ToolRegistry::registerBuiltin() {
    // File operations
    registerTool({
        "file_read", "Read file contents", "filesystem", {"path"},
        [](const auto& args) -> ToolResult {
            ToolResult r;
            auto it = args.find("path");
            if (it == args.end()) { r.error = "Missing path"; return r; }
            std::ifstream f(it->second, std::ios::binary);
            if (!f) { r.error = "Cannot open file: " + it->second; return r; }
            r.output = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            r.success = true;
            r.metadata["size"] = std::to_string(r.output.size());
            return r;
        }
    });

    registerTool({
        "file_write", "Write file contents", "filesystem", {"path", "content"},
        [](const auto& args) -> ToolResult {
            ToolResult r;
            auto pit = args.find("path"), cit = args.find("content");
            if (pit == args.end() || cit == args.end()) { r.error = "Missing path or content"; return r; }
            std::ofstream f(pit->second, std::ios::binary);
            if (!f) { r.error = "Cannot open file: " + pit->second; return r; }
            f << cit->second;
            r.success = f.good();
            r.output = r.success ? "File written: " + pit->second : "Write failed";
            return r;
        }, true
    });

    registerTool({
        "list_dir", "List directory contents", "filesystem", {"path"},
        [](const auto& args) -> ToolResult {
            ToolResult r;
            std::string path = args.count("path") ? args.at("path") : ".";
            try {
                for (const auto& e : std::filesystem::directory_iterator(path)) {
                    r.output += e.path().filename().string();
                    r.output += e.is_directory() ? "/\n" : "\n";
                }
                r.success = true;
            } catch (const std::exception& ex) {
                r.error = ex.what();
            }
            return r;
        }
    });

    registerTool({
        "search_code", "Search for pattern in files", "code", {"pattern", "path", "ext"},
        [](const auto& args) -> ToolResult {
            ToolResult r;
            std::string pattern = args.count("pattern") ? args.at("pattern") : "";
            std::string path = args.count("path") ? args.at("path") : ".";
            std::string ext = args.count("ext") ? args.at("ext") : "";
            if (pattern.empty()) { r.error = "Missing pattern"; return r; }

            int matches = 0;
            try {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
                    if (!entry.is_regular_file()) continue;
                    if (!ext.empty() && entry.path().extension() != ext) continue;

                    std::ifstream f(entry.path());
                    std::string line;
                    int lineNum = 0;
                    while (std::getline(f, line)) {
                        lineNum++;
                        if (line.find(pattern) != std::string::npos) {
                            r.output += entry.path().string() + ":" + std::to_string(lineNum) + ": " + line + "\n";
                            matches++;
                            if (matches > 100) { r.output += "... (truncated)\n"; break; }
                        }
                    }
                    if (matches > 100) break;
                }
            } catch (const std::exception& ex) {
                r.error = ex.what();
            }
            r.success = true;
            r.metadata["matches"] = std::to_string(matches);
            return r;
        }
    });

    registerTool({
        "execute", "Execute shell command", "system", {"command"},
        [](const auto& args) -> ToolResult {
            ToolResult r;
            auto it = args.find("command");
            if (it == args.end()) { r.error = "Missing command"; return r; }

            FILE* pipe = _popen(it->second.c_str(), "r");
            if (!pipe) { r.error = "Failed to execute"; return r; }

            char buf[4096];
            while (fgets(buf, sizeof(buf), pipe)) r.output += buf;
            int status = _pclose(pipe);
            r.success = (status == 0);
            r.metadata["exit_code"] = std::to_string(status);
            return r;
        }, true
    });

    registerTool({
        "grep", "Search for pattern in text", "text", {"pattern", "text"},
        [](const auto& args) -> ToolResult {
            ToolResult r;
            std::string pattern = args.count("pattern") ? args.at("pattern") : "";
            std::string text = args.count("text") ? args.at("text") : "";
            if (pattern.empty()) { r.error = "Missing pattern"; return r; }

            std::istringstream ss(text);
            std::string line;
            int lineNum = 0;
            while (std::getline(ss, line)) {
                lineNum++;
                if (line.find(pattern) != std::string::npos) {
                    r.output += std::to_string(lineNum) + ": " + line + "\n";
                }
            }
            r.success = true;
            return r;
        }
    });

    registerTool({
        "edit_file", "Edit file by replacing text", "code", {"path", "old_text", "new_text"},
        [](const auto& args) -> ToolResult {
            ToolResult r;
            auto pit = args.find("path");
            auto oit = args.find("old_text");
            auto nit = args.find("new_text");
            if (pit == args.end() || oit == args.end() || nit == args.end()) {
                r.error = "Missing path, old_text, or new_text";
                return r;
            }

            std::ifstream f(pit->second, std::ios::binary);
            if (!f) { r.error = "Cannot open file"; return r; }
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            f.close();

            size_t pos = content.find(oit->second);
            if (pos == std::string::npos) {
                r.error = "old_text not found in file";
                return r;
            }

            content.replace(pos, oit->second.length(), nit->second);

            std::ofstream of(pit->second, std::ios::binary);
            of << content;
            r.success = of.good();
            r.output = r.success ? "File edited successfully" : "Write failed";
            return r;
        }, true
    });
}

ToolResult ToolRegistry::execute(const std::string& name, const std::map<std::string, std::string>& args) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        ToolResult r;
        r.error = "Unknown tool: " + name;
        return r;
    }
    try {
        return it->second.execute(args);
    } catch (const std::exception& ex) {
        ToolResult r;
        r.error = std::string("Exception: ") + ex.what();
        return r;
    }
}

std::vector<Tool> ToolRegistry::listTools() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<Tool> result;
    for (const auto& kv : tools_) {
        result.push_back(kv.second);
    }
    return result;
}

bool ToolRegistry::hasTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    return tools_.find(name) != tools_.end();
}

// ============================================================================
// SemanticSearch Implementation
// ============================================================================
void SemanticSearch::setWorkspace(const std::string& path) {
    std::lock_guard<std::mutex> lock(mtx_);
    workspacePath_ = path;
}

void SemanticSearch::indexFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;

    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // Extract symbols (simple regex-based)
    std::regex symbolPattern(R"(\b(class|struct|function|def|fn|void|int|string|auto|var|let|const)\s+(\w+))");
    std::sregex_iterator it(content.begin(), content.end(), symbolPattern);
    std::sregex_iterator end;

    std::lock_guard<std::mutex> lock(mtx_);
    while (it != end) {
        std::string symbol = (*it)[2].str();
        symbolIndex_[symbol].push_back(path);
        ++it;
    }
}

void SemanticSearch::indexWorkspace() {
    if (workspacePath_.empty()) return;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(workspacePath_)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c" ||
                ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".rs" ||
                ext == ".go" || ext == ".java" || ext == ".cs" || ext == ".asm") {
                indexFile(entry.path().string());
            }
        }
    } catch (...) {}
}

std::vector<SearchResult> SemanticSearch::search(const std::string& query, int maxResults) {
    std::vector<SearchResult> results;
    std::lock_guard<std::mutex> lock(mtx_);

    // Simple text search in indexed files
    for (const auto& kv : symbolIndex_) {
        if (kv.first.find(query) != std::string::npos) {
            for (const auto& path : kv.second) {
                SearchResult r;
                r.filePath = path;
                r.text = kv.first;
                r.score = 1.0 / (1.0 + abs((int)kv.first.size() - (int)query.size()));
                results.push_back(r);
                if ((int)results.size() >= maxResults) break;
            }
        }
        if ((int)results.size() >= maxResults) break;
    }

    return results;
}

std::vector<SearchResult> SemanticSearch::searchSymbols(const std::string& symbol, int maxResults) {
    return search(symbol, maxResults);
}

// ============================================================================
// CodeCompleter Implementation
// ============================================================================
CodeCompleter::CodeCompleter() {
    initLanguagePatterns();
}

void CodeCompleter::initLanguagePatterns() {
    // C/C++ keywords
    languageKeywords_["cpp"] = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
        "bool", "break", "case", "catch", "char", "class", "compl", "const",
        "constexpr", "const_cast", "continue", "decltype", "default", "delete",
        "do", "double", "dynamic_cast", "else", "enum", "explicit", "export",
        "extern", "false", "float", "for", "friend", "goto", "if", "inline",
        "int", "long", "mutable", "namespace", "new", "noexcept", "not", "not_eq",
        "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
        "register", "reinterpret_cast", "return", "short", "signed", "sizeof",
        "static", "static_assert", "static_cast", "struct", "switch", "template",
        "this", "thread_local", "throw", "true", "try", "typedef", "typeid",
        "typename", "union", "unsigned", "using", "virtual", "void", "volatile",
        "wchar_t", "while", "xor", "xor_eq"
    };

    // Python keywords
    languageKeywords_["python"] = {
        "False", "None", "True", "and", "as", "assert", "async", "await",
        "break", "class", "continue", "def", "del", "elif", "else", "except",
        "finally", "for", "from", "global", "if", "import", "in", "is",
        "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
        "while", "with", "yield"
    };

    // MASM keywords
    languageKeywords_["masm"] = {
        "mov", "push", "pop", "call", "ret", "jmp", "je", "jne", "jz", "jnz",
        "cmp", "test", "add", "sub", "mul", "div", "inc", "dec", "and", "or",
        "xor", "not", "shl", "shr", "lea", "nop", "int", "syscall", "db", "dw",
        "dd", "dq", "equ", "proc", "endp", "macro", "endm", "if", "else", "endif",
        "repeat", "until", "while", "endw", "for", "endf", "struct", "ends"
    };

    // Common snippets
    snippets_["cpp"].push_back({"fori", "snippet", "For loop with index", "Standard for loop",
        "for (int i = 0; i < ${1:count}; ++i) {\n    ${2:// body}\n}", 0});
    snippets_["cpp"].push_back({"main", "snippet", "Main function", "Entry point",
        "int main(int argc, char* argv[]) {\n    ${1:// code}\n    return 0;\n}", 0});
    snippets_["cpp"].push_back({"class", "snippet", "Class definition", "Create a class",
        "class ${1:ClassName} {\npublic:\n    ${1:ClassName}();\n    ~${1:ClassName}();\nprivate:\n    ${2:// members}\n};", 0});

    snippets_["python"].push_back({"def", "snippet", "Function definition", "Define a function",
        "def ${1:function_name}(${2:args}):\n    ${3:pass}", 0});
    snippets_["python"].push_back({"class", "snippet", "Class definition", "Define a class",
        "class ${1:ClassName}:\n    def __init__(self):\n        ${2:pass}", 0});
}

std::vector<CompletionItem> CodeCompleter::complete(const std::string& code, int line, int column, const std::string& language) {
    std::vector<CompletionItem> results;
    std::lock_guard<std::mutex> lock(mtx_);

    // Get the current word being typed
    std::string currentWord;
    int wordStart = column - 1;
    while (wordStart >= 0 && (isalnum(code[wordStart]) || code[wordStart] == '_')) {
        currentWord = code[wordStart] + currentWord;
        wordStart--;
    }

    if (currentWord.empty()) return results;

    // Add matching keywords
    std::string lang = language.empty() ? "cpp" : language;
    auto it = languageKeywords_.find(lang);
    if (it != languageKeywords_.end()) {
        for (const auto& kw : it->second) {
            if (kw.find(currentWord) == 0) {
                CompletionItem item;
                item.label = kw;
                item.kind = "keyword";
                item.insertText = kw;
                item.sortText = 1;
                results.push_back(item);
            }
        }
    }

    // Add matching snippets
    auto sit = snippets_.find(lang);
    if (sit != snippets_.end()) {
        for (const auto& snip : sit->second) {
            if (snip.label.find(currentWord) == 0) {
                results.push_back(snip);
            }
        }
    }

    // Sort by sortText
    std::sort(results.begin(), results.end(), [](const CompletionItem& a, const CompletionItem& b) {
        return a.sortText < b.sortText;
    });

    return results;
}

void CodeCompleter::addSnippet(const std::string& language, const CompletionItem& snippet) {
    std::lock_guard<std::mutex> lock(mtx_);
    snippets_[language].push_back(snippet);
}

void CodeCompleter::indexSymbols(const std::string& code, const std::string& language) {
    // Extract symbols from code and add to completion
    std::regex symbolPattern(R"(\b(\w+)\s*[\(=])");
    std::sregex_iterator it(code.begin(), code.end(), symbolPattern);
    std::sregex_iterator end;

    std::lock_guard<std::mutex> lock(mtx_);
    while (it != end) {
        std::string symbol = (*it)[1].str();
        // Skip keywords
        auto kit = languageKeywords_.find(language);
        if (kit != languageKeywords_.end()) {
            if (std::find(kit->second.begin(), kit->second.end(), symbol) != kit->second.end()) {
                ++it;
                continue;
            }
        }

        CompletionItem item;
        item.label = symbol;
        item.kind = "function";
        item.insertText = symbol;
        item.sortText = 0;
        snippets_[language].push_back(item);
        ++it;
    }
}

// ============================================================================
// AgentPlanner Implementation
// ============================================================================
std::string AgentPlanner::addTask(const std::string& description, const std::string& tool, const std::map<std::string, std::string>& args) {
    std::lock_guard<std::mutex> lock(mtx_);

    // Generate ID
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::string id = "task_" + std::to_string(now);

    AgentTask task;
    task.id = id;
    task.description = description;
    task.tool = tool;
    task.args = args;
    task.status = "pending";

    tasks_.push_back(task);
    taskIndex_[id] = task;

    return id;
}

void AgentPlanner::addDependency(const std::string& taskId, const std::string& dependsOn) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = taskIndex_.find(taskId);
    if (it != taskIndex_.end()) {
        it->second.dependencies.push_back(dependsOn);
    }
}

std::vector<AgentTask> AgentPlanner::getReadyTasks() {
    std::vector<AgentTask> ready;
    std::lock_guard<std::mutex> lock(mtx_);

    for (auto& task : tasks_) {
        if (task.status != "pending") continue;

        bool allDepsMet = true;
        for (const auto& dep : task.dependencies) {
            auto dit = taskIndex_.find(dep);
            if (dit == taskIndex_.end() || dit->second.status != "completed") {
                allDepsMet = false;
                break;
            }
        }

        if (allDepsMet) {
            ready.push_back(task);
        }
    }

    return ready;
}

void AgentPlanner::updateTaskStatus(const std::string& taskId, const std::string& status, const std::string& result) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = taskIndex_.find(taskId);
    if (it != taskIndex_.end()) {
        it->second.status = status;
        it->second.result = result;

        // Update in vector too
        for (auto& task : tasks_) {
            if (task.id == taskId) {
                task.status = status;
                task.result = result;
                break;
            }
        }
    }
}

std::vector<AgentTask> AgentPlanner::getAllTasks() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return tasks_;
}

void AgentPlanner::clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    tasks_.clear();
    taskIndex_.clear();
}

// ============================================================================
// TCPServer Implementation
// ============================================================================
void TCPServer::acceptLoop() {
    while (running_.load()) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSock_, &readSet);

        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int sel = select(0, &readSet, nullptr, nullptr, &timeout);
        if (sel <= 0) continue;

        sockaddr_in clientAddr = {};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSock = accept(listenSock_, (sockaddr*)&clientAddr, &addrLen);

        if (clientSock != INVALID_SOCKET) {
            std::lock_guard<std::mutex> lock(clientMtx_);
            clientThreads_.emplace_back(&TCPServer::handleClient, this, clientSock);
        }
    }
}

void TCPServer::handleClient(SOCKET clientSock) {
    char buf[8192];
    std::string request;

    // Read request
    int r = recv(clientSock, buf, sizeof(buf) - 1, 0);
    if (r > 0) {
        buf[r] = 0;
        request = buf;

        // Handle request
        std::string response = handler_(request);

        // Send response
        std::string httpResp = "HTTP/1.1 200 OK\r\n";
        httpResp += "Content-Type: application/json\r\n";
        httpResp += "Content-Length: " + std::to_string(response.size()) + "\r\n";
        httpResp += "Access-Control-Allow-Origin: *\r\n";
        httpResp += "Connection: close\r\n\r\n";
        httpResp += response;

        send(clientSock, httpResp.c_str(), (int)httpResp.size(), 0);
    }

    closesocket(clientSock);
}

bool TCPServer::start(std::function<std::string(const std::string&)> handler) {
    handler_ = std::move(handler);

    // Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    // Create socket
    listenSock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock_ == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    // Allow reuse
    int opt = 1;
    setsockopt(listenSock_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    // Bind
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port_);

    if (bind(listenSock_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(listenSock_);
        WSACleanup();
        return false;
    }

    // Listen
    if (listen(listenSock_, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSock_);
        WSACleanup();
        return false;
    }

    running_.store(true);
    acceptThread_ = std::thread(&TCPServer::acceptLoop, this);

    return true;
}

void TCPServer::stop() {
    if (!running_.load()) return;

    running_.store(false);

    if (listenSock_ != INVALID_SOCKET) {
        closesocket(listenSock_);
        listenSock_ = INVALID_SOCKET;
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    // Wait for client threads
    std::lock_guard<std::mutex> lock(clientMtx_);
    for (auto& t : clientThreads_) {
        if (t.joinable()) t.join();
    }
    clientThreads_.clear();

    WSACleanup();
}

// ============================================================================
// NativeAIIntegration Implementation
// ============================================================================
NativeAIIntegration& NativeAIIntegration::Instance() {
    static NativeAIIntegration instance;
    return instance;
}

bool NativeAIIntegration::initialize(const AIConfig& config) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (initialized_.load()) return true;

    config_ = config;

    // Initialize LLM bridge
    llm_ = std::make_unique<LLMBridge>(config.llmEndpoint, config.llmPort);

    // Initialize tool registry
    tools_ = std::make_unique<ToolRegistry>();
    if (config.enableTools) {
        tools_->registerBuiltin();
    }

    // Initialize search
    if (config.enableSearch) {
        search_ = std::make_unique<SemanticSearch>();
        search_->setWorkspace(config.workspacePath);
    }

    // Initialize completer
    if (config.enableCompletion) {
        completer_ = std::make_unique<CodeCompleter>();
    }

    // Initialize planner
    planner_ = std::make_unique<AgentPlanner>();

    initialized_.store(true);

    if (onLog_) {
        onLog_("[AI] Native AI Integration initialized");
        onLog_("[AI] LLM endpoint: " + config.llmEndpoint + ":" + std::to_string(config.llmPort));
        onLog_("[AI] Server port: " + std::to_string(config.serverPort));
    }

    return true;
}

void NativeAIIntegration::shutdown() {
    std::lock_guard<std::mutex> lock(mtx_);

    if (!initialized_.load()) return;

    // Stop server first
    if (server_) {
        server_->stop();
        server_.reset();
    }

    llm_.reset();
    tools_.reset();
    search_.reset();
    completer_.reset();
    planner_.reset();

    initialized_.store(false);
    serverRunning_.store(false);

    if (onLog_) {
        onLog_("[AI] Native AI Integration shutdown complete");
    }
}

LLMResponse NativeAIIntegration::chat(const std::string& message, StreamCallback callback) {
    if (!initialized_.load() || !llm_) {
        LLMResponse resp;
        resp.error = "AI not initialized";
        return resp;
    }

    std::vector<LLMMessage> messages;
    messages.push_back({"user", message});

    LLMResponse resp = llm_->chat(messages, config_.defaultModel, callback);

    if (onResponse_) onResponse_(resp);
    return resp;
}

LLMResponse NativeAIIntegration::complete(const std::string& prompt, StreamCallback callback) {
    if (!initialized_.load() || !llm_) {
        LLMResponse resp;
        resp.error = "AI not initialized";
        return resp;
    }

    LLMRequest req;
    req.messages.push_back({"user", prompt});
    req.model = config_.defaultModel;
    req.maxTokens = config_.maxTokens;
    req.temperature = config_.temperature;

    LLMResponse resp = llm_->generate(req, callback);

    if (onResponse_) onResponse_(resp);
    return resp;
}

std::vector<std::string> NativeAIIntegration::listModels() {
    if (!initialized_.load() || !llm_) return {};
    return llm_->listModels();
}

bool NativeAIIntegration::loadModel(const std::string& model) {
    if (!initialized_.load() || !llm_) return false;
    return llm_->loadModel(model);
}

ToolResult NativeAIIntegration::executeTool(const std::string& name, const std::map<std::string, std::string>& args) {
    if (!initialized_.load() || !tools_) {
        ToolResult r;
        r.error = "Tools not initialized";
        return r;
    }

    ToolResult result = tools_->execute(name, args);

    if (onToolExec_) {
        onToolExec_(name, result.success ? result.output : result.error);
    }

    return result;
}

std::vector<Tool> NativeAIIntegration::listTools() {
    if (!initialized_.load() || !tools_) return {};
    return tools_->listTools();
}

void NativeAIIntegration::registerTool(const Tool& tool) {
    if (!initialized_.load() || !tools_) return;
    tools_->registerTool(tool);
}

std::vector<SearchResult> NativeAIIntegration::search(const std::string& query, int maxResults) {
    if (!initialized_.load() || !search_) return {};
    return search_->search(query, maxResults);
}

void NativeAIIntegration::indexWorkspace() {
    if (!initialized_.load() || !search_) return;
    search_->indexWorkspace();
    if (onLog_) onLog_("[AI] Workspace indexed");
}

std::vector<CompletionItem> NativeAIIntegration::complete(const std::string& code, int line, int column, const std::string& language) {
    if (!initialized_.load() || !completer_) return {};
    return completer_->complete(code, line, column, language);
}

std::string NativeAIIntegration::planTask(const std::string& description) {
    if (!initialized_.load() || !planner_) return "";

    // Simple task planning - parse description and create tasks
    std::string taskId = planner_->addTask(description, "auto", {});

    if (onLog_) onLog_("[AI] Task planned: " + taskId + " - " + description);

    return taskId;
}

std::vector<AgentTask> NativeAIIntegration::getTasks() {
    if (!initialized_.load() || !planner_) return {};
    return planner_->getAllTasks();
}

ToolResult NativeAIIntegration::executeTask(const std::string& taskId) {
    ToolResult result;

    if (!initialized_.load() || !planner_ || !tools_) {
        result.error = "Not initialized";
        return result;
    }

    auto tasks = planner_->getAllTasks();
    for (const auto& task : tasks) {
        if (task.id == taskId) {
            planner_->updateTaskStatus(taskId, "running", "");

            if (task.tool == "auto") {
                // Use LLM to determine tool
                LLMResponse resp = chat("What tool should I use for: " + task.description + "? Reply with just the tool name and args as JSON.");
                if (!resp.error.empty()) {
                    planner_->updateTaskStatus(taskId, "failed", resp.error);
                    result.error = resp.error;
                    return result;
                }

                // Parse response
                Json j = Json::parse(resp.content);
                if (j.has("tool")) {
                    std::string toolName = j["tool"].as_s();
                    std::map<std::string, std::string> args;
                    if (j.has("args") && j["args"].is(Json::OBJ)) {
                        for (const auto& kv : j["args"].o) {
                            args[kv.first] = kv.second.as_s();
                        }
                    }
                    result = tools_->execute(toolName, args);
                } else {
                    result.error = "Could not determine tool from LLM response";
                }
            } else {
                result = tools_->execute(task.tool, task.args);
            }

            planner_->updateTaskStatus(taskId, result.success ? "completed" : "failed", result.output);
            return result;
        }
    }

    result.error = "Task not found: " + taskId;
    return result;
}

bool NativeAIIntegration::startServer() {
    if (!initialized_.load()) return false;
    if (serverRunning_.load()) return true;

    server_ = std::make_unique<TCPServer>(config_.serverPort);

    bool started = server_->start([this](const std::string& req) {
        return handleRequest(req);
    });

    if (started) {
        serverRunning_.store(true);
        if (onLog_) onLog_("[AI] Server started on port " + std::to_string(config_.serverPort));
    }

    return started;
}

void NativeAIIntegration::stopServer() {
    if (!serverRunning_.load()) return;

    if (server_) {
        server_->stop();
        server_.reset();
    }

    serverRunning_.store(false);

    if (onLog_) onLog_("[AI] Server stopped");
}

bool NativeAIIntegration::isLLMAvailable() {
    if (!initialized_.load() || !llm_) return false;
    return llm_->isAvailable();
}

std::string NativeAIIntegration::handleRequest(const std::string& request) {
    // Parse HTTP request
    size_t bodyStart = request.find("\r\n\r\n");
    std::string body = bodyStart != std::string::npos ? request.substr(bodyStart + 4) : request;

    // Parse JSON
    Json req = Json::parse(body);
    Json resp;

    if (!req.has("action")) {
        resp["error"] = "Missing action";
        return resp.dump();
    }

    std::string action = req["action"].as_s();

    if (action == "chat") {
        std::string message = req.has("message") ? req["message"].as_s() : "";
        LLMResponse llmResp = chat(message);
        resp["response"] = llmResp.content;
        resp["error"] = llmResp.error;
        resp["tokens"] = llmResp.tokensGenerated;
        resp["latency_ms"] = llmResp.latencyMs;
    }
    else if (action == "complete") {
        std::string prompt = req.has("prompt") ? req["prompt"].as_s() : "";
        LLMResponse llmResp = complete(prompt);
        resp["response"] = llmResp.content;
        resp["error"] = llmResp.error;
    }
    else if (action == "tool") {
        std::string name = req.has("name") ? req["name"].as_s() : "";
        std::map<std::string, std::string> args;
        if (req.has("args") && req["args"].is(Json::OBJ)) {
            for (const auto& kv : req["args"].o) {
                args[kv.first] = kv.second.as_s();
            }
        }
        ToolResult toolResult = executeTool(name, args);
        resp["success"] = toolResult.success;
        resp["output"] = toolResult.output;
        resp["error"] = toolResult.error;
    }
    else if (action == "list_tools") {
        auto tools = listTools();
        Json arr;
        for (size_t i = 0; i < tools.size(); ++i) {
            Json t;
            t["name"] = tools[i].name;
            t["description"] = tools[i].description;
            t["category"] = tools[i].category;
            arr[i] = t;
        }
        resp["tools"] = arr;
    }
    else if (action == "search") {
        std::string query = req.has("query") ? req["query"].as_s() : "";
        int maxResults = req.has("max_results") ? (int)req["max_results"].as_n() : 50;
        auto results = search(query, maxResults);
        Json arr;
        for (size_t i = 0; i < results.size(); ++i) {
            Json r;
            r["file"] = results[i].filePath;
            r["line"] = results[i].line;
            r["text"] = results[i].text;
            r["score"] = results[i].score;
            arr[i] = r;
        }
        resp["results"] = arr;
    }
    else if (action == "list_models") {
        auto models = listModels();
        Json arr;
        for (size_t i = 0; i < models.size(); ++i) {
            arr[i] = models[i];
        }
        resp["models"] = arr;
    }
    else if (action == "status") {
        resp["initialized"] = initialized_.load();
        resp["server_running"] = serverRunning_.load();
        resp["llm_available"] = isLLMAvailable();
        resp["port"] = config_.serverPort;
    }
    else {
        resp["error"] = "Unknown action: " + action;
    }

    return resp.dump();
}

// ============================================================================
// CLIIntegration Implementation
// ============================================================================
bool CLIIntegration::initialize(const AIConfig& config) {
    ai_.setLogCallback([](const std::string& msg) {
        std::cout << msg << std::endl;
    });
    return ai_.initialize(config);
}

int CLIIntegration::runInteractive() {
    interactive_ = true;

    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          RawrXD Native AI IDE - Interactive Mode           ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  /chat <msg>     - Chat with AI                            ║\n";
    std::cout << "║  /complete       - Get code completion                     ║\n";
    std::cout << "║  /search <query> - Search codebase                         ║\n";
    std::cout << "║  /tool <name>    - Execute tool                            ║\n";
    std::cout << "║  /models         - List available models                   ║\n";
    std::cout << "║  /status         - Show system status                      ║\n";
    std::cout << "║  /help           - Show this help                          ║\n";
    std::cout << "║  /quit           - Exit                                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        if (line.empty()) continue;

        if (line[0] == '/') {
            size_t sp = line.find(' ');
            std::string cmd = (sp == std::string::npos) ? line : line.substr(0, sp);
            std::string args = (sp == std::string::npos) ? "" : line.substr(sp + 1);

            if (cmd == "/quit" || cmd == "/exit") {
                break;
            } else if (cmd == "/help") {
                printHelp();
            } else if (cmd == "/chat") {
                cmdChat(args);
            } else if (cmd == "/complete") {
                cmdComplete(args);
            } else if (cmd == "/search") {
                cmdSearch(args);
            } else if (cmd == "/tool") {
                std::cout << "Tool name: ";
                std::string toolName;
                std::getline(std::cin, toolName);
                cmdTool(toolName, {});
            } else if (cmd == "/models") {
                cmdModel("list", "");
            } else if (cmd == "/status") {
                cmdStatus();
            } else {
                std::cout << "Unknown command. Type /help for help.\n";
            }
        } else {
            // Default to chat
            cmdChat(line);
        }
    }

    return 0;
}

int CLIIntegration::runCommand(const std::string& cmd, const std::vector<std::string>& args) {
    if (cmd == "chat") {
        std::string msg;
        for (const auto& a : args) msg += a + " ";
        cmdChat(msg);
        return 0;
    } else if (cmd == "search") {
        if (!args.empty()) cmdSearch(args[0]);
        return 0;
    } else if (cmd == "status") {
        cmdStatus();
        return 0;
    } else if (cmd == "models") {
        cmdModel("list", "");
        return 0;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    return 1;
}

void CLIIntegration::printHelp() {
    std::cout << "\nRawrXD Native AI IDE Commands:\n\n";
    std::cout << "  /chat <message>    Send message to AI\n";
    std::cout << "  /complete <code>   Get code completion\n";
    std::cout << "  /search <query>    Search codebase\n";
    std::cout << "  /tool <name>       Execute a tool\n";
    std::cout << "  /models            List available LLM models\n";
    std::cout << "  /model <name>      Switch to model\n";
    std::cout << "  /file <path>       Set current file context\n";
    std::cout << "  /status            Show system status\n";
    std::cout << "  /help              Show this help\n";
    std::cout << "  /quit              Exit\n\n";
}

void CLIIntegration::cmdChat(const std::string& message) {
    if (message.empty()) {
        std::cout << "Usage: /chat <message>\n";
        return;
    }

    std::cout << "\n[AI] ";
    LLMResponse resp = ai_.chat(message, [](const std::string& chunk, bool done) {
        std::cout << chunk << std::flush;
    });
    std::cout << "\n\n";

    if (!resp.error.empty()) {
        std::cout << "[Error] " << resp.error << "\n";
    }
}

void CLIIntegration::cmdComplete(const std::string& code) {
    auto items = ai_.complete(code, 1, code.length(), currentLanguage_);

    if (items.empty()) {
        std::cout << "No completions available.\n";
        return;
    }

    std::cout << "\nCompletions:\n";
    for (const auto& item : items) {
        std::cout << "  " << item.label << " [" << item.kind << "]\n";
        if (!item.detail.empty()) {
            std::cout << "    " << item.detail << "\n";
        }
    }
}

void CLIIntegration::cmdSearch(const std::string& query) {
    if (query.empty()) {
        std::cout << "Usage: /search <query>\n";
        return;
    }

    auto results = ai_.search(query);

    if (results.empty()) {
        std::cout << "No results found.\n";
        return;
    }

    std::cout << "\nSearch Results:\n";
    for (const auto& r : results) {
        std::cout << "  " << r.filePath << ":" << r.line << " - " << r.text << "\n";
    }
}

void CLIIntegration::cmdTool(const std::string& name, const std::map<std::string, std::string>& args) {
    if (name.empty()) {
        std::cout << "Available tools:\n";
        for (const auto& tool : ai_.listTools()) {
            std::cout << "  " << tool.name << " - " << tool.description << "\n";
        }
        return;
    }

    ToolResult result = ai_.executeTool(name, args);

    if (result.success) {
        std::cout << result.output << "\n";
    } else {
        std::cout << "[Error] " << result.error << "\n";
    }
}

void CLIIntegration::cmdModel(const std::string& action, const std::string& model) {
    if (action == "list") {
        auto models = ai_.listModels();
        if (models.empty()) {
            std::cout << "No models available. Is Ollama running?\n";
        } else {
            std::cout << "Available models:\n";
            for (const auto& m : models) {
                std::cout << "  " << m << "\n";
            }
        }
    } else if (action == "load") {
        if (model.empty()) {
            std::cout << "Usage: /model load <name>\n";
        } else {
            if (ai_.loadModel(model)) {
                std::cout << "Model loaded: " << model << "\n";
            } else {
                std::cout << "Failed to load model: " << model << "\n";
            }
        }
    }
}

void CLIIntegration::cmdFile(const std::string& path) {
    currentFile_ = path;

    // Detect language
    if (path.find(".cpp") != std::string::npos || path.find(".hpp") != std::string::npos) {
        currentLanguage_ = "cpp";
    } else if (path.find(".py") != std::string::npos) {
        currentLanguage_ = "python";
    } else if (path.find(".asm") != std::string::npos) {
        currentLanguage_ = "masm";
    } else {
        currentLanguage_ = "";
    }

    std::cout << "Current file: " << path << " (" << currentLanguage_ << ")\n";
}

void CLIIntegration::cmdStatus() {
    std::cout << "\n=== RawrXD Native AI Status ===\n";
    std::cout << "  Initialized: " << (ai_.isInitialized() ? "Yes" : "No") << "\n";
    std::cout << "  LLM Available: " << (ai_.isLLMAvailable() ? "Yes" : "No") << "\n";
    std::cout << "  Server Running: " << (ai_.isServerRunning() ? "Yes" : "No") << "\n";
    std::cout << "  Server Port: " << ai_.getServerPort() << "\n";
    std::cout << "  Current File: " << (currentFile_.empty() ? "(none)" : currentFile_) << "\n";
    std::cout << "  Language: " << (currentLanguage_.empty() ? "(auto)" : currentLanguage_) << "\n";

    auto config = ai_.getConfig();
    std::cout << "  LLM Endpoint: " << config.llmEndpoint << ":" << config.llmPort << "\n";
    std::cout << "  Default Model: " << config.defaultModel << "\n";
    std::cout << "\n";
}

// ============================================================================
// GUIIntegration Implementation
// ============================================================================
bool GUIIntegration::initialize(const AIConfig& config, HWND parentWnd) {
    parentWnd_ = parentWnd;
    return ai_.initialize(config);
}

void GUIIntegration::shutdown() {
    ai_.shutdown();
}

void GUIIntegration::setEditorContent(const std::string& code) {
    currentCode_ = code;
}

void GUIIntegration::setCursorPosition(int line, int column) {
    cursorLine_ = line;
    cursorColumn_ = column;
}

void GUIIntegration::setCurrentFile(const std::string& path) {
    currentFile_ = path;
}

std::vector<CompletionItem> GUIIntegration::getCompletions() {
    return ai_.complete(currentCode_, cursorLine_, cursorColumn_, "");
}

LLMResponse GUIIntegration::requestCompletion(const std::string& context) {
    std::string prompt = "Complete the following code:\n\n" + context;
    return ai_.complete(prompt);
}

LLMResponse GUIIntegration::requestExplanation(const std::string& code) {
    std::string prompt = "Explain the following code:\n\n" + code;
    return ai_.chat(prompt);
}

LLMResponse GUIIntegration::requestRefactor(const std::string& code, const std::string& instruction) {
    std::string prompt = "Refactor the following code";
    if (!instruction.empty()) {
        prompt += " (" + instruction + ")";
    }
    prompt += ":\n\n" + code;
    return ai_.chat(prompt);
}

LLMResponse GUIIntegration::sendChatMessage(const std::string& message) {
    return ai_.chat(message);
}

void GUIIntegration::setChatCallback(std::function<void(const LLMResponse&)> cb) {
    ai_.setResponseCallback(std::move(cb));
}

std::string GUIIntegration::getStatusMessage() {
    std::string status = "RawrXD AI: ";
    status += ai_.isInitialized() ? "Ready" : "Not Initialized";
    status += " | LLM: ";
    status += ai_.isLLMAvailable() ? "Connected" : "Disconnected";
    return status;
}

} // namespace rawrxd
