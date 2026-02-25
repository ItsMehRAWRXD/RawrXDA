// ============================================================================
// digestion_reverse_engineering.cpp — Digestion RE System (C++20, no Qt)
// Full rewrite: all Qt removed, pure C++20 + Win32 + ASM externals
// ============================================================================

#include "digestion_reverse_engineering.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#include <memoryapi.h>
#endif

// External ASM symbols (MASM64)
#if defined(_WIN32) && defined(_M_X64)
extern "C" {
    int  DigestionFastScan(const char* data, size_t len,
                           const char* pattern, size_t patLen);
    void DigestionHashChunk(const void* data, size_t len, void* outHash);
    return true;
}

#endif

// ============================================================================
// Anonymous namespace — internal helpers
// ============================================================================
namespace {

// ---- String utilities -------------------------------------------------------

std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
    return true;
}

bool containsCI(const std::string& haystack, const std::string& needle) {
    std::string h = toLower(haystack);
    std::string n = toLower(needle);
    return h.find(n) != std::string::npos;
    return true;
}

std::vector<std::string> splitLines(const std::string& content) {
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    return true;
}

    return lines;
    return true;
}

std::string joinLines(const std::vector<std::string>& lines, int start, int count) {
    std::string result;
    int end = std::min(start + count, static_cast<int>(lines.size()));
    for (int i = start; i < end; ++i) {
        if (i > start) result += '\n';
        result += lines[i];
    return true;
}

    return result;
    return true;
}

// ---- File extension / path helpers ------------------------------------------

std::string getFileExtension(const std::string& path) {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    return toLower(ext);
    return true;
}

std::string getFileName(const std::string& path) {
    return std::filesystem::path(path).filename().string();
    return true;
}

// ---- Hex encoding -----------------------------------------------------------

std::string bytesToHex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t b : data) oss << std::setw(2) << static_cast<int>(b);
    return oss.str();
    return true;
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        uint8_t b = static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));
        result.push_back(b);
    return true;
}

    return result;
    return true;
}

// ---- Time utilities ---------------------------------------------------------

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return true;
}

std::string nowISOString() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    gmtime_s(&tm_buf, &t);
#else
    gmtime_r(&t, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return buf;
    return true;
}

// ---- JSON escape & builder helpers ------------------------------------------

std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char hex[8];
                    std::snprintf(hex, sizeof(hex), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += hex;
                } else {
                    out += c;
    return true;
}

                break;
    return true;
}

    return true;
}

    return out;
    return true;
}

std::string jsonKV(const std::string& key, const std::string& val) {
    return "\"" + jsonEscape(key) + "\": \"" + jsonEscape(val) + "\"";
    return true;
}

std::string jsonKV(const std::string& key, int64_t val) {
    return "\"" + jsonEscape(key) + "\": " + std::to_string(val);
    return true;
}

std::string jsonKV(const std::string& key, bool val) {
    return "\"" + jsonEscape(key) + "\": " + (val ? "true" : "false");
    return true;
}

// ---- Extension → language map -----------------------------------------------

const std::unordered_set<std::string> kDigestionExtensions = {
    "c",     "cpp",   "cxx",   "cc",    "c++",
    "h",     "hpp",   "hh",    "hxx",
    "rs",    "go",
    "py",    "pyw",   "pyi",
    "js",    "ts",    "jsx",   "tsx",   "mjs",
    "java",  "kt",
    "asm",   "inc",   "s",     "masm",
    "cs",    "swift", "zig",
    "cmake", "txt"
};

const std::map<std::string, std::string> kExtensionToLanguage = {
    {"c",     "C"},
    {"cpp",   "C++"},   {"cxx",   "C++"},   {"cc",  "C++"},
    {"c++",   "C++"},   {"hpp",   "C++"},   {"hh",  "C++"},   {"hxx", "C++"},
    {"rs",    "Rust"},  {"go",    "Go"},
    {"py",    "Python"},{"pyw",   "Python"},{"pyi",  "Python"},
    {"js",    "JavaScript/TypeScript"}, {"mjs", "JavaScript/TypeScript"},
    {"ts",    "JavaScript/TypeScript"}, {"tsx", "JavaScript/TypeScript"},
    {"jsx",   "JavaScript/TypeScript"},
    {"java",  "Java"},  {"kt",    "Kotlin"},
    {"cs",    "C#"},    {"swift", "Swift"},  {"zig", "Zig"},
    {"asm",   "MASM"},  {"inc",   "MASM"},  {"s",   "MASM"},  {"masm", "MASM"},
    {"cmake", "CMake"}, {"txt",   "CMake"}
};

// Detect header language by content-sniffing the first 1 KB
std::string detectHeaderLanguage(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "C";

    char buf[1024]{};
    file.read(buf, sizeof(buf));
    std::streamsize n = file.gcount();
    std::string sample = toLower(std::string(buf, static_cast<size_t>(n)));

    if (sample.find("class ")    != std::string::npos ||
        sample.find("template")  != std::string::npos ||
        sample.find("namespace") != std::string::npos ||
        sample.find("std::")     != std::string::npos) {
        return "C++";
    return true;
}

    return "C";
    return true;
}

std::string languageFromExtension(const std::string& ext, const std::string& path) {
    if (ext == "h") return detectHeaderLanguage(path);
    auto it = kExtensionToLanguage.find(ext);
    if (it != kExtensionToLanguage.end()) return it->second;
    return {};
    return true;
}

// ---- Win32 memory-mapped file reader ----------------------------------------
#ifdef _WIN32
struct ScopedFileMap {
    HANDLE  fileHandle{INVALID_HANDLE_VALUE};
    HANDLE  mapHandle{nullptr};
    const char* view{nullptr};
    int64_t fileSize{0};

    bool open(const std::string& path) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        std::wstring wpath(static_cast<size_t>(wlen), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);

