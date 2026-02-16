// RawrXD_GoogleDork_Scanner.h — Google Dork Scanner + SQLi detection (bug-safe)
// C/C++ API for RawrXD IDE Security integration; C API for MASM x64.
#pragma once

#ifdef __cplusplus
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
extern "C" {
#endif

#ifdef _WIN32
#  ifdef RAWRXD_DORKSCANNER_EXPORTS
#    define DORKSCANNER_API __declspec(dllexport)
#  else
#    define DORKSCANNER_API __declspec(dllimport)
#  endif
#else
#  define DORKSCANNER_API
#endif

// --- Config (C-compatible) ---
typedef struct DorkScannerConfig {
    int   threadCount;      // 1–32
    int   delayMs;          // rate limit between requests (default 1500)
    int   timeoutMs;        // HTTP timeout (default 10000)
    const char* userAgent;  // NULL = use built-in rotation
    const char* proxyUrl;   // NULL = no proxy; "http://host:port" or "socks5://..."
    int   maxIterations;    // safety: max extraction loops (default 100) — Bug 1 fix
    int   enableErrorBased; // 1 = MySQL/PostgreSQL/MSSQL error signatures
    int   enableTimeBased;  // 1 = SLEEP/pg_sleep/WAITFOR
    int   enableBoolean;    // 1 = true/false/0/1 payloads — Bug 2 fix
} DorkScannerConfig;

// --- Result (C-compatible) ---
typedef struct DorkResult {
    char  url[2048];
    char  dork[512];
    int   vulnType;         // 0=none, 1=error, 2=time, 3=boolean, 4=union
    char  dbType[32];       // "mysql", "postgresql", "mssql", "oracle", "sqlite"
    char  detail[1024];
    int   statusCode;
} DorkResult;

// --- Progress callback: (current, total, message, userData) ---
typedef void (*DorkProgressFn)(int current, int total, const char* message, void* userData);

// --- C API (MASM-callable) ---
DORKSCANNER_API void* DorkScanner_Create(const DorkScannerConfig* config);
DORKSCANNER_API void  DorkScanner_Destroy(void* scanner);
DORKSCANNER_API int   DorkScanner_Initialize(void* scanner, void* userData);
DORKSCANNER_API void  DorkScanner_SetProgressCallback(void* scanner, DorkProgressFn fn);
DORKSCANNER_API int   DorkScanner_ScanSingle(void* scanner, const char* dork,
                                              DorkResult* results, int maxResults);
DORKSCANNER_API int   DorkScanner_ScanFile(void* scanner, const char* dorkFilePath,
                                            DorkResult* results, int maxResults);
DORKSCANNER_API int   DorkScanner_ExportToJson(void* scanner, const char* filePath);
DORKSCANNER_API int   DorkScanner_GetBuiltinDorkCount(void* scanner);
DORKSCANNER_API int   DorkScanner_GetBuiltinDork(void* scanner, int index, char* buf, int bufSize);

// --- SQL injection URL helpers (bug-safe) ---
// Bug 2 fix: replace suffix with proper value (e.g. "=null" -> "=1", "=true")
DORKSCANNER_API int   DorkScanner_TestBooleanPayloads(const char* baseUrl, char* outVerdict, int outSize);

#ifdef __cplusplus
}

// --- C++ API ---
namespace RawrXD {
namespace Security {

struct DorkTarget {
    std::string url;
    std::string dork;
    int         vulnType = 0;
    std::string dbType;
    std::string detail;
    int         statusCode = 0;
};

class GoogleDorkScanner {
public:
    using ProgressCallback = std::function<void(int current, int total, const std::string& message)>;

    GoogleDorkScanner();
    explicit GoogleDorkScanner(const DorkScannerConfig& config);
    ~GoogleDorkScanner();

    bool initialize();
    void setProgressCallback(ProgressCallback cb) { m_progressCb = std::move(cb); }

    std::vector<DorkTarget> scanSingle(const std::string& dork);
    std::vector<DorkTarget> scanFile(const std::string& dorkFilePath);
    bool exportToJson(const std::string& filePath) const;
    bool exportToCsv(const std::string& filePath) const;

    int getBuiltinDorkCount() const;
    std::string getBuiltinDork(int index) const;

    const std::vector<DorkTarget>& getLastResults() const { return m_results; }

private:
    DorkScannerConfig m_config;
    std::vector<DorkTarget> m_results;
    ProgressCallback m_progressCb;
    void* m_impl = nullptr;  // opaque C impl
};

} // namespace Security
} // namespace RawrXD

#endif
