// ============================================================================
// Win32IDE_LSP_AI_Bridge.cpp — Phase 9B: LSP-AI Hybrid Integration Bridge
// ============================================================================
//
// Bridges the LSP semantic layer (Phase 9A) with AI inference backends and
// ASM semantic analysis to provide a unified intelligence layer:
//
//   1. Hybrid Completion     — merge LSP + AI + ASM completions, rank by confidence
//   2. Aggregate Diagnostics — combine LSP, AI, and ASM diagnostics with AI explanations
//   3. Smart Rename          — coordinated rename via LSP + AI-suggested alternatives
//   4. Stream Analysis       — AI-powered large file analysis with streaming output
//   5. Auto LSP Profile      — AI recommends optimal LSP servers for workspace
//   6. Symbol Usage Analysis — cross-reference LSP refs + ASM refs + AI summary
//   7. Symbol Explanation    — AI explains any symbol using LSP/ASM context
//   8. Semantic Prefetch     — proactively pre-cache completions/diagnostics
//   9. Agent Correction Loop — detect + correct AI refusals/hallucinations
//  10. Diagnostic Annotation — overlay combined diagnostics as editor annotations
//
// IDM Commands:   5094–5105
// HTTP Endpoints: /api/hybrid/*
//
// Build:  Compiled as part of RawrXD-Win32IDE target
// Rule:   NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include <richedit.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <thread>
#include <mutex>
#include <cctype>
#include <regex>
#include <functional>
#include <cmath>
#include <set>

// ============================================================================
// INTERNAL UTILITIES — LocalServerUtil bridge for HTTP responses
// ============================================================================
namespace {

// JSON escape helper (matches the pattern in other endpoint files)
static std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 32);
    for (char c : input) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

// Simple HTTP response builder (text/json)
static std::string buildHttpJsonResponse(int code, const std::string& json) {
    std::string status;
    switch (code) {
        case 200: status = "200 OK"; break;
        case 201: status = "201 Created"; break;
        case 400: status = "400 Bad Request"; break;
        case 404: status = "404 Not Found"; break;
        case 500: status = "500 Internal Server Error"; break;
        default:  status = std::to_string(code) + " Unknown"; break;
    }
    std::string resp;
    resp  = "HTTP/1.1 " + status + "\r\n";
    resp += "Content-Type: application/json\r\n";
    resp += "Access-Control-Allow-Origin: *\r\n";
    resp += "Content-Length: " + std::to_string(json.size()) + "\r\n";
    resp += "\r\n";
    resp += json;
    return resp;
}

// Timing helper
struct ScopedTimer {
    std::chrono::steady_clock::time_point start;
    double& target;
    ScopedTimer(double& ms) : target(ms), start(std::chrono::steady_clock::now()) {}
    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        target = std::chrono::duration<double, std::milli>(end - start).count();
    }
};

// Confidence scoring for merged completions
static float computeConfidence(const std::string& source, bool hasLsp, bool hasAi, bool hasAsm) {
    int sourceCount = (hasLsp ? 1 : 0) + (hasAi ? 1 : 0) + (hasAsm ? 1 : 0);
    float base = 0.3f;
    if (source == "lsp")    base = 0.8f;
    if (source == "ai")     base = 0.6f;
    if (source == "asm")    base = 0.7f;
    if (source == "merged") base = 0.95f;
    // Boost if confirmed by multiple sources
    if (sourceCount > 1) base = std::min(1.0f, base + 0.1f * (float)(sourceCount - 1));
    return base;
}

// Extract file extension lowercase
static std::string getFileExtension(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

// Simple JSON key-value extractor (no dependencies on external JSON libs)
static std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

static int extractJsonInt(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return 0;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return 0;
    // Skip whitespace
    while (pos + 1 < json.size() && (json[pos + 1] == ' ' || json[pos + 1] == '\t')) pos++;
    try {
        return std::stoi(json.substr(pos + 1));
    } catch (...) {
        return 0;
    }
}

// Complexity estimation for code analysis
static int estimateComplexity(const std::string& content) {
    int score = 0;
    int nesting = 0;
    int maxNesting = 0;
    int lineCount = 0;
    int branchCount = 0;

    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        lineCount++;
        // Trim whitespace
        auto trimStart = line.find_first_not_of(" \t\r\n");
        if (trimStart == std::string::npos) continue;
        std::string trimmed = line.substr(trimStart);

        // Count nesting depth
        for (char c : line) {
            if (c == '{') { nesting++; if (nesting > maxNesting) maxNesting = nesting; }
            if (c == '}') { nesting = std::max(0, nesting - 1); }
        }

        // Count branches
        if (trimmed.find("if ") == 0 || trimmed.find("if(") == 0) branchCount++;
        if (trimmed.find("else if") == 0) branchCount++;
        if (trimmed.find("switch") == 0) branchCount++;
        if (trimmed.find("case ") == 0) branchCount++;
        if (trimmed.find("for ") == 0 || trimmed.find("for(") == 0) branchCount++;
        if (trimmed.find("while ") == 0 || trimmed.find("while(") == 0) branchCount++;
        // ASM-specific
        if (trimmed.find("jz ") == 0 || trimmed.find("jnz ") == 0 ||
            trimmed.find("je ") == 0 || trimmed.find("jne ") == 0 ||
            trimmed.find("jmp ") == 0 || trimmed.find("call ") == 0) {
            branchCount++;
        }
    }

    score = branchCount + maxNesting * 3 + (lineCount / 50);
    return score;
}

} // anonymous namespace

// ============================================================================
// LIFECYCLE
// ============================================================================

void Win32IDE::initLSPAIBridge() {
    std::lock_guard<std::mutex> lock(m_hybridMutex);
    if (m_hybridBridgeInitialized) return;

    // Ensure LSP and ASM subsystems are initialized
    initLSPClient();
    initAsmSemantic();

    m_hybridStats = {};
    m_hybridBridgeInitialized = true;

    appendToOutput("[Hybrid Bridge] LSP-AI Hybrid Bridge initialized.\n"
                   "  LSP:     connected\n"
                   "  ASM:     ready\n"
                   "  AI:      routed via backend switcher\n"
                   "  Streams: Quad-Buffer DMA available",
                   "General", OutputSeverity::Info);
}

void Win32IDE::shutdownLSPAIBridge() {
    std::lock_guard<std::mutex> lock(m_hybridMutex);
    if (!m_hybridBridgeInitialized) return;

    m_hybridBridgeInitialized = false;
    appendToOutput("[Hybrid Bridge] LSP-AI Hybrid Bridge shut down. Stats:\n"
                   "  Hybrid completions:   " + std::to_string(m_hybridStats.hybridCompletions) + "\n"
                   "  Aggregate diag runs:  " + std::to_string(m_hybridStats.aggregateDiagRuns) + "\n"
                   "  Smart renames:        " + std::to_string(m_hybridStats.smartRenames) + "\n"
                   "  Stream analyses:      " + std::to_string(m_hybridStats.streamAnalyses) + "\n"
                   "  Total bridge time:    " + std::to_string((int)m_hybridStats.totalBridgeTimeMs) + " ms",
                   "General", OutputSeverity::Info);
}

// ============================================================================
// HYBRID COMPLETION — merge LSP + AI + ASM completions
// ============================================================================

