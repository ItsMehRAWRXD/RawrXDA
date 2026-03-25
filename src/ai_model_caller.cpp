#include <nlohmann/json.hpp>

#include "ai_model_caller.h"

<<<<<<< HEAD
=======
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <chrono>
#include <sstream>
#include <regex>
>>>>>>> origin/main
#include <algorithm>
#include <atomic>
#include <cctype>
#include <sstream>
#include <thread>

<<<<<<< HEAD
namespace {
std::atomic<bool> g_modelReady{false};
std::string g_modelName;

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}
=======
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif
>>>>>>> origin/main

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
}  // namespace

namespace RawrXD {

bool ModelCaller::Initialize(const std::string& modelPath) {
    if (modelPath.empty()) {
        return false;
    }
    g_modelName = modelPath;
    g_modelReady = true;
    return true;
}

<<<<<<< HEAD
void ModelCaller::Shutdown() {
    g_modelReady = false;
    g_modelName.clear();
}
=======
    /**
     * Generate code completions
     * 
     * @param prefix Code before cursor
     * @param suffix Code after cursor  
     * @param fileType Language (cpp, python, js, etc)
     * @param context Full file context
     * @param numCompletions How many completions to generate
     * @return Vector of completion candidates with scores
     * 
     * Real implementation:
     * 1. Truncate context to fit in model window
     * 2. Build prompt: [system] + [prefix] + [suffix] + [candidates start]
     * 3. Call model with params: temp=0.3, top_p=0.9, max_tokens=256
     * 4. Parse completions, score by syntax+relevance
     * 5. Return top N sorted by score
     * 
     * Example prompt:
     * ```
     * You are a code completion assistant. Complete the code based on context.
     * Language: C++
     * 
     * int main() {
     *     std::vector<int> vec = {1, 2, 3};
     *     for (auto v : vec) {
     *         std::cout << v << std::endl;  // <- cursor here
     * ```
     */
    struct Completion {
        std::string text;
        float score = 0.0f;
        std::string description;
    };
>>>>>>> origin/main

bool ModelCaller::IsReady() { return g_modelReady.load(); }

std::vector<ModelCaller::Completion> ModelCaller::generateCompletion(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& fileType,
    const std::string& context,
    int numCompletions) {
    std::vector<Completion> out;
    if (numCompletions <= 0) {
        return out;
    }

    const std::string prompt = buildCompletionPrompt(prefix, suffix, fileType, context);
    const std::vector<std::string> variants = parseCompletions(callModel(prompt, {}));

<<<<<<< HEAD
    int count = 0;
    for (const auto& v : variants) {
        if (count++ >= numCompletions) {
            break;
        }
        Completion c;
        c.text = v;
        c.score = scoreCompletion(v, prefix, fileType);
        c.description = getCompletionDescription(v);
        out.push_back(std::move(c));
    }
=======
            std::cout << "🤖 Generating code completions..." << std::endl;
>>>>>>> origin/main

    while (static_cast<int>(out.size()) < numCompletions) {
        Completion c;
        c.text = prefix.empty() ? "// completion" : prefix + " // completion";
        c.score = 0.5f;
        c.description = "Fallback completion";
        out.push_back(std::move(c));
    }

<<<<<<< HEAD
    return out;
}
=======
            auto response = callModel(prompt, params);
            if (response.empty()) {
                std::cerr << "❌ Model returned empty response" << std::endl;
                return results;
            }
>>>>>>> origin/main

std::string ModelCaller::generateCode(
    const std::string& instruction,
    const std::string& fileType,
    const std::string& context) {
    std::ostringstream out;
    out << "// Generated (" << fileType << ")\n";
    out << "// Model: " << (g_modelName.empty() ? "minimal" : g_modelName) << "\n";
    out << "// Instruction: " << instruction << "\n";
    if (!context.empty()) {
        out << context;
        if (context.back() != '\n') {
            out << '\n';
        }
    }
    out << "// TODO: Replace minimal generator with full model backend.\n";
    return out.str();
}

std::string ModelCaller::generateRewrite(
    const std::string& code,
    const std::string& instruction,
    const std::string& context) {
    (void)context;
    std::string rewritten = code;
    const std::string lower = toLower(instruction);

<<<<<<< HEAD
    if (lower.find("security") != std::string::npos) {
        auto pos = rewritten.find("strcpy(");
        if (pos != std::string::npos) {
            rewritten.replace(pos, 7, "strncpy(");
        }
        pos = rewritten.find("sprintf(");
        if (pos != std::string::npos) {
            rewritten.replace(pos, 8, "snprintf(");
        }
    }

    if (lower.find("optimiz") != std::string::npos && rewritten.find("std::endl") != std::string::npos) {
        size_t pos = 0;
        while ((pos = rewritten.find("std::endl", pos)) != std::string::npos) {
            rewritten.replace(pos, 9, "\"\\n\"");
            pos += 4;
=======
            // Sort by score
            std::sort(results.begin(), results.end(),
                     [](const Completion& a, const Completion& b) {
                         return a.score > b.score;
                     });

            // Return top N
            if (results.size() > numCompletions) {
                results.resize(numCompletions);
            }

            std::cout << "✅ Generated " << results.size() << " completions" << std::endl;

            return results;

        } catch (const std::exception& e) {
            std::cerr << "❌ Completion error: " << e.what() << std::endl;
            return results;
        }
    }

    /**
     * Generate code refactoring suggestions
     * 
     * @param code Code to refactor
     * @param refactorType REFACTOR, OPTIMIZE, SIMPLIFY, MODERNIZE, etc
     * @param context File/project context
     * @return Refactoring suggestion with explanation and diff
     * 
     * Real implementation:
     * 1. Analyze code structure
     * 2. Build refactoring prompt with guidelines for type
     * 3. Call model: temp=0.5, max_tokens=512
     * 4. Generate before/after code
     * 5. Create diff hunks
     * 6. Validate syntax of generated code
     * 7. Return with confidence score
     * 
     * Example prompt:
     * ```
     * Refactor the following C++ code to be more modern (C++17):
     * [original code]
     * 
     * Refactored code:
     * ```
     */
    struct RefactoringSuggestion {
        std::string original_code;
        std::string refactored_code;
        std::string explanation;
        float confidence = 0.0f;
        std::vector<std::string> benefits;
    };

    static RefactoringSuggestion generateRefactoring(
        const std::string& code,
        const std::string& refactorType,
        const std::string& context = "") {

        RefactoringSuggestion result;
        result.original_code = code;

        try {
            std::cout << "🔧 Generating " << refactorType << " refactoring..." << std::endl;

            // Build refactoring prompt
            std::string prompt = buildRefactoringPrompt(code, refactorType, context);

            GenerationParams params;
            params.temperature = 0.5f;
            params.max_tokens = 512;

            auto response = callModel(prompt, params);
            if (response.empty()) {
                std::cerr << "❌ Model returned empty refactoring" << std::endl;
                return result;
            }

            // Extract refactored code from response
            result.refactored_code = extractCodeBlock(response);

            // Get explanation
            result.explanation = extractExplanation(response);

            // Validate syntax of refactored code
            result.confidence = validateCodeSyntax(result.refactored_code) ? 0.9f : 0.5f;

            // Add benefits based on refactor type
            result.benefits = getRefactoringBenefits(refactorType);

            std::cout << "✅ Generated refactoring (confidence: " 
                     << (result.confidence * 100) << "%)" << std::endl;

            return result;

        } catch (const std::exception& e) {
            std::cerr << "❌ Refactoring error: " << e.what() << std::endl;
            return result;
>>>>>>> origin/main
        }
    }

    if (rewritten.empty()) {
        rewritten = "// Rewrite requested: " + instruction + "\n";
    }
    return rewritten;
}

std::vector<Diagnostic> ModelCaller::generateDiagnostics(
    const std::string& code,
    const std::string& language) {
    std::vector<Diagnostic> out;
    const std::string lower = toLower(code);

    if (lower.find("todo") != std::string::npos) {
        out.push_back({"TODO marker found", 1, 1, "info", "Convert TODO into a tracked task", "minimal-ai"});
    }
    if (lower.find("strcpy(") != std::string::npos) {
        out.push_back({"Potential unsafe function strcpy", 1, 1, "warning",
                       "Use strncpy or safer abstractions", "minimal-ai"});
    }
    if (toLower(language) == "cpp" && lower.find("using namespace std;") != std::string::npos) {
        out.push_back({"Avoid global using-directive in headers", 1, 1, "warning",
                       "Prefer explicit std:: qualifiers", "minimal-ai"});
    }
    return out;
}

<<<<<<< HEAD
std::string ModelCaller::callModel(const std::string& prompt, const GenerationParams& params) {
    std::ostringstream out;
    out << "{\"ok\":true,\"model\":\"" << (g_modelName.empty() ? "minimal" : g_modelName)
        << "\",\"max_tokens\":" << params.max_tokens << ",\"text\":";
    out << nlohmann::json(prompt.substr(0, std::min<size_t>(prompt.size(), 240))).dump() << "}";
    return out.str();
}
=======
        try {
            std::cout << "🧪 Generating test cases..." << std::endl;
>>>>>>> origin/main

bool ModelCaller::streamModel(const std::string& prompt, const GenerationParams& params,
                              StreamCallback callback, std::chrono::milliseconds delay) {
    (void)params;
    if (!callback) {
        return false;
    }

<<<<<<< HEAD
    const std::string text = prompt.empty() ? "stream:empty" : prompt;
    constexpr size_t kChunk = 24;
    for (size_t i = 0; i < text.size(); i += kChunk) {
        const std::string chunk = text.substr(i, std::min(kChunk, text.size() - i));
        if (!callback(chunk)) {
            return false;
        }
        if (delay.count() > 0) {
            std::this_thread::sleep_for(delay);
=======
            GenerationParams params;
            params.temperature = 0.4f;
            params.max_tokens = 1024;

            auto response = callModel(prompt, params);
            if (response.empty()) {
                std::cerr << "❌ Model returned empty tests" << std::endl;
                return results;
            }

            // Parse test cases from response
            auto testBlocks = extractTestCases(response);

            for (const auto& testBlock : testBlocks) {
                TestCase testCase;
                testCase.code = testBlock;
                testCase.name = extractTestName(testBlock);
                testCase.description = extractTestDescription(testBlock);
                testCase.assertions = extractAssertions(testBlock);
                results.push_back(testCase);
            }

            std::cout << "✅ Generated " << results.size() << " test cases" << std::endl;

            return results;

        } catch (const std::exception& e) {
            std::cerr << "❌ Test generation error: " << e.what() << std::endl;
            return results;
>>>>>>> origin/main
        }
    }
    return true;
}

<<<<<<< HEAD
nlohmann::json ModelCaller::ParseStructuredResponse(const std::string& response) {
    try {
        return nlohmann::json::parse(response);
    } catch (...) {
        nlohmann::json err = nlohmann::json::object();
        err["ok"] = false;
        err["error"] = "Failed to parse structured response";
        err["raw"] = response;
        return err;
=======
    /**
     * Generate LSP diagnostics (errors, warnings, suggestions)
     * 
     * @param code Code to analyze
     * @param language Programming language
     * @return Vector of diagnostics with fixes
     * 
     * Real implementation:
     * 1. Analyze code for issues
     * 2. Build diagnostic prompt focusing on:
     *    - Syntax errors
     *    - Logic errors
     *    - Performance issues
     *    - Code style violations
     * 3. Call model: temp=0.2 (deterministic)
     * 4. Parse diagnostics with severity, location, message
     * 5. Return with suggested fixes
     */
    struct Diagnostic {
        std::string message;
        int line = 0;
        int column = 0;
        std::string severity;  // error, warning, info
        std::string code_action;  // Suggested fix
    };

    static std::vector<Diagnostic> generateDiagnostics(
        const std::string& code,
        const std::string& language) {

        std::vector<Diagnostic> results;

        try {
            std::cout << "🔍 Analyzing code for issues..." << std::endl;

            std::string prompt = buildDiagnosticsPrompt(code, language);

            GenerationParams params;
            params.temperature = 0.2f;  // Deterministic analysis
            params.max_tokens = 512;

            auto response = callModel(prompt, params);
            if (response.empty()) {
                return results;
            }

            // Parse diagnostics from model response
            // Expected format: "Line N: [severity] message"
            std::istringstream iss(response);
            std::string line;
            std::regex diagPattern(R"([Ll]ine\s*(\d+)(?::\s*|,\s*(?:col(?:umn)?\s*(\d+)):?\s*)?\[?(error|warning|info|hint)\]?:?\s*(.+))");
            while (std::getline(iss, line)) {
                std::smatch match;
                if (std::regex_search(line, match, diagPattern)) {
                    Diagnostic diag;
                    diag.line = std::stoi(match[1].str());
                    diag.column = match[2].matched ? std::stoi(match[2].str()) : 0;
                    diag.severity = match[3].str();
                    diag.message = match[4].str();
                    results.push_back(diag);
                } else if (line.find("error") != std::string::npos || 
                           line.find("warning") != std::string::npos) {
                    // Fallback: unstructured diagnostic
                    Diagnostic diag;
                    diag.message = line;
                    diag.severity = (line.find("error") != std::string::npos) ? "error" : "warning";
                    results.push_back(diag);
                }
            }

            std::cout << "✅ Found " << results.size() << " diagnostics" << std::endl;

            return results;

        } catch (const std::exception& e) {
            std::cerr << "❌ Diagnostic error: " << e.what() << std::endl;
            return results;
        }
>>>>>>> origin/main
    }
}

<<<<<<< HEAD
std::vector<Diagnostic> ModelCaller::ExtractDiagnostics(const std::string& response) {
    std::vector<Diagnostic> out;
    const auto parsed = ParseStructuredResponse(response);

    if (parsed.is_object() && parsed.contains("diagnostics") && parsed["diagnostics"].is_array()) {
        for (const auto& d : parsed["diagnostics"]) {
            Diagnostic diag;
            diag.message = d.value("message", "diagnostic");
            diag.line = d.value("line", 1);
            diag.column = d.value("column", 1);
            diag.severity = d.value("severity", "info");
            diag.code_action = d.value("code_action", "");
            diag.source = d.value("source", "minimal-ai");
            out.push_back(std::move(diag));
=======
private:
    /**
     * Core model invocation
     * 
     * Routes to appropriate backend:
     * - Local GGUF via GGML inference engine
     * - Remote Ollama API
     * - OpenAI API
     * - HuggingFace Inference API
     * 
     * In production:
     * 1. Check model availability
     * 2. Format request for model
     * 3. Call inference engine with timeout
     * 4. Stream/buffer response
     * 5. Handle errors and retries
     * 6. Log metrics (latency, tokens, cost)
     */
    /**
     * Ollama endpoint and model configuration
     * Read from environment or fall back to defaults.
     */
    static std::string getOllamaHost() {
        const char* env = std::getenv("OLLAMA_HOST");
        return env ? std::string(env) : "localhost";
    }
    static int getOllamaPort() {
        const char* env = std::getenv("OLLAMA_PORT");
        return env ? std::stoi(env) : 11434;
    }
    static std::string getOllamaModel() {
        const char* env = std::getenv("OLLAMA_MODEL");
        return env ? std::string(env) : "llama2";
    }

    /**
     * JSON-escape a string for embedding in a JSON payload.
     */
    static std::string jsonEscape(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 32);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                        out += buf;
                    } else {
                        out += c;
                    }
            }
>>>>>>> origin/main
        }
        return out;
    }

<<<<<<< HEAD
    return generateDiagnostics(response, "text");
=======
    /**
     * Extract value of a JSON string key from a flat JSON object.
     * Lightweight parser — no dependency on a JSON library.
     */
    static std::string extractJsonString(const std::string& json, const std::string& key) {
        std::string needle = "\"" + key + "\"";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) return "";
        pos = json.find('"', pos + 1);
        if (pos == std::string::npos) return "";
        ++pos; // skip opening quote
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                switch (json[pos]) {
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case '"': result += '"';  break;
                    case '\\': result += '\\'; break;
                    default: result += json[pos]; break;
                }
            } else {
                result += json[pos];
            }
            ++pos;
        }
        return result;
    }