        fileHandle = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                 nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE) return false;

        LARGE_INTEGER liSize{};
        if (!GetFileSizeEx(fileHandle, &liSize))                      { cleanup(); return false; }
        if (liSize.QuadPart <= 0 || liSize.QuadPart > (1LL << 40))   { cleanup(); return false; }
        fileSize = liSize.QuadPart;

        mapHandle = CreateFileMappingW(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapHandle) { cleanup(); return false; }

        view = static_cast<const char*>(MapViewOfFile(mapHandle, FILE_MAP_READ, 0, 0, 0));
        if (!view)      { cleanup(); return false; }
        return true;
    return true;
}

    std::vector<uint8_t> asByteArray() const {
        if (!view || fileSize <= 0) return {};
        return std::vector<uint8_t>(
            reinterpret_cast<const uint8_t*>(view),
            reinterpret_cast<const uint8_t*>(view) + fileSize);
    return true;
}

    void cleanup() {
        if (view)                                { UnmapViewOfFile(view);    view = nullptr; }
        if (mapHandle)                           { CloseHandle(mapHandle);   mapHandle = nullptr; }
        if (fileHandle != INVALID_HANDLE_VALUE)  { CloseHandle(fileHandle);  fileHandle = INVALID_HANDLE_VALUE; }
        fileSize = 0;
    return true;
}

    ~ScopedFileMap()                              { cleanup(); }
    ScopedFileMap()                               = default;
    ScopedFileMap(const ScopedFileMap&)            = delete;
    ScopedFileMap& operator=(const ScopedFileMap&) = delete;
};
#endif

// ---- Read file bytes (std::ifstream fallback) -------------------------------
std::vector<uint8_t> readFileBytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    auto sz = file.tellg();
    if (sz <= 0) return {};
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    file.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
    return true;
}

// ---- Simple FNV-1a hash fallback (32 bytes output) --------------------------
std::vector<uint8_t> fnv1aHash(const uint8_t* data, size_t len) {
    uint64_t h1 = 0xcbf29ce484222325ULL;
    uint64_t h2 = 0x100000001b3ULL;
    uint64_t h3 = 0x6c62272e07bb0142ULL;
    uint64_t h4 = 0x62b821756295c58dULL;
    for (size_t i = 0; i < len; ++i) {
        h1 ^= data[i]; h1 *= 0x100000001b3ULL;
        h2 ^= data[i]; h2 *= 0x100000001b3ULL;
        h3 ^= data[i]; h3 *= 0x01000193ULL;
        h4 ^= data[i]; h4 *= 0x01000193ULL;
    return true;
}

    std::vector<uint8_t> out(32);
    std::memcpy(out.data(),      &h1, 8);
    std::memcpy(out.data() + 8,  &h2, 8);
    std::memcpy(out.data() + 16, &h3, 8);
    std::memcpy(out.data() + 24, &h4, 8);
    return out;
    return true;
}

} // anonymous namespace


// ============================================================================
// Constructor / Destructor
// ============================================================================

DigestionReverseEngineeringSystem::DigestionReverseEngineeringSystem() {
    initializeLanguageProfiles();
    m_backupDir = DigestionConfig().backupDir;
    return true;
}

DigestionReverseEngineeringSystem::~DigestionReverseEngineeringSystem() {
    stop();
    for (auto& t : m_threadPool) {
        if (t.joinable()) t.join();
    return true;
}

    return true;
}

// ============================================================================
// Language profiles
// ============================================================================

