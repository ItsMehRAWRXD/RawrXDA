// RawrXD_Universal_Dorker.cpp — LDOAGTIAC, XOR obfuscation, hotpatch, universal PHP dorking
// C API + C++ engine; integrates with RawrXD IDE Security menu.
#ifdef _WIN32
#define RAWRXD_UNIVERSAL_DORKER_EXPORTS
#endif
#include "RawrXD_Universal_Dorker.h"
#include <vector>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace RawrXD {
namespace Security {

namespace {

const unsigned char DEFAULT_XOR_KEY = 0x5A;

void xorBlock(const unsigned char* inBuf, int inLen, unsigned char* outBuf, unsigned char key) {
    for (int i = 0; i < inLen; i++)
        outBuf[i] = inBuf[i] ^ key;
}

void xorBlockSchedule(const unsigned char* inBuf, int inLen, unsigned char* outBuf,
                      const unsigned char* keySchedule, int keyLen) {
    if (!keySchedule || keyLen <= 0) {
        xorBlock(inBuf, inLen, outBuf, DEFAULT_XOR_KEY);
        return;
    }
    for (int i = 0; i < inLen; i++)
        outBuf[i] = inBuf[i] ^ keySchedule[i % keyLen];
}

// 8 scan categories: SQLi, XSS, LFI, RCE, Info, Admin, Backup, Other
enum { CAT_SQLI, CAT_XSS, CAT_LFI, CAT_RCE, CAT_INFO, CAT_ADMIN, CAT_BACKUP, CAT_OTHER };

// 25+ universal PHP dork patterns with category
struct DorkEntry { const char* dork; int cat; };
const DorkEntry UNIVERSAL_PHP_DORKS[] = {
    {"inurl:.php?id=", CAT_SQLI},
    {"inurl:.asp?id=", CAT_SQLI},
    {"inurl:.jsp?id=", CAT_SQLI},
    {"inurl:.php?cat=", CAT_SQLI},
    {"inurl:.php?page=", CAT_SQLI},
    {"inurl:.php?item=", CAT_SQLI},
    {"inurl:.php?file=", CAT_LFI},
    {"inurl:.php?doc=", CAT_LFI},
    {"inurl:index.php?id=", CAT_SQLI},
    {"inurl:view.php?id=", CAT_SQLI},
    {"inurl:page.php?id=", CAT_SQLI},
    {"inurl:product.php?id=", CAT_SQLI},
    {"inurl:article.php?id=", CAT_SQLI},
    {"inurl:detail.php?id=", CAT_SQLI},
    {"inurl:download.php?file=", CAT_LFI},
    {"inurl:include.php?file=", CAT_LFI},
    {"inurl:admin/login.php", CAT_ADMIN},
    {"inurl:administrator/login", CAT_ADMIN},
    {"inurl:wp-content", CAT_OTHER},
    {"filetype:sql inurl:backup", CAT_BACKUP},
    {"filetype:log inurl:log", CAT_INFO},
    {"filetype:bak inurl:backup", CAT_BACKUP},
    {"intitle:index.of config", CAT_INFO},
    {"intitle:index.of .env", CAT_BACKUP},
    {"inurl:phpmyadmin", CAT_INFO},
    {"inurl:mysql", CAT_INFO},
    {"inurl:.php?cmd=", CAT_RCE},
    {"inurl:.php?action=", CAT_XSS},
    {"inurl:config.php", CAT_INFO},
    {"inurl:db.php", CAT_INFO},
};
const int UNIVERSAL_PHP_DORK_COUNT = sizeof(UNIVERSAL_PHP_DORKS) / sizeof(UNIVERSAL_PHP_DORKS[0]);

bool parseWhitelist(const char* list, const char* token) {
    if (!list || !token) return false;
    std::string s(list);
    std::string t(token);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t next = s.find(',', pos);
        std::string item = (next == std::string::npos) ? s.substr(pos) : s.substr(pos, next - pos);
        while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) item.pop_back();
        size_t i = 0;
        while (i < item.size() && (item[i] == ' ' || item[i] == '\t')) i++;
        if (i < item.size()) item = item.substr(i);
        if (item == t) return true;
        pos = (next == std::string::npos) ? s.size() : next + 1;
    }
    return false;
}

bool hasLimit1(const std::string& sql) {
    std::string u;
    for (char c : sql) u += (char)std::toupper((unsigned char)c);
    return u.find("LIMIT 1") != std::string::npos || u.find("LIMIT  1") != std::string::npos;
}

