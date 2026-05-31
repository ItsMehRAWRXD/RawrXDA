#include "agentic_executor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <windows.h>
#include <fstream>

namespace {

constexpr uint64_t kMaxReadBytes = 8ULL * 1024ULL * 1024ULL;

std::atomic<uint64_t> g_readCalls{0};
std::atomic<uint64_t> g_readFailures{0};
std::atomic<uint64_t> g_readTruncated{0};
std::atomic<uint64_t> g_readBytesTotal{0};
std::atomic<uint64_t> g_listCalls{0};
std::atomic<uint64_t> g_listFailures{0};
std::atomic<uint64_t> g_listEntriesTotal{0};
std::atomic<uint64_t> g_lastModeMask{0};

uint64_t configuredMaxReadBytes()
{
    if (const char* env = std::getenv("RAWRXD_AGENTIC_READ_MAX")) {
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(env, &end, 10);
        if (end != env && parsed > 0ULL) {
            const uint64_t cap = 64ULL * 1024ULL * 1024ULL;
            return static_cast<uint64_t>(parsed > cap ? cap : parsed);
        }
    }
    return kMaxReadBytes;
}

inline bool isDirectoryAttributes(DWORD attrs)
{
    return (attrs != INVALID_FILE_ATTRIBUTES) && ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

inline bool caseInsensitiveLess(std::string a, std::string b)
{
    std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return a < b;
}

} // namespace

std::string AgenticExecutor::readFile(const std::string& path)
{
    g_readCalls.fetch_add(1, std::memory_order_relaxed);
    if (path.empty()) {
        g_readFailures.fetch_add(1, std::memory_order_relaxed);
        return "";
    }

    const DWORD attrs = GetFileAttributesA(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || isDirectoryAttributes(attrs)) {
        g_readFailures.fetch_add(1, std::memory_order_relaxed);
        return "";
    }

    HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        g_readFailures.fetch_add(1, std::memory_order_relaxed);
        return "";
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0) {
        CloseHandle(h);
        g_readFailures.fetch_add(1, std::memory_order_relaxed);
        return "";
    }

    const uint64_t maxRead = configuredMaxReadBytes();
    const uint64_t bounded = static_cast<uint64_t>(size.QuadPart) > maxRead
        ? maxRead
        : static_cast<uint64_t>(size.QuadPart);

    if (static_cast<uint64_t>(size.QuadPart) > bounded) {
        g_readTruncated.fetch_add(1, std::memory_order_relaxed);
    }

    std::string out;
    out.resize(static_cast<size_t>(bounded));
    size_t totalRead = 0;

    while (totalRead < out.size()) {
        const DWORD remaining = static_cast<DWORD>(std::min<size_t>(out.size() - totalRead, 1U << 20));
        DWORD bytesRead = 0;
        if (!ReadFile(h, out.data() + totalRead, remaining, &bytesRead, nullptr)) {
            out.clear();
            g_readFailures.fetch_add(1, std::memory_order_relaxed);
            break;
        }
        if (bytesRead == 0) {
            break;
        }
        totalRead += static_cast<size_t>(bytesRead);
    }

    CloseHandle(h);
    if (totalRead < out.size()) {
        out.resize(totalRead);
    }

    g_readBytesTotal.fetch_add(static_cast<uint64_t>(out.size()), std::memory_order_relaxed);
    uint64_t modeMask = 0;
    if (bounded > 0) modeMask |= 0x1ULL;
    if (static_cast<uint64_t>(size.QuadPart) > bounded) modeMask |= 0x2ULL;
    if (!out.empty()) modeMask |= 0x4ULL;
    g_lastModeMask.store(modeMask, std::memory_order_relaxed);

    return out;
}

std::vector<std::string> AgenticExecutor::listDirectory(const std::string& path)
{
    g_listCalls.fetch_add(1, std::memory_order_relaxed);
    std::vector<std::string> entries;

    if (path.empty()) {
        g_listFailures.fetch_add(1, std::memory_order_relaxed);
        return entries;
    }

    const DWORD attrs = GetFileAttributesA(path.c_str());
    if (!isDirectoryAttributes(attrs)) {
        g_listFailures.fetch_add(1, std::memory_order_relaxed);
        return entries;
    }

    std::string pattern = path;
    if (!pattern.empty() && pattern.back() != '\\' && pattern.back() != '/') {
        pattern += "\\\\";
    }
    pattern += "*";

    WIN32_FIND_DATAA fd{};
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        g_listFailures.fetch_add(1, std::memory_order_relaxed);
        return entries;
    }

    do {
        const char* n = fd.cFileName;
        if (std::strcmp(n, ".") == 0 || std::strcmp(n, "..") == 0) {
            continue;
        }
        std::string name(n);
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            name.push_back('/');
        }
        entries.emplace_back(std::move(name));
    } while (FindNextFileA(h, &fd));

    FindClose(h);

    std::sort(entries.begin(), entries.end(), [](const std::string& a, const std::string& b) {
        return caseInsensitiveLess(a, b);
    });
    g_listEntriesTotal.fetch_add(static_cast<uint64_t>(entries.size()), std::memory_order_relaxed);
    uint64_t modeMask = g_lastModeMask.load(std::memory_order_relaxed);
    if (!entries.empty()) {
        modeMask |= 0x8ULL;
    }
    g_lastModeMask.store(modeMask, std::memory_order_relaxed);
    return entries;
}

extern "C" unsigned __int64 rawrxd_agentic_executor_stub_stats()
{
    // [63:56] list_fail, [55:48] list_calls, [47:40] read_fail, [39:32] read_calls,
    // [31:24] truncated, [23:16] mode_mask, [15:8] list_entries_total(low8), [7:0] read_bytes_total(low8).
    const uint64_t listFail = g_listFailures.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t listCalls = g_listCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t readFail = g_readFailures.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t readCalls = g_readCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t truncated = g_readTruncated.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t mode = g_lastModeMask.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t listEntries = g_listEntriesTotal.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t readBytes = g_readBytesTotal.load(std::memory_order_relaxed) & 0xFFu;
    return (listFail << 56) | (listCalls << 48) | (readFail << 40) | (readCalls << 32) |
           (truncated << 24) | (mode << 16) | (listEntries << 8) | readBytes;
}

extern "C" unsigned __int64 rawrxd_agentic_executor_stub_read_bytes_total()
{
    return g_readBytesTotal.load(std::memory_order_relaxed);
}