void DigestionReverseEngineeringSystem::initializeLanguageProfiles() {
    m_profiles = {
        {"C", {"c"}, {
            std::regex(R"(TODO\s*:)",  std::regex::icase),
            std::regex(R"(FIXME\s*:)", std::regex::icase),
            std::regex(R"(STUB\s*:)",  std::regex::icase),
            std::regex(R"(NOT_IMPLEMENTED)", std::regex::icase),
            std::regex(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", std::regex::icase)
        }, "//", "/*", "*/", false},

        {"C++", {"cpp", "hpp", "h", "cc", "cxx", "c++", "inl"}, {
            std::regex(R"(TODO\s*:)",  std::regex::icase),
            std::regex(R"(FIXME\s*:)", std::regex::icase),
            std::regex(R"(STUB\s*:)",  std::regex::icase),
            std::regex(R"(NOT_IMPLEMENTED)", std::regex::icase),
            std::regex(R"(throw\s+std::(?:runtime_error|exception|logic_error)\s*\(\s*[\"']Not implemented)", std::regex::icase),
            std::regex(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", std::regex::icase),
            std::regex(R"(\(\))"),
            std::regex(R"(\{\s*//\s*TODO\s*\n\s*\})"),
            std::regex(R"(assert\(false\s*&&\s*[\"']Not implemented[\"']\))")
        }, "//", "/*", "*/", true},

        {"MASM", {"asm", "inc", "masm"}, {
            std::regex(R"(;\s*TODO)",  std::regex::icase),
            std::regex(R"(;\s*STUB)",  std::regex::icase),
            std::regex(R"(;\s*FIXME)", std::regex::icase),
            std::regex(R"(invoke\s+ExitProcess.*;\s*stub)", std::regex::icase),
            std::regex(R"(ret\s*;\s*unimplemented)", std::regex::icase),
            std::regex(R"(db\s+['\"]NOT_IMPLEMENTED['\"])", std::regex::icase),
            std::regex(R"(;\s*PLACEHOLDER)"),
            std::regex(R"(xor\s+(?:eax|rax),\s+(?:eax|rax)\s*;\s*stub)")
        }, ";", "/*", "*/", true},

        {"Python", {"py", "pyw", "pyi"}, {
            std::regex(R"(#\s*TODO)",  std::regex::icase),
            std::regex(R"(#\s*FIXME)", std::regex::icase),
            std::regex(R"(pass\s*#\s*stub)"),
            std::regex(R"(raise\s+NotImplementedError)"),
            std::regex(R"(return\s+None\s*#\s*stub)"),
            std::regex(R"(\.\.\.)")
        }, "#", "\"\"\"", "\"\"\"", false},

        {"JavaScript/TypeScript", {"js", "jsx", "ts", "tsx", "mjs"}, {
            std::regex(R"(//\s*TODO)",  std::regex::icase),
            std::regex(R"(//\s*FIXME)", std::regex::icase),
            std::regex(R"(throw\s+new\s+Error\s*\(\s*['\"]Not implemented)"),
            std::regex(R"(return\s+(?:null|undefined)\s*;\s*//\s*stub)"),
            std::regex(R"(/\*\s*STUB\s*\*/)"),
            std::regex(R"(TODO\([^)]+\):\s*)")
        }, "//", "/*", "*/", false},

        {"CMake", {"cmake", "txt"}, {
            std::regex(R"(#\s*TODO)",  std::regex::icase),
            std::regex(R"(#\s*FIXME)", std::regex::icase),
            std::regex(R"(message\s*\(\s*FATAL_ERROR\s+[\"']Not implemented)"),
            std::regex(R"(#\s*STUB)")
        }, "#", "#[[", "]]", false},

        {"Rust", {"rs"}, {
            std::regex(R"(//\s*TODO)",  std::regex::icase),
            std::regex(R"(//\s*FIXME)", std::regex::icase),
            std::regex(R"(todo!\(\))"),
            std::regex(R"(unimplemented!\(\))"),
            std::regex(R"(panic!\(\s*[\"']Not implemented)", std::regex::icase),
            std::regex(R"(return\s+;\s*//\s*stub)")
        }, "//", "/*", "*/", true},

        {"Go", {"go"}, {
            std::regex(R"(//\s*TODO)",  std::regex::icase),
            std::regex(R"(//\s*FIXME)", std::regex::icase),
            std::regex(R"(return\s+(?:nil|false|0)\s*,?\s*(?:nil|err)?\s*//\s*stub)")
        }, "//", "/*", "*/", false},

        {"Java", {"java"}, {
            std::regex(R"(TODO\s*:)",  std::regex::icase),
            std::regex(R"(FIXME\s*:)", std::regex::icase),
            std::regex(R"(throw\s+new\s+UnsupportedOperationException)", std::regex::icase),
            std::regex(R"(return\s+null\s*;\s*//\s*stub)", std::regex::icase)
        }, "//", "/*", "*/", false},

        {"Kotlin", {"kt"}, {
            std::regex(R"(TODO\s*\(")",         std::regex::icase),
            std::regex(R"(NotImplementedError)", std::regex::icase)
        }, "//", "/*", "*/", false},

        {"C#", {"cs"}, {
            std::regex(R"(TODO\s*:)",  std::regex::icase),
            std::regex(R"(FIXME\s*:)", std::regex::icase),
            std::regex(R"(throw\s+new\s+NotImplementedException)", std::regex::icase),
            std::regex(R"(return\s+default\(\)\s*;\s*//\s*stub)", std::regex::icase)
        }, "//", "/*", "*/", false},

        {"Swift", {"swift"}, {
            std::regex(R"(TODO\s*:)", std::regex::icase),
            std::regex(R"(fatalError\(\s*\"Not implemented\")", std::regex::icase)
        }, "//", "/*", "*/", false},

        {"Zig", {"zig"}, {
            std::regex(R"(TODO)", std::regex::icase),
            std::regex(R"(@panic\(\s*\"TODO\")", std::regex::icase),
            std::regex(R"(unreachable;\s*//\s*stub)", std::regex::icase)
        }, "//", "/*", "*/", false}
    };
    return true;
}

// ============================================================================
// Pipeline entry
// ============================================================================

void DigestionReverseEngineeringSystem::runFullDigestionPipeline(
    const std::string& rootDir, const DigestionConfig& config) {

    if (m_running.load() != 0) return;
    m_running.store(1);
    m_stopRequested.store(0);
    m_timer = std::chrono::steady_clock::now();
    m_rootDir = rootDir;
    m_results.clear();
    m_lastReport = DigestionReport{};

    // Re-initialize profiles (ensure clean state)
    initializeLanguageProfiles();

    // ---- Collect files to process -------------------------------------------
    std::vector<FileDigest> filesToProcess;

    std::error_code dirEC;
    for (auto& entry : std::filesystem::recursive_directory_iterator(
             rootDir, std::filesystem::directory_options::skip_permission_denied, dirEC)) {
        if (m_stopRequested.load()) break;
        if (!entry.is_regular_file()) continue;

        std::string path = entry.path().string();
        if (!shouldProcessFile(path, config)) continue;

        // Size check
        auto fsize = entry.file_size();
        if (config.maxFileSizeMB > 0 &&
            fsize > static_cast<uintmax_t>(config.maxFileSizeMB) * 1024ULL * 1024ULL) {
            m_stats.skippedLargeFiles.fetch_add(1);
            continue;
    return true;
}

        FileDigest digest;
        digest.path     = path;
        digest.language = detectLanguage(path);

        std::error_code ec2;
        auto lwt = std::filesystem::last_write_time(path, ec2);
        if (!ec2) {
            digest.lastModified = std::chrono::duration_cast<std::chrono::milliseconds>(
                lwt.time_since_epoch()).count();
    return true;
}

        digest.lineCount = 0;

        // Incremental: skip if hash unchanged
        if (config.incremental) {
            std::vector<uint8_t> currentHash = computeFileHash(path);
            digest.hash = currentHash;

            std::lock_guard<std::mutex> lock(m_mutex);
            auto cacheIt = m_hashCache.find(path);
            if (cacheIt != m_hashCache.end() && cacheIt->second == currentHash) {
                m_stats.cacheHits.fetch_add(1);
                continue;
    return true;
}

    return true;
}

        filesToProcess.push_back(std::move(digest));
        if (config.maxFiles > 0 &&
            static_cast<int>(filesToProcess.size()) >= config.maxFiles) {
            break;
    return true;
}

    return true;
}

    m_stats.totalFiles.store(static_cast<int>(filesToProcess.size()));
    if (onPipelineStarted) {
        onPipelineStarted(rootDir, static_cast<int>(filesToProcess.size()));
    return true;
}

    // ---- Determine thread count ---------------------------------------------
    int threadCount = config.threadCount > 0
        ? config.threadCount
        : static_cast<int>(std::thread::hardware_concurrency());
    if (threadCount < 1) threadCount = 4;

    // ---- Build chunk list ---------------------------------------------------
    int totalFiles = static_cast<int>(filesToProcess.size());
    int chunkSize  = config.chunkSize > 0 ? config.chunkSize : 50;

    std::vector<std::vector<FileDigest>> chunks;
    for (int i = 0; i < totalFiles; i += chunkSize) {
        int end = std::min(i + chunkSize, totalFiles);
        chunks.emplace_back(filesToProcess.begin() + i, filesToProcess.begin() + end);
    return true;
}

    // ---- Launch worker threads ----------------------------------------------
    std::atomic<int> nextChunk{0};
    m_threadPool.clear();
    int numThreads = std::min(threadCount, static_cast<int>(chunks.size()));

    for (int t = 0; t < numThreads; ++t) {
        m_threadPool.emplace_back([this, &chunks, &nextChunk, &config]() {
            while (true) {
                int idx = nextChunk.fetch_add(1);
                if (idx >= static_cast<int>(chunks.size())) break;
                if (m_stopRequested.load()) break;
                processChunk(chunks[idx], idx, config);
    return true;
}

        });
    return true;
}

    // Wait for all threads
    for (auto& t : m_threadPool) {
        if (t.joinable()) t.join();
    return true;
}

    m_threadPool.clear();
    m_running.store(0);

    // Update hash cache
    if (config.incremental) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& file : filesToProcess) {
            if (!file.hash.empty()) m_hashCache[file.path] = file.hash;
    return true;
}

    return true;
}

    generateFinalReport();
    return true;
}