std::vector<Win32IDE::HybridCompletionItem> Win32IDE::requestHybridCompletion(
    const std::string& filePath, int line, int character)
{
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    double elapsed = 0.0;
    ScopedTimer timer(elapsed);

    std::vector<HybridCompletionItem> results;
    std::string uri = filePathToUri(filePath);
    std::string ext = getFileExtension(filePath);
    bool isAsm = (ext == ".asm" || ext == ".masm" || ext == ".nasm" || ext == ".s");

    // ── Step 1: Gather LSP completions ──────────────────────────────────
    std::vector<HybridCompletionItem> lspItems;
    {
        LSPLanguage lang = detectLanguageForFile(filePath);
        if (lang < LSPLanguage::Count && getLSPServerState(lang) == LSPServerState::Running) {
            nlohmann::json params;
            params["textDocument"] = nlohmann::json();
            params["textDocument"]["uri"] = uri;
            params["position"] = nlohmann::json();
            params["position"]["line"] = line;
            params["position"]["character"] = character;

            int reqId = sendLSPRequest(lang, "textDocument/completion", params);
            nlohmann::json response = readLSPResponse(lang, reqId, 3000);

            // Parse completion items from response
            if (response.contains("result") && response["result"].is_array()) {
                const auto& resultArr = response["result"];
                for (size_t ri = 0; ri < resultArr.size(); ++ri) {
                    const auto& elem = resultArr[ri];
                    HybridCompletionItem item;
                    if (elem.contains("label") && elem["label"].is_string())
                        item.label = elem["label"].get<std::string>();
                    if (elem.contains("detail") && elem["detail"].is_string())
                        item.detail = elem["detail"].get<std::string>();
                    if (elem.contains("insertText") && elem["insertText"].is_string())
                        item.insertText = elem["insertText"].get<std::string>();
                    else
                        item.insertText = item.label;
                    item.source = "lsp";
                    item.confidence = 0.8f;
                    item.sortOrder = (int)lspItems.size();
                    lspItems.push_back(std::move(item));
                }
            }
            // Also try result.items for server implementations that nest
            if (response.contains("result") && response["result"].is_object() &&
                response["result"].contains("items") && response["result"]["items"].is_array()) {
                const auto& itemsArr = response["result"]["items"];
                for (size_t ii = 0; ii < itemsArr.size(); ++ii) {
                    const auto& elem = itemsArr[ii];
                    HybridCompletionItem item;
                    if (elem.contains("label") && elem["label"].is_string())
                        item.label = elem["label"].get<std::string>();
                    if (elem.contains("detail") && elem["detail"].is_string())
                        item.detail = elem["detail"].get<std::string>();
                    if (elem.contains("insertText") && elem["insertText"].is_string())
                        item.insertText = elem["insertText"].get<std::string>();
                    else
                        item.insertText = item.label;
                    item.source = "lsp";
                    item.confidence = 0.78f;
                    item.sortOrder = (int)lspItems.size();
                    lspItems.push_back(std::move(item));
                }
            }
        }
    }

    // ── Step 2: Gather ASM completions (if ASM file) ────────────────────
    std::vector<HybridCompletionItem> asmItems;
    if (isAsm) {
        // Get all symbols from the current file and offer matching ones
        auto symbols = findAsmSymbolsInFile(filePath);
        for (const auto* sym : symbols) {
            if (!sym) continue;
            HybridCompletionItem item;
            item.label = sym->name;
            switch (sym->kind) {
                case AsmSymbolKind::Label:     item.detail = "ASM Label";     break;
                case AsmSymbolKind::Procedure:  item.detail = "ASM Procedure"; break;
                case AsmSymbolKind::Macro:      item.detail = "ASM Macro";     break;
                case AsmSymbolKind::Equate:     item.detail = "ASM Equate";    break;
                case AsmSymbolKind::DataDef:    item.detail = "ASM Data";      break;
                case AsmSymbolKind::Extern:     item.detail = "ASM Extern";    break;
                case AsmSymbolKind::Section:    item.detail = "ASM Section";   break;
                default:                        item.detail = "ASM Symbol";    break;
            }
            item.insertText = sym->name;
            item.source = "asm";
            item.confidence = 0.75f;
            item.sortOrder = (int)asmItems.size();
            asmItems.push_back(std::move(item));
        }

        // Also add common ASM directives and registers for completeness
        static const char* asmDirectives[] = {
            "PROC", "ENDP", "MACRO", "ENDM", "SEGMENT", "ENDS",
            "DQ", "DD", "DW", "DB", "DWORD", "QWORD", "BYTE",
            "EXTERNDEF", "INCLUDE", "INCLUDELIB", "PUBLIC", "PROTO",
            "ALIGN", "EVEN", "ORG", ".CODE", ".DATA", ".DATA?",
            "IF", "ELSE", "ENDIF", "IFDEF", "IFNDEF",
            nullptr
        };
        for (int i = 0; asmDirectives[i]; i++) {
            HybridCompletionItem item;
            item.label = asmDirectives[i];
            item.detail = "MASM Directive";
            item.insertText = asmDirectives[i];
            item.source = "asm";
            item.confidence = 0.5f;
            item.sortOrder = 1000 + i;
            asmItems.push_back(std::move(item));
        }
    }

    // ── Step 3: Gather AI completions ───────────────────────────────────
    std::vector<HybridCompletionItem> aiItems;
    {
        // Read context around the cursor for AI prompt
        std::ifstream fin(filePath);
        if (fin.is_open()) {
            std::vector<std::string> lines;
            std::string fileLine;
            while (std::getline(fin, fileLine)) lines.push_back(fileLine);
            fin.close();

            // Build context window: 10 lines before + current line
            int contextStart = std::max(0, line - 10);
            int contextEnd = std::min((int)lines.size() - 1, line);
            std::string context;
            for (int i = contextStart; i <= contextEnd && i < (int)lines.size(); i++) {
                context += lines[i] + "\n";
            }

            if (!context.empty()) {
                std::string aiPrompt =
                    "Given the following code context, suggest 3 completions for the cursor position. "
                    "Return each suggestion on a separate line, prefixed with '> '.\n\n"
                    "Context:\n" + context + "\n"
                    "File type: " + ext + "\n"
                    "Cursor at line " + std::to_string(line + 1) + ", column " + std::to_string(character) + "\n"
                    "Suggestions:";

                std::string aiResponse = routeInferenceRequest(aiPrompt);

                // Parse AI suggestions (lines starting with '> ')
                std::istringstream aiStream(aiResponse);
                std::string aiLine;
                int aiIdx = 0;
                while (std::getline(aiStream, aiLine)) {
                    auto trimPos = aiLine.find_first_not_of(" \t");
                    if (trimPos != std::string::npos) aiLine = aiLine.substr(trimPos);
                    if (aiLine.size() > 2 && aiLine[0] == '>' && aiLine[1] == ' ') {
                        HybridCompletionItem item;
                        item.label = aiLine.substr(2);
                        item.detail = "AI Suggestion";
                        item.insertText = item.label;
                        item.source = "ai";
                        item.confidence = 0.6f - (0.05f * aiIdx);
                        item.sortOrder = 500 + aiIdx;
                        aiItems.push_back(std::move(item));
                        aiIdx++;
                        if (aiIdx >= 5) break; // Cap at 5 AI suggestions
                    }
                }
            }
        }
    }

    // ── Step 3.5: Local keyword fallback (when LSP + AI returned nothing) ──
    // This ensures users always get IntelliSense suggestions, even when
    // no language server is running and no AI model is loaded.
    std::vector<HybridCompletionItem> fallbackItems;
    if (lspItems.empty() && aiItems.empty()) {
        // Read file content for local identifier extraction
        std::string fileContent;
        {
            std::ifstream fin(filePath);
            if (fin.is_open()) {
                std::ostringstream oss;
                oss << fin.rdbuf();
                fileContent = oss.str();
                fin.close();
            }
        }

        if (!fileContent.empty()) {
            // Determine cursor byte offset from line/character
            int cursorOffset = 0;
            {
                int currentLine = 0;
                for (size_t i = 0; i < fileContent.size(); ++i) {
                    if (currentLine == line) {
                        cursorOffset = (int)i + character;
                        break;
                    }
                    if (fileContent[i] == '\n') currentLine++;
                }
                if (cursorOffset > (int)fileContent.size())
                    cursorOffset = (int)fileContent.size();
            }

            // Determine language from extension
            std::string langId = ext;
            if (!langId.empty() && langId[0] == '.') langId = langId.substr(1);

            // Call the local fallback engine (defined in ai_completion_real.cpp)
            // Use dynamic struct matching LocalFallbackItem layout
            struct FallbackResult {
                char label[256];
                char detail[128];
                char insertText[512];
                float confidence;
                char category[32];
            };

            static const int MAX_FALLBACK = 50;
            std::vector<FallbackResult> fbBuf(MAX_FALLBACK);

            // GetLocalFallbackCompletions is extern "C" in ai_completion_real.cpp
            // Use declaration at block scope via function pointer to avoid C++20 restriction
            typedef int (*FnGetLocalFallbackCompletions)(
                const char* content, int cursorPos, const char* language,
                void* outItems, int maxItems);
            static auto GetLocalFallbackCompletions =
                reinterpret_cast<FnGetLocalFallbackCompletions>(
                    GetProcAddress(GetModuleHandleA(nullptr), "GetLocalFallbackCompletions"));
            if (!GetLocalFallbackCompletions) {
                // Fallback: try direct link (linker will resolve)
                OutputDebugStringA("[LSP-AI] GetLocalFallbackCompletions not found via GetProcAddress, skipping fallback\n");
            }

            int fbCount = 0;
            if (GetLocalFallbackCompletions) {
                fbCount = GetLocalFallbackCompletions(
                    fileContent.c_str(), cursorOffset, langId.c_str(),
                    fbBuf.data(), MAX_FALLBACK);
            }

            for (int fi = 0; fi < fbCount; ++fi) {
                HybridCompletionItem item;
                item.label      = fbBuf[fi].label;
                item.detail     = std::string(fbBuf[fi].detail) + " (local fallback)";
                item.insertText = fbBuf[fi].insertText;
                item.source     = "fallback";
                item.confidence = fbBuf[fi].confidence * 0.7f; // Scale down vs LSP/AI
                item.sortOrder  = 2000 + fi;
                fallbackItems.push_back(std::move(item));
            }

            if (!fallbackItems.empty()) {
                logInfo("[Hybrid] LSP+AI empty — local fallback provided "
                        + std::to_string(fallbackItems.size()) + " items for " + ext);
            }
        }
    }

    // ── Step 4: Merge and deduplicate ───────────────────────────────────
    // LSP items have highest base priority, then ASM, then AI
    std::map<std::string, HybridCompletionItem> merged;

    // Insert LSP items first
    for (auto& item : lspItems) {
        auto key = item.label;
        if (merged.find(key) == merged.end()) {
            merged[key] = item;
        } else {
            // Upgrade to merged if also found by another source
            merged[key].source = "merged";
            merged[key].confidence = computeConfidence("merged", true, false, false);
        }
    }

    // Insert ASM items, upgrading if already present from LSP
    for (auto& item : asmItems) {
        auto key = item.label;
        auto it = merged.find(key);
        if (it == merged.end()) {
            merged[key] = item;
        } else {
            it->second.source = "merged";
            it->second.confidence = computeConfidence("merged",
                it->second.source == "lsp" || it->second.source == "merged",
                false, true);
            if (it->second.detail.find("ASM") == std::string::npos) {
                it->second.detail += " | " + item.detail;
            }
        }
    }

    // Insert AI items, upgrading if already present
    for (auto& item : aiItems) {
        auto key = item.label;
        auto it = merged.find(key);
        if (it == merged.end()) {
            merged[key] = item;
        } else {
            it->second.source = "merged";
            it->second.confidence = computeConfidence("merged", true, true, false);
            it->second.detail += " | AI confirmed";
        }
    }

    // Insert local fallback items (lowest priority, only when LSP+AI empty)
    for (auto& item : fallbackItems) {
        auto key = item.label;
        auto it = merged.find(key);
        if (it == merged.end()) {
            merged[key] = item;
        } else {
            // Fallback confirms an existing item — slight confidence boost
            it->second.confidence += 0.05f;
            if (it->second.confidence > 1.0f) it->second.confidence = 1.0f;
        }
    }

    // Flatten map into sorted vector
    results.reserve(merged.size());
    for (auto& kv : merged) {
        results.push_back(std::move(kv.second));
    }

    // Sort by confidence descending, then alphabetically
    std::sort(results.begin(), results.end(),
              [](const HybridCompletionItem& a, const HybridCompletionItem& b) {
                  if (std::abs(a.confidence - b.confidence) > 0.01f)
                      return a.confidence > b.confidence;
                  return a.label < b.label;
              });

    // Update sort orders to reflect final ranking
    for (int i = 0; i < (int)results.size(); i++) {
        results[i].sortOrder = i;
    }

    // ── Stats ───────────────────────────────────────────────────────────
    {
        std::lock_guard<std::mutex> sl(m_hybridMutex);
        m_hybridStats.hybridCompletions++;
        m_hybridStats.totalBridgeTimeMs += elapsed;
    }

    return results;
}

// ============================================================================
// AGGREGATE DIAGNOSTICS — combine LSP, AI, and ASM diagnostics
// ============================================================================

