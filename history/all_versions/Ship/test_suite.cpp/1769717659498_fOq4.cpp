// ═══════════════════════════════════════════════════════════════════════════════
// test_suite.cpp - RawrXD Agent Comprehensive Test Suite
// Pure C++20 - No external dependencies except Windows SDK
// ═══════════════════════════════════════════════════════════════════════════════

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <chrono>
#include <functional>
#include <cassert>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <io.h>
#include <fcntl.h>

// ═══════════════════════════════════════════════════════════════════════════════
// Simple Test Framework (No Catch2 dependency)
// ═══════════════════════════════════════════════════════════════════════════════

namespace Test {

struct TestResult {
    std::wstring name;
    bool passed{false};
    std::wstring message;
    double durationMs{0.0};
};

static std::vector<TestResult> g_results;
static int g_passed = 0;
static int g_failed = 0;

#define TEST_CASE(name) \
    void test_##name(); \
    struct TestRegistrar_##name { \
        TestRegistrar_##name() { \
            auto start = std::chrono::high_resolution_clock::now(); \
            TestResult result; \
            result.name = L#name; \
            try { \
                test_##name(); \
                result.passed = true; \
                g_passed++; \
            } catch (const std::exception& e) { \
                result.passed = false; \
                result.message = L"Exception: "; \
                g_failed++; \
            } catch (...) { \
                result.passed = false; \
                result.message = L"Unknown exception"; \
                g_failed++; \
            } \
            auto end = std::chrono::high_resolution_clock::now(); \
            result.durationMs = std::chrono::duration<double, std::milli>(end - start).count(); \
            g_results.push_back(result); \
        } \
    } g_registrar_##name; \
    void test_##name()

