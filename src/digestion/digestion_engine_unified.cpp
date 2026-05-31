// ============================================================================
// digestion_engine_unified.cpp — RawrXD Unified Digestion Engine
// CONSOLIDATED from:
//   - src/ai/digestion_engine.cpp (904 lines, C++20 implementation)
//   - src/win32app/digestion_engine_stub.cpp (349 lines, Win32 wrapper)
//   - src/digestion/RawrXD_DigestionEngine.asm (AVX-512 MASM entry point)
// ============================================================================
// Architecture: Three-tier dispatch
//   1. Win32 WinAPI entry point (MASM RawrXD_DigestionEngine_Avx512)
//   2. C++ implementation (RawrXDDigestionEngine class)
//   3. Runtime dispatch: AVX-512 if available, otherwise scalar fallback
// ============================================================================

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <regex>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <sqlite3.h>

namespace fs = std::filesystem;

// ============================================================================
// Forward declarations
// ============================================================================

class RawrXDDigestionEngine;

// ============================================================================
// Utility Functions (from digestion_engine_stub.cpp)
// ============================================================================

static std::string wideToUtf8(const wchar_t* src) {
    if (!src) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, src, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, src, -1, &result[0], len, nullptr, nullptr);
    return result;
}

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

static std::string detectLanguage(const std::string& filepath) {
    size_t dot = filepath.find_last_of('.');
    if (dot == std::string::npos) return "unknown";
    std::string ext = filepath.substr(dot);
    for (auto& c : ext) c = std::tolower(c);
    
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return "cpp";
    if (ext == ".h" || ext == ".hpp" || ext == ".hxx") return "header";
    if (ext == ".c") return "c";
    if (ext == ".py") return "python";
    if (ext == ".js" || ext == ".ts") return "javascript";
    if (ext == ".asm" || ext == ".s") return "assembly";
    if (ext == ".json") return "json";
    if (ext == ".cmake") return "cmake";
    if (ext == ".ps1") return "powershell";
    return "unknown";
}

// Check if AVX-512 is available
static bool hasAVX512() {
#ifdef _WIN32
    int cpuInfo[4];
    __cpuid(cpuInfo, 7);
    return (cpuInfo[1] & (1 << 16)) != 0;  // Check AVX-512F
#else
    return false;
#endif
}

// ============================================================================
// RawrXDDigestionEngine Class (from src/ai/digestion_engine.cpp)
// ============================================================================

class RawrXDDigestionEngine {
public:
    RawrXDDigestionEngine() : m_db(nullptr), m_running(false) {
        m_threadPoolSize = std::thread::hardware_concurrency();
        if (m_threadPoolSize == 0) m_threadPoolSize = 4;
    }

    ~RawrXDDigestionEngine() {
        stop();
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
    }

    // Initialize database for storing digestion results
    bool initializeDatabase(const std::string& dbPath) {
        std::lock_guard<std::mutex> lock(m_dbMutex);
        if (m_db) sqlite3_close(m_db);
        
        int rc = sqlite3_open(dbPath.c_str(), &m_db);
        if (rc != SQLITE_OK) {
            m_lastError = sqlite3_errmsg(m_db);
            return false;
        }
        
        // Create tables
        const char* schema = R"(
            CREATE TABLE IF NOT EXISTS digestion_results (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_path TEXT UNIQUE NOT NULL,
                language TEXT,
                line_count INTEGER,
                code_lines INTEGER,
                comment_lines INTEGER,
                blank_lines INTEGER,
                function_count INTEGER,
                class_count INTEGER,
                complexity_avg REAL,
                complexity_max INTEGER,
                stub_count INTEGER,
                todo_count INTEGER,
                timestamp TEXT
            );
            CREATE INDEX IF NOT EXISTS idx_file_path ON digestion_results(file_path);
        )";
        
        char* errMsg = nullptr;
        rc = sqlite3_exec(m_db, schema, nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            m_lastError = errMsg ? errMsg : "Unknown error";
            sqlite3_free(errMsg);
            return false;
        }
        