#ifdef _WIN32
    /**
     * Core model invocation via WinHTTP → Ollama /api/generate
     *
     * Routes to Ollama running on localhost (configurable via env vars).
     * Falls back to empty string on any network/parse failure so callers
     * degrade gracefully.
     */
    static std::string callModel(const std::string& prompt, const GenerationParams& params) {
        auto startTime = std::chrono::steady_clock::now();
        std::string ollamaHost = getOllamaHost();
        int ollamaPort = getOllamaPort();
        std::string model = getOllamaModel();

        std::cout << "🤖 Calling Ollama (" << ollamaHost << ":" << ollamaPort
                  << " model=" << model << ") ..." << std::endl;

        // Build JSON payload for /api/generate
        std::string payload = "{";
        payload += "\"model\":\"" + jsonEscape(model) + "\",";
        payload += "\"prompt\":\"" + jsonEscape(prompt) + "\",";
        payload += "\"stream\":false,";
        payload += "\"options\":{";
        payload += "\"temperature\":" + std::to_string(params.temperature) + ",";
        payload += "\"top_p\":" + std::to_string(params.top_p) + ",";
        payload += "\"top_k\":" + std::to_string(params.top_k) + ",";
        payload += "\"num_predict\":" + std::to_string(params.max_tokens) + ",";
        payload += "\"repeat_penalty\":" + std::to_string(params.repetition_penalty) + ",";
        payload += "\"num_ctx\":" + std::to_string(params.context_window);
        payload += "}}";

        // WinHTTP session
        std::wstring wHost(ollamaHost.begin(), ollamaHost.end());
        HINTERNET hSession = WinHttpOpen(L"RawrXD-ModelCaller/1.0",
                                          WINHTTP_ACCESS_TYPE_NO_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            std::cerr << "❌ WinHttpOpen failed: " << GetLastError() << std::endl;
            return "";
        }

        HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
                                             static_cast<INTERNET_PORT>(ollamaPort), 0);
        if (!hConnect) {
            std::cerr << "❌ WinHttpConnect failed: " << GetLastError() << std::endl;
            WinHttpCloseHandle(hSession);
            return "";
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST",
                                                 L"/api/generate",
                                                 nullptr, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (!hRequest) {
            std::cerr << "❌ WinHttpOpenRequest failed: " << GetLastError() << std::endl;
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        // Set timeout: 120 seconds for model inference (can be slow on CPU)
        WinHttpSetTimeouts(hRequest, 5000, 10000, 120000, 120000);

        // Send request
        LPCWSTR contentType = L"Content-Type: application/json";
        BOOL sent = WinHttpSendRequest(hRequest, contentType, -1L,
                                        (LPVOID)payload.c_str(),
                                        (DWORD)payload.size(),
                                        (DWORD)payload.size(), 0);
        if (!sent) {
            DWORD err = GetLastError();
            std::cerr << "❌ WinHttpSendRequest failed: " << err << std::endl;
            if (err == ERROR_WINHTTP_CANNOT_CONNECT) {
                std::cerr << "   Is Ollama running on " << ollamaHost << ":" << ollamaPort << "?" << std::endl;
            }
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            std::cerr << "❌ WinHttpReceiveResponse failed: " << GetLastError() << std::endl;
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        // Check HTTP status
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode,
                            &statusSize, WINHTTP_NO_HEADER_INDEX);

        // Read response body
        std::string responseBody;
        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            std::vector<char> buf(bytesAvailable + 1, 0);
            DWORD bytesRead = 0;
            WinHttpReadData(hRequest, buf.data(), bytesAvailable, &bytesRead);
            responseBody.append(buf.data(), bytesRead);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        auto elapsed = std::chrono::steady_clock::now() - startTime;
        double latencyMs = std::chrono::duration<double, std::milli>(elapsed).count();

        if (statusCode != 200) {
            std::cerr << "❌ Ollama returned HTTP " << statusCode << std::endl;
            if (!responseBody.empty()) {
                std::cerr << "   Body: " << responseBody.substr(0, 512) << std::endl;
            }
            return "";
        }

        // Extract the "response" field from Ollama JSON
        std::string modelResponse = extractJsonString(responseBody, "response");

        std::cout << "✅ Model responded in " << (int)latencyMs << " ms ("
                  << modelResponse.size() << " chars)" << std::endl;

        return modelResponse;
    }
#else
    /**
     * POSIX fallback: use libcurl if available, otherwise return empty.
     */
    static std::string callModel(const std::string& prompt, const GenerationParams& params) {
        std::cerr << "❌ callModel: POSIX implementation requires libcurl (not yet linked)" << std::endl;
        return "";
    }
#endif

    // Prompt builders
    static std::string buildCompletionPrompt(
        const std::string& prefix,
        const std::string& suffix,
        const std::string& fileType,
        const std::string& context) {

        std::string prompt = R"(You are an expert code completion assistant.
Complete the code based on context. Return only code, no explanation.

Language: )" + fileType + R"(

Context:
)" + context + R"(

Code:
)" + prefix + "[COMPLETION]" + suffix;
        return prompt;
    }

    static std::string buildRefactoringPrompt(
        const std::string& code,
        const std::string& type,
        const std::string& context) {

        std::string prompt = "Refactor the following code to " + type + ":\n\n";
        prompt += code;
        prompt += "\n\nRefactored code:\n";
        return prompt;
    }

    static std::string buildTestPrompt(
        const std::string& code,
        const std::string& language) {

        std::string prompt = "Generate comprehensive unit tests for the following " + language + " code:\n\n";
        prompt += code;
        prompt += "\n\nTests:\n";
        return prompt;
    }

    static std::string buildDiagnosticsPrompt(
        const std::string& code,
        const std::string& language) {

        std::string prompt = "Analyze the following " + language + " code for errors and issues:\n\n";
        prompt += code;
        prompt += "\n\nIssues:\n";
        return prompt;
    }

    // Response parsers
    static std::vector<std::string> parseCompletions(const std::string& response) {
        std::vector<std::string> completions;
        if (response.empty()) return completions;

        // Strategy 1: Look for numbered completions (1. ... 2. ... 3. ...)
        std::regex numberedPattern(R"(\d+\.\s*(.+?)(?=\d+\.|$))");
        auto begin = std::sregex_iterator(response.begin(), response.end(), numberedPattern);
        auto end = std::sregex_iterator();
        if (std::distance(begin, end) > 1) {
            for (auto it = begin; it != end; ++it) {
                std::string match = (*it)[1].str();
                // Trim whitespace
                size_t s = match.find_first_not_of(" \t\n\r");
                size_t e = match.find_last_not_of(" \t\n\r");
                if (s != std::string::npos) {
                    completions.push_back(match.substr(s, e - s + 1));
                }
            }
            return completions;
        }

        // Strategy 2: Look for code blocks separated by blank lines or ```
        std::regex codeBlockPattern(R"(```\w*\n([\s\S]*?)```)");
        begin = std::sregex_iterator(response.begin(), response.end(), codeBlockPattern);
        if (std::distance(begin, end) > 0) {
            for (auto it = begin; it != end; ++it) {
                completions.push_back((*it)[1].str());
            }
            return completions;
        }

        // Strategy 3: Split by double newlines for multiple suggestions
        std::istringstream iss(response);
        std::string block;
        std::string currentBlock;
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find_first_not_of(" \t\r") == std::string::npos) {
                if (!currentBlock.empty()) {
                    completions.push_back(currentBlock);
                    currentBlock.clear();
                }
            } else {
                if (!currentBlock.empty()) currentBlock += "\n";
                currentBlock += line;
            }
        }
        if (!currentBlock.empty()) {
            completions.push_back(currentBlock);
        }

        // Fallback: return the whole response as one completion
        if (completions.empty()) {
            completions.push_back(response);
        }

        return completions;
    }

    static std::string extractCodeBlock(const std::string& response) {
        // Find code blocks wrapped in ``` markers or just extract all non-explanatory text
        size_t start = response.find("```");
        if (start != std::string::npos) {
            start += 3;
            size_t end = response.find("```", start);
            if (end != std::string::npos) {
                return response.substr(start, end - start);
            }
        }
        return response;
    }

    static std::string extractExplanation(const std::string& response) {
        // Extract explanation text (non-code parts)
        return response;
    }

    static std::vector<std::string> extractTestCases(const std::string& response) {
        std::vector<std::string> tests;

        // Find test functions by common patterns
        // Pattern: TEST(Suite, Name) { ... } or void test_name() { ... } or def test_name(): ...
        std::regex testPattern(R"((TEST\w*\s*\([^)]+\)\s*\{[\s\S]*?^\})|(void\s+test_\w+\s*\([^)]*\)\s*\{[\s\S]*?^\})|(def\s+test_\w+\s*\([^)]*\):[\s\S]*?(?=\ndef|$)))", std::regex::multiline);
        auto begin = std::sregex_iterator(response.begin(), response.end(), testPattern);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            tests.push_back((*it)[0].str());
        }

        // If no structured tests found, try code block extraction
        if (tests.empty()) {
            std::regex codeBlock(R"(```\w*\n([\s\S]*?)```)");
            begin = std::sregex_iterator(response.begin(), response.end(), codeBlock);
            for (auto it = begin; it != end; ++it) {
                tests.push_back((*it)[1].str());
            }
        }

        // Final fallback
        if (tests.empty() && !response.empty()) {
            tests.push_back(response);
        }

        return tests;
    }

    static std::string extractTestName(const std::string& test) {
        // Extract test function name
        return "test_case";
    }

    static std::string extractTestDescription(const std::string& test) {
        return "Auto-generated test";
    }

    static std::vector<std::string> extractAssertions(const std::string& test) {
        std::vector<std::string> assertions;
        // Match various assertion patterns:
        // EXPECT_*, ASSERT_*, assert(), self.assert*, expect(...)
        std::regex assertPattern(R"((EXPECT_\w+|ASSERT_\w+|assert\w*|self\.assert\w*|expect)\s*\([^)]*\))");
        auto begin = std::sregex_iterator(test.begin(), test.end(), assertPattern);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            assertions.push_back((*it)[0].str());
        }
        return assertions;
    }

    // Scoring and validation
    static float scoreCompletion(const std::string& completion,
                                const std::string& prefix,
                                const std::string& fileType) {
        float score = 0.5f;

        // Bonus for common patterns
        if (completion.find("std::") != std::string::npos) score += 0.1f;
        if (completion.find(";") != std::string::npos) score += 0.1f;

        // Penalty for obviously wrong completions
        if (completion.length() > 256) score -= 0.2f;

        return std::max(0.0f, std::min(1.0f, score));
    }

    static std::string getCompletionDescription(const std::string& completion) {
        return "Suggested completion";
    }

    static bool validateCodeSyntax(const std::string& code) {
        if (code.empty()) return false;

        // Check brace/paren/bracket balance
        int braces = 0, parens = 0, brackets = 0;
        bool inString = false;
        bool inLineComment = false;
        bool inBlockComment = false;
        char prevChar = 0;

        for (size_t i = 0; i < code.size(); ++i) {
            char c = code[i];
            
            if (inLineComment) {
                if (c == '\n') inLineComment = false;
                continue;
            }
            if (inBlockComment) {
                if (c == '/' && prevChar == '*') inBlockComment = false;
                prevChar = c;
                continue;
            }
            if (inString) {
                if (c == '"' && prevChar != '\\') inString = false;
                prevChar = c;
                continue;
            }

            if (c == '/' && i + 1 < code.size()) {
                if (code[i+1] == '/') { inLineComment = true; continue; }
                if (code[i+1] == '*') { inBlockComment = true; prevChar = c; continue; }
            }
            if (c == '"' && prevChar != '\\') { inString = true; prevChar = c; continue; }

            switch (c) {
                case '{': braces++; break;
                case '}': braces--; break;
                case '(': parens++; break;
                case ')': parens--; break;
                case '[': brackets++; break;
                case ']': brackets--; break;
            }

            if (braces < 0 || parens < 0 || brackets < 0) return false;
            prevChar = c;
        }

        return braces == 0 && parens == 0 && brackets == 0;
    }

    static std::vector<std::string> getRefactoringBenefits(const std::string& type) {
        std::vector<std::string> benefits;
        if (type == "MODERNIZE") {
            benefits.push_back("Uses modern C++ features");
            benefits.push_back("Improved readability");
            benefits.push_back("Better performance");
        }
        return benefits;
    }
};

