// secrets_scanner.cpp — Top-50 Security: secrets detection implementation
#include "secrets_scanner.hpp"
#include "core/problems_aggregator.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <regex>

namespace RawrXD {
namespace Security {

namespace {

// Simple entropy (Shannon) on byte histogram; high value => random-looking.
double entropy(const uint8_t* p, size_t n) {
    if (n == 0) return 0.0;
    int hist[256] = {};
    for (size_t i = 0; i < n; i++) hist[p[i]]++;
    double e = 0.0;
    for (int i = 0; i < 256; i++) {
        if (hist[i] == 0) continue;
        double p_i = static_cast<double>(hist[i]) / static_cast<double>(n);
        e -= p_i * (p_i > 0 ? log2(p_i) : 0.0);
    }
    return e;
}

bool isBase64Char(uint8_t c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
}

} // namespace

size_t SecretsScanner::scan(const uint8_t* data, size_t size, std::vector<SecretFinding>& outFindings) {
    if (!data) return 0;
    size_t before = outFindings.size();
    scanPatterns(data, size, outFindings);
    if (m_checkHighEntropy)
        scanHighEntropy(data, size, outFindings);
    return outFindings.size() - before;
}

size_t SecretsScanner::scan(const std::string& text, std::vector<SecretFinding>& outFindings) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(text.data());
    return scan(p, text.size(), outFindings);
}

void SecretsScanner::addLineColumn(const uint8_t* base, size_t offset, size_t size, SecretFinding& f) {
    f.line = 1;
    f.column = 1;
    for (size_t i = 0; i < offset && i < size; i++) {
        if (base[i] == '\n') {
            f.line++;
            f.column = 1;
        } else {
            f.column++;
        }
    }
}

void SecretsScanner::scanPatterns(const uint8_t* data, size_t size, std::vector<SecretFinding>& out) {
    const char* text = reinterpret_cast<const char*>(data);
    std::string buf(text, size);

    // Patterns: name=value or "name":"value" (simplified; no full JSON parse)
    const std::pair<const char*, const char*> patterns[] = {
        { "api_key", "api[_-]?key[\"']?\\s*[:=]\\s*[\"']?([a-zA-Z0-9_\\-]{20,})" },
        { "aws_key", "aws[_-]?(?:secret[_-]?)?access[_-]?key[\"']?\\s*[:=]\\s*[\"']?([A-Za-z0-9/+=]{40})" },
        { "password", "password[\"']?\\s*[:=]\\s*[\"']([^\"'\\s]{8,})" },
        { "secret", "secret[\"']?\\s*[:=]\\s*[\"']?([a-zA-Z0-9_\\-]{16,})" },
        { "token", "token[\"']?\\s*[:=]\\s*[\"']?([a-zA-Z0-9_\\-.]{20,})" },
    };

    for (const auto& [kind, pattern] : patterns) {
        try {
            std::regex re(pattern, std::regex::icase);
            std::cregex_iterator it(buf.c_str(), buf.c_str() + buf.size(), re);
            std::cregex_iterator end;
            for (; it != end; ++it) {
                SecretFinding f;
                f.offset = static_cast<uint32_t>(it->position(0));
                f.length = static_cast<uint32_t>(it->length(0));
                f.kind   = kind;
                size_t snipStart = (it->position(0) >= 10) ? it->position(0) - 10 : 0;
                size_t snipLen   = std::min<size_t>(it->length(0) + 20, buf.size() - snipStart);
                f.snippet = buf.substr(snipStart, snipLen);
                if (f.snippet.size() > 60) f.snippet = f.snippet.substr(0, 57) + "...";
                addLineColumn(data, f.offset, size, f);
                out.push_back(f);
            }
        } catch (...) {
            // regex error; skip pattern
        }
    }
}

void SecretsScanner::scanHighEntropy(const uint8_t* data, size_t size, std::vector<SecretFinding>& out) {
    const size_t window = 32;
    const double entropyThreshold = 4.5; // base64 random is ~5.9; 4.5 catches many keys
    const size_t minLen = 24;

    for (size_t i = 0; i + minLen <= size; ) {
        size_t j = i;
        while (j < size && (isBase64Char(data[j]) || data[j] == '-' || data[j] == '_')) j++;
        size_t run = j - i;
        if (run >= minLen) {
            double e = entropy(data + i, run);
            if (e >= entropyThreshold) {
                SecretFinding f;
                f.offset = static_cast<uint32_t>(i);
                f.length = static_cast<uint32_t>(run);
                f.kind   = "high_entropy";
                f.snippet = std::string(reinterpret_cast<const char*>(data + i), std::min(run, size_t(40)));
                if (run > 40) f.snippet += "...";
                addLineColumn(data, i, size, f);
                out.push_back(f);
            }
            i = j;
        } else {
            i++;
        }
    }
}

void SecretsScanner::reportToProblems(const std::vector<SecretFinding>& findings, const std::string& path) {
    RawrXD::ProblemsAggregator& agg = RawrXD::ProblemsAggregator::instance();
    for (const auto& f : findings) {
        std::string msg = f.kind + ": " + f.snippet;
        agg.add("Secrets", path, (int)f.line, (int)f.column, 2, "SECRET", msg, f.kind);
    }
}

} // namespace Security
} // namespace RawrXD

// ---------------------------------------------------------------------------
// C API for IDE / CLI integration (P0)
// ---------------------------------------------------------------------------
#include "core/problems_aggregator.hpp"
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {

#ifdef _WIN32
__declspec(dllexport)
#endif
void SecretsScanner_ScanFile(const char* filePath) {
    if (!filePath) return;
    std::ifstream f(filePath, std::ios::binary);
    if (!f) return;
    f.seekg(0, std::ios::end);
    size_t size = static_cast<size_t>(f.tellg());
    f.seekg(0);
    if (size > 10 * 1024 * 1024) return;  // Skip > 10MB
    std::string buffer(size, '\0');
    if (!f.read(&buffer[0], size)) return;
    if (buffer.find('\0') != std::string::npos) return;  // Binary
    RawrXD::Security::SecretsScanner scanner;
    std::vector<RawrXD::Security::SecretFinding> findings;
    scanner.scan(buffer, findings);
    scanner.reportToProblems(findings, filePath);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
void SecretsScanner_ScanDirectory(const char* dirPath) {
    if (!dirPath) return;
    RawrXD::ProblemsAggregator::instance().clear("Secrets");
#ifdef _WIN32
    std::string searchPath = std::string(dirPath) + "\\*";
    WIN32_FIND_DATAA fd = {};
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                std::string subDir = std::string(dirPath) + "\\" + fd.cFileName;
                SecretsScanner_ScanDirectory(subDir.c_str());
            }
        } else {
            std::string name = fd.cFileName;
            std::string lower = name;
            for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
            bool scan = (lower.find(".cpp") != std::string::npos || lower.find(".c") != std::string::npos ||
                         lower.find(".h") != std::string::npos || lower.find(".hpp") != std::string::npos ||
                         lower.find(".py") != std::string::npos || lower.find(".js") != std::string::npos ||
                         lower.find(".json") != std::string::npos || lower.find(".xml") != std::string::npos ||
                         lower.find(".config") != std::string::npos);
            if (scan) {
                std::string fullPath = std::string(dirPath) + "\\" + fd.cFileName;
                SecretsScanner_ScanFile(fullPath.c_str());
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#endif
}

} // extern "C"