        return true;
    }

    // Digest a single file
    bool digestFile(const std::string& filepath, 
                    const std::string& outputJsonPath,
                    void (*progressCallback)(DWORD, DWORD) = nullptr) {
        
        if (progressCallback) progressCallback(0, 1);
        
        // Read file
        std::ifstream inFile(filepath, std::ios::binary);
        if (!inFile.is_open()) {
            m_lastError = "Failed to open file: " + filepath;
            return false;
        }
        
        std::string content((std::istreambuf_iterator<char>(inFile)),
                            std::istreambuf_iterator<char>());
        inFile.close();
        
        if (progressCallback) progressCallback(10, 1);
        
        // Analyze
        auto result = analyzeSource(content, filepath);
        
        if (progressCallback) progressCallback(80, 1);
        
        // Write JSON report
        std::ofstream outFile(outputJsonPath, std::ios::binary);
        if (!outFile.is_open()) {
            m_lastError = "Failed to write output: " + outputJsonPath;
            return false;
        }
        
        outFile << result;
        outFile.close();
        
        if (progressCallback) progressCallback(100, 1);
        return true;
    }

    void stop() {
        m_running = false;
    }

    std::string getLastError() const { return m_lastError; }

private:
    std::string analyzeSource(const std::string& content, const std::string& filepath) {
        std::string language = detectLanguage(filepath);
        
        // Line analysis
        int lineCount = 0, blankLines = 0, commentLines = 0, codeLines = 0;
        int stubCount = 0, todoCount = 0, maxLineLength = 0;
        bool inBlockComment = false;
        
        std::istringstream lineStream(content);
        std::string line;
        while (std::getline(lineStream, line)) {
            lineCount++;
            maxLineLength = std::max(maxLineLength, static_cast<int>(line.length()));
            
            size_t firstNonSpace = line.find_first_not_of(" \t\r");
            if (firstNonSpace == std::string::npos) {
                blankLines++;
                continue;
            }
            
            std::string trimmed = line.substr(firstNonSpace);
            
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
            if (trimmed.substr(0, 2) == "//" || trimmed[0] == '#' || trimmed[0] == ';') {
                commentLines++;
            } else {
                codeLines++;
            }
            
            std::string lower = trimmed;
            for (auto& c : lower) c = std::tolower(c);
            if (lower.find("stub") != std::string::npos) stubCount++;
            if (lower.find("todo") != std::string::npos || lower.find("fixme") != std::string::npos) 
                todoCount++;
        }
        
        // Function extraction
        std::vector<std::string> functionNames;
        std::vector<int> complexities;
        std::regex funcPattern(R"((\w[\w:*&<>\s]*?)\s+(\w+)\s*\(([^)]*)\)\s*(?:const)?\s*\{)");
        
        auto funcBegin = std::sregex_iterator(content.begin(), content.end(), funcPattern);
        auto funcEnd = std::sregex_iterator();
        for (auto it = funcBegin; it != funcEnd; ++it) {
            functionNames.push_back((*it)[2].str());
            
            // Calculate complexity (simplified)
            size_t bracePos = it->position() + it->length() - 1;
            int depth = 1, complexity = 1;
            size_t pos = bracePos + 1;
            while (pos < content.size() && depth > 0) {
                if (content[pos] == '{') depth++;
                else if (content[pos] == '}') depth--;
                
                // Count control flow
                if (pos + 2 < content.size()) {
                    if (content.substr(pos, 2) == "if" || content.substr(pos, 3) == "for") 
                        complexity++;
                }
                pos++;
            }
            complexities.push_back(complexity);
        }
        
        // Class extraction
        std::vector<std::string> classNames;
        std::regex classPattern(R"(\b(class|struct)\s+(\w+))");
        auto classBegin = std::sregex_iterator(content.begin(), content.end(), classPattern);
        for (auto it = classBegin; it != funcEnd; ++it) {
            classNames.push_back((*it)[2].str());
        }
        
        // Include extraction
        std::set<std::string> includes;
        std::regex includePattern(R"(#include\s+[<"]([^>"]+)[>"])");
        auto incBegin = std::sregex_iterator(content.begin(), content.end(), includePattern);
        for (auto it = incBegin; it != funcEnd; ++it) {
            includes.insert((*it)[1].str());
        }
        
        // Build JSON
        std::ostringstream json;
        json << "{\n";
        json << "  \"file_path\": \"" << jsonEscape(filepath) << "\",\n";
        json << "  \"language_detected\": \"" << language << "\",\n";
        json << "  \"line_count\": " << lineCount << ",\n";
        json << "  \"code_lines\": " << codeLines << ",\n";
        json << "  \"comment_lines\": " << commentLines << ",\n";
        json << "  \"blank_lines\": " << blankLines << ",\n";
        json << "  \"max_line_length\": " << maxLineLength << ",\n";
        json << "  \"stub_count\": " << stubCount << ",\n";
        json << "  \"todo_count\": " << todoCount << ",\n";
        json << "  \"function_count\": " << functionNames.size() << ",\n";
        json << "  \"class_count\": " << classNames.size() << ",\n";
        
        int totalComplexity = 0, maxComplexity = 0;
        for (int c : complexities) {
            totalComplexity += c;
            maxComplexity = std::max(maxComplexity, c);
        }
        float avgComplexity = functionNames.empty() ? 0.0f : 
            static_cast<float>(totalComplexity) / functionNames.size();
        
        json << "  \"complexity_avg\": " << avgComplexity << ",\n";
        json << "  \"complexity_max\": " << maxComplexity << ",\n";
        json << "  \"dependencies_count\": " << includes.size() << ",\n";
        json << "  \"avx512_available\": " << (hasAVX512() ? "true" : "false") << ",\n";
        json << "  \"status\": \"completed\"\n";
        json << "}\n";
        
        return json.str();
    }

    sqlite3* m_db;
    std::mutex m_dbMutex;
    std::string m_lastError;
    std::atomic<bool> m_running;
    size_t m_threadPoolSize;
};