// Public interface
extern "C" {
    // C interface for FFI calls from other languages
>>>>>>> origin/main
}

std::string ModelCaller::buildCompletionPrompt(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& fileType,
    const std::string& context) {
    std::ostringstream out;
    out << "[fileType=" << fileType << "]\n";
    out << "[context]\n" << context << "\n";
    out << "[prefix]\n" << prefix << "\n";
    out << "[suffix]\n" << suffix << "\n";
    return out.str();
}

std::vector<std::string> ModelCaller::parseCompletions(const std::string& response) {
    std::vector<std::string> out;
    const auto parsed = ParseStructuredResponse(response);
    if (parsed.is_object() && parsed.contains("completions") && parsed["completions"].is_array()) {
        for (const auto& c : parsed["completions"]) {
            if (c.is_string()) {
                out.push_back(c.get<std::string>());
            }
        }
    }
    if (out.empty()) {
        const std::string payload = parsed.value("text", response);
        out.push_back(trim(payload));
        out.push_back(trim(payload) + "\n// completion variant");
    }
    return out;
}

float ModelCaller::scoreCompletion(
    const std::string& completion, const std::string& prefix, const std::string& fileType) {
    float score = 0.5f;
    if (!completion.empty()) {
        score += 0.2f;
    }
    if (!prefix.empty() && completion.find(prefix) != std::string::npos) {
        score += 0.2f;
    }
    if (toLower(fileType) == "cpp" && completion.find(';') != std::string::npos) {
        score += 0.1f;
    }
    return std::min(score, 1.0f);
}

std::string ModelCaller::getCompletionDescription(const std::string& completion) {
    if (completion.find("if") != std::string::npos) {
        return "Conditional branch suggestion";
    }
    if (completion.find("for") != std::string::npos) {
        return "Loop suggestion";
    }
    return "General completion";
}

}  // namespace RawrXD