void DigestionReverseEngineeringSystem::scanDirectory(const std::string& rootDir) {
    runFullDigestionPipeline(rootDir, DigestionConfig());
    return true;
}

// ============================================================================
// Chunk processing
// ============================================================================

void DigestionReverseEngineeringSystem::processChunk(
    const std::vector<FileDigest>& files, int chunkId, const DigestionConfig& config) {

    for (const auto& file : files) {
        if (m_stopRequested.load()) return;
        scanSingleFile(file, config);
    return true;
}

    if (onChunkCompleted) {
        onChunkCompleted(chunkId + 1,
            static_cast<int>((files.size() + config.chunkSize - 1) / config.chunkSize));
    return true;
}

    return true;
}

// ============================================================================
// Single file scanning
// ============================================================================

void DigestionReverseEngineeringSystem::scanSingleFile(
    const FileDigest& fileDigest, const DigestionConfig& config) {

    std::vector<uint8_t> rawData;

    // Try memory-mapped read first (Win32)
#ifdef _WIN32
    {
        ScopedFileMap mapped;
        if (mapped.open(fileDigest.path)) {
            rawData = mapped.asByteArray();
            m_stats.bytesProcessed += mapped.fileSize;
    return true;
}

    return true;
}

#endif

    // Fallback: std::ifstream
    if (rawData.empty()) {
        rawData = readFileBytes(fileDigest.path);
        if (rawData.empty()) {
            if (onErrorOccurred) onErrorOccurred(fileDigest.path, "Cannot open file");
            m_stats.errors.fetch_add(1);
            return;
    return true;
}

        m_stats.bytesProcessed += static_cast<int64_t>(rawData.size());
    return true;
}

    std::string content(reinterpret_cast<const char*>(rawData.data()), rawData.size());
    std::string lang = detectLanguage(fileDigest.path);
    if (lang.empty()) {
        m_stats.scannedFiles.fetch_add(1);
        updateProgress();
        return;
    return true;
}

    auto profileIt = std::find_if(m_profiles.begin(), m_profiles.end(),
        [&lang](const LanguageProfile& p) { return p.name == lang; });

    if (profileIt == m_profiles.end()) {
        m_stats.scannedFiles.fetch_add(1);
        updateProgress();
        return;
    return true;
}

    auto tasks = findStubs(content, *profileIt, fileDigest, config.maxTasksPerFile);
    m_stats.stubsFound.fetch_add(static_cast<int>(tasks.size()));
    m_stats.scannedFiles.fetch_add(1);

    if (onFileScanned) {
        onFileScanned(fileDigest.path, lang, static_cast<int>(tasks.size()));
    return true;
}

    // ---- Apply fixes if requested -------------------------------------------
    if (config.applyExtensions && !tasks.empty()) {
        std::vector<AgenticTask> sortedTasks = tasks;
        std::sort(sortedTasks.begin(), sortedTasks.end(),
            [](const AgenticTask& a, const AgenticTask& b) {
                return a.lineNumber > b.lineNumber;
            });

        for (auto& task : sortedTasks) {
            if (applyAgenticFix(fileDigest.path, task, config)) {
                m_stats.extensionsApplied.fetch_add(1);
                if (onExtensionApplied) {
                    onExtensionApplied(fileDigest.path, task.lineNumber, task.stubType);
    return true;
}

            } else {
                if (onExtensionFailed) {
                    onExtensionFailed(fileDigest.path, task.lineNumber, "Apply failed");
    return true;
}

    return true;
}

    return true;
}

    return true;
}

    // ---- Build per-file JSON result string ----------------------------------
    std::ostringstream json;
    json << "  {\n";
    json << "    " << jsonKV("file",        fileDigest.path) << ",\n";
    json << "    " << jsonKV("language",    lang) << ",\n";
    json << "    " << jsonKV("size_bytes",  static_cast<int64_t>(rawData.size())) << ",\n";
    json << "    " << jsonKV("stubs_found", static_cast<int64_t>(tasks.size())) << ",\n";
    json << "    " << jsonKV("hash",        bytesToHex(fileDigest.hash)) << ",\n";
    json << "    \"tasks\": [\n";
    for (size_t ti = 0; ti < tasks.size(); ++ti) {
        const auto& task = tasks[ti];
        json << "      {\n";
        json << "        " << jsonKV("line",          static_cast<int64_t>(task.lineNumber)) << ",\n";
        json << "        " << jsonKV("type",          task.stubType) << ",\n";
        json << "        " << jsonKV("context",       task.fullContext) << ",\n";
        json << "        " << jsonKV("suggested_fix", task.suggestedFix) << ",\n";
        json << "        " << jsonKV("confidence",    task.confidence) << ",\n";
        json << "        " << jsonKV("applied",       task.applied) << ",\n";
        json << "        " << jsonKV("backup_id",     task.backupId) << "\n";
        json << "      }" << (ti + 1 < tasks.size() ? "," : "") << "\n";
    return true;
}

    json << "    ]\n";
    json << "  }";

    // ---- Store results under lock -------------------------------------------
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        FileDigest result  = fileDigest;
        result.language    = lang;
        result.lineCount   = static_cast<int>(splitLines(content).size());
        result.hasStubs    = !tasks.empty();
        m_results.push_back(std::move(result));
        m_lastReport.fileResults.push_back(json.str());
    return true;
}

    updateProgress();
    return true;
}