// Extract table identifiers from FROM and JOIN clauses (simple parse)
void extractTables(const std::string& sql, std::vector<std::string>& out) {
    std::string u;
    for (char c : sql) u += (char)std::toupper((unsigned char)c);
    const char* keywords[] = { " FROM ", " JOIN ", " INNER JOIN ", " LEFT JOIN ", " RIGHT JOIN " };
    for (const char* kw : keywords) {
        size_t pos = 0;
        while ((pos = u.find(kw, pos)) != std::string::npos) {
            pos += std::strlen(kw);
            while (pos < sql.size() && (sql[pos] == ' ' || sql[pos] == '\t')) pos++;
            std::string tbl;
            while (pos < sql.size() && (std::isalnum((unsigned char)sql[pos]) || sql[pos] == '_')) {
                tbl += sql[pos++];
            }
            if (!tbl.empty()) out.push_back(tbl);
        }
    }
}

// Extract column identifiers from SELECT clause (before FROM)
void extractSelectColumns(const std::string& sql, std::vector<std::string>& out) {
    std::string u;
    for (char c : sql) u += (char)std::toupper((unsigned char)c);
    size_t sel = u.find("SELECT ");
    if (sel == std::string::npos) return;
    size_t from = u.find(" FROM ", sel + 7);
    if (from == std::string::npos) return;
    std::string list = sql.substr(sel + 7, from - (sel + 7));
    size_t p = 0;
    while (p < list.size()) {
        size_t comma = list.find(',', p);
        std::string part = (comma == std::string::npos) ? list.substr(p) : list.substr(p, comma - p);
        p = (comma == std::string::npos) ? list.size() : comma + 1;
        while (!part.empty() && (part.back() == ' ' || part.back() == '\t')) part.pop_back();
        size_t i = 0;
        while (i < part.size() && (part[i] == ' ' || part[i] == '\t')) i++;
        if (i < part.size()) part = part.substr(i);
        if (part == "*") continue;
        std::string col;
        for (size_t j = 0; j < part.size() && (std::isalnum((unsigned char)part[j]) || part[j] == '_' || part[j] == '.'); j++)
            col += part[j];
        size_t dot = col.rfind('.');
        if (dot != std::string::npos && dot + 1 < col.size()) col = col.substr(dot + 1);  // table.col -> col
        if (!col.empty()) out.push_back(col);
    }
}

// Base64 decode (standard alphabet)
std::string base64Decode(const std::string& in) {
    static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() * 3) / 4);
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (c == '=') break;
        const char* p = std::strchr(kTable, (char)c);
        if (!p) continue;
        val |= (int)(p - kTable) << (valb += 6);
        if (valb >= 0) {
            out += (char)(val & 0xFF);
            val >>= 8;
            valb -= 8;
        }
    }
    return out;
}

// RFC 3986 URL encode (unreserved A-Za-z0-9-_.~ stay; others become %XX)
std::string urlEncode(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);
    static const char hex[] = "0123456789ABCDEF";
    for (unsigned char c : in) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

// RFC 3986 URL decode (%XX and + -> space)
std::string urlDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        if (in[i] == '%' && i + 2 < in.size()) {
            int h = 0;
            for (int j = 1; j <= 2; j++) {
                char c = in[i + j];
                if (c >= '0' && c <= '9') h = h * 16 + (c - '0');
                else if (c >= 'A' && c <= 'F') h = h * 16 + (c - 'A' + 10);
                else if (c >= 'a' && c <= 'f') h = h * 16 + (c - 'a' + 10);
                else { h = -1; break; }
            }
            if (h >= 0 && h <= 255) { out += (char)h; i += 2; }
            else out += in[i];
        } else if (in[i] == '+') {
            out += ' ';
        } else {
            out += in[i];
        }
    }
    return out;
}

// XOR decode with single-byte key
std::string xorDecode(const std::string& in, unsigned char key) {
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) out += (char)(c ^ key);
    return out;
}

} // namespace

// --- XorObfuscator ---
void XorObfuscator::encode(const unsigned char* inBuf, int inLen, unsigned char* outBuf, int* outLen, unsigned char key) {
    if (!inBuf || !outBuf || inLen <= 0) { if (outLen) *outLen = 0; return; }
    xorBlock(inBuf, inLen, outBuf, key);
    if (outLen) *outLen = inLen;
}

void XorObfuscator::decode(const unsigned char* inBuf, int inLen, unsigned char* outBuf, int* outLen, unsigned char key) {
    encode(inBuf, inLen, outBuf, outLen, key);
}

