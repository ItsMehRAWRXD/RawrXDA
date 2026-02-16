// digestion_engine_stub.cpp — IDE build: production implementation (filename retained for link).
// RawrXD_DigestionEngine_Avx512: full source analysis, function extraction, dependency mapping,
// line/comment/stub/TODO metrics, JSON report. Not a no-op stub.

#include <windows.h>
#include <cwchar>
#include <fstream>
#include <sstream>
#include <vector>
#include <cctype>
#include <map>
#include <set>
#include <regex>
#include <algorithm>

// SCAFFOLD_240: digestion_engine_stub production


// Helper to convert wide to UTF-8
static std::string wideToUtf8(const wchar_t* src) {
    if (!src) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, src, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, src, -1, &result[0], len, nullptr, nullptr);
    return result;
}

// Helper to escape JSON strings
static std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

// Detect language from file extension
static std::string detectLanguage(const std::string& filepath) {
    size_t dot = filepath.find_last_of('.');
    if (dot == std::string::npos) return "unknown";
    std::string ext = filepath.substr(dot);
    // Convert to lowercase
    for (auto& c : ext) c = std::tolower(c);
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return "cpp";
    if (ext == ".h" || ext == ".hpp" || ext == ".hxx") return "header";
    if (ext == ".c") return "c";
    if (ext == ".py") return "python";
    if (ext == ".js" || ext == ".ts") return "javascript";
    if (ext == ".asm" || ext == ".s") return "assembly";
    if (ext == ".json") return "json";
    if (ext == ".cmake") return "cmake";
    return "unknown";
}