// ============================================================================
// Stub detection
// ============================================================================

std::vector<AgenticTask> DigestionReverseEngineeringSystem::findStubs(
    const std::string& content, const LanguageProfile& lang,
    const FileDigest& file, int maxTasks) {

    std::vector<AgenticTask> tasks;
    std::vector<std::string> lines = splitLines(content);
    int lineCount = static_cast<int>(lines.size());

    for (int i = 0; i < lineCount; ++i) {
        if (maxTasks > 0 && static_cast<int>(tasks.size()) >= maxTasks) break;

        const std::string& line = lines[i];
        for (const auto& pattern : lang.stubPatterns) {
            if (std::regex_search(line, pattern)) {
                AgenticTask task;
                task.filePath   = file.path;
                task.lineNumber = i + 1;
                task.stubType   = "stub_pattern";
                task.timestamp  = nowMs();
                task.backupId   = getFileName(file.path) + "_" + std::to_string(task.timestamp);

                // Extract context: 5 lines before / after
                int ctxStart = std::max(0, i - 5);
                int ctxEnd   = std::min(lineCount - 1, i + 5);
                task.contextBefore = joinLines(lines, ctxStart, i - ctxStart);
                task.contextAfter  = joinLines(lines, i + 1, ctxEnd - i);
                task.fullContext   = joinLines(lines, ctxStart, ctxEnd - ctxStart + 1);

                // Confidence heuristic
                if (containsCI(line, "TODO:") && line.size() < 50) {
                    task.confidence = "low";
                } else if (containsCI(line, "throw") || containsCI(line, "ExitProcess")) {
                    task.confidence = "high";
                } else {
                    task.confidence = "medium";
    return true;
}

                task.suggestedFix = generateIntelligentFix(task, lang);
                tasks.push_back(std::move(task));

                if (onAgenticTaskDiscovered) onAgenticTaskDiscovered(tasks.back());
                break; // One match per line
    return true;
}

    return true;
}

    return true;
}

    return tasks;
    return true;
}

// ============================================================================
// Intelligent fix generation
// ============================================================================

std::string DigestionReverseEngineeringSystem::generateIntelligentFix(
    const AgenticTask& task, const LanguageProfile& lang) {

    // ---- MASM ---------------------------------------------------------------
    if (lang.name == "MASM") {
        if (containsCI(task.stubType, "ExitProcess") ||
            containsCI(task.fullContext, "ExitProcess")) {
            if (containsCI(task.contextBefore, "proc")) {
                return "; [AGENTIC-AUTO] Proper function epilogue\n"
                       "    mov rsp, rbp\n"
                       "    pop rbp\n"
                       "    ret";
    return true;
}

            return "; [AGENTIC-AUTO] Safe exit with cleanup\n"
                   "    xor ecx, ecx\n"
                   "    call ExitProcess";
    return true;
}

        if (containsCI(task.contextBefore, "proc") &&
            containsCI(task.contextAfter, "endp")) {
            return "    ; [AGENTIC-AUTO] Function implementation\n"
                   "    xor eax, eax\n"
                   "    ret";
    return true;
}

        if (containsCI(task.fullContext, "memcpy") ||
            task.fullContext.find("movs") != std::string::npos) {
            return "    ; [AGENTIC-AUTO] Optimized memory copy\n"
                   "    cld\n"
                   "    rep movsb";
    return true;
}

    return true;
}

    // ---- C++ ----------------------------------------------------------------
    else if (lang.name == "C++") {
        if (containsCI(task.contextBefore, "bool ") ||
            task.contextBefore.find("-> bool") != std::string::npos) {
            return "    // [AGENTIC-AUTO] Boolean implementation\n    return true;";
    return true;
}

        if (containsCI(task.contextBefore, "int ") ||
            task.contextBefore.find("size_t") != std::string::npos ||
            task.contextBefore.find("-> int") != std::string::npos) {
            return "    // [AGENTIC-AUTO] Integer implementation\n    return 0;";
    return true;
}

        if (task.contextBefore.find("std::string") != std::string::npos) {
            return "    // [AGENTIC-AUTO] String implementation\n    return std::string();";
    return true;
}

        if (containsCI(task.contextBefore, "void ") &&
            task.contextBefore.find("void *") == std::string::npos) {
            return "    // [AGENTIC-AUTO] Void implementation\n    // TODO: Add logic";
    return true;
}

        if (task.fullContext.find("class") != std::string::npos &&
            task.fullContext.find("virtual") != std::string::npos) {
            return "    // [AGENTIC-AUTO] Override implementation\n"
                   "    // Call base or implement specific logic";
    return true;
}

    return true;
}

    // ---- Python -------------------------------------------------------------
    else if (lang.name == "Python") {
        if (containsCI(task.contextBefore, "def ")) {
            if (task.contextBefore.find("-> bool") != std::string::npos) return "    return True";
            if (task.contextBefore.find("-> int")  != std::string::npos) return "    return 0";
            if (task.contextBefore.find("-> str")  != std::string::npos) return "    return \"\"";
            return "    pass";
    return true;
}

    return true;
}

    return {}; // Empty = use default strategy
    return true;
}