#define REQUIRE(cond) \
    do { if (!(cond)) { throw std::runtime_error("REQUIRE failed: " #cond); } } while(0)

#define REQUIRE_EQ(a, b) \
    do { if (!((a) == (b))) { throw std::runtime_error("REQUIRE_EQ failed: " #a " == " #b); } } while(0)

#define REQUIRE_NEAR(a, b, tol) \
    do { if (std::abs((a) - (b)) > (tol)) { throw std::runtime_error("REQUIRE_NEAR failed"); } } while(0)

#define REQUIRE_FALSE(cond) REQUIRE(!(cond))

void PrintResults() {
    std::wcout << L"\n═══════════════════════════════════════════════════════════════════\n";
    std::wcout << L"                        TEST RESULTS\n";
    std::wcout << L"═══════════════════════════════════════════════════════════════════\n\n";
    
    for (const auto& r : g_results) {
        std::wcout << (r.passed ? L"  ✓ PASS " : L"  ✗ FAIL ");
        std::wcout << r.name;
        std::wcout << L" (" << std::fixed << std::setprecision(2) << r.durationMs << L"ms)";
        if (!r.message.empty()) {
            std::wcout << L" - " << r.message;
        }
        std::wcout << L"\n";
    }
    
    std::wcout << L"\n───────────────────────────────────────────────────────────────────\n";
    std::wcout << L"  Total: " << g_results.size() << L"  Passed: " << g_passed << L"  Failed: " << g_failed << L"\n";
    std::wcout << L"═══════════════════════════════════════════════════════════════════\n";
}

} // namespace Test

// ═══════════════════════════════════════════════════════════════════════════════
// Minimal JSON Parser (for testing)
// ═══════════════════════════════════════════════════════════════════════════════

namespace Json {

using String = std::wstring;
struct JsonValue;
using JsonObject = std::map<String, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    std::variant<std::nullptr_t, bool, int64_t, double, String, JsonArray, JsonObject> data;
    
    bool isNull() const { return std::holds_alternative<std::nullptr_t>(data); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isInt() const { return std::holds_alternative<int64_t>(data); }
    bool isDouble() const { return std::holds_alternative<double>(data); }
    bool isString() const { return std::holds_alternative<String>(data); }
    bool isArray() const { return std::holds_alternative<JsonArray>(data); }
    bool isObject() const { return std::holds_alternative<JsonObject>(data); }
    
    bool asBool() const { return std::get<bool>(data); }
    int64_t asInt() const { return std::get<int64_t>(data); }
    double asDouble() const { return std::get<double>(data); }
    const String& asString() const { return std::get<String>(data); }
    const JsonArray& asArray() const { return std::get<JsonArray>(data); }
    const JsonObject& asObject() const { return std::get<JsonObject>(data); }
};

class Parser {
public:
    static std::optional<JsonValue> Parse(const std::string& json) {
        Parser p(json);
        return p.parseValue();
    }
    
private:
    std::string src;
    size_t pos{0};
    
    explicit Parser(const std::string& s) : src(s) {}
    
    void skipWhitespace() {
        while (pos < src.size() && std::isspace(src[pos])) pos++;
    }
    
    char peek() const { return pos < src.size() ? src[pos] : '\0'; }
    char consume() { return pos < src.size() ? src[pos++] : '\0'; }
    
    std::optional<JsonValue> parseValue() {
        skipWhitespace();
        char c = peek();
        
        if (c == 'n') return parseNull();
        if (c == 't' || c == 'f') return parseBool();
        if (c == '"') return parseString();
        if (c == '[') return parseArray();
        if (c == '{') return parseObject();
        if (c == '-' || std::isdigit(c)) return parseNumber();
        
        return std::nullopt;
    }
    
    std::optional<JsonValue> parseNull() {
        if (src.substr(pos, 4) == "null") {
            pos += 4;
            return JsonValue{nullptr};
        }
        return std::nullopt;
    }
    
    std::optional<JsonValue> parseBool() {
        if (src.substr(pos, 4) == "true") {
            pos += 4;
            return JsonValue{true};
        }
        if (src.substr(pos, 5) == "false") {
            pos += 5;
            return JsonValue{false};
        }
        return std::nullopt;
    }
    
    std::optional<JsonValue> parseNumber() {
        size_t start = pos;
        if (peek() == '-') consume();
        
        while (std::isdigit(peek())) consume();
        
        bool isFloat = false;
        if (peek() == '.') {
            isFloat = true;
            consume();
            while (std::isdigit(peek())) consume();
        }
        
        if (peek() == 'e' || peek() == 'E') {
            isFloat = true;
            consume();
            if (peek() == '+' || peek() == '-') consume();
            while (std::isdigit(peek())) consume();
        }
        
        std::string numStr = src.substr(start, pos - start);
        
        if (isFloat) {
            return JsonValue{std::stod(numStr)};
        }
        return JsonValue{static_cast<int64_t>(std::stoll(numStr))};
    }
    
    std::optional<JsonValue> parseString() {
        if (consume() != '"') return std::nullopt;
        
        String result;
        while (peek() != '"' && peek() != '\0') {
            char c = consume();
            if (c == '\\') {
                char esc = consume();
                switch (esc) {
                    case 'n': result += L'\n'; break;
                    case 't': result += L'\t'; break;
                    case 'r': result += L'\r'; break;
                    case '"': result += L'"'; break;
                    case '\\': result += L'\\'; break;
                    default: result += static_cast<wchar_t>(esc);
                }
            } else {
                result += static_cast<wchar_t>(c);
            }
        }
        
        if (consume() != '"') return std::nullopt;
        return JsonValue{result};
    }
    
    std::optional<JsonValue> parseArray() {
        if (consume() != '[') return std::nullopt;
        
        JsonArray arr;
        skipWhitespace();
        
        if (peek() == ']') {
            consume();
            return JsonValue{arr};
        }
        
        while (true) {
            auto val = parseValue();
            if (!val) return std::nullopt;
            arr.push_back(*val);
            
            skipWhitespace();
            char c = consume();
            if (c == ']') break;
            if (c != ',') return std::nullopt;
        }
        
        return JsonValue{arr};
    }
    
    std::optional<JsonValue> parseObject() {
        if (consume() != '{') return std::nullopt;
        
        JsonObject obj;
        skipWhitespace();
        
        if (peek() == '}') {
            consume();
            return JsonValue{obj};
        }
        
        while (true) {
            skipWhitespace();
            auto keyVal = parseString();
            if (!keyVal || !keyVal->isString()) return std::nullopt;
            
            skipWhitespace();
            if (consume() != ':') return std::nullopt;
            
            auto val = parseValue();
            if (!val) return std::nullopt;
            
            obj[keyVal->asString()] = *val;
            
            skipWhitespace();
            char c = consume();
            if (c == '}') break;
            if (c != ',') return std::nullopt;
        }
        
        return JsonValue{obj};
    }
};

std::string Serialize(const JsonValue& val, int indent = 0) {
    std::ostringstream oss;
    
    if (val.isNull()) {
        oss << "null";
    } else if (val.isBool()) {
        oss << (val.asBool() ? "true" : "false");
    } else if (val.isInt()) {
        oss << val.asInt();
    } else if (val.isDouble()) {
        oss << std::fixed << std::setprecision(6) << val.asDouble();
    } else if (val.isString()) {
        oss << '"';
        for (wchar_t c : val.asString()) {
            if (c == '"') oss << "\\\"";
            else if (c == '\\') oss << "\\\\";
            else if (c == '\n') oss << "\\n";
            else if (c == '\t') oss << "\\t";
            else if (c < 128) oss << static_cast<char>(c);
            else oss << "?";
        }
        oss << '"';
    } else if (val.isArray()) {
        const auto& arr = val.asArray();
        if (arr.empty()) {
            oss << "[]";
        } else {
            oss << "[";
            for (size_t i = 0; i < arr.size(); i++) {
                if (i > 0) oss << ", ";
                oss << Serialize(arr[i], indent);
            }
            oss << "]";
        }
    } else if (val.isObject()) {
        const auto& obj = val.asObject();
        if (obj.empty()) {
            oss << "{}";
        } else {
            oss << "{";
            bool first = true;
            for (const auto& [k, v] : obj) {
                if (!first) oss << ", ";
                first = false;
                oss << '"';
                for (wchar_t c : k) {
                    if (c < 128) oss << static_cast<char>(c);
                }
                oss << "\": " << Serialize(v, indent);
            }
            oss << "}";
        }
    }
    
    return oss.str();
}

} // namespace Json

// ═══════════════════════════════════════════════════════════════════════════════
// JSON Parser Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE(JSON_ParseNull) {
    auto result = Json::Parser::Parse("null");
    REQUIRE(result.has_value());
    REQUIRE(result->isNull());
}

TEST_CASE(JSON_ParseBoolTrue) {
    auto result = Json::Parser::Parse("true");
    REQUIRE(result.has_value());
    REQUIRE(result->isBool());
    REQUIRE_EQ(result->asBool(), true);
}

TEST_CASE(JSON_ParseBoolFalse) {
    auto result = Json::Parser::Parse("false");
    REQUIRE(result.has_value());
    REQUIRE(result->isBool());
    REQUIRE_EQ(result->asBool(), false);
}

TEST_CASE(JSON_ParseInteger) {
    auto result = Json::Parser::Parse("42");
    REQUIRE(result.has_value());
    REQUIRE(result->isInt());
    REQUIRE_EQ(result->asInt(), 42);
}

TEST_CASE(JSON_ParseNegativeInteger) {
    auto result = Json::Parser::Parse("-123");
    REQUIRE(result.has_value());
    REQUIRE(result->isInt());
    REQUIRE_EQ(result->asInt(), -123);
}

TEST_CASE(JSON_ParseDouble) {
    auto result = Json::Parser::Parse("3.14159");
    REQUIRE(result.has_value());
    REQUIRE(result->isDouble());
    REQUIRE_NEAR(result->asDouble(), 3.14159, 0.00001);
}

TEST_CASE(JSON_ParseScientific) {
    auto result = Json::Parser::Parse("1.5e10");
    REQUIRE(result.has_value());
    REQUIRE(result->isDouble());
    REQUIRE_NEAR(result->asDouble(), 1.5e10, 1.0);
}

TEST_CASE(JSON_ParseString) {
    auto result = Json::Parser::Parse("\"hello world\"");
    REQUIRE(result.has_value());
    REQUIRE(result->isString());
    REQUIRE_EQ(result->asString(), L"hello world");
}

TEST_CASE(JSON_ParseStringEscapes) {
    auto result = Json::Parser::Parse("\"line1\\nline2\\ttab\"");
    REQUIRE(result.has_value());
    REQUIRE(result->isString());
    REQUIRE(result->asString().find(L'\n') != std::wstring::npos);
    REQUIRE(result->asString().find(L'\t') != std::wstring::npos);
}

TEST_CASE(JSON_ParseEmptyArray) {
    auto result = Json::Parser::Parse("[]");
    REQUIRE(result.has_value());
    REQUIRE(result->isArray());
    REQUIRE_EQ(result->asArray().size(), 0u);
}

TEST_CASE(JSON_ParseNumberArray) {
    auto result = Json::Parser::Parse("[1, 2, 3]");
    REQUIRE(result.has_value());
    REQUIRE(result->isArray());
    const auto& arr = result->asArray();
    REQUIRE_EQ(arr.size(), 3u);
    REQUIRE_EQ(arr[0].asInt(), 1);
    REQUIRE_EQ(arr[1].asInt(), 2);
    REQUIRE_EQ(arr[2].asInt(), 3);
}

TEST_CASE(JSON_ParseMixedArray) {
    auto result = Json::Parser::Parse("[1, \"two\", true, null]");
    REQUIRE(result.has_value());
    REQUIRE(result->isArray());
    const auto& arr = result->asArray();
    REQUIRE_EQ(arr.size(), 4u);
    REQUIRE(arr[0].isInt());
    REQUIRE(arr[1].isString());
    REQUIRE(arr[2].isBool());
    REQUIRE(arr[3].isNull());
}

TEST_CASE(JSON_ParseEmptyObject) {
    auto result = Json::Parser::Parse("{}");
    REQUIRE(result.has_value());
    REQUIRE(result->isObject());
    REQUIRE_EQ(result->asObject().size(), 0u);
}

TEST_CASE(JSON_ParseSimpleObject) {
    auto result = Json::Parser::Parse("{\"name\": \"John\", \"age\": 30}");
    REQUIRE(result.has_value());
    REQUIRE(result->isObject());
    const auto& obj = result->asObject();
    REQUIRE_EQ(obj.size(), 2u);
    REQUIRE_EQ(obj.at(L"name").asString(), L"John");
    REQUIRE_EQ(obj.at(L"age").asInt(), 30);
}

TEST_CASE(JSON_ParseNestedObject) {
    auto result = Json::Parser::Parse(R"({
        "user": {
            "name": "Alice",
            "settings": {
                "theme": "dark"
            }
        }
    })");
    REQUIRE(result.has_value());
    REQUIRE(result->isObject());
    const auto& obj = result->asObject();
    const auto& user = obj.at(L"user").asObject();
    const auto& settings = user.at(L"settings").asObject();
    REQUIRE_EQ(settings.at(L"theme").asString(), L"dark");
}

TEST_CASE(JSON_Serialize) {
    Json::JsonObject obj;
    obj[L"name"] = Json::JsonValue{L"Test"};
    obj[L"value"] = Json::JsonValue{static_cast<int64_t>(42)};
    obj[L"active"] = Json::JsonValue{true};
    
    Json::JsonValue val{obj};
    std::string json = Json::Serialize(val);
    
    REQUIRE(json.find("\"name\"") != std::string::npos);
    REQUIRE(json.find("\"Test\"") != std::string::npos);
    REQUIRE(json.find("42") != std::string::npos);
    REQUIRE(json.find("true") != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Process Execution Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE(Process_EchoCommand) {
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    
    wchar_t cmd[] = L"cmd /c echo Hello Test";
    BOOL success = CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWritePipe);
    
    REQUIRE(success);
    
    char buffer[256];
    DWORD bytesRead;
    std::string output;
    
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }
    
    WaitForSingleObject(pi.hProcess, 5000);
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    REQUIRE_EQ(exitCode, 0u);
    REQUIRE(output.find("Hello Test") != std::string::npos);
}

TEST_CASE(Process_ExitCode) {
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    
    wchar_t cmd[] = L"cmd /c exit 42";
    BOOL success = CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    REQUIRE(success);
    
    WaitForSingleObject(pi.hProcess, 5000);
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    REQUIRE_EQ(exitCode, 42u);
}

TEST_CASE(Process_Timeout) {
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    
    wchar_t cmd[] = L"cmd /c timeout /t 10 /nobreak";
    BOOL success = CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    REQUIRE(success);
    
    auto start = std::chrono::steady_clock::now();
    
    // Wait only 100ms
    DWORD result = WaitForSingleObject(pi.hProcess, 100);
    
    auto elapsed = std::chrono::steady_clock::now() - start;
    
    // Should have timed out (WAIT_TIMEOUT)
    REQUIRE_EQ(result, WAIT_TIMEOUT);
    
    // Terminate the process
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, 1000);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Should have completed quickly
    REQUIRE(elapsed < std::chrono::milliseconds(500));
}

// ═══════════════════════════════════════════════════════════════════════════════
// File Operations Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE(File_WriteAndRead) {
    const wchar_t* testPath = L"test_output_file.txt";
    const char* testContent = "Hello from test suite!\nLine 2\n";
    
    // Write
    HANDLE hFile = CreateFileW(testPath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(hFile != INVALID_HANDLE_VALUE);
    
    DWORD written;
    WriteFile(hFile, testContent, (DWORD)strlen(testContent), &written, nullptr);
    CloseHandle(hFile);
    
    REQUIRE_EQ(written, (DWORD)strlen(testContent));
    
    // Read
    hFile = CreateFileW(testPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(hFile != INVALID_HANDLE_VALUE);
    
    char buffer[256];
    DWORD bytesRead;
    ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
    buffer[bytesRead] = '\0';
    CloseHandle(hFile);
    
    REQUIRE_EQ(std::string(buffer), std::string(testContent));
    
    // Cleanup
    DeleteFileW(testPath);
}

TEST_CASE(File_GetSize) {
    const wchar_t* testPath = L"test_size_file.txt";
    const char* content = "0123456789";  // 10 bytes
    
    HANDLE hFile = CreateFileW(testPath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(hFile != INVALID_HANDLE_VALUE);
    
    DWORD written;
    WriteFile(hFile, content, 10, &written, nullptr);
    CloseHandle(hFile);
    
    // Get size
    hFile = CreateFileW(testPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(hFile != INVALID_HANDLE_VALUE);
    
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    CloseHandle(hFile);
    
    REQUIRE_EQ(size.QuadPart, 10);
    
    DeleteFileW(testPath);
}

TEST_CASE(File_DirectoryListing) {
    // List current directory
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(L"*.*", &findData);
    
    REQUIRE(hFind != INVALID_HANDLE_VALUE);
    
    int count = 0;
    do {
        count++;
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
    
    REQUIRE(count > 0);  // Should find at least . and ..
}

// ═══════════════════════════════════════════════════════════════════════════════
// DLL Loading Tests (Native Bridge)
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE(DLL_LoadNativeBridge) {
    HMODULE hDll = LoadLibraryW(L"RawrXD_NativeModelBridge.dll");
    
    if (hDll) {
        // Get exports
        auto LoadModelNative = GetProcAddress(hDll, "LoadModelNative");
        auto UnloadModelNative = GetProcAddress(hDll, "UnloadModelNative");
        auto ForwardPass = GetProcAddress(hDll, "ForwardPass");
        auto GenerateTokens = GetProcAddress(hDll, "GenerateTokens");
        auto DequantizeRow_Q4_0 = GetProcAddress(hDll, "DequantizeRow_Q4_0");
        auto SoftMax_SSE = GetProcAddress(hDll, "SoftMax_SSE");
        auto SampleToken_Argmax = GetProcAddress(hDll, "SampleToken_Argmax");
        
        REQUIRE(LoadModelNative != nullptr);
        REQUIRE(UnloadModelNative != nullptr);
        REQUIRE(ForwardPass != nullptr);
        REQUIRE(GenerateTokens != nullptr);
        REQUIRE(DequantizeRow_Q4_0 != nullptr);
        REQUIRE(SoftMax_SSE != nullptr);
        REQUIRE(SampleToken_Argmax != nullptr);
        
        FreeLibrary(hDll);
    } else {
        // DLL not found - skip test but don't fail
        std::wcout << L"  (DLL not found, skipping export check)\n";
    }
}

TEST_CASE(DLL_SoftMaxExecution) {
    HMODULE hDll = LoadLibraryW(L"RawrXD_NativeModelBridge.dll");
    
    if (hDll) {
        typedef int (*SoftMax_SSE_t)(float*, float*, int);
        auto SoftMax_SSE = (SoftMax_SSE_t)GetProcAddress(hDll, "SoftMax_SSE");
        
        if (SoftMax_SSE) {
            float logits[5] = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
            float probs[5] = {0};
            
            int result = SoftMax_SSE(logits, probs, 5);
            
            REQUIRE_EQ(result, 1);
            
            // Check probabilities sum to ~1
            float sum = probs[0] + probs[1] + probs[2] + probs[3] + probs[4];
            REQUIRE_NEAR(sum, 1.0f, 0.1f);  // Allow some tolerance
        }
        
        FreeLibrary(hDll);
    }
}

TEST_CASE(DLL_ArgmaxExecution) {
    HMODULE hDll = LoadLibraryW(L"RawrXD_NativeModelBridge.dll");
    
    if (hDll) {
        typedef int (*SampleToken_Argmax_t)(float*, int);
        auto SampleToken_Argmax = (SampleToken_Argmax_t)GetProcAddress(hDll, "SampleToken_Argmax");
        
        if (SampleToken_Argmax) {
            float probs[5] = {0.1f, 0.2f, 0.5f, 0.15f, 0.05f};
            
            int token = SampleToken_Argmax(probs, 5);
            
            REQUIRE_EQ(token, 2);  // Index 2 has highest probability (0.5)
        }
        
        FreeLibrary(hDll);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Performance Benchmarks
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE(Perf_JSONParsing1000Objects) {
    // Build large JSON
    std::string largeJson = R"({"users": [)";
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) largeJson += ",";
        largeJson += "{\"id\": " + std::to_string(i) + 
                    ", \"name\": \"User" + std::to_string(i) + "\"}";
    }
    largeJson += "]}";
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = Json::Parser::Parse(largeJson);
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    
    REQUIRE(result.has_value());
    REQUIRE(result->isObject());
    
    auto ms = std::chrono::duration<double, std::milli>(elapsed).count();
    std::wcout << L"  (Parsed " << largeJson.size() << L" bytes in " << ms << L"ms)\n";
    
    // Should complete in reasonable time
    REQUIRE(elapsed < std::chrono::milliseconds(500));
}

TEST_CASE(Perf_ProcessSpawn100) {
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100; ++i) {
        STARTUPINFOW si{sizeof(si)};
        PROCESS_INFORMATION pi{};
        
        wchar_t cmd[] = L"cmd /c echo test";
        CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        WaitForSingleObject(pi.hProcess, 1000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto ms = std::chrono::duration<double, std::milli>(elapsed).count();
    
    std::wcout << L"  (100 process spawns in " << ms << L"ms = " 
              << (ms / 100) << L"ms/process)\n";
    
    // Should complete in reasonable time (10s max)
    REQUIRE(elapsed < std::chrono::seconds(10));
}

TEST_CASE(Perf_FileIO) {
    const wchar_t* testPath = L"perf_test_file.txt";
    const size_t dataSize = 1024 * 1024;  // 1 MB
    
    std::vector<char> data(dataSize, 'X');
    
    // Write benchmark
    auto startWrite = std::chrono::high_resolution_clock::now();
    
    HANDLE hFile = CreateFileW(testPath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD written;
    WriteFile(hFile, data.data(), (DWORD)dataSize, &written, nullptr);
    CloseHandle(hFile);
    
    auto writeElapsed = std::chrono::high_resolution_clock::now() - startWrite;
    
    // Read benchmark
    auto startRead = std::chrono::high_resolution_clock::now();
    
    hFile = CreateFileW(testPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    std::vector<char> readData(dataSize);
    DWORD bytesRead;
    ReadFile(hFile, readData.data(), (DWORD)dataSize, &bytesRead, nullptr);
    CloseHandle(hFile);
    
    auto readElapsed = std::chrono::high_resolution_clock::now() - startRead;
    
    DeleteFileW(testPath);
    
    auto writeMs = std::chrono::duration<double, std::milli>(writeElapsed).count();
    auto readMs = std::chrono::duration<double, std::milli>(readElapsed).count();
    
    std::wcout << L"  (1MB write: " << writeMs << L"ms, read: " << readMs << L"ms)\n";
    
    REQUIRE(written == dataSize);
    REQUIRE(bytesRead == dataSize);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Memory Allocation Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE(Memory_HeapAlloc) {
    HANDLE hHeap = GetProcessHeap();
    REQUIRE(hHeap != nullptr);
    
    void* ptr = HeapAlloc(hHeap, 0, 1024);
    REQUIRE(ptr != nullptr);
    
    // Write and read
    memset(ptr, 0xAB, 1024);
    REQUIRE(((unsigned char*)ptr)[0] == 0xAB);
    REQUIRE(((unsigned char*)ptr)[1023] == 0xAB);
    
    BOOL freed = HeapFree(hHeap, 0, ptr);
    REQUIRE(freed);
}

TEST_CASE(Memory_VirtualAlloc) {
    void* ptr = VirtualAlloc(nullptr, 64 * 1024, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(ptr != nullptr);
    
    // Write and read
    memset(ptr, 0xCD, 64 * 1024);
    REQUIRE(((unsigned char*)ptr)[0] == 0xCD);
    
    BOOL freed = VirtualFree(ptr, 0, MEM_RELEASE);
    REQUIRE(freed);
}

// ═══════════════════════════════════════════════════════════════════════════════
// String Utility Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_CASE(String_WideToNarrow) {
    std::wstring wide = L"Hello World";
    
    int bufSize = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrow(bufSize - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, narrow.data(), bufSize, nullptr, nullptr);
    
    REQUIRE_EQ(narrow, "Hello World");
}

TEST_CASE(String_NarrowToWide) {
    std::string narrow = "Hello World";
    
    int bufSize = MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), -1, nullptr, 0);
    std::wstring wide(bufSize - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(), -1, wide.data(), bufSize);
    
    REQUIRE_EQ(wide, L"Hello World");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Main Entry Point
// ═══════════════════════════════════════════════════════════════════════════════

int wmain() {
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U16TEXT);
    
    std::wcout << L"\n";
    std::wcout << L"╔══════════════════════════════════════════════════════════════════╗\n";
    std::wcout << L"║           RawrXD Agent - Comprehensive Test Suite                ║\n";
    std::wcout << L"║                 Pure C++20 - No Dependencies                     ║\n";
    std::wcout << L"╚══════════════════════════════════════════════════════════════════╝\n";
    std::wcout << L"\n";
    std::wcout << L"Running tests...\n\n";
    
    // Tests run automatically via static initialization
    
    Test::PrintResults();
    
    return Test::g_failed > 0 ? 1 : 0;
}

int main() {
    return wmain();
}