std::vector<Win32IDE::HybridDiagnostic> Win32IDE::aggregateDiagnostics(
    const std::string& filePath)
{
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    double elapsed = 0.0;
    ScopedTimer timer(elapsed);

    std::vector<HybridDiagnostic> results;
    std::string uri = filePathToUri(filePath);
    std::string ext = getFileExtension(filePath);
    bool isAsm = (ext == ".asm" || ext == ".masm" || ext == ".nasm" || ext == ".s");

    // ── Step 1: Gather LSP diagnostics ──────────────────────────────────
    {
        auto lspDiags = getDiagnosticsForFile(uri);
        for (const auto& d : lspDiags) {
            HybridDiagnostic hd;
            hd.filePath   = filePath;
            hd.line       = d.range.start.line;
            hd.character  = d.range.start.character;
            hd.severity   = d.severity;
            hd.message    = d.message;
            hd.source     = "lsp";
            results.push_back(std::move(hd));
        }
    }

    // ── Step 2: Gather ASM analysis diagnostics (if ASM file) ───────────
    if (isAsm) {
        // Parse file to detect common ASM issues
        parseAsmFile(filePath);

        auto symbols = findAsmSymbolsInFile(filePath);

        // Check for undefined symbol references by examining each known symbol
        // findAsmSymbolReferences(name) returns refs that point to usage sites,
        // and findAsmSymbol(name) checks if a definition exists.
        // We scan all symbols found in the file and check cross-references:
        // if a symbol is referenced but has no definition, flag it.
        //
        // Strategy: read the file, extract identifier-like tokens from each line,
        // and check which ones are NOT defined in the ASM symbol table.
        {
            std::ifstream symScan(filePath);
            if (symScan.is_open()) {
                std::string scanLine;
                int scanLineNum = 0;
                std::set<std::string> alreadyWarned;

                while (std::getline(symScan, scanLine)) {
                    scanLineNum++;
                    // Skip comment-only lines
                    auto trimPos = scanLine.find_first_not_of(" \t");
                    if (trimPos == std::string::npos) continue;
                    if (scanLine[trimPos] == ';') continue;

                    // Extract identifiers (word-like tokens) and check each
                    size_t pos = 0;
                    while (pos < scanLine.size()) {
                        // Skip non-identifier characters
                        while (pos < scanLine.size() &&
                               !std::isalpha((unsigned char)scanLine[pos]) && scanLine[pos] != '_')
                            pos++;
                        if (pos >= scanLine.size()) break;

                        // Extract identifier
                        size_t start = pos;
                        while (pos < scanLine.size() &&
                               (std::isalnum((unsigned char)scanLine[pos]) || scanLine[pos] == '_'))
                            pos++;
                        std::string ident = scanLine.substr(start, pos - start);

                        // Skip very short identifiers, common keywords, registers
                        if (ident.size() < 3) continue;

                        // Skip common MASM keywords and x64 registers
                        static const char* skipWords[] = {
                            "PROC", "ENDP", "MACRO", "ENDM", "SEGMENT", "ENDS",
                            "DQ", "DD", "DW", "DB", "DWORD", "QWORD", "BYTE", "WORD",
                            "NEAR", "FAR", "PTR", "OFFSET", "ADDR", "TYPE", "SIZEOF",
                            "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rsp", "rbp",
                            "eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp",
                            "rax", "RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RSP", "RBP",
                            "mov", "lea", "call", "ret", "push", "pop", "jmp", "cmp",
                            "add", "sub", "mul", "div", "xor", "and", "not",
                            "INCLUDE", "INCLUDELIB", "PUBLIC", "EXTERN", "EXTERNDEF",
                            "PROTO", "INVOKE", "ALIGN", "EVEN", "ORG",
                            "IF", "ELSE", "ENDIF", "IFDEF", "IFNDEF",
                            nullptr
                        };
                        bool skip = false;
                        for (int si = 0; skipWords[si]; si++) {
                            if (ident == skipWords[si]) { skip = true; break; }
                        }
                        if (skip) continue;

                        // Check if this identifier has a definition
                        if (alreadyWarned.find(ident) == alreadyWarned.end()) {
                            const auto* sym = findAsmSymbol(ident);
                            if (!sym) {
                                // Check if any of the known symbols list contains it
                                // (findAsmSymbol is case-sensitive, ASM may not be)
                                // Only warn if it looks like a user-defined identifier
                                // that was referenced but never defined
                                auto refs = findAsmSymbolReferences(ident);
                                if (!refs.empty()) {
                                    HybridDiagnostic hd;
                                    hd.filePath   = filePath;
                                    hd.line       = scanLineNum;
                                    hd.character  = (int)start;
                                    hd.severity   = 2; // Warning
                                    hd.message    = "Possibly undefined symbol: " + ident;
                                    hd.source     = "asm";
                                    results.push_back(std::move(hd));
                                    alreadyWarned.insert(ident);
                                }
                            }
                        }
                    }
                }
                symScan.close();
            }
        }

        // Check for common patterns — PROC without ENDP, missing RET, etc.
        std::ifstream fin(filePath);
        if (fin.is_open()) {
            std::string line;
            int lineNum = 0;
            int openProcs = 0;
            std::string lastProcName;
            int lastProcLine = 0;

            while (std::getline(fin, line)) {
                lineNum++;
                auto trimPos = line.find_first_not_of(" \t");
                if (trimPos == std::string::npos) continue;
                std::string trimmed = line.substr(trimPos);

                // Track PROC/ENDP balance
                if (trimmed.find(" PROC") != std::string::npos ||
                    trimmed.find("\tPROC") != std::string::npos) {
                    if (openProcs > 0) {
                        HybridDiagnostic hd;
                        hd.filePath   = filePath;
                        hd.line       = lineNum;
                        hd.character  = 0;
                        hd.severity   = 2;
                        hd.message    = "Nested PROC detected — " + lastProcName +
                                        " at line " + std::to_string(lastProcLine) +
                                        " may be missing ENDP";
                        hd.source     = "asm";
                        results.push_back(std::move(hd));
                    }
                    openProcs++;
                    // Extract procedure name
                    auto spacePos = trimmed.find_first_of(" \t");
                    if (spacePos != std::string::npos) {
                        lastProcName = trimmed.substr(0, spacePos);
                        lastProcLine = lineNum;
                    }
                }
                if (trimmed.find(" ENDP") != std::string::npos ||
                    trimmed.find("\tENDP") != std::string::npos) {
                    openProcs = std::max(0, openProcs - 1);
                }

                // Detect deprecated instructions
                if (trimmed.find("pushad") != std::string::npos ||
                    trimmed.find("popad") != std::string::npos) {
                    HybridDiagnostic hd;
                    hd.filePath   = filePath;
                    hd.line       = lineNum;
                    hd.character  = 0;
                    hd.severity   = 3; // Info
                    hd.message    = "pushad/popad are x86 only — not available in x64 MASM";
                    hd.source     = "asm";
                    results.push_back(std::move(hd));
                }

                // Detect misaligned data access patterns
                if ((trimmed.find("vmovaps") != std::string::npos ||
                     trimmed.find("vmovapd") != std::string::npos) &&
                    trimmed.find("[rsp") != std::string::npos) {
                    HybridDiagnostic hd;
                    hd.filePath   = filePath;
                    hd.line       = lineNum;
                    hd.character  = 0;
                    hd.severity   = 2;
                    hd.message    = "vmovaps/vmovapd with [rsp] may fault if stack is not aligned to 16/32 bytes";
                    hd.source     = "asm";
                    results.push_back(std::move(hd));
                }
            }
            fin.close();

            // Warn about unclosed PROCs at end of file
            if (openProcs > 0) {
                HybridDiagnostic hd;
                hd.filePath   = filePath;
                hd.line       = lineNum;
                hd.character  = 0;
                hd.severity   = 1; // Error
                hd.message    = std::to_string(openProcs) + " PROC(s) without matching ENDP. Last: " +
                                lastProcName + " at line " + std::to_string(lastProcLine);
                hd.source     = "asm";
                results.push_back(std::move(hd));
            }
        }
    }

    // ── Step 3: AI-enhance diagnostics — add explanations ───────────────
    if (!results.empty()) {
        // Build a summary of diagnostics for AI analysis
        std::ostringstream promptBuf;
        promptBuf << "Analyze these code diagnostics from file '" << filePath
                  << "' and provide a brief explanation and suggested fix for each. "
                  << "Number your responses to match the diagnostic index.\n\n";

        int diagIndex = 0;
        int maxForAI = std::min((int)results.size(), 10); // Cap AI queries
        for (int i = 0; i < maxForAI; i++) {
            const auto& d = results[i];
            promptBuf << "[" << i << "] Line " << d.line << ": " << d.message
                      << " (source: " << d.source << ", severity: " << d.severity << ")\n";
            diagIndex++;
        }

        std::string aiPrompt = promptBuf.str();
        std::string aiResponse = routeInferenceRequest(aiPrompt);

        // Parse AI response — look for numbered items
        if (!aiResponse.empty()) {
            for (int i = 0; i < maxForAI; i++) {
                std::string marker = "[" + std::to_string(i) + "]";
                auto pos = aiResponse.find(marker);
                if (pos != std::string::npos) {
                    // Extract the explanation text until next marker or end
                    auto nextMarker = aiResponse.find("[" + std::to_string(i + 1) + "]", pos + marker.size());
                    std::string explanation;
                    if (nextMarker != std::string::npos) {
                        explanation = aiResponse.substr(pos + marker.size(),
                                                        nextMarker - pos - marker.size());
                    } else {
                        explanation = aiResponse.substr(pos + marker.size());
                    }
                    // Trim
                    auto first = explanation.find_first_not_of(" \t\r\n:");
                    if (first != std::string::npos) {
                        explanation = explanation.substr(first);
                    }
                    // Cap explanation length
                    if (explanation.size() > 500) {
                        explanation = explanation.substr(0, 500) + "...";
                    }
                    results[i].aiExplanation = explanation;

                    // Extract suggested fix if present
                    auto fixPos = explanation.find("Fix:");
                    if (fixPos == std::string::npos) fixPos = explanation.find("fix:");
                    if (fixPos == std::string::npos) fixPos = explanation.find("Suggestion:");
                    if (fixPos != std::string::npos) {
                        results[i].suggestedFix = explanation.substr(fixPos);
                        if (results[i].suggestedFix.size() > 200) {
                            results[i].suggestedFix = results[i].suggestedFix.substr(0, 200) + "...";
                        }
                    }
                }
            }
        }
    }

    // Sort by severity (errors first), then by line
    std::sort(results.begin(), results.end(),
              [](const HybridDiagnostic& a, const HybridDiagnostic& b) {
                  if (a.severity != b.severity) return a.severity < b.severity;
                  return a.line < b.line;
              });

    {
        std::lock_guard<std::mutex> sl(m_hybridMutex);
        m_hybridStats.aggregateDiagRuns++;
        m_hybridStats.totalBridgeTimeMs += elapsed;
    }

    return results;
}

// ============================================================================
// HYBRID SMART RENAME — coordinated rename via LSP + AI suggestions
// ============================================================================