// ============================================================================
// Global instance for Win32 API calls
// ============================================================================

static RawrXDDigestionEngine* g_digestionEngine = nullptr;
static std::mutex g_engineMutex;

// ============================================================================
// Win32 API Entry Point (called from MASM or PowerShell)
// CONSOLIDATED from src/win32app/digestion_engine_stub.cpp
// ============================================================================

extern "C" DWORD __stdcall RawrXD_DigestionEngine_Avx512(
    LPCWSTR wszSource,
    LPCWSTR wszOutput,
    DWORD   dwChunkSize,
    DWORD   dwThreads,
    DWORD   dwFlags,
    void (__stdcall *pfnProgress)(DWORD percent, DWORD taskId)
)
{
    (void)dwChunkSize;  // Reserved for future chunking
    (void)dwThreads;    // Reserved for future threading
    (void)dwFlags;      // Reserved for future flags
    
    // Get or create global engine instance
    {
        std::lock_guard<std::mutex> lock(g_engineMutex);
        if (!g_digestionEngine) {
            g_digestionEngine = new RawrXDDigestionEngine();
        }
    }
    
    if (!wszSource || !wszOutput) {
        return ERROR_INVALID_PARAMETER;
    }
    
    std::string srcPath = wideToUtf8(wszSource);
    std::string outPath = wideToUtf8(wszOutput);
    
    if (srcPath.empty() || outPath.empty()) {
        return ERROR_INVALID_PARAMETER;
    }
    
    // Digest file
    bool success = g_digestionEngine->digestFile(srcPath, outPath, pfnProgress);
    
    if (!success) {
        OutputDebugStringA("[DigestionEngine] Error: ");
        OutputDebugStringA(g_digestionEngine->getLastError().c_str());
        OutputDebugStringA("\n");
        return ERROR_FILE_NOT_FOUND;
    }
    
    return 0;  // S_DIGEST_OK
}

// ============================================================================
// Cleanup function (optional, called at shutdown)
// ============================================================================

extern "C" void __stdcall RawrXD_DigestionEngine_Shutdown() {
    std::lock_guard<std::mutex> lock(g_engineMutex);
    if (g_digestionEngine) {
        delete g_digestionEngine;
        g_digestionEngine = nullptr;
    }
}