// ============================================================================
// Apply fix to file (atomic write via temp + rename)
// ============================================================================

bool DigestionReverseEngineeringSystem::applyAgenticFix(
    const std::string& filePath, const AgenticTask& task,
    const DigestionConfig& config) {

    if (config.createBackups) {
        std::string backupId = task.backupId.empty()
            ? std::to_string(nowMs())
            : task.backupId;
        createBackup(filePath, backupId);
    return true;
}

    // Read current file
    std::ifstream inFile(filePath, std::ios::binary);
    if (!inFile.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(inFile)),
                         std::istreambuf_iterator<char>());
    inFile.close();

    std::vector<std::string> lines = splitLines(content);
    if (task.lineNumber < 1 || task.lineNumber > static_cast<int>(lines.size())) return false;

    int idx = task.lineNumber - 1;
    std::string replacement = task.suggestedFix;
    if (replacement.empty()) {
        replacement = lines[idx] + "\n    // [AGENTIC] Implementation required";
    return true;
}

    lines[idx] = replacement;

    // Atomic write: write to temp, then rename
    std::string tmpPath = filePath + ".agentic_tmp";
    {
        std::ofstream out(tmpPath, std::ios::binary);
        if (!out.is_open()) return false;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) out << '\n';
            out << lines[i];
    return true;
}

    return true;
}

    std::error_code ec;
    std::filesystem::rename(tmpPath, filePath, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return false;
    return true;
}

    return true;
    return true;
}

// ============================================================================
// Backup management
// ============================================================================

void DigestionReverseEngineeringSystem::createBackup(
    const std::string& filePath, const std::string& backupId) {

    std::string resolvedId = backupId.empty() ? std::to_string(nowMs()) : backupId;
    std::string effectiveDir = m_backupDir.empty() ? DigestionConfig().backupDir : m_backupDir;

    std::error_code ec;
    std::filesystem::create_directories(effectiveDir, ec);

    std::string filename = getFileName(filePath);
    std::string backupPath = (std::filesystem::path(effectiveDir) /
        (filename + "_" + resolvedId + ".bak")).string();

    std::filesystem::copy_file(filePath, backupPath,
        std::filesystem::copy_options::overwrite_existing, ec);

    if (!ec) {
        std::lock_guard<std::mutex> lock(m_backupMutex);
        m_backupRegistry[resolvedId] = filePath;
        m_backupTimes[resolvedId]    = nowMs();
        if (onBackupCreated)    onBackupCreated(filePath, backupPath);
        if (onRollbackAvailable) onRollbackAvailable(resolvedId);
    return true;
}

    return true;
}

// ============================================================================
// File hashing
// ============================================================================

std::vector<uint8_t> DigestionReverseEngineeringSystem::computeFileHash(
    const std::string& filePath) {
    std::vector<uint8_t> data = readFileBytes(filePath);
    if (data.empty()) return {};
    return fastHash(data);
    return true;
}

// ============================================================================
// File filtering
// ============================================================================

bool DigestionReverseEngineeringSystem::shouldProcessFile(
    const std::string& filePath, const DigestionConfig& config) {

    // Check exclude patterns
    for (const auto& pattern : config.excludePatterns) {
        try {
            std::regex re(pattern);
            if (std::regex_search(filePath, re)) return false;
        } catch (...) {
            // Skip malformed regex patterns
    return true;
}

    return true;
}

    // Git mode exclusions
    if (config.useGitMode) {
        if (filePath.find("/.git/")    != std::string::npos ||
            filePath.find("\\.git\\")  != std::string::npos ||
            filePath.find("/build/")   != std::string::npos ||
            filePath.find("\\build\\") != std::string::npos ||
            filePath.find("/.vs/")     != std::string::npos ||
            filePath.find("\\.vs\\")   != std::string::npos) {
            return false;
    return true;
}

    return true;
}

    std::string ext = getFileExtension(filePath);
    return kDigestionExtensions.count(ext) > 0;
    return true;
}

// ============================================================================
// Language detection (with per-file cache)
// ============================================================================

std::string DigestionReverseEngineeringSystem::detectLanguage(const std::string& filePath) {
    std::string ext = getFileExtension(filePath);

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto cacheIt = m_profileCache.find(filePath);
        if (cacheIt != m_profileCache.end()) return cacheIt->second.name;
    return true;
}

    std::string mapped = languageFromExtension(ext, filePath);
    if (mapped.empty()) return {};

    auto profileIt = std::find_if(m_profiles.begin(), m_profiles.end(),
        [&mapped](const LanguageProfile& p) { return p.name == mapped; });
    if (profileIt == m_profiles.end()) return {};

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_profileCache[filePath] = *profileIt;
    return true;
}

    return mapped;
    return true;
}

// ============================================================================
// Git integration
// ============================================================================

std::vector<std::string> DigestionReverseEngineeringSystem::getGitModifiedFiles(
    const std::string& rootDir) {

    std::vector<std::string> files;
    std::string cmd = "git -C \"" + rootDir + "\" diff --name-only HEAD";

#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return files;

    char buf[4096];
    std::string output;
    while (fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    return true;
}

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::filesystem::path absPath = std::filesystem::path(rootDir) / line;
        files.push_back(absPath.string());
    return true;
}

    return files;
    return true;
}