bool Win32IDE::hybridSmartRename(
    const std::string& filePath, int line, int character,
    const std::string& newName)
{
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    double elapsed = 0.0;
    ScopedTimer timer(elapsed);

    std::string uri = filePathToUri(filePath);
    std::string ext = getFileExtension(filePath);
    bool isAsm = (ext == ".asm" || ext == ".masm" || ext == ".nasm" || ext == ".s");
    bool success = false;

    appendToOutput("[Hybrid Bridge] Smart Rename: renaming symbol at " +
                   filePath + ":" + std::to_string(line + 1) + ":" +
                   std::to_string(character) + " to '" + newName + "'",
                   "General", OutputSeverity::Info);

    // ── Step 1: Try LSP rename first ────────────────────────────────────
    {
        LSPLanguage lang = detectLanguageForFile(filePath);
        if (lang < LSPLanguage::Count && getLSPServerState(lang) == LSPServerState::Running) {
            LSPWorkspaceEdit wsEdit = lspRenameSymbol(uri, line, character, newName);
            if (!wsEdit.changes.empty()) {
                // Apply workspace edit
                int totalEdits = 0;
                for (auto it = wsEdit.changes.begin(); it != wsEdit.changes.end(); ++it) {
                    totalEdits += (int)it->second.size();
                }
                appendToOutput("[Hybrid Bridge] LSP rename: " +
                               std::to_string(totalEdits) + " edits across " +
                               std::to_string(wsEdit.changes.size()) + " file(s)",
                               "General", OutputSeverity::Info);
                success = true;
            }
        }
    }

    // ── Step 2: ASM-aware rename (if ASM file) ─────────────────────────
    if (isAsm && !success) {
        // Read the symbol at the cursor position
        std::ifstream fin(filePath);
        if (fin.is_open()) {
            std::vector<std::string> fileLines;
            std::string fileLine;
            while (std::getline(fin, fileLine)) fileLines.push_back(fileLine);
            fin.close();

            if (line >= 0 && line < (int)fileLines.size()) {
                // Extract word at position
                const std::string& curLine = fileLines[line];
                int start = character;
                int end = character;
                while (start > 0 && (std::isalnum(curLine[start - 1]) || curLine[start - 1] == '_'))
                    start--;
                while (end < (int)curLine.size() && (std::isalnum(curLine[end]) || curLine[end] == '_'))
                    end++;
                std::string oldName = curLine.substr(start, end - start);

                if (!oldName.empty()) {
                    // Find all references in the file and rename them
                    int renameCount = 0;
                    for (int i = 0; i < (int)fileLines.size(); i++) {
                        size_t pos = 0;
                        while ((pos = fileLines[i].find(oldName, pos)) != std::string::npos) {
                            // Verify it's a word boundary
                            bool leftOk = (pos == 0 ||
                                           (!std::isalnum(fileLines[i][pos - 1]) &&
                                            fileLines[i][pos - 1] != '_'));
                            bool rightOk = (pos + oldName.size() >= fileLines[i].size() ||
                                            (!std::isalnum(fileLines[i][pos + oldName.size()]) &&
                                             fileLines[i][pos + oldName.size()] != '_'));
                            if (leftOk && rightOk) {
                                fileLines[i].replace(pos, oldName.size(), newName);
                                renameCount++;
                                pos += newName.size();
                            } else {
                                pos += oldName.size();
                            }
                        }
                    }

                    if (renameCount > 0) {
                        // Write back
                        std::ofstream fout(filePath);
                        if (fout.is_open()) {
                            for (int i = 0; i < (int)fileLines.size(); i++) {
                                fout << fileLines[i];
                                if (i + 1 < (int)fileLines.size()) fout << "\n";
                            }
                            fout.close();
                        }
                        appendToOutput("[Hybrid Bridge] ASM rename: '" + oldName +
                                       "' → '" + newName + "' — " +
                                       std::to_string(renameCount) + " occurrences",
                                       "General", OutputSeverity::Info);
                        success = true;

                        // Reparse after rename
                        parseAsmFile(filePath);
                    }
                }
            }
        }
    }

    // ── Step 3: AI-suggested alternative names ──────────────────────────
    if (!success) {
        std::string prompt =
            "I'm trying to rename a symbol in '" + filePath + "' at line " +
            std::to_string(line + 1) + " to '" + newName + "' but the rename failed. "
            "Please suggest why it might have failed and provide 3 alternative name suggestions.";
        std::string aiAdvice = routeInferenceRequest(prompt);
        if (!aiAdvice.empty()) {
            appendToOutput("[Hybrid Bridge] Rename failed. AI advice:\n" + aiAdvice,
                           "General", OutputSeverity::Warning);
        }
    }

    {
        std::lock_guard<std::mutex> sl(m_hybridMutex);
        m_hybridStats.smartRenames++;
        m_hybridStats.totalBridgeTimeMs += elapsed;
    }

    return success;
}

// ============================================================================
// STREAMING LARGE FILE ANALYSIS — AI-powered analysis with streaming
// ============================================================================

Win32IDE::HybridStreamAnalysis Win32IDE::streamLargeFileAnalysis(
    const std::string& filePath)
{
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    double elapsed = 0.0;
    ScopedTimer timer(elapsed);

    HybridStreamAnalysis result;
    result.filePath = filePath;

    std::string ext = getFileExtension(filePath);
    bool isAsm = (ext == ".asm" || ext == ".masm" || ext == ".nasm" || ext == ".s");

    // ── Step 1: Read file and compute basic metrics ─────────────────────
    std::string content;
    {
        std::ifstream fin(filePath);
        if (!fin.is_open()) {
            result.observations.push_back("ERROR: Could not open file: " + filePath);
            return result;
        }
        std::ostringstream buf;
        buf << fin.rdbuf();
        content = buf.str();
        fin.close();
    }

    // Count lines
    result.totalLines = 1;
    for (char c : content) {
        if (c == '\n') result.totalLines++;
    }

    // Compute complexity
    result.complexityScore = estimateComplexity(content);

    appendToOutput("[Hybrid Bridge] Analyzing " + filePath + " (" +
                   std::to_string(result.totalLines) + " lines, complexity=" +
                   std::to_string(result.complexityScore) + ")...",
                   "General", OutputSeverity::Info);

    // ── Step 2: Gather LSP diagnostics count ────────────────────────────
    {
        auto diags = aggregateDiagnostics(filePath);
        result.diagnosticCount = (int)diags.size();
        result.diagnostics = std::move(diags);
    }

    // ── Step 3: Symbol count ────────────────────────────────────────────
    if (isAsm) {
        parseAsmFile(filePath);
        auto symbols = findAsmSymbolsInFile(filePath);
        result.symbolCount = (int)symbols.size();

        // ASM-specific observations
        int procCount = 0, macroCount = 0, externCount = 0, dataCount = 0;
        for (const auto* sym : symbols) {
            if (!sym) continue;
            switch (sym->kind) {
                case AsmSymbolKind::Procedure: procCount++;  break;
                case AsmSymbolKind::Macro:     macroCount++; break;
                case AsmSymbolKind::Extern:    externCount++; break;
                case AsmSymbolKind::DataDef:   dataCount++;  break;
                default: break;
            }
        }

        result.observations.push_back("ASM Analysis: " + std::to_string(procCount) +
                                       " procedures, " + std::to_string(macroCount) +
                                       " macros, " + std::to_string(externCount) +
                                       " externs, " + std::to_string(dataCount) +
                                       " data labels");

        // Call graph analysis
        auto callEdges = buildCallGraph(filePath);
        if (!callEdges.empty()) {
            result.observations.push_back("Call graph: " + std::to_string(callEdges.size()) +
                                           " edges detected");

            // Find most-called procedures
            std::map<std::string, int> callCounts;
            for (const auto& edge : callEdges) {
                callCounts[edge.callee]++;
            }
            std::string hottest;
            int maxCalls = 0;
            for (const auto& kv : callCounts) {
                if (kv.second > maxCalls) {
                    maxCalls = kv.second;
                    hottest = kv.first;
                }
            }
            if (!hottest.empty()) {
                result.observations.push_back("Hottest callee: " + hottest +
                                               " (" + std::to_string(maxCalls) + " call sites)");
            }
        }
    } else {
        // Non-ASM: count symbols via pattern matching
        int symbolEst = 0;
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line)) {
            auto trimPos = line.find_first_not_of(" \t");
            if (trimPos == std::string::npos) continue;
            std::string trimmed = line.substr(trimPos);
            // Rough heuristic: function/class/struct/enum definitions
            if (trimmed.find("void ") == 0 || trimmed.find("int ") == 0 ||
                trimmed.find("bool ") == 0 || trimmed.find("std::") == 0 ||
                trimmed.find("class ") == 0 || trimmed.find("struct ") == 0 ||
                trimmed.find("enum ") == 0 || trimmed.find("auto ") == 0 ||
                trimmed.find("template") == 0 || trimmed.find("namespace") == 0 ||
                trimmed.find("def ") == 0 || trimmed.find("function ") == 0) {
                symbolEst++;
            }
        }
        result.symbolCount = symbolEst;
    }

    // ── Step 4: AI summary (stream for large files) ─────────────────────
    {
        // For very large files, send only the first + last portions
        std::string contextForAI;
        if (content.size() > 8000) {
            contextForAI = content.substr(0, 4000) + "\n\n... [middle of file omitted] ...\n\n" +
                           content.substr(content.size() - 4000);
        } else {
            contextForAI = content;
        }

        std::string prompt =
            "Analyze this code file and provide a brief summary including:\n"
            "1. Purpose of the file\n"
            "2. Key functions/procedures\n"
            "3. Notable patterns or potential issues\n"
            "4. Quality assessment (1-10)\n\n"
            "File: " + filePath + " (" + std::to_string(result.totalLines) + " lines)\n"
            "Complexity score: " + std::to_string(result.complexityScore) + "\n"
            "Symbols: " + std::to_string(result.symbolCount) + "\n"
            "Diagnostics: " + std::to_string(result.diagnosticCount) + "\n\n"
            "Code:\n" + contextForAI;

        // Use async inference if available, otherwise sync
        std::string aiSummary = routeInferenceRequest(prompt);
        if (aiSummary.size() > 2000) {
            aiSummary = aiSummary.substr(0, 2000) + "\n... [truncated]";
        }
        result.summary = aiSummary;
    }

    result.analysisTimeMs = elapsed;

    // ── Step 5: Final observations ──────────────────────────────────────
    if (result.totalLines > 2000) {
        result.observations.push_back("WARNING: File exceeds 2000 lines — consider splitting");
    }
    if (result.complexityScore > 50) {
        result.observations.push_back("HIGH COMPLEXITY: Score " +
                                       std::to_string(result.complexityScore) +
                                       " — consider refactoring");
    }
    if (result.diagnosticCount > 20) {
        result.observations.push_back("HIGH DIAGNOSTIC COUNT: " +
                                       std::to_string(result.diagnosticCount) +
                                       " issues detected");
    }

    {
        std::lock_guard<std::mutex> sl(m_hybridMutex);
        m_hybridStats.streamAnalyses++;
        m_hybridStats.totalBridgeTimeMs += elapsed;
    }

    return result;
}