// Production implementation: full source analysis, line/comment/stub/TODO metrics, JSON report
extern "C" DWORD __stdcall RawrXD_DigestionEngine_Avx512(
    LPCWSTR wszSource,
    LPCWSTR wszOutput,
    DWORD   dwChunkSize,
    DWORD   dwThreads,
    DWORD   dwFlags,
    void (__stdcall *pfnProgress)(DWORD percent, DWORD taskId)
)
{
    (void)dwChunkSize;
    (void)dwThreads;
    (void)dwFlags;
    
    OutputDebugStringA("[RawrXD] Digestion engine entered\n");
    
    if (!wszSource || !wszOutput) {
        OutputDebugStringA("[RawrXD] Invalid parameters (source or output null)\n");
        return ERROR_INVALID_PARAMETER;
    }

    std::string srcPath = wideToUtf8(wszSource);
    std::string outPath = wideToUtf8(wszOutput);
    
    if (srcPath.empty() || outPath.empty()) {
        OutputDebugStringA("[RawrXD] Failed to convert wide strings\n");
        return ERROR_INVALID_PARAMETER;
    }

    char debugMsg[512];
    sprintf_s(debugMsg, sizeof(debugMsg), "[RawrXD] Digestion: src=%s, out=%s\n", srcPath.c_str(), outPath.c_str());
    OutputDebugStringA(debugMsg);
    
    // Progress: start
    if (pfnProgress) pfnProgress(0, 1);

    // Read the entire source file
    std::ifstream inFile(srcPath, std::ios::binary);
    if (!inFile.is_open()) {
        sprintf_s(debugMsg, sizeof(debugMsg), "[RawrXD] Failed to open source file: %s\n", srcPath.c_str());
        OutputDebugStringA(debugMsg);
        return ERROR_FILE_NOT_FOUND;
    }
    
    std::string content((std::istreambuf_iterator<char>(inFile)),
                         std::istreambuf_iterator<char>());
    inFile.close();

    if (pfnProgress) pfnProgress(10, 1);

    std::string language = detectLanguage(srcPath);

    // === Phase 1: Line & Comment Analysis ===
    int lineCount = 0;
    int blankLines = 0;
    int commentLines = 0;
    int codeLines = 0;
    int stubCount = 0;
    int todoCount = 0;
    int maxLineLength = 0;
    bool inBlockComment = false;

    std::istringstream lineStream(content);
    std::string line;
    while (std::getline(lineStream, line)) {
        lineCount++;
        if (static_cast<int>(line.length()) > maxLineLength)
            maxLineLength = static_cast<int>(line.length());

        std::string trimmed;
        size_t firstNonSpace = line.find_first_not_of(" \t\r");
        if (firstNonSpace == std::string::npos) {
            blankLines++;
            continue;
        }
        trimmed = line.substr(firstNonSpace);

        // Track block comments
        if (inBlockComment) {
            commentLines++;
            if (trimmed.find("*/") != std::string::npos) inBlockComment = false;
            continue;
        }
        if (trimmed.substr(0, 2) == "/*") {
            commentLines++;
            if (trimmed.find("*/") == std::string::npos) inBlockComment = true;
            continue;
        }
        if (trimmed.substr(0, 2) == "//" || trimmed[0] == '#') {
            commentLines++;
        } else {
            codeLines++;
        }

        // Detect stubs/TODOs
        std::string lower = trimmed;
        for (auto& c : lower) c = std::tolower(c);
        if (lower.find("stub") != std::string::npos) stubCount++;
        if (lower.find("todo") != std::string::npos || lower.find("fixme") != std::string::npos) todoCount++;
    }

    if (pfnProgress) pfnProgress(30, 1);

    // === Phase 2: Function/Class Extraction ===
    struct FunctionInfo {
        std::string name;
        std::string returnType;
        int startLine;
        int endLine;
        int complexity; // cyclomatic
    };

    std::vector<FunctionInfo> functions;
    std::vector<std::string> classNames;

    // Function detection regex (C/C++ style)
    std::regex funcPattern(R"((\w[\w:*&<>\s]*?)\s+(\w+)\s*\(([^)]*)\)\s*(?:const)?\s*\{)");
    std::regex classPattern(R"(\b(class|struct)\s+(\w+))");
    
    // Find functions
    auto funcBegin = std::sregex_iterator(content.begin(), content.end(), funcPattern);
    auto funcEnd = std::sregex_iterator();
    for (auto it = funcBegin; it != funcEnd; ++it) {
        FunctionInfo fi;
        fi.returnType = (*it)[1].str();
        fi.name = (*it)[2].str();
        // Calculate start line
        std::string prefix = content.substr(0, it->position());
        fi.startLine = static_cast<int>(std::count(prefix.begin(), prefix.end(), '\n')) + 1;
        
        // Find matching closing brace
        size_t bracePos = it->position() + it->length() - 1;
        int depth = 1;
        size_t pos = bracePos + 1;
        while (pos < content.size() && depth > 0) {
            if (content[pos] == '{') depth++;
            else if (content[pos] == '}') depth--;
            pos++;
        }
        std::string funcBody = content.substr(bracePos, pos - bracePos);
        std::string endPrefix = content.substr(0, pos);
        fi.endLine = static_cast<int>(std::count(endPrefix.begin(), endPrefix.end(), '\n')) + 1;
        
        // Calculate cyclomatic complexity for this function
        fi.complexity = 1;
        for (const auto& kw : {"if", "while", "for", "case", "catch"}) {
            size_t p = 0;
            std::string keyword(kw);
            while ((p = funcBody.find(keyword, p)) != std::string::npos) {
                bool leftOk = (p == 0 || !std::isalnum(funcBody[p-1]));
                bool rightOk = (p + keyword.size() >= funcBody.size() || !std::isalnum(funcBody[p + keyword.size()]));
                if (leftOk && rightOk) fi.complexity++;
                p += keyword.size();
            }
        }
        // Count && and ||
        for (size_t i = 0; i + 1 < funcBody.size(); ++i) {
            if ((funcBody[i] == '&' && funcBody[i+1] == '&') || (funcBody[i] == '|' && funcBody[i+1] == '|'))
                fi.complexity++;
        }
        
        functions.push_back(fi);
    }

    // Find classes/structs
    auto classBegin = std::sregex_iterator(content.begin(), content.end(), classPattern);
    for (auto it = classBegin; it != funcEnd; ++it) {
        classNames.push_back((*it)[2].str());
    }

    if (pfnProgress) pfnProgress(60, 1);

    // === Phase 3: Dependency Analysis ===
    std::set<std::string> includes;
    std::regex includePattern(R"(#include\s+[<"]([^>"]+)[>"])");
    auto incBegin = std::sregex_iterator(content.begin(), content.end(), includePattern);
    for (auto it = incBegin; it != funcEnd; ++it) {
        includes.insert((*it)[1].str());
    }

    // === Phase 4: Duplication Detection ===
    std::vector<std::string> codeLineVec;
    std::istringstream dupStream(content);
    while (std::getline(dupStream, line)) {
        size_t first = line.find_first_not_of(" \t\r");
        if (first != std::string::npos && line.size() > 10) {
            codeLineVec.push_back(line.substr(first));
        }
    }
    int duplicateLines = 0;
    std::map<std::string, int> lineFreq;
    for (const auto& cl : codeLineVec) lineFreq[cl]++;
    for (const auto& [k, v] : lineFreq) {
        if (v > 1) duplicateLines += (v - 1);
    }
    float duplicationRatio = codeLineVec.empty() ? 0.0f : 
        static_cast<float>(duplicateLines) / static_cast<float>(codeLineVec.size());

    if (pfnProgress) pfnProgress(80, 1);

    // === Phase 5: Build JSON Report ===
    std::ostringstream json;
    json << "{\n";
    json << "  \"file_path\": \"" << jsonEscape(srcPath) << "\",\n";
    json << "  \"language_detected\": \"" << language << "\",\n";
    json << "  \"line_count\": " << lineCount << ",\n";
    json << "  \"code_lines\": " << codeLines << ",\n";
    json << "  \"comment_lines\": " << commentLines << ",\n";
    json << "  \"blank_lines\": " << blankLines << ",\n";
    json << "  \"max_line_length\": " << maxLineLength << ",\n";
    json << "  \"stub_count\": " << stubCount << ",\n";
    json << "  \"todo_count\": " << todoCount << ",\n";
    json << "  \"duplication_ratio\": " << duplicationRatio << ",\n";

    // Functions
    json << "  \"functions\": [\n";
    for (size_t i = 0; i < functions.size(); ++i) {
        const auto& f = functions[i];
        json << "    {\"name\": \"" << jsonEscape(f.name) 
             << "\", \"return_type\": \"" << jsonEscape(f.returnType)
             << "\", \"start_line\": " << f.startLine
             << ", \"end_line\": " << f.endLine
             << ", \"cyclomatic_complexity\": " << f.complexity << "}";
        if (i + 1 < functions.size()) json << ",";
        json << "\n";
    }
    json << "  ],\n";

    // Classes
    json << "  \"classes\": [";
    for (size_t i = 0; i < classNames.size(); ++i) {
        json << "\"" << jsonEscape(classNames[i]) << "\"";
        if (i + 1 < classNames.size()) json << ", ";
    }
    json << "],\n";

    // Dependencies
    json << "  \"dependencies\": [";
    {
        bool first = true;
        for (const auto& inc : includes) {
            if (!first) json << ", ";
            first = false;
            json << "\"" << jsonEscape(inc) << "\"";
        }
    }
    json << "],\n";

    // Summary metrics
    int totalComplexity = 0;
    int maxComplexity = 0;
    for (const auto& f : functions) {
        totalComplexity += f.complexity;
        if (f.complexity > maxComplexity) maxComplexity = f.complexity;
    }
    float avgComplexity = functions.empty() ? 0.0f : 
        static_cast<float>(totalComplexity) / static_cast<float>(functions.size());

    json << "  \"metrics\": {\n";
    json << "    \"total_functions\": " << functions.size() << ",\n";
    json << "    \"total_classes\": " << classNames.size() << ",\n";
    json << "    \"total_includes\": " << includes.size() << ",\n";
    json << "    \"avg_cyclomatic_complexity\": " << avgComplexity << ",\n";
    json << "    \"max_cyclomatic_complexity\": " << maxComplexity << ",\n";
    json << "    \"comment_ratio\": " << (lineCount > 0 ? static_cast<float>(commentLines) / lineCount : 0.0f) << ",\n";
    json << "    \"code_to_comment_ratio\": " << (commentLines > 0 ? static_cast<float>(codeLines) / commentLines : 0.0f) << "\n";
    json << "  },\n";

    json << "  \"analysis_summary\": \"Full digestion analysis complete\",\n";
    json << "  \"timestamp\": \"" << __DATE__ << " " << __TIME__ << "\",\n";
    json << "  \"status\": \"completed\"\n";
    json << "}\n";

    if (pfnProgress) pfnProgress(90, 1);

    // Write report to output file
    std::ofstream outFile(outPath, std::ios::binary);
    if (!outFile.is_open()) {
        sprintf_s(debugMsg, sizeof(debugMsg), "[RawrXD] Failed to open output file: %s\n", outPath.c_str());
        OutputDebugStringA(debugMsg);
        return ERROR_FILE_NOT_FOUND;
    }
    
    outFile << json.str();
    outFile.close();

    if (pfnProgress) pfnProgress(100, 1);
    OutputDebugStringA("[RawrXD] Digestion report written successfully\n");
    return 0;  // S_DIGEST_OK
}