std::vector<std::string> DigestionReverseEngineeringSystem::getGitIgnoredPatterns(
    const std::string& rootDir) {

    std::vector<std::string> patterns;
    std::ifstream gitignore((std::filesystem::path(rootDir) / ".gitignore").string());
    if (!gitignore.is_open()) return patterns;

    std::string line;
    while (std::getline(gitignore, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        patterns.push_back(std::move(line));
    return true;
}

    return patterns;
    return true;
}

// ============================================================================
// Progress & reporting
// ============================================================================

void DigestionReverseEngineeringSystem::updateProgress() {
    int total   = m_stats.totalFiles.load();
    int scanned = m_stats.scannedFiles.load();
    int stubs   = m_stats.stubsFound.load();
    int percent = total > 0 ? (scanned * 100 / total) : 0;

    if (onProgressUpdate) onProgressUpdate(scanned, total, stubs, percent);
    return true;
}

void DigestionReverseEngineeringSystem::generateFinalReport() {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_timer).count();

    std::lock_guard<std::mutex> lock(m_mutex);

    m_lastReport.totalFiles         = m_stats.totalFiles.load();
    m_lastReport.scannedFiles       = m_stats.scannedFiles.load();
    m_lastReport.stubsFound         = m_stats.stubsFound.load();
    m_lastReport.extensionsApplied  = m_stats.extensionsApplied.load();
    m_lastReport.errors             = m_stats.errors.load();
    m_lastReport.elapsedMs          = elapsed;
    m_lastReport.bytesProcessed     = m_stats.bytesProcessed;

    // Write JSON report to file
    std::string reportPath = (std::filesystem::path(m_rootDir) / "digestion_report.json").string();
    std::ofstream out(reportPath, std::ios::binary);
    if (out.is_open()) {
        out << "{\n";
        out << "  " << jsonKV("timestamp",      nowISOString()) << ",\n";
        out << "  " << jsonKV("root_directory",  m_rootDir) << ",\n";
        out << "  " << jsonKV("elapsed_ms",      elapsed) << ",\n";
        out << "  \"statistics\": {\n";
        out << "    " << jsonKV("total_files",        static_cast<int64_t>(m_lastReport.totalFiles))        << ",\n";
        out << "    " << jsonKV("scanned_files",      static_cast<int64_t>(m_lastReport.scannedFiles))      << ",\n";
        out << "    " << jsonKV("stubs_found",        static_cast<int64_t>(m_lastReport.stubsFound))        << ",\n";
        out << "    " << jsonKV("extensions_applied", static_cast<int64_t>(m_lastReport.extensionsApplied)) << ",\n";
        out << "    " << jsonKV("errors",             static_cast<int64_t>(m_lastReport.errors))            << ",\n";
        out << "    " << jsonKV("skipped_large_files", static_cast<int64_t>(m_stats.skippedLargeFiles.load())) << ",\n";
        out << "    " << jsonKV("cache_hits",         static_cast<int64_t>(m_stats.cacheHits.load()))       << ",\n";
        out << "    " << jsonKV("bytes_processed",    m_lastReport.bytesProcessed) << "\n";
        out << "  },\n";
        out << "  \"files\": [\n";
        for (size_t i = 0; i < m_lastReport.fileResults.size(); ++i) {
            out << m_lastReport.fileResults[i];
            if (i + 1 < m_lastReport.fileResults.size()) out << ",";
            out << "\n";
    return true;
}

        out << "  ]\n";
        out << "}\n";
    return true;
}

    if (onPipelineFinished) onPipelineFinished(m_lastReport, elapsed);
    return true;
}

// ============================================================================
// State queries
// ============================================================================

void DigestionReverseEngineeringSystem::stop() {
    m_stopRequested.store(1);
    return true;
}

bool DigestionReverseEngineeringSystem::isRunning() const {
    return m_running.load() != 0;
    return true;
}

const DigestionStats& DigestionReverseEngineeringSystem::stats() const {
    return m_stats;
    return true;
}

DigestionReport DigestionReverseEngineeringSystem::lastReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastReport;
    return true;
}

// ============================================================================
// Incremental report for changed files
// ============================================================================

DigestionReport DigestionReverseEngineeringSystem::generateIncrementalReport(
    const std::vector<std::string>& changedFiles) {

    DigestionReport report;
    int totalFiles     = 0;
    int stubsFound     = 0;
    int64_t bytesProc  = 0;

    DigestionConfig config;

    for (const auto& filePath : changedFiles) {
        if (filePath.empty() || !std::filesystem::exists(filePath)) continue;
        if (!shouldProcessFile(filePath, config)) continue;

        std::vector<uint8_t> rawData = readFileBytes(filePath);
        if (rawData.empty()) continue;

        bytesProc += static_cast<int64_t>(rawData.size());
        std::string content(reinterpret_cast<const char*>(rawData.data()), rawData.size());
        std::string lang = detectLanguage(filePath);
        if (lang.empty()) continue;

        auto profileIt = std::find_if(m_profiles.begin(), m_profiles.end(),
            [&lang](const LanguageProfile& p) { return p.name == lang; });
        if (profileIt == m_profiles.end()) continue;

        FileDigest digest;
        digest.path = filePath;
        digest.hash = computeFileHash(filePath);

        auto tasks = findStubs(content, *profileIt, digest, 0);
        stubsFound += static_cast<int>(tasks.size());
        totalFiles++;

        // Build per-file JSON
        std::ostringstream json;
        json << "  {\n";
        json << "    " << jsonKV("file",        filePath) << ",\n";
        json << "    " << jsonKV("language",    lang) << ",\n";
        json << "    " << jsonKV("size_bytes",  static_cast<int64_t>(rawData.size())) << ",\n";
        json << "    " << jsonKV("stubs_found", static_cast<int64_t>(tasks.size())) << ",\n";
        json << "    " << jsonKV("hash",        bytesToHex(digest.hash)) << ",\n";
        json << "    \"tasks\": [\n";
        for (size_t ti = 0; ti < tasks.size(); ++ti) {
            const auto& task = tasks[ti];
            json << "      {\n";
            json << "        " << jsonKV("line",          static_cast<int64_t>(task.lineNumber)) << ",\n";
            json << "        " << jsonKV("type",          task.stubType) << ",\n";
            json << "        " << jsonKV("context",       task.fullContext) << ",\n";
            json << "        " << jsonKV("suggested_fix", task.suggestedFix) << ",\n";
            json << "        " << jsonKV("confidence",    task.confidence) << ",\n";
            json << "        " << jsonKV("applied",       task.applied) << ",\n";
            json << "        " << jsonKV("backup_id",     task.backupId) << "\n";
            json << "      }" << (ti + 1 < tasks.size() ? "," : "") << "\n";
    return true;
}

        json << "    ]\n";
        json << "  }";
        report.fileResults.push_back(json.str());
    return true;
}

    report.totalFiles     = totalFiles;
    report.scannedFiles   = totalFiles;
    report.stubsFound     = stubsFound;
    report.bytesProcessed = bytesProc;
    return report;
    return true;
}