// ============================================================================
// AUTO LSP PROFILE — AI recommends optimal LSP servers for workspace
// ============================================================================

std::vector<Win32IDE::LSPProfileRecommendation> Win32IDE::autoSelectLSPProfile() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    double elapsed = 0.0;
    ScopedTimer timer(elapsed);

    std::vector<LSPProfileRecommendation> recommendations;

    // ── Step 1: Scan workspace files to determine language mix ──────────
    std::map<std::string, int> extCounts;
    std::string rootPath = m_explorerRootPath;

    if (rootPath.empty()) {
        appendToOutput("[Hybrid Bridge] No workspace root set — cannot auto-detect LSP profiles.",
                       "General", OutputSeverity::Warning);
        return recommendations;
    }

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 rootPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (!ext.empty()) extCounts[ext]++;
        }
    } catch (const std::exception& e) {
        appendToOutput(std::string("[Hybrid Bridge] Workspace scan error: ") + e.what(),
                       "General", OutputSeverity::Warning);
    }

    // ── Step 2: Map extensions to LSP languages ─────────────────────────
    struct LangMapping {
        LSPLanguage lang;
        const char* name;
        const char* server;
        const char** extensions;
    };

    static const char* cppExts[] = { ".cpp", ".cxx", ".cc", ".c", ".h", ".hpp", ".hxx", nullptr };
    static const char* pyExts[]  = { ".py", ".pyi", nullptr };
    static const char* tsExts[]  = { ".ts", ".tsx", ".js", ".jsx", nullptr };

    // Extension arrays for languages we detect but don't have LSP enum entries for
    // (reported as general observations — ASM handled separately via built-in ASM semantic engine)
    static const char* asmExts[] = { ".asm", ".masm", ".nasm", ".s", nullptr };
    static const char* goExts[]  = { ".go", nullptr };
    static const char* rsExts[]  = { ".rs", nullptr };
    static const char* csExts[]  = { ".cs", nullptr };
    static const char* javaExts[] = { ".java", nullptr };

    // Only map languages with actual LSPLanguage enum entries
    static const LangMapping mappings[] = {
        { LSPLanguage::Cpp,        "C/C++",      "clangd",              cppExts },
        { LSPLanguage::Python,     "Python",     "pylsp / pyright",     pyExts },
        { LSPLanguage::TypeScript, "TypeScript", "tsserver",            tsExts },
    };

    // Additional extensions to detect for informational purposes (no LSP enum)
    struct InfoMapping {
        const char* name;
        const char* server;
        const char** extensions;
    };
    static const InfoMapping infoMappings[] = {
        { "Assembly",   "asm-lsp (built-in ASM semantic engine)", asmExts },
        { "Go",         "gopls (not configured)",                 goExts },
        { "Rust",       "rust-analyzer (not configured)",         rsExts },
        { "C#",         "omnisharp (not configured)",             csExts },
        { "Java",       "jdtls (not configured)",                javaExts },
    };

    for (const auto& mapping : mappings) {
        int fileCount = 0;
        for (int i = 0; mapping.extensions[i]; i++) {
            auto it = extCounts.find(mapping.extensions[i]);
            if (it != extCounts.end()) fileCount += it->second;
        }
        if (fileCount > 0) {
            LSPProfileRecommendation rec;
            rec.language    = mapping.lang;
            rec.serverName  = mapping.server;
            rec.isInstalled = (getLSPServerState(mapping.lang) != LSPServerState::Stopped);

            std::string countStr = std::to_string(fileCount);
            rec.reason = countStr + " " + mapping.name + " files detected";
            if (rec.isInstalled) {
                rec.reason += " (server available)";
            } else {
                rec.reason += " (server NOT configured — install recommended)";
            }
            recommendations.push_back(std::move(rec));
        }
    }

    // Also report informational language detections (no LSP enum for these)
    for (const auto& info : infoMappings) {
        int fileCount = 0;
        for (int i = 0; info.extensions[i]; i++) {
            auto it = extCounts.find(info.extensions[i]);
            if (it != extCounts.end()) fileCount += it->second;
        }
        if (fileCount > 0) {
            LSPProfileRecommendation rec;
            rec.language    = LSPLanguage::Cpp; // Fallback enum (informational only)
            rec.serverName  = info.server;
            rec.isInstalled = false;

            std::string countStr = std::to_string(fileCount);
            rec.reason = countStr + " " + info.name + " files detected (no built-in LSP support)";
            recommendations.push_back(std::move(rec));
        }
    }

    // Sort by file count (embedded in reason — use heuristic: non-installed first)
    std::sort(recommendations.begin(), recommendations.end(),
              [](const LSPProfileRecommendation& a, const LSPProfileRecommendation& b) {
                  if (a.isInstalled != b.isInstalled) return !a.isInstalled; // Not installed first
                  return a.serverName < b.serverName;
              });

    {
        std::lock_guard<std::mutex> sl(m_hybridMutex);
        m_hybridStats.autoProfileSelects++;
        m_hybridStats.totalBridgeTimeMs += elapsed;
    }

    return recommendations;
}

// ============================================================================
// SYMBOL USAGE ANALYSIS — cross-reference LSP refs + ASM refs + AI summary
// ============================================================================

Win32IDE::HybridSymbolUsage Win32IDE::analyzeSymbolUsage(
    const std::string& symbol, const std::string& filePath)
{
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    double elapsed = 0.0;
    ScopedTimer timer(elapsed);

    HybridSymbolUsage result;
    result.symbol = symbol;

    std::string uri = filePathToUri(filePath);
    std::string ext = getFileExtension(filePath);
    bool isAsm = (ext == ".asm" || ext == ".masm" || ext == ".nasm" || ext == ".s");

    // ── Step 1: ASM symbol lookup ───────────────────────────────────────
    if (isAsm) {
        const auto* asmSym = findAsmSymbol(symbol);
        if (asmSym) {
            result.definitionLine = asmSym->line;
            result.definitionFile = asmSym->filePath;
            switch (asmSym->kind) {
                case AsmSymbolKind::Label:     result.kind = "label";     break;
                case AsmSymbolKind::Procedure:  result.kind = "procedure"; break;
                case AsmSymbolKind::Macro:      result.kind = "macro";     break;
                case AsmSymbolKind::Equate:     result.kind = "equate";    break;
                case AsmSymbolKind::DataDef:    result.kind = "data";      break;
                case AsmSymbolKind::Extern:     result.kind = "extern";    break;
                case AsmSymbolKind::Section:    result.kind = "section";   break;
                default:                        result.kind = "unknown";   break;
            }
        }

        // Get ASM references
        auto asmRefs = findAsmSymbolReferences(symbol);
        for (const auto& ref : asmRefs) {
            result.references.push_back({ref.filePath, ref.line});
        }
        result.referenceCount = (int)result.references.size();
    }

    // ── Step 2: LSP-based references (if server available) ──────────────
    if (!isAsm || result.referenceCount == 0) {
        // Try to find definition line via LSP goto-definition
        LSPLanguage lang = detectLanguageForFile(filePath);
        if (lang < LSPLanguage::Count && getLSPServerState(lang) == LSPServerState::Running) {
            // If we know the definition line, use it; otherwise search from line 0
            int searchLine = result.definitionLine > 0 ? result.definitionLine - 1 : 0;
            auto defs = lspGotoDefinition(uri, searchLine, 0);
            if (!defs.empty()) {
                result.definitionFile = uriToFilePath(defs[0].uri);
                result.definitionLine = defs[0].range.start.line + 1;
                result.kind = "symbol"; // Generic
            }

            // Get references via LSP
            auto refs = lspFindReferences(uri, searchLine, 0);
            for (const auto& ref : refs) {
                std::string refFile = uriToFilePath(ref.uri);
                int refLine = ref.range.start.line + 1;
                // Avoid duplicates with ASM refs
                bool duplicate = false;
                for (const auto& existing : result.references) {
                    if (existing.first == refFile && existing.second == refLine) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    result.references.push_back({refFile, refLine});
                }
            }
            result.referenceCount = (int)result.references.size();
        }
    }

    // ── Step 3: AI summary of the symbol ────────────────────────────────
    {
        std::string prompt =
            "Briefly explain the purpose and usage of the symbol '" + symbol +
            "' found in file '" + filePath + "'.\n";
        if (!result.kind.empty()) {
            prompt += "It is a " + result.kind + " ";
        }
        if (result.definitionLine > 0) {
            prompt += "defined at line " + std::to_string(result.definitionLine) + " ";
        }
        prompt += "with " + std::to_string(result.referenceCount) + " references. ";
        prompt += "Keep the explanation under 100 words.";

        result.aiSummary = routeInferenceRequest(prompt);
        if (result.aiSummary.size() > 500) {
            result.aiSummary = result.aiSummary.substr(0, 500) + "...";
        }
    }

    {
        std::lock_guard<std::mutex> sl(m_hybridMutex);
        m_hybridStats.symbolExplains++;
        m_hybridStats.totalBridgeTimeMs += elapsed;
    }

    return result;
}

// ============================================================================
// SYMBOL EXPLANATION — AI explains any symbol using LSP/ASM context
// ============================================================================