void XorObfuscator::encodeWithSchedule(const unsigned char* inBuf, int inLen, unsigned char* outBuf, int* outLen,
                                       const unsigned char* keySchedule, int keyLen) {
    if (!inBuf || !outBuf || inLen <= 0) { if (outLen) *outLen = 0; return; }
    xorBlockSchedule(inBuf, inLen, outBuf, keySchedule, keyLen);
    if (outLen) *outLen = inLen;
}

// --- ParameterizedQueryEngine (LDOAGTIAC) ---
bool ParameterizedQueryEngine::validateTable(const char* table, const char* whitelistCommaSeparated) {
    return parseWhitelist(whitelistCommaSeparated, table);
}

bool ParameterizedQueryEngine::validateColumn(const char* column, const char* whitelistCommaSeparated) {
    return parseWhitelist(whitelistCommaSeparated, column);
}

int ParameterizedQueryEngine::buildSecureQuery(const std::string& templateSql, const std::string& tableWhitelist,
                                               const std::string& columnWhitelist, bool enforceLimit1,
                                               std::string& outSql) {
    if (!tableWhitelist.empty()) {
        std::vector<std::string> tables;
        extractTables(templateSql, tables);
        for (const std::string& t : tables) {
            if (!validateTable(t.c_str(), tableWhitelist.c_str()))
                return -3;  // table not in whitelist
        }
    }
    if (!columnWhitelist.empty()) {
        std::vector<std::string> cols;
        extractSelectColumns(templateSql, cols);
        for (const std::string& c : cols) {
            if (!validateColumn(c.c_str(), columnWhitelist.c_str()))
                return -4;  // column not in whitelist
        }
    }
    outSql = templateSql;
    if (enforceLimit1 && !hasLimit1(outSql)) {
        if (outSql.back() != ';') outSql += " ";
        outSql += "LIMIT 1";
    }
    return 0;
}

static int toCat(DorkCategory dc) {
    int c = (int)dc;
    if (c < 0 || c >= 8) return CAT_OTHER;
    static const int map[] = {CAT_SQLI, CAT_XSS, CAT_LFI, CAT_RCE, CAT_INFO, CAT_ADMIN, CAT_BACKUP, CAT_OTHER};
    return map[c];
}

std::vector<std::string> UniversalPhpDorker::GenerateUniversalDorks(bool obfuscate) {
    std::vector<std::string> out;
    out.reserve(UNIVERSAL_PHP_DORK_COUNT);
    for (int i = 0; i < UNIVERSAL_PHP_DORK_COUNT; i++) {
        std::string d(UNIVERSAL_PHP_DORKS[i].dork);
        if (obfuscate) {
            std::vector<unsigned char> enc(d.begin(), d.end());
            unsigned char key = DEFAULT_XOR_KEY;
            for (size_t j = 0; j < enc.size(); j++) enc[j] ^= key;
            d.assign(enc.begin(), enc.end());
        }
        out.push_back(d);
    }
    return out;
}

std::vector<std::string> UniversalPhpDorker::GetDorksByCategory(DorkCategory cat, bool obfuscate) {
    std::vector<std::string> out;
    int target = toCat(cat);
    for (int i = 0; i < UNIVERSAL_PHP_DORK_COUNT; i++) {
        if (UNIVERSAL_PHP_DORKS[i].cat != target) continue;
        std::string d(UNIVERSAL_PHP_DORKS[i].dork);
        if (obfuscate) {
            std::vector<unsigned char> enc(d.begin(), d.end());
            unsigned char key = DEFAULT_XOR_KEY;
            for (size_t j = 0; j < enc.size(); j++) enc[j] ^= key;
            d.assign(enc.begin(), enc.end());
        }
        out.push_back(d);
    }
    return out;
}

int UniversalPhpDorker::GetBuiltinCount() {
    return UNIVERSAL_PHP_DORK_COUNT;
}

// --- UrlHotpatchEngine ---
std::string UrlHotpatchEngine::applyHotpatch(const std::string& url, const std::string& marker) {
    if (url.empty() || marker.empty()) return url;
    size_t q = url.find('?');
    std::string path = (q != std::string::npos) ? url.substr(0, q) : url;
    std::string rest = (q != std::string::npos) ? url.substr(q) : "";

    // Special rule: insert [t] before .php (e.g. index.php -> index[t].php)
    if (marker == "[t]") {
        size_t dotPhp = path.rfind(".php");
        if (dotPhp != std::string::npos) {
            return path.substr(0, dotPhp) + marker + path.substr(dotPhp) + rest;
        }
    }

    size_t insert = path.size();
    if (insert > 0 && path[insert - 1] == '/') {
        return path + marker + rest;
    }
    size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos && slash + 1 < path.size()) {
        std::string base = path.substr(0, slash + 1);
        std::string file = path.substr(slash + 1);
        return base + file + marker + rest;
    }
    return path + marker + rest;
}

