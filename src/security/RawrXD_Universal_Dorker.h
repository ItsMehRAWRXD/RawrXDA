// RawrXD_Universal_Dorker.h — LDOAGTIAC, XOR obfuscation, hotpatching, universal PHP dorking
// C/C++ API for RawrXD IDE Security; C API for MASM x64.
// Logic + Data = Logic Data Only Allows Genuine Transactions In All Cases.
#pragma once

#ifdef __cplusplus
#include <string>
#include <vector>
#include <cstdint>
extern "C" {
#endif

#ifdef _WIN32
#  ifdef RAWRXD_UNIVERSAL_DORKER_EXPORTS
#    define UDORKER_API __declspec(dllexport)
#  else
#    define UDORKER_API __declspec(dllimport)
#  endif
#else
#  define UDORKER_API
#endif

// --- XOR obfuscation (evasion; key schedule) ---
// inBuf/inLen: plaintext; outBuf/outLen: cipher; keyByte: single-byte key or key schedule seed
UDORKER_API void UniversalDorker_XorObfuscate(const unsigned char* inBuf, int inLen,
                                               unsigned char* outBuf, int* outLen, unsigned char keyByte);

// --- URL hotpatch: inject markers [t]/[d]/_test/_bak for variant discovery ---
// urlIn: base URL; marker: e.g. "_test", ".bak"; resultBuf/resultSize: output
UDORKER_API int UniversalDorker_ApplyHotpatch(const char* urlIn, const char* marker,
                                               char* resultBuf, int resultSize);

// --- Parameterized query builder (LDOAGTIAC: logic = template, data = bound only) ---
// templateSql: e.g. "SELECT * FROM users WHERE id=?"; tableWhitelist/columnWhitelist: comma-separated; limit1: 1 = enforce LIMIT 1
UDORKER_API int UniversalDorker_BuildSecureQuery(const char* templateSql, const char* tableWhitelist,
                                                    const char* columnWhitelist, int limit1,
                                                    char* outSql, int outSize);

// --- Universal PHP dork list (25+ patterns); obfuscate: 1 = XOR-encode dork strings ---
UDORKER_API int UniversalDorker_GenerateUniversalDorks(int obfuscate, char** outDorks, int maxDorks, int* actualCount);

// --- 8 scan categories: 0=SQLi, 1=XSS, 2=LFI, 3=RCE, 4=Info, 5=Admin, 6=Backup, 7=Other ---
UDORKER_API int UniversalDorker_GetCategoryCount(void);
UDORKER_API int UniversalDorker_GetDorksByCategory(int category, int obfuscate, char** outDorks, int maxDorks, int* actualCount);

// --- Result reversal: decode Base64+XOR response, run vulnerability detection ---
UDORKER_API int UniversalDorker_AnalyzeResult(const char* url, const char* responseBody, int bodyLen,
                                                char* outVerdict, int outSize, int* severity);

// --- URL encode (RFC 3986): inBuf/inLen = UTF-8; outBuf/outSize = encoded; returns length or -1 ---
UDORKER_API int UniversalDorker_UrlEncode(const char* inBuf, int inLen, char* outBuf, int outSize);

// --- IDE Security menu: run Universal Dork Scan (uses existing DorkScanner + hotpatch + reverser) ---
UDORKER_API void UniversalDorker_IDE_Command_UniversalDorkScan(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <cstring>
#include <algorithm>

namespace RawrXD {
namespace Security {

// --- XorObfuscator: runtime XOR with optional key schedule ---
class XorObfuscator {
public:
    static const int MAX_KEY_SCHEDULE = 32;

    static void encode(const unsigned char* inBuf, int inLen, unsigned char* outBuf, int* outLen, unsigned char key = 0x5A);
    static void decode(const unsigned char* inBuf, int inLen, unsigned char* outBuf, int* outLen, unsigned char key = 0x5A);
    static void encodeWithSchedule(const unsigned char* inBuf, int inLen, unsigned char* outBuf, int* outLen,
                                   const unsigned char* keySchedule, int keyLen);
};

// --- ParameterizedQueryEngine: LDOAGTIAC — templates only, data bound; whitelist validation ---
class ParameterizedQueryEngine {
public:
    static bool validateTable(const char* table, const char* whitelistCommaSeparated);
    static bool validateColumn(const char* column, const char* whitelistCommaSeparated);
    static int buildSecureQuery(const std::string& templateSql, const std::string& tableWhitelist,
                                const std::string& columnWhitelist, bool enforceLimit1,
                                std::string& outSql);
};

// --- Scan category IDs (8 categories) ---
enum DorkCategory { DORK_SQLI = 0, DORK_XSS, DORK_LFI, DORK_RCE, DORK_INFO, DORK_ADMIN, DORK_BACKUP, DORK_OTHER, DORK_CATEGORY_COUNT };

// --- UniversalPhpDorker: 25+ PHP attack surface patterns; 8 scan categories ---
class UniversalPhpDorker {
public:
    static std::vector<std::string> GenerateUniversalDorks(bool obfuscate = false);
    static int GetBuiltinCount();
    static int GetCategoryCount() { return (int)DORK_CATEGORY_COUNT; }
    static std::vector<std::string> GetDorksByCategory(DorkCategory cat, bool obfuscate = false);
};

// --- UrlHotpatchEngine: runtime URL manipulation for test variants ([t], [d], _test, _bak) ---
class UrlHotpatchEngine {
public:
    static std::string applyHotpatch(const std::string& url, const std::string& marker);
    static std::vector<std::string> getDefaultMarkers();  // _test, _bak, .bak, [t], [d]. For [t], applyHotpatch inserts before .php (e.g. index[t].php); others append after path segment.
};

// --- UrlCodec: RFC 3986 encode/decode ---
class UrlCodec {
public:
    static std::string encode(const std::string& in);
    static std::string decode(const std::string& in);
};

// --- SearchResultReverser: signature-based vulnerability detection (SQL/DB error strings); severity scoring. No Base64/XOR decode in this class. ---
struct ReverserResult {
    std::string verdict;   // "safe" | "suspicious" | "vulnerable"
    int severity;          // 0–10
    std::string decodedSnippet;
};

class SearchResultReverser {
public:
    static ReverserResult AnalyzeResult(const std::string& url, const char* responseBody, int bodyLen);
    static ReverserResult AnalyzeResult(const std::string& url, const std::string& responseBody);
};

} // namespace Security
} // namespace RawrXD

#endif