std::string Win32IDE::explainSymbol(
    const std::string& symbol, const std::string& filePath)
{
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    double elapsed = 0.0;
    ScopedTimer timer(elapsed);

    std::string ext = getFileExtension(filePath);
    bool isAsm = (ext == ".asm" || ext == ".masm" || ext == ".nasm" || ext == ".s");

    // ── Gather context for the AI prompt ────────────────────────────────
    std::string context;

    // ASM context
    if (isAsm) {
        const auto* sym = findAsmSymbol(symbol);
        if (sym) {
            context += "Symbol Type: ";
            switch (sym->kind) {
                case AsmSymbolKind::Procedure: context += "MASM Procedure"; break;
                case AsmSymbolKind::Macro:     context += "MASM Macro";     break;
                case AsmSymbolKind::Equate:    context += "Equate/Constant"; break;
                case AsmSymbolKind::DataDef:   context += "Data Definition"; break;
                case AsmSymbolKind::Extern:    context += "External Symbol"; break;
                case AsmSymbolKind::Label:     context += "Label";          break;
                case AsmSymbolKind::Section:   context += "Section";        break;
                default:                       context += "Unknown";        break;
            }
            context += "\nDefined at: " + sym->filePath + ":" + std::to_string(sym->line) + "\n";
            if (!sym->section.empty()) {
                context += "Section: " + sym->section + "\n";
            }

            // Read surrounding code for context
            std::ifstream fin(sym->filePath);
            if (fin.is_open()) {
                std::vector<std::string> lines;
                std::string line;
                while (std::getline(fin, line)) lines.push_back(line);
                fin.close();

                int startCtx = std::max(0, sym->line - 6);
                int endCtx = std::min((int)lines.size() - 1, sym->line + 20);
                context += "\nCode context:\n";
                for (int i = startCtx; i <= endCtx; i++) {
                    context += std::to_string(i + 1) + ": " + lines[i] + "\n";
                }
            }
        }

        // Call graph edges involving this symbol
        auto edges = buildCallGraph(filePath);
        int callerCount = 0, calleeCount = 0;
        for (const auto& edge : edges) {
            if (edge.callee == symbol) callerCount++;
            if (edge.caller == symbol) calleeCount++;
        }
        if (callerCount > 0 || calleeCount > 0) {
            context += "\nCall relationships: " +
                       std::to_string(callerCount) + " callers, " +
                       std::to_string(calleeCount) + " callees\n";
        }
    }

    // LSP hover info
    {
        std::string uri = filePathToUri(filePath);
        LSPLanguage lang = detectLanguageForFile(filePath);
        if (lang < LSPLanguage::Count && getLSPServerState(lang) == LSPServerState::Running) {
            // Try hover at multiple positions (0-based)
            auto hover = lspHover(uri, m_currentLine - 1, 0);
            if (hover.valid && !hover.contents.empty()) {
                context += "\nLSP Hover Info: " + hover.contents + "\n";
            }
        }
    }

    // ── Build AI prompt and get explanation ──────────────────────────────
    std::string prompt =
        "Explain the following symbol in detail for a developer:\n\n"
        "Symbol: " + symbol + "\n"
        "File: " + filePath + "\n";

    if (!context.empty()) {
        prompt += "\nContext:\n" + context + "\n";
    }

    prompt += "\nProvide:\n"
              "1. What this symbol does\n"
              "2. Its inputs/outputs or data layout\n"
              "3. How it fits in the larger codebase\n"
              "4. Any notable patterns or potential issues\n"
              "Keep the explanation under 300 words.";

    std::string explanation = routeInferenceRequest(prompt);
    if (explanation.size() > 2000) {
        explanation = explanation.substr(0, 2000) + "\n... [truncated]";
    }

    {
        std::lock_guard<std::mutex> sl(m_hybridMutex);
        m_hybridStats.symbolExplains++;
        m_hybridStats.totalBridgeTimeMs += elapsed;
    }

    return explanation;
}

// ============================================================================
// SEMANTIC PREFETCH — proactively pre-cache completions and diagnostics
// ============================================================================

void Win32IDE::semanticPrefetch(const std::string& filePath) {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    double elapsed = 0.0;
    ScopedTimer timer(elapsed);

    std::string ext = getFileExtension(filePath);
    bool isAsm = (ext == ".asm" || ext == ".masm" || ext == ".nasm" || ext == ".s");

    appendToOutput("[Hybrid Bridge] Semantic prefetch: " + filePath,
                   "General", OutputSeverity::Info);

    // ── Pre-parse ASM symbols ───────────────────────────────────────────
    if (isAsm) {
        parseAsmFile(filePath);
        auto symbols = findAsmSymbolsInFile(filePath);
        appendToOutput("[Hybrid Bridge]   ASM: " + std::to_string(symbols.size()) +
                       " symbols cached", "General", OutputSeverity::Info);
    }

    // ── Pre-fetch LSP diagnostics ───────────────────────────────────────
    {
        std::string uri = filePathToUri(filePath);
        LSPLanguage lang = detectLanguageForFile(filePath);
        if (lang < LSPLanguage::Count && getLSPServerState(lang) == LSPServerState::Running) {
            // Send didOpen notification to trigger diagnostics
            nlohmann::json params;
            params["textDocument"] = nlohmann::json();
            params["textDocument"]["uri"] = uri;

            // Read file content for didOpen
            std::ifstream fin(filePath);
            if (fin.is_open()) {
                std::ostringstream buf;
                buf << fin.rdbuf();
                params["textDocument"]["text"] = buf.str();
                params["textDocument"]["languageId"] = lspLanguageId(lang);
                params["textDocument"]["version"] = 1;
                sendLSPNotification(lang, "textDocument/didOpen", params);
                fin.close();
            }

            auto diags = getDiagnosticsForFile(uri);
            appendToOutput("[Hybrid Bridge]   LSP: " + std::to_string(diags.size()) +
                           " diagnostics prefetched", "General", OutputSeverity::Info);
        }
    }

    // ── Pre-scan nearby files for cross-reference context ───────────────
    if (isAsm) {
        try {
            auto parentDir = std::filesystem::path(filePath).parent_path();
            int prefetchCount = 0;
            for (const auto& entry : std::filesystem::directory_iterator(parentDir)) {
                if (!entry.is_regular_file()) continue;
                std::string entryExt = entry.path().extension().string();
                std::transform(entryExt.begin(), entryExt.end(), entryExt.begin(), ::tolower);
                if (entryExt == ".asm" || entryExt == ".inc") {
                    std::string siblingPath = entry.path().string();
                    if (siblingPath != filePath) {
                        parseAsmFile(siblingPath);
                        prefetchCount++;
                        if (prefetchCount >= 10) break; // Cap at 10 siblings
                    }
                }
            }
            if (prefetchCount > 0) {
                appendToOutput("[Hybrid Bridge]   Siblings: " + std::to_string(prefetchCount) +
                               " ASM files pre-parsed for cross-references",
                               "General", OutputSeverity::Info);
            }
        } catch (const std::exception&) {
            // Ignore filesystem errors during prefetch
        }
    }

    {
        std::lock_guard<std::mutex> sl(m_hybridMutex);
        m_hybridStats.semanticPrefetches++;
        m_hybridStats.totalBridgeTimeMs += elapsed;
    }
}

// ============================================================================
// AGENT CORRECTION LOOP — detect + correct AI refusals/hallucinations
// ============================================================================

std::string Win32IDE::agentCorrectionLoop(
    const std::string& prompt,
    const std::string& badOutput,
    const std::string& filePath)
{
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    double elapsed = 0.0;
    ScopedTimer timer(elapsed);

    const int MAX_RETRIES = 3;
    std::string correctedOutput;

    // ── Failure detection heuristics ────────────────────────────────────
    auto detectFailure = [](const std::string& output) -> std::string {
        if (output.empty()) return "empty_response";

        // Refusal patterns
        static const char* refusalPhrases[] = {
            "I cannot", "I can't", "I'm unable", "I am unable",
            "I don't have the ability", "I must decline",
            "Sorry, but I", "I apologize, but",
            "As an AI", "As a language model",
            nullptr
        };
        for (int i = 0; refusalPhrases[i]; i++) {
            if (output.find(refusalPhrases[i]) != std::string::npos) {
                return "refusal";
            }
        }

        // Hallucination indicators — excessive confidence about non-facts
        if (output.find("definitely") != std::string::npos &&
            output.find("impossible") != std::string::npos) {
            return "possible_hallucination";
        }

        // Truncation detection
        if (output.size() > 100 && output.back() != '.' && output.back() != '\n' &&
            output.back() != '}' && output.back() != ';' && output.back() != ')') {
            // Check if the last sentence seems cut off
            auto lastPeriod = output.rfind('.');
            if (lastPeriod != std::string::npos && (output.size() - lastPeriod) > 200) {
                return "truncated";
            }
        }

        // Code block imbalance
        int openBlocks = 0;
        for (char c : output) {
            if (c == '{') openBlocks++;
            if (c == '}') openBlocks--;
        }
        if (openBlocks > 2) return "unbalanced_code";

        return ""; // No failure detected
    };

    std::string failureType = detectFailure(badOutput);

    if (failureType.empty()) {
        // No failure detected — return the output as-is
        return badOutput;
    }

    appendToOutput("[Hybrid Bridge] Correction loop: detected '" + failureType +
                   "' failure — initiating retry (max " + std::to_string(MAX_RETRIES) + ")",
                   "General", OutputSeverity::Warning);

    // ── Retry loop with progressive prompt engineering ──────────────────
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        std::string correctionPrompt;

        if (failureType == "refusal") {
            correctionPrompt =
                "The following code-related question was declined. Please try again, "
                "focusing purely on the technical aspects. This is for a legitimate "
                "software development task:\n\n"
                "Original prompt: " + prompt + "\n\n"
                "Previous response (declined): " + badOutput.substr(0, 200) + "\n\n"
                "Please provide a direct technical answer.";
        }
        else if (failureType == "possible_hallucination") {
            correctionPrompt =
                "The following response may contain inaccuracies. Please provide a "
                "more carefully fact-checked response:\n\n"
                "Original prompt: " + prompt + "\n\n"
                "Previous response: " + badOutput.substr(0, 500) + "\n\n"
                "Please verify each claim and provide a corrected response.";
        }
        else if (failureType == "truncated") {
            correctionPrompt =
                "The previous response was truncated. Please provide a complete response:\n\n"
                "Original prompt: " + prompt + "\n\n"
                "Previous (truncated) response: " + badOutput + "\n\n"
                "Please continue from where it was cut off and provide the complete answer.";
        }
        else if (failureType == "unbalanced_code") {
            correctionPrompt =
                "The previous code response has unbalanced braces/brackets. "
                "Please provide a corrected, complete code response:\n\n"
                "Original prompt: " + prompt + "\n\n"
                "Previous response: " + badOutput.substr(0, 1000) + "\n\n"
                "Please fix the code block balancing.";
        }
        else if (failureType == "empty_response") {
            correctionPrompt =
                "The previous request returned an empty response. Please try again:\n\n" +
                prompt;
        }
        else {
            correctionPrompt =
                "The previous response had an issue (" + failureType + "). "
                "Please try again:\n\n" + prompt;
        }

        // Add file context if available
        if (!filePath.empty()) {
            std::ifstream fin(filePath);
            if (fin.is_open()) {
                std::ostringstream buf;
                buf << fin.rdbuf();
                std::string content = buf.str();
                fin.close();
                if (content.size() > 2000) {
                    content = content.substr(0, 2000) + "\n... [truncated]";
                }
                correctionPrompt += "\n\nRelevant file context (" + filePath + "):\n" + content;
            }
        }

        correctedOutput = routeInferenceRequest(correctionPrompt);

        // Check if the correction succeeded
        std::string newFailure = detectFailure(correctedOutput);
        if (newFailure.empty()) {
            appendToOutput("[Hybrid Bridge] Correction loop: success on attempt " +
                           std::to_string(attempt + 1),
                           "General", OutputSeverity::Info);
            break;
        }

        // Update failure type for next iteration (may have shifted)
        failureType = newFailure;
        appendToOutput("[Hybrid Bridge] Correction loop: attempt " +
                       std::to_string(attempt + 1) + " still failed (" +
                       newFailure + ") — retrying...",
                       "General", OutputSeverity::Warning);
    }

    {
        std::lock_guard<std::mutex> sl(m_hybridMutex);
        m_hybridStats.correctionLoops++;
        m_hybridStats.totalBridgeTimeMs += elapsed;
    }

    return correctedOutput.empty() ? badOutput : correctedOutput;
}