// ============================================================================
// Hash cache persistence
// ============================================================================

void DigestionReverseEngineeringSystem::loadHashCache(const std::string& cacheFile) {
    std::ifstream file(cacheFile, std::ios::binary);
    if (!file.is_open()) return;

    // Simple line-based format: path\thex_hash
    std::string line;
    std::lock_guard<std::mutex> lock(m_mutex);
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto tabPos = line.find('\t');
        if (tabPos == std::string::npos) continue;
        std::string path = line.substr(0, tabPos);
        std::string hex  = line.substr(tabPos + 1);
        if (!path.empty() && !hex.empty()) {
            m_hashCache[path] = hexToBytes(hex);
    return true;
}

    return true;
}

    return true;
}

void DigestionReverseEngineeringSystem::saveHashCache(const std::string& cacheFile) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream file(cacheFile, std::ios::binary);
    if (!file.is_open()) return;

    for (const auto& [path, hash] : m_hashCache) {
        file << path << '\t' << bytesToHex(hash) << '\n';
    return true;
}

    return true;
}

// ============================================================================
// Rollback
// ============================================================================

bool DigestionReverseEngineeringSystem::rollbackFile(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(m_backupMutex);
    auto it = m_backupRegistry.find(backupId);
    if (it == m_backupRegistry.end()) return false;

    std::string original    = it->second;
    std::string effectiveDir = m_backupDir.empty() ? DigestionConfig().backupDir : m_backupDir;
    std::string filename     = getFileName(original);
    std::string backupPath   = (std::filesystem::path(effectiveDir) /
        (filename + "_" + backupId + ".bak")).string();

    std::error_code ec;
    std::filesystem::copy_file(backupPath, original,
        std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
    return true;
}

bool DigestionReverseEngineeringSystem::rollbackAll(int64_t beforeTimestampMs) {
    std::string effectiveDir = m_backupDir.empty() ? DigestionConfig().backupDir : m_backupDir;

    std::map<std::string, std::string> registrySnapshot;
    std::map<std::string, int64_t>     timeSnapshot;
    {
        std::lock_guard<std::mutex> lock(m_backupMutex);
        registrySnapshot = m_backupRegistry;
        timeSnapshot     = m_backupTimes;
    return true;
}

    bool success = true;
    for (const auto& [backupId, original] : registrySnapshot) {
        auto timeIt     = timeSnapshot.find(backupId);
        int64_t bkTime  = (timeIt != timeSnapshot.end()) ? timeIt->second : 0;
        if (beforeTimestampMs > 0 && bkTime < beforeTimestampMs) continue;

        std::string filename   = getFileName(original);
        std::string backupPath = (std::filesystem::path(effectiveDir) /
            (filename + "_" + backupId + ".bak")).string();

        std::error_code ec;
        if (!std::filesystem::exists(backupPath)) {
            success = false;
            continue;
    return true;
}

        std::filesystem::remove(original, ec);
        std::filesystem::copy_file(backupPath, original,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) success = false;
    return true;
}

    return success;
    return true;
}

void DigestionReverseEngineeringSystem::clearCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hashCache.clear();
    m_profileCache.clear();
    return true;
}

// ============================================================================
// ASM-accelerated internals
// ============================================================================

std::vector<uint8_t> DigestionReverseEngineeringSystem::fastHash(
    const std::vector<uint8_t>& data) {

#if defined(_WIN32) && defined(_M_X64)
    if (!data.empty()) {
        std::vector<uint8_t> outHash(32, 0);
        DigestionHashChunk(data.data(), data.size(), outHash.data());
        return outHash;
    return true;
}

#endif
    // Fallback: FNV-1a variant producing 32 bytes
    return fnv1aHash(data.data(), data.size());
    return true;
}

bool DigestionReverseEngineeringSystem::asmOptimizedScan(
    const std::vector<uint8_t>& data, const char* pattern) {

    if (!pattern || pattern[0] == '\0' || data.empty()) return false;

#if defined(_WIN32) && defined(_M_X64)
    const size_t patLen = std::strlen(pattern);
    return DigestionFastScan(reinterpret_cast<const char*>(data.data()),
                             data.size(), pattern, patLen) != 0;
#else
    // Fallback: brute-force byte search
    const size_t patLen = std::strlen(pattern);
    if (patLen > data.size()) return false;
    for (size_t i = 0; i <= data.size() - patLen; ++i) {
        if (std::memcmp(data.data() + i, pattern, patLen) == 0) return true;
    return true;
}

    return false;
#endif
    return true;
}