std::vector<std::string> UrlHotpatchEngine::getDefaultMarkers() {
    return { "_test", "_bak", ".bak", "[t]", "[d]" };
}

// --- UrlCodec (RFC 3986) ---
std::string UrlCodec::encode(const std::string& in) { return urlEncode(in); }
std::string UrlCodec::decode(const std::string& in) { return urlDecode(in); }

// --- SearchResultReverser (Base64 + XOR decode, RFC 3986 URL decode, signature detection) ---
static int analyzeOne(const std::string& body, std::string& snippet) {
    if (body.find("SQL syntax") != std::string::npos ||
        body.find("mysql_fetch") != std::string::npos ||
        body.find("Warning: mysql_") != std::string::npos ||
        body.find("pg_query()") != std::string::npos ||
        body.find("Microsoft SQL Server") != std::string::npos ||
        body.find("ORA-") != std::string::npos) {
        snippet = body.size() > 256 ? body.substr(0, 256) + "..." : body;
        return 8;  // vulnerable
    }
    if (body.find("error") != std::string::npos || body.find("Exception") != std::string::npos) {
        snippet = body.size() > 128 ? body.substr(0, 128) + "..." : body;
        return 4;  // suspicious
    }
    return 0;  // safe
}

ReverserResult SearchResultReverser::AnalyzeResult(const std::string& url, const char* responseBody, int bodyLen) {
    ReverserResult r;
    r.verdict = "safe";
    r.severity = 0;
    if (!responseBody || bodyLen <= 0) return r;
    std::string body(responseBody, (size_t)bodyLen);
    return AnalyzeResult(url, body);
}

ReverserResult SearchResultReverser::AnalyzeResult(const std::string& url, const std::string& responseBody) {
    ReverserResult r;
    r.verdict = "safe";
    r.severity = 0;
    (void)url;
    int bestSev = 0;
    std::string bestSnippet;
    std::string b64 = base64Decode(responseBody);
    std::vector<std::string> candidates;
    candidates.push_back(responseBody);
    candidates.push_back(urlDecode(responseBody));
    candidates.push_back(b64);
    candidates.push_back(xorDecode(responseBody, DEFAULT_XOR_KEY));
    candidates.push_back(xorDecode(b64, DEFAULT_XOR_KEY));
    candidates.push_back(urlDecode(b64));
    for (const std::string& cand : candidates) {
        if (cand.empty()) continue;
        std::string snip;
        int sev = analyzeOne(cand, snip);
        if (sev > bestSev) {
            bestSev = sev;
            bestSnippet = snip;
        }
    }
    r.severity = bestSev;
    r.decodedSnippet = bestSnippet;
    if (bestSev >= 8) r.verdict = "vulnerable";
    else if (bestSev >= 4) r.verdict = "suspicious";
    return r;
}

} // namespace Security
} // namespace RawrXD