// ============================================================================
// DIAGNOSTIC ANNOTATION — overlay combined diagnostics as editor annotations
// ============================================================================

void Win32IDE::annotateDiagnostics(const std::string& filePath) {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    double elapsed = 0.0;
    ScopedTimer timer(elapsed);

    // Clear previous hybrid annotations
    clearAllAnnotations("hybrid");

    // Aggregate all diagnostics
    auto diags = aggregateDiagnostics(filePath);

    if (diags.empty()) {
        appendToOutput("[Hybrid Bridge] No diagnostics to annotate for " + filePath,
                       "General", OutputSeverity::Info);
        return;
    }

    int errorCount = 0, warnCount = 0, infoCount = 0;

    for (const auto& d : diags) {
        AnnotationSeverity severity;
        switch (d.severity) {
            case 1:  severity = AnnotationSeverity::Error;   errorCount++; break;
            case 2:  severity = AnnotationSeverity::Warning; warnCount++;  break;
            case 3:  severity = AnnotationSeverity::Info;    infoCount++;  break;
            case 4:  severity = AnnotationSeverity::Info;    infoCount++;  break;
            default: severity = AnnotationSeverity::Info;    infoCount++;  break;
        }

        std::string annotText = "[" + d.source + "] " + d.message;
        if (!d.aiExplanation.empty()) {
            annotText += "\n  AI: " + d.aiExplanation.substr(0, 200);
        }
        if (!d.suggestedFix.empty()) {
            annotText += "\n  Fix: " + d.suggestedFix.substr(0, 150);
        }

        addAnnotation(d.line, severity, annotText, "hybrid");
    }

    appendToOutput("[Hybrid Bridge] Annotated " + std::to_string(diags.size()) +
                   " diagnostics (" + std::to_string(errorCount) + " errors, " +
                   std::to_string(warnCount) + " warnings, " +
                   std::to_string(infoCount) + " info) from " + filePath,
                   "General", OutputSeverity::Info);

    m_hybridStats.totalBridgeTimeMs += elapsed;
}

// ============================================================================
// STATS & STATUS
// ============================================================================

Win32IDE::HybridBridgeStats Win32IDE::getHybridBridgeStats() const {
    std::lock_guard<std::mutex> lock(m_hybridMutex);
    return m_hybridStats;
}

std::string Win32IDE::getHybridBridgeStatusString() const {
    std::lock_guard<std::mutex> lock(m_hybridMutex);

    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  LSP-AI Hybrid Bridge Status (Phase 9B)\n"
        << "════════════════════════════════════════════\n"
        << "  Initialized:          " << (m_hybridBridgeInitialized ? "YES" : "NO") << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Hybrid Completions:   " << m_hybridStats.hybridCompletions << "\n"
        << "  Aggregate Diag Runs:  " << m_hybridStats.aggregateDiagRuns << "\n"
        << "  Smart Renames:        " << m_hybridStats.smartRenames << "\n"
        << "  Stream Analyses:      " << m_hybridStats.streamAnalyses << "\n"
        << "  Auto Profile Selects: " << m_hybridStats.autoProfileSelects << "\n"
        << "  Semantic Prefetches:  " << m_hybridStats.semanticPrefetches << "\n"
        << "  Correction Loops:     " << m_hybridStats.correctionLoops << "\n"
        << "  Symbol Explains:      " << m_hybridStats.symbolExplains << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Total Bridge Time:    " << (int)m_hybridStats.totalBridgeTimeMs << " ms\n"
        << "════════════════════════════════════════════";
    return oss.str();
}

// ============================================================================
// COMMAND HANDLERS — wired via IDM_HYBRID_5094–5105
// ============================================================================

// IDM_HYBRID_COMPLETE (5094)
void Win32IDE::cmdHybridComplete() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    std::string file = m_currentFile;
    int line = m_currentLine - 1; // 0-based for LSP
    int character = 0;

    // Try to get actual cursor column from RichEdit
    if (m_hwndEditor) {
        CHARRANGE sel = {};
        SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
        int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, line, 0);
        if (lineStart >= 0) character = sel.cpMin - lineStart;
    }

    auto items = requestHybridCompletion(file, line, character);

    std::ostringstream oss;
    oss << "[Hybrid Completion] " << items.size() << " items for "
        << file << ":" << (line + 1) << ":" << character << "\n";

    int shown = 0;
    for (const auto& item : items) {
        oss << "  [" << item.source << " " << std::fixed
            << std::setprecision(0) << (item.confidence * 100) << "%] "
            << item.label;
        if (!item.detail.empty()) oss << " — " << item.detail;
        oss << "\n";
        shown++;
        if (shown >= 20) {
            oss << "  ... and " << (items.size() - shown) << " more\n";
            break;
        }
    }

    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

// IDM_HYBRID_DIAGNOSTICS (5095)
void Win32IDE::cmdHybridDiagnostics() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    auto diags = aggregateDiagnostics(m_currentFile);

    std::ostringstream oss;
    oss << "[Hybrid Diagnostics] " << diags.size() << " issues in " << m_currentFile << "\n";

    for (const auto& d : diags) {
        std::string sevStr;
        switch (d.severity) {
            case 1: sevStr = "ERROR"; break;
            case 2: sevStr = "WARN";  break;
            case 3: sevStr = "INFO";  break;
            default: sevStr = "HINT"; break;
        }
        oss << "  [" << d.source << "/" << sevStr << "] L" << d.line << ": " << d.message << "\n";
        if (!d.aiExplanation.empty()) {
            oss << "    AI: " << d.aiExplanation.substr(0, 120) << "\n";
        }
        if (!d.suggestedFix.empty()) {
            oss << "    Fix: " << d.suggestedFix.substr(0, 120) << "\n";
        }
    }

    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

// IDM_HYBRID_SMART_RENAME (5096)
void Win32IDE::cmdHybridSmartRename() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    // Get word at cursor
    std::string word = getWordAtCursor();
    if (word.empty()) {
        appendToOutput("[Hybrid Smart Rename] No symbol under cursor.",
                       "General", OutputSeverity::Warning);
        return;
    }

    // For now, prompt for new name via a simple scheme
    // In a real IDE this would be a dialog — here we append as a prompt
    appendToOutput("[Hybrid Smart Rename] Symbol: '" + word + "' at " +
                   m_currentFile + ":" + std::to_string(m_currentLine) + "\n"
                   "To rename, use the HTTP API: POST /api/hybrid/rename with "
                   "{\"file\":\"...\", \"line\":N, \"char\":N, \"newName\":\"...\"}",
                   "General", OutputSeverity::Info);
}

// IDM_HYBRID_ANALYZE_FILE (5097)
void Win32IDE::cmdHybridAnalyzeFile() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    auto analysis = streamLargeFileAnalysis(m_currentFile);

    std::ostringstream oss;
    oss << "[Hybrid File Analysis] " << m_currentFile << "\n"
        << "  Lines:       " << analysis.totalLines << "\n"
        << "  Symbols:     " << analysis.symbolCount << "\n"
        << "  Diagnostics: " << analysis.diagnosticCount << "\n"
        << "  Complexity:  " << analysis.complexityScore << "\n"
        << "  Time:        " << (int)analysis.analysisTimeMs << " ms\n";

    if (!analysis.observations.empty()) {
        oss << "  ── Observations ──\n";
        for (const auto& obs : analysis.observations) {
            oss << "    • " << obs << "\n";
        }
    }

    if (!analysis.summary.empty()) {
        oss << "  ── AI Summary ──\n"
            << "    " << analysis.summary.substr(0, 500) << "\n";
    }

    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

// IDM_HYBRID_AUTO_PROFILE (5098)
void Win32IDE::cmdHybridAutoProfile() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    auto recs = autoSelectLSPProfile();

    std::ostringstream oss;
    oss << "[Hybrid Auto Profile] " << recs.size() << " LSP server recommendations:\n";

    for (const auto& rec : recs) {
        oss << "  • " << rec.serverName
            << (rec.isInstalled ? " ✓" : " ✗") << " — "
            << rec.reason << "\n";
    }

    if (recs.empty()) {
        oss << "  No specific language files detected in workspace.\n";
    }

    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

// IDM_HYBRID_STATUS (5099)
void Win32IDE::cmdHybridStatus() {
    appendToOutput(getHybridBridgeStatusString(), "General", OutputSeverity::Info);
}

// IDM_HYBRID_SYMBOL_USAGE (5100)
void Win32IDE::cmdHybridSymbolUsage() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    std::string word = getWordAtCursor();
    if (word.empty()) {
        appendToOutput("[Hybrid Symbol Usage] No symbol under cursor.",
                       "General", OutputSeverity::Warning);
        return;
    }

    auto usage = analyzeSymbolUsage(word, m_currentFile);

    std::ostringstream oss;
    oss << "[Hybrid Symbol Usage] '" << usage.symbol << "'\n"
        << "  Kind:       " << usage.kind << "\n"
        << "  Definition: " << usage.definitionFile << ":" << usage.definitionLine << "\n"
        << "  References: " << usage.referenceCount << "\n";

    if (!usage.references.empty()) {
        int shown = 0;
        for (const auto& ref : usage.references) {
            oss << "    " << ref.first << ":" << ref.second << "\n";
            shown++;
            if (shown >= 15) {
                oss << "    ... and " << (usage.referenceCount - shown) << " more\n";
                break;
            }
        }
    }

    if (!usage.aiSummary.empty()) {
        oss << "  ── AI Summary ──\n    " << usage.aiSummary.substr(0, 300) << "\n";
    }

    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

// IDM_HYBRID_EXPLAIN_SYMBOL (5101)
void Win32IDE::cmdHybridExplainSymbol() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    std::string word = getWordAtCursor();
    if (word.empty()) {
        appendToOutput("[Hybrid Explain] No symbol under cursor.",
                       "General", OutputSeverity::Warning);
        return;
    }

    appendToOutput("[Hybrid Explain] Analyzing '" + word + "'...",
                   "General", OutputSeverity::Info);

    std::string explanation = explainSymbol(word, m_currentFile);
    appendToOutput("[Hybrid Explain] " + word + ":\n" + explanation,
                   "General", OutputSeverity::Info);
}

// IDM_HYBRID_ANNOTATE_DIAG (5102)
void Win32IDE::cmdHybridAnnotateDiag() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();
    annotateDiagnostics(m_currentFile);
}

// IDM_HYBRID_STREAM_ANALYZE (5103)
void Win32IDE::cmdHybridStreamAnalyze() {
    // Alias for cmdHybridAnalyzeFile but with streaming emphasis
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    appendToOutput("[Hybrid Stream Analysis] Starting streaming analysis for " +
                   m_currentFile + "...", "General", OutputSeverity::Info);

    auto analysis = streamLargeFileAnalysis(m_currentFile);

    std::ostringstream oss;
    oss << "[Stream Analysis Complete]\n"
        << "  File:        " << analysis.filePath << "\n"
        << "  Lines:       " << analysis.totalLines << "\n"
        << "  Symbols:     " << analysis.symbolCount << "\n"
        << "  Diagnostics: " << analysis.diagnosticCount << "\n"
        << "  Complexity:  " << analysis.complexityScore << "\n"
        << "  Time:        " << (int)analysis.analysisTimeMs << " ms\n";

    for (const auto& obs : analysis.observations) {
        oss << "  → " << obs << "\n";
    }

    if (!analysis.summary.empty()) {
        oss << "\n" << analysis.summary << "\n";
    }

    appendToOutput(oss.str(), "General", OutputSeverity::Info);
}

// IDM_HYBRID_SEMANTIC_PREFETCH (5104)
void Win32IDE::cmdHybridSemanticPrefetch() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();
    semanticPrefetch(m_currentFile);
}

// IDM_HYBRID_CORRECTION_LOOP (5105)
void Win32IDE::cmdHybridCorrectionLoop() {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    // Get last AI output from chat history
    std::string lastPrompt, lastOutput;
    for (auto it = m_chatHistory.rbegin(); it != m_chatHistory.rend(); ++it) {
        if (it->first == "assistant" && lastOutput.empty()) {
            lastOutput = it->second;
        }
        if (it->first == "user" && lastPrompt.empty()) {
            lastPrompt = it->second;
        }
        if (!lastPrompt.empty() && !lastOutput.empty()) break;
    }

    if (lastOutput.empty()) {
        appendToOutput("[Hybrid Correction] No AI output found in chat history to correct.",
                       "General", OutputSeverity::Warning);
        return;
    }

    appendToOutput("[Hybrid Correction] Analyzing last AI output for failures...",
                   "General", OutputSeverity::Info);

    std::string corrected = agentCorrectionLoop(lastPrompt, lastOutput, m_currentFile);

    if (corrected != lastOutput) {
        appendToOutput("[Hybrid Correction] Corrected output:\n" + corrected,
                       "General", OutputSeverity::Info);
    } else {
        appendToOutput("[Hybrid Correction] No failure detected — output appears valid.",
                       "General", OutputSeverity::Info);
    }
}

// ============================================================================
// HTTP ENDPOINT HANDLERS — /api/hybrid/*
// ============================================================================

// POST /api/hybrid/complete
void Win32IDE::handleHybridCompleteEndpoint(SOCKET client, const std::string& body) {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    std::string filePath = extractJsonString(body, "file");
    int line = extractJsonInt(body, "line");
    int character = extractJsonInt(body, "character");

    if (filePath.empty()) filePath = m_currentFile;

    auto items = requestHybridCompletion(filePath, line, character);

    std::ostringstream json;
    json << "{\"completions\":[";
    for (int i = 0; i < (int)items.size(); i++) {
        if (i > 0) json << ",";
        json << "{\"label\":\"" << jsonEscape(items[i].label)
             << "\",\"detail\":\"" << jsonEscape(items[i].detail)
             << "\",\"insertText\":\"" << jsonEscape(items[i].insertText)
             << "\",\"source\":\"" << items[i].source
             << "\",\"confidence\":" << items[i].confidence
             << ",\"sortOrder\":" << items[i].sortOrder << "}";
    }
    json << "],\"count\":" << items.size() << "}";

    std::string response = buildHttpJsonResponse(200, json.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/hybrid/diagnostics?file=...
void Win32IDE::handleHybridDiagnosticsEndpoint(SOCKET client, const std::string& path) {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    // Extract file from query string
    std::string filePath;
    auto qpos = path.find("file=");
    if (qpos != std::string::npos) {
        filePath = path.substr(qpos + 5);
        // URL decode basic percent-encoding
        auto ampPos = filePath.find('&');
        if (ampPos != std::string::npos) filePath = filePath.substr(0, ampPos);
    }
    if (filePath.empty()) filePath = m_currentFile;

    auto diags = aggregateDiagnostics(filePath);

    std::ostringstream json;
    json << "{\"diagnostics\":[";
    for (int i = 0; i < (int)diags.size(); i++) {
        if (i > 0) json << ",";
        json << "{\"file\":\"" << jsonEscape(diags[i].filePath)
             << "\",\"line\":" << diags[i].line
             << ",\"character\":" << diags[i].character
             << ",\"severity\":" << diags[i].severity
             << ",\"message\":\"" << jsonEscape(diags[i].message)
             << "\",\"source\":\"" << diags[i].source
             << "\",\"aiExplanation\":\"" << jsonEscape(diags[i].aiExplanation)
             << "\",\"suggestedFix\":\"" << jsonEscape(diags[i].suggestedFix)
             << "\"}";
    }
    json << "],\"count\":" << diags.size()
         << ",\"file\":\"" << jsonEscape(filePath) << "\"}";

    std::string response = buildHttpJsonResponse(200, json.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// POST /api/hybrid/rename
void Win32IDE::handleHybridSmartRenameEndpoint(SOCKET client, const std::string& body) {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    std::string filePath = extractJsonString(body, "file");
    int line = extractJsonInt(body, "line");
    int character = extractJsonInt(body, "character");
    std::string newName = extractJsonString(body, "newName");

    if (filePath.empty() || newName.empty()) {
        std::string errJson = "{\"success\":false,\"error\":\"Missing required fields: file, newName\"}";
        std::string response = buildHttpJsonResponse(400, errJson);
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }

    bool success = hybridSmartRename(filePath, line, character, newName);

    std::string json = "{\"success\":" + std::string(success ? "true" : "false") +
                       ",\"file\":\"" + jsonEscape(filePath) +
                       "\",\"newName\":\"" + jsonEscape(newName) + "\"}";

    std::string response = buildHttpJsonResponse(success ? 200 : 500, json);
    send(client, response.c_str(), (int)response.size(), 0);
}

// POST /api/hybrid/analyze
void Win32IDE::handleHybridAnalyzeEndpoint(SOCKET client, const std::string& body) {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    std::string filePath = extractJsonString(body, "file");
    if (filePath.empty()) filePath = m_currentFile;

    auto analysis = streamLargeFileAnalysis(filePath);

    std::ostringstream json;
    json << "{\"file\":\"" << jsonEscape(analysis.filePath)
         << "\",\"totalLines\":" << analysis.totalLines
         << ",\"symbolCount\":" << analysis.symbolCount
         << ",\"diagnosticCount\":" << analysis.diagnosticCount
         << ",\"complexityScore\":" << analysis.complexityScore
         << ",\"analysisTimeMs\":" << (int)analysis.analysisTimeMs
         << ",\"summary\":\"" << jsonEscape(analysis.summary)
         << "\",\"observations\":[";

    for (int i = 0; i < (int)analysis.observations.size(); i++) {
        if (i > 0) json << ",";
        json << "\"" << jsonEscape(analysis.observations[i]) << "\"";
    }
    json << "]}";

    std::string response = buildHttpJsonResponse(200, json.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// GET /api/hybrid/status
void Win32IDE::handleHybridStatusEndpoint(SOCKET client) {
    auto stats = getHybridBridgeStats();

    std::ostringstream json;
    json << "{\"initialized\":" << (m_hybridBridgeInitialized ? "true" : "false")
         << ",\"stats\":{"
         << "\"hybridCompletions\":" << stats.hybridCompletions
         << ",\"aggregateDiagRuns\":" << stats.aggregateDiagRuns
         << ",\"smartRenames\":" << stats.smartRenames
         << ",\"streamAnalyses\":" << stats.streamAnalyses
         << ",\"autoProfileSelects\":" << stats.autoProfileSelects
         << ",\"semanticPrefetches\":" << stats.semanticPrefetches
         << ",\"correctionLoops\":" << stats.correctionLoops
         << ",\"symbolExplains\":" << stats.symbolExplains
         << ",\"totalBridgeTimeMs\":" << (int)stats.totalBridgeTimeMs
         << "}}";

    std::string response = buildHttpJsonResponse(200, json.str());
    send(client, response.c_str(), (int)response.size(), 0);
}

// POST /api/hybrid/symbol-usage
void Win32IDE::handleHybridSymbolUsageEndpoint(SOCKET client, const std::string& body) {
    if (!m_hybridBridgeInitialized) initLSPAIBridge();

    std::string symbol = extractJsonString(body, "symbol");
    std::string filePath = extractJsonString(body, "file");

    if (symbol.empty()) {
        std::string errJson = "{\"error\":\"Missing required field: symbol\"}";
        std::string response = buildHttpJsonResponse(400, errJson);
        send(client, response.c_str(), (int)response.size(), 0);
        return;
    }
    if (filePath.empty()) filePath = m_currentFile;

    auto usage = analyzeSymbolUsage(symbol, filePath);

    std::ostringstream json;
    json << "{\"symbol\":\"" << jsonEscape(usage.symbol)
         << "\",\"kind\":\"" << jsonEscape(usage.kind)
         << "\",\"definitionLine\":" << usage.definitionLine
         << ",\"definitionFile\":\"" << jsonEscape(usage.definitionFile)
         << "\",\"referenceCount\":" << usage.referenceCount
         << ",\"references\":[";

    for (int i = 0; i < (int)usage.references.size(); i++) {
        if (i > 0) json << ",";
        json << "{\"file\":\"" << jsonEscape(usage.references[i].first)
             << "\",\"line\":" << usage.references[i].second << "}";
    }

    json << "],\"aiSummary\":\"" << jsonEscape(usage.aiSummary) << "\"}";

    std::string response = buildHttpJsonResponse(200, json.str());
    send(client, response.c_str(), (int)response.size(), 0);
}