// --- C API (MASM-callable) ---
extern "C" {

UDORKER_API void UniversalDorker_XorObfuscate(const unsigned char* inBuf, int inLen,
                                               unsigned char* outBuf, int* outLen, unsigned char keyByte) {
    RawrXD::Security::XorObfuscator::encode(inBuf, inLen, outBuf, outLen, keyByte);
}

UDORKER_API int UniversalDorker_ApplyHotpatch(const char* urlIn, const char* marker,
                                               char* resultBuf, int resultSize) {
    if (!urlIn || !marker || !resultBuf || resultSize <= 0) return -1;
    std::string out = RawrXD::Security::UrlHotpatchEngine::applyHotpatch(urlIn, marker);
    if (out.size() >= (size_t)resultSize) return -2;
    std::memcpy(resultBuf, out.c_str(), out.size() + 1);
    return (int)out.size();
}

UDORKER_API int UniversalDorker_BuildSecureQuery(const char* templateSql, const char* tableWhitelist,
                                                    const char* columnWhitelist, int limit1,
                                                    char* outSql, int outSize) {
    if (!templateSql || !outSql || outSize <= 0) return -1;
    std::string out;
    int ret = RawrXD::Security::ParameterizedQueryEngine::buildSecureQuery(
        templateSql,
        tableWhitelist ? tableWhitelist : "",
        columnWhitelist ? columnWhitelist : "",
        limit1 != 0,
        out);
    if (ret != 0) return ret;
    if (out.size() >= (size_t)outSize) return -2;
    std::memcpy(outSql, out.c_str(), out.size() + 1);
    return (int)out.size();
}

static std::vector<std::string> s_lastDorks;
static std::vector<char> s_lastDorkChars;

UDORKER_API int UniversalDorker_GenerateUniversalDorks(int obfuscate, char** outDorks, int maxDorks, int* actualCount) {
    auto dorks = RawrXD::Security::UniversalPhpDorker::GenerateUniversalDorks(obfuscate != 0);
    if (actualCount) *actualCount = (int)dorks.size();
    if (!outDorks || maxDorks <= 0) return 0;
    s_lastDorks = std::move(dorks);
    s_lastDorkChars.clear();
    for (const auto& d : s_lastDorks) {
        for (char c : d) s_lastDorkChars.push_back(c);
        s_lastDorkChars.push_back('\0');
    }
    int n = (int)s_lastDorks.size();
    if (n > maxDorks) n = maxDorks;
    size_t idx = 0;
    for (int i = 0; i < n; i++) {
        outDorks[i] = &s_lastDorkChars[idx];
        while (idx < s_lastDorkChars.size() && s_lastDorkChars[idx]) idx++;
        idx++;
    }
    return n;
}

static std::vector<std::string> s_lastCatDorks;
static std::vector<char> s_lastCatDorkChars;

UDORKER_API int UniversalDorker_GetCategoryCount(void) {
    return RawrXD::Security::UniversalPhpDorker::GetCategoryCount();
}

UDORKER_API int UniversalDorker_GetDorksByCategory(int category, int obfuscate, char** outDorks, int maxDorks, int* actualCount) {
    if (category < 0 || category >= 8) { if (actualCount) *actualCount = 0; return 0; }
    auto dorks = RawrXD::Security::UniversalPhpDorker::GetDorksByCategory(
        (RawrXD::Security::DorkCategory)category, obfuscate != 0);
    if (actualCount) *actualCount = (int)dorks.size();
    if (!outDorks || maxDorks <= 0) return 0;
    s_lastCatDorks = std::move(dorks);
    s_lastCatDorkChars.clear();
    for (const auto& d : s_lastCatDorks) {
        for (char c : d) s_lastCatDorkChars.push_back(c);
        s_lastCatDorkChars.push_back('\0');
    }
    int n = (int)s_lastCatDorks.size();
    if (n > maxDorks) n = maxDorks;
    size_t idx = 0;
    for (int i = 0; i < n; i++) {
        outDorks[i] = &s_lastCatDorkChars[idx];
        while (idx < s_lastCatDorkChars.size() && s_lastCatDorkChars[idx]) idx++;
        idx++;
    }
    return n;
}

UDORKER_API int UniversalDorker_UrlEncode(const char* inBuf, int inLen, char* outBuf, int outSize) {
    if (!inBuf || !outBuf || outSize <= 0) return -1;
    size_t len = (inLen >= 0) ? (size_t)inLen : std::strlen(inBuf);
    std::string in(inBuf, len);
    std::string out = RawrXD::Security::UrlCodec::encode(in);
    if (out.size() >= (size_t)outSize) return -2;
    std::memcpy(outBuf, out.c_str(), out.size() + 1);
    return (int)out.size();
}

UDORKER_API int UniversalDorker_AnalyzeResult(const char* url, const char* responseBody, int bodyLen,
                                                char* outVerdict, int outSize, int* severity) {
    if (!url || !outVerdict || outSize <= 0) return -1;
    auto r = RawrXD::Security::SearchResultReverser::AnalyzeResult(url, responseBody, bodyLen);
    if (r.verdict.size() >= (size_t)outSize) return -2;
    std::memcpy(outVerdict, r.verdict.c_str(), r.verdict.size() + 1);
    if (severity) *severity = r.severity;
    return (int)r.verdict.size();
}

UDORKER_API void UniversalDorker_IDE_Command_UniversalDorkScan(void) {
    // IDE integration: invoke Security menu "Universal Dork Scan".
    // RawrXD IDE can register this and call DorkScanner + hotpatch + reverser.
    (void)0;
}

} // extern "C"
