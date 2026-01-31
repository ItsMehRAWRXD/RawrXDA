#include "digestion_reverse_engineering.h"
{"C", {"c"}, {
            std::regex(R"(TODO\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(FIXME\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(STUB\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(NOT_IMPLEMENTED)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", std::regex::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"C++", {"cpp", "hpp", "h", "cc", "cxx", "c++", "inl"}, {
            std::regex(R"(TODO\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(FIXME\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(STUB\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(NOT_IMPLEMENTED)", std::regex::CaseInsensitiveOption),
            std::regex(R"(throw\s+std::(?:runtime_error|exception|logic_error)\s*\(\s*[\"']Not implemented)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", std::regex::CaseInsensitiveOption),
            std::regex(R"(\(\))"),
            std::regex(R"(\{\s*//\s*TODO\s*\n\s*\})"),
            std::regex(R"(assert\(false\s*&&\s*[\"']Not implemented[\"']\))")
        }, "//", "/*", "*/", true},

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#include <memoryapi.h>
#endif

#if defined(_WIN32) && defined(_M_X64)
extern "C" {
    int DigestionFastScan(const char* data, size_t len, const char* pattern, size_t patLen);
    void DigestionHashChunk(const void* data, size_t len, void* outHash);
}
#endif

namespace {

const std::unordered_set<std::string> kDigestionExtensions = {
    std::stringLiteral("c"), std::stringLiteral("cpp"), std::stringLiteral("cxx"), std::stringLiteral("cc"), std::stringLiteral("c++"),
    std::stringLiteral("h"), std::stringLiteral("hpp"), std::stringLiteral("hh"), std::stringLiteral("hxx"),
    std::stringLiteral("rs"), std::stringLiteral("go"),
    std::stringLiteral("py"), std::stringLiteral("pyw"), std::stringLiteral("pyi"),
    std::stringLiteral("js"), std::stringLiteral("ts"), std::stringLiteral("jsx"), std::stringLiteral("tsx"), std::stringLiteral("mjs"),
    std::stringLiteral("java"), std::stringLiteral("kt"),
    std::stringLiteral("asm"), std::stringLiteral("inc"), std::stringLiteral("s"), std::stringLiteral("masm"),
    std::stringLiteral("cs"), std::stringLiteral("swift"), std::stringLiteral("zig"),
    std::stringLiteral("cmake"), std::stringLiteral("txt")
};

const std::map<std::string, std::string> kExtensionToLanguage = {
    {std::stringLiteral("c"), std::stringLiteral("C")},
    {std::stringLiteral("cpp"), std::stringLiteral("C++")},
    {std::stringLiteral("cxx"), std::stringLiteral("C++")},
    {std::stringLiteral("cc"), std::stringLiteral("C++")},
    {std::stringLiteral("c++"), std::stringLiteral("C++")},
    {std::stringLiteral("hpp"), std::stringLiteral("C++")},
    {std::stringLiteral("hh"), std::stringLiteral("C++")},
    {std::stringLiteral("hxx"), std::stringLiteral("C++")},
    {std::stringLiteral("rs"), std::stringLiteral("Rust")},
    {std::stringLiteral("go"), std::stringLiteral("Go")},
    {std::stringLiteral("py"), std::stringLiteral("Python")},
    {std::stringLiteral("pyw"), std::stringLiteral("Python")},
    {std::stringLiteral("pyi"), std::stringLiteral("Python")},
    {std::stringLiteral("js"), std::stringLiteral("JavaScript/TypeScript")},
    {std::stringLiteral("mjs"), std::stringLiteral("JavaScript/TypeScript")},
    {std::stringLiteral("ts"), std::stringLiteral("JavaScript/TypeScript")},
    {std::stringLiteral("tsx"), std::stringLiteral("JavaScript/TypeScript")},
    {std::stringLiteral("jsx"), std::stringLiteral("JavaScript/TypeScript")},
    {std::stringLiteral("java"), std::stringLiteral("Java")},
    {std::stringLiteral("kt"), std::stringLiteral("Kotlin")},
    {std::stringLiteral("cs"), std::stringLiteral("C#")},
    {std::stringLiteral("swift"), std::stringLiteral("Swift")},
    {std::stringLiteral("zig"), std::stringLiteral("Zig")},
    {std::stringLiteral("asm"), std::stringLiteral("MASM")},
    {std::stringLiteral("inc"), std::stringLiteral("MASM")},
    {std::stringLiteral("s"), std::stringLiteral("MASM")},
    {std::stringLiteral("masm"), std::stringLiteral("MASM")},
    {std::stringLiteral("cmake"), std::stringLiteral("CMake")},
    {std::stringLiteral("txt"), std::stringLiteral("CMake")}
};

std::string detectHeaderLanguage(const std::string& path) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly)) {
        return std::stringLiteral("C");
    }

    std::vector<uint8_t> sample = file.read(1024).toLower();
    if (sample.contains("class ") || sample.contains("template") || sample.contains("namespace") || sample.contains("std::")) {
        return std::stringLiteral("C++");
    }
    return std::stringLiteral("C");
}

std::string languageFromExtension(const std::string& ext, const std::string& path) {
    if (ext == "h") {
        return detectHeaderLanguage(path);
    }
    auto it = kExtensionToLanguage.constFind(ext);
    if (it != kExtensionToLanguage.constEnd()) {
        return *it;
    }
    return std::string();
}

#ifdef _WIN32
struct ScopedFileMap {
    HANDLE fileHandle{INVALID_HANDLE_VALUE};
    HANDLE mapHandle{nullptr};
    const char* view{nullptr};
    int64_t fileSize{0};

    bool open(const std::string& path) {
        fileHandle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE) return false;

        LARGE_INTEGER liSize{};
        if (!GetFileSizeEx(fileHandle, &liSize)) {
            cleanup();
            return false;
        }
        if (liSize.QuadPart <= 0 || liSize.QuadPart > (1LL << 40)) { // 1 TB ceiling
            cleanup();
            return false;
        }
        fileSize = liSize.QuadPart;

        mapHandle = CreateFileMappingW(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapHandle) {
            cleanup();
            return false;
        }

        view = static_cast<const char*>(MapViewOfFile(mapHandle, FILE_MAP_READ, 0, 0, 0));
        if (!view) {
            cleanup();
            return false;
        }
        return true;
    }

    std::vector<uint8_t> asByteArray() const {
        return view && fileSize > 0 ? std::vector<uint8_t>::fromRawData(view, static_cast<int>(fileSize)) : std::vector<uint8_t>();
    }

    void cleanup() {
        if (view) {
            UnmapViewOfFile(view);
            view = nullptr;
        }
        if (mapHandle) {
            CloseHandle(mapHandle);
            mapHandle = nullptr;
        }
        if (fileHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(fileHandle);
            fileHandle = INVALID_HANDLE_VALUE;
        }
        fileSize = 0;
    }

    ~ScopedFileMap() {
        cleanup();
    }
};
#endif

}

DigestionReverseEngineeringSystem::DigestionReverseEngineeringSystem()
    , m_profileCache(1000) {
    initializeLanguageProfiles();
    m_threadPool = std::make_unique<std::threadPool>();
    m_backupDir = DigestionConfig().backupDir;
}

DigestionReverseEngineeringSystem::~DigestionReverseEngineeringSystem() {
    stop();
    m_threadPool->waitForDone(5000);
}

void DigestionReverseEngineeringSystem::initializeLanguageProfiles() {
    m_profiles = {
        {"C++", {"cpp", "hpp", "h", "cc", "cxx", "c++", "inl"}, {
            std::regex(R"(TODO\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(FIXME\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(STUB\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(NOT_IMPLEMENTED)", std::regex::CaseInsensitiveOption),
            std::regex(R"(throw\s+std::(?:runtime_error|exception|logic_error)\s*\(\s*[\"']Not implemented)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", std::regex::CaseInsensitiveOption),
            std::regex(R"(\(\))"),
            std::regex(R"(\{\s*//\s*TODO\s*\n\s*\})"),
            std::regex(R"(assert\(false\s*&&\s*[\"']Not implemented[\"']\))")
        }, "//", "/*", "*/", true},
        
        {"MASM", {"asm", "inc", "masm"}, {
            std::regex(R"(;\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(;\s*STUB)", std::regex::CaseInsensitiveOption),
            std::regex(R"(;\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(invoke\s+ExitProcess.*;\s*stub)", std::regex::CaseInsensitiveOption),
            std::regex(R"(ret\s*;\s*unimplemented)", std::regex::CaseInsensitiveOption),
            std::regex(R"(db\s+['\"]NOT_IMPLEMENTED['\"])", std::regex::CaseInsensitiveOption),
            std::regex(R"(;\s*PLACEHOLDER)"),
            std::regex(R"(xor\s+(?:eax|rax),\s+(?:eax|rax)\s*;\s*stub)")
        }, ";", "/*", "*/", true},
        
        {"Python", {"py", "pyw", "pyi"}, {
            std::regex(R"(#\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(#\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(pass\s*#\s*stub)"),
            std::regex(R"(raise\s+NotImplementedError)"),
            std::regex(R"(return\s+None\s*#\s*stub)"),
            std::regex(R"(\.\.\.)")
        }, "#", "\"\"\"", "\"\"\"", false},
        
        {"JavaScript/TypeScript", {"js", "jsx", "ts", "tsx", "mjs"}, {
            std::regex(R"(//\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(//\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(throw\s+new\s+Error\s*\(\s*['\"]Not implemented)"),
            std::regex(R"(return\s+(?:null|undefined)\s*;\s*//\s*stub)"),
            std::regex(R"(/\*\s*STUB\s*\*/)"),
            std::regex(R"(TODO\([^)]+\):\s*)")
        }, "//", "/*", "*/", false},
        
        {"CMake", {"cmake", "txt"}, {
            std::regex(R"(#\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(#\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(message\s*\(\s*FATAL_ERROR\s+[\"']Not implemented)"),
            std::regex(R"(#\s*STUB)")
        }, "#", "#[[", "]]", false},
        
        {"Rust", {"rs"}, {
                {"C", {"c"}, {
                    std::regex(R"(TODO\s*:)", std::regex::CaseInsensitiveOption),
                    std::regex(R"(FIXME\s*:)", std::regex::CaseInsensitiveOption),
                    std::regex(R"(STUB\s*:)", std::regex::CaseInsensitiveOption),
                    std::regex(R"(NOT_IMPLEMENTED)", std::regex::CaseInsensitiveOption),
                    std::regex(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", std::regex::CaseInsensitiveOption)
                }, "//", "/*", "*/", false},

            std::regex(R"(//\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(//\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(todo!\(\))"),
            std::regex(R"(unimplemented!\(\))"),
            std::regex(R"(panic!\(\s*[\"']Not implemented)"),
            std::regex(R"(return\s+;\s*//\s*stub)")
        }, "//", "/*", "*/", true},
        
        {"Go", {"go"}, {
            std::regex(R"(//\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(//\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+(?:nil|false|0)\s*,?\s*(?:nil|err)?\s*//\s*stub)")
        }, "//", "/*", "*/", false},

        {"Java", {"java"}, {
            std::regex(R"(TODO\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(FIXME\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(throw\s+new\s+UnsupportedOperationException)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+null\s*;\s*//\s*stub)", std::regex::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"Kotlin", {"kt"}, {
            std::regex(R"(TODO\s*\(")", std::regex::CaseInsensitiveOption),
            std::regex(R"(NotImplementedError)", std::regex::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"C#", {"cs"}, {
            std::regex(R"(TODO\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(FIXME\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(throw\s+new\s+NotImplementedException)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+default\(\)\s*;\s*//\s*stub)", std::regex::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"Swift", {"swift"}, {
            std::regex(R"(TODO\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(fatalError\(\s*\"Not implemented\")", std::regex::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"Zig", {"zig"}, {
            std::regex(R"(TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(@panic\(\s*\"TODO\")", std::regex::CaseInsensitiveOption),
            std::regex(R"(unreachable;\s*//\s*stub)", std::regex::CaseInsensitiveOption)
        }, "//", "/*", "*/", false}
    };
}

void DigestionReverseEngineeringSystem::runFullDigestionPipeline(const std::string &rootDir, const DigestionConfig &config) {
    if (m_running.loadAcquire()) return;
    m_running.storeRelease(1);
    m_stopRequested.storeRelease(0);

                {"Java", {"java"}, {
                    std::regex(R"(TODO\s*:)", std::regex::CaseInsensitiveOption),
                    std::regex(R"(FIXME\s*:)", std::regex::CaseInsensitiveOption),
                    std::regex(R"(throw\s+new\s+UnsupportedOperationException)", std::regex::CaseInsensitiveOption),
                    std::regex(R"(return\s+null\s*;\s*//\s*stub)", std::regex::CaseInsensitiveOption)
                }, "//", "/*", "*/", false},

                {"Kotlin", {"kt"}, {
                    std::regex(R"(TODO\s*\(")", std::regex::CaseInsensitiveOption),
                    std::regex(R"(NotImplementedError)", std::regex::CaseInsensitiveOption)
                }, "//", "/*", "*/", false},
    
    m_rootDir = rootDir;
    m_profiles = {
        {"C", {"c"}, {
            std::regex(R"(TODO\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(FIXME\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(STUB\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(NOT_IMPLEMENTED)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", std::regex::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"C++", {"cpp", "hpp", "h", "cc", "cxx", "c++", "inl"}, {
            std::regex(R"(TODO\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(FIXME\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(STUB\s*:)", std::regex::CaseInsensitiveOption),
            std::regex(R"(NOT_IMPLEMENTED)", std::regex::CaseInsensitiveOption),
            std::regex(R"(throw\s+std::(?:runtime_error|exception|logic_error)\s*\(\s*[\"']Not implemented)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", std::regex::CaseInsensitiveOption),
            std::regex(R"(\(\))"),
            std::regex(R"(\{\s*//\s*TODO\s*\n\s*\})"),
            std::regex(R"(assert\(false\s*&&\s*[\"']Not implemented[\"']\))")
        }, "//", "/*", "*/", true},

        {"MASM", {"asm", "inc", "masm"}, {
            std::regex(R"(;\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(;\s*STUB)", std::regex::CaseInsensitiveOption),
            std::regex(R"(;\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(invoke\s+ExitProcess.*;\s*stub)", std::regex::CaseInsensitiveOption),
            std::regex(R"(ret\s*;\s*unimplemented)", std::regex::CaseInsensitiveOption),
            std::regex(R"(db\s+['\"]NOT_IMPLEMENTED['\"])", std::regex::CaseInsensitiveOption),
            std::regex(R"(;\s*PLACEHOLDER)"),
            std::regex(R"(xor\s+(?:eax|rax),\s+(?:eax|rax)\s*;\s*stub)")
        }, ";", "/*", "*/", true},

        {"Python", {"py", "pyw", "pyi"}, {
            std::regex(R"(#\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(#\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(pass\s*#\s*stub)"),
            std::regex(R"(raise\s+NotImplementedError)"),
            std::regex(R"(return\s+None\s*#\s*stub)"),
            std::regex(R"(\.\.\.)")
        }, "#", "\"\"\"", "\"\"\"", false},

        {"JavaScript/TypeScript", {"js", "jsx", "ts", "tsx", "mjs"}, {
            std::regex(R"(//\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(//\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(throw\s+new\s+Error\s*\(\s*['\"]Not implemented)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+(?:null|undefined)\s*;\s*//\s*stub)", std::regex::CaseInsensitiveOption),
            std::regex(R"(/\*\s*STUB\s*\*/)", std::regex::CaseInsensitiveOption),
            std::regex(R"(TODO\([^)]+\):\s*)")
        }, "//", "/*", "*/", false},

        {"CMake", {"cmake", "txt"}, {
            std::regex(R"(#\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(#\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(message\s*\(\s*FATAL_ERROR\s+[\"']Not implemented)", std::regex::CaseInsensitiveOption),
            std::regex(R"(#\s*STUB)")
        }, "#", "#[[", "]]", false},

        {"Rust", {"rs"}, {
            std::regex(R"(//\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(//\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(todo!\(\))"),
            std::regex(R"(unimplemented!\(\))"),
            std::regex(R"(panic!\(\s*[\"']Not implemented)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+;\s*//\s*stub)")
        }, "//", "/*", "*/", true},

        {"Go", {"go"}, {
            std::regex(R"(//\s*TODO)", std::regex::CaseInsensitiveOption),
            std::regex(R"(//\s*FIXME)", std::regex::CaseInsensitiveOption),
            std::regex(R"(return\s+(?:nil|false|0)\s*,?\s*(?:nil|err)?\s*//\s*stub)")
        }, "//", "/*", "*/", false}
    };
        digest.lastModified = info.lastModified().toMSecsSinceEpoch();
        digest.lineCount = 0; // Will fill during scan
        
        if (config.incremental) {
            std::vector<uint8_t> currentHash = computeFileHash(path);
            digest.hash = currentHash;
            
            std::mutexLocker lock(&m_mutex);
            if (m_hashCache.contains(path) && m_hashCache[path] == currentHash) {
                m_stats.cacheHits.ref();
                continue; // Skip unchanged
            }
        }
        
        filesToProcess.append(digest);
        if (config.maxFiles > 0 && filesToProcess.size() >= config.maxFiles) break;
    }
    
    m_stats.totalFiles.store(filesToProcess.size());
    pipelineStarted(rootDir, filesToProcess.size());
    
    // Process in chunks
    int chunkCount = (filesToProcess.size() + config.chunkSize - 1) / config.chunkSize;
    QFutureSynchronizer<void> synchronizer;
    
    for (int i = 0; i < filesToProcess.size() && !m_stopRequested.loadAcquire(); i += config.chunkSize) {
        int end = qMin(i + config.chunkSize, filesToProcess.size());
        std::vector<FileDigest> chunk = filesToProcess.mid(i, end - i);
        int chunkId = i / config.chunkSize;
        
        QFuture<void> future = [](auto f){f();}(m_threadPool.get(), [this, chunk, chunkId, config]() {
            processChunk(chunk, chunkId, config);
        });
        synchronizer.addFuture(future);
    }
    
    synchronizer.waitForFinished();
    m_running.storeRelease(0);
    
    // Update hash cache
    if (config.incremental) {
        std::mutexLocker lock(&m_mutex);
        for (const auto &file : filesToProcess) {
            if (!file.hash.empty()) m_hashCache[file.path] = file.hash;
        }
    }
    
    generateFinalReport();
}

void DigestionReverseEngineeringSystem::scanDirectory(const std::string &rootDir) {
    runFullDigestionPipeline(rootDir, DigestionConfig());
}

void DigestionReverseEngineeringSystem::processChunk(const std::vector<FileDigest> &files, int chunkId, const DigestionConfig &config) {
    for (const auto &file : files) {
        if (m_stopRequested.loadAcquire()) return;
        scanSingleFile(file, config);
    }
    chunkCompleted(chunkId + 1, (files.size() + config.chunkSize - 1) / config.chunkSize);
}

void DigestionReverseEngineeringSystem::scanSingleFile(const FileDigest &fileDigest, const DigestionConfig &config) {
    std::vector<uint8_t> rawData;
#ifdef _WIN32
    ScopedFileMap mapped;
    if (mapped.open(fileDigest.path)) {
        rawData = mapped.asByteArray();
        m_stats.bytesProcessed += mapped.fileSize;
    }
#endif

    // File operation removed;
    if (rawData.empty()) {
        if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
            errorOccurred(fileDigest.path, "Cannot open file: " + file.errorString());
            m_stats.errors.ref();
            return;
        }
        rawData = file.readAll();
        file.close();
        m_stats.bytesProcessed += rawData.size();
    }
    
    std::string content = std::string::fromUtf8(rawData);
    std::string lang = detectLanguage(fileDigest.path);
    if (lang.empty()) {
        m_stats.scannedFiles.ref();
        updateProgress();
        return;
    }
    
    auto profileIt = std::find_if(m_profiles.begin(), m_profiles.end(),
        [&lang](const LanguageProfile &p) { return p.name == lang; });
    
    if (profileIt == m_profiles.end()) {
        m_stats.scannedFiles.ref();
        updateProgress();
        return;
    }
    
    auto tasks = findStubs(content, *profileIt, fileDigest, config.maxTasksPerFile);
    
    m_stats.stubsFound.fetchAndAddAcquire(tasks.size());
    m_stats.scannedFiles.ref();
    
    fileScanned(fileDigest.path, lang, tasks.size());
    
    // Apply fixes if requested
    if (config.applyExtensions && !tasks.empty()) {
        std::string modifiedContent = content;
        bool modified = false;
        
        // Sort by line descending to avoid offset issues
        std::vector<AgenticTask> sortedTasks = tasks;
        std::sort(sortedTasks.begin(), sortedTasks.end(), 
                  [](const AgenticTask &a, const AgenticTask &b) { return a.lineNumber > b.lineNumber; });
        
        for (auto &task : sortedTasks) {
            if (applyAgenticFix(fileDigest.path, task, config)) {
                m_stats.extensionsApplied.ref();
                extensionApplied(fileDigest.path, task.lineNumber, task.stubType);
                modified = true;
            } else {
                extensionFailed(fileDigest.path, task.lineNumber, "Apply failed");
            }
        }
    }
    
    // Record results
    void* fileResult;
    fileResult["file"] = fileDigest.path;
    fileResult["language"] = lang;
    fileResult["size_bytes"] = rawData.size();
    fileResult["stubs_found"] = tasks.size();
    fileResult["hash"] = std::string(fileDigest.hash.toHex());
    
    void* taskArray;
    for (const auto &task : tasks) {
        void* t;
        t["line"] = task.lineNumber;
        t["type"] = task.stubType;
        t["context"] = task.fullContext;
        t["suggested_fix"] = task.suggestedFix;
        t["confidence"] = task.confidence;
        t["applied"] = task.applied;
        t["backup_id"] = task.backupId;
        taskArray.append(t);
    }
    fileResult["tasks"] = taskArray;
    
    std::mutexLocker lock(&m_mutex);
    m_results.append(fileResult);
    lock.unlock();
    
    updateProgress();
}

std::vector<AgenticTask> DigestionReverseEngineeringSystem::findStubs(
    const std::string &content, const LanguageProfile &lang, const FileDigest &file, int maxTasks) {
    
    std::vector<AgenticTask> tasks;
    std::stringList lines = content.split('\n');
    
    for (int i = 0; i < lines.size() && (maxTasks == 0 || tasks.size() < maxTasks); ++i) {
        const std::string &line = lines[i];
        
        for (const auto &pattern : lang.stubPatterns) {
            if (pattern.match(line).hasMatch()) {
                AgenticTask task;
                task.filePath = file.path;
                task.lineNumber = i + 1;
                task.stubType = pattern.pattern();
                task.timestamp = // DateTime::currentDateTime().toMSecsSinceEpoch();
                task.backupId = std::string("%1_%2");
                
                // Extract context (5 lines before/after for better AI context)
                int start = qMax(0, i - 5);
                int end = qMin(lines.size() - 1, i + 5);
                task.contextBefore = lines.mid(start, i - start).join('\n');
                task.contextAfter = lines.mid(i + 1, end - i).join('\n');
                task.fullContext = lines.mid(start, end - start + 1).join('\n');
                
                // Determine confidence based on context
                if (line.contains("TODO:", CaseInsensitive) && line.length() < 50) {
                    task.confidence = "low"; // Just a marker
                } else if (line.contains("throw") || line.contains("ExitProcess")) {
                    task.confidence = "high"; // Hard stub
                } else {
                    task.confidence = "medium";
                }
                
                task.suggestedFix = generateIntelligentFix(task, lang);
                tasks.append(task);
                break;
            }
        }
    }
    return tasks;
}

std::string DigestionReverseEngineeringSystem::generateIntelligentFix(const AgenticTask &task, const LanguageProfile &lang) {
    // Context-aware fix generation based on surrounding code patterns
    if (lang.name == "MASM") {
        if (task.stubType.contains("ExitProcess", CaseInsensitive)) {
            if (task.contextBefore.contains("proc", CaseInsensitive)) {
                return "; [AGENTIC-AUTO] Proper function epilogue\n    mov rsp, rbp\n    pop rbp\n    ret";
            }
            return "; [AGENTIC-AUTO] Safe exit with cleanup\n    xor ecx, ecx\n    call ExitProcess";
        }
        if (task.contextBefore.contains("proc", CaseInsensitive) && task.contextAfter.contains("endp", CaseInsensitive)) {
            return "    ; [AGENTIC-AUTO] Function implementation\n    xor eax, eax\n    ret";
        }
        if (task.fullContext.contains("memcpy", CaseInsensitive) || task.fullContext.contains("movs")) {
            return "    ; [AGENTIC-AUTO] Optimized memory copy\n    cld\n    rep movsb";
        }
    } else if (lang.name == "C++") {
        // Check return type from context
        if (task.contextBefore.contains("bool ", CaseInsensitive) || 
            task.contextBefore.contains("-> bool")) {
            return "    // [AGENTIC-AUTO] Boolean implementation\n    return true;";
        }
        if (task.contextBefore.contains("int ", CaseInsensitive) || 
            task.contextBefore.contains("size_t") ||
            task.contextBefore.contains("-> int")) {
            return "    // [AGENTIC-AUTO] Integer implementation\n    return 0;";
        }
        if (task.contextBefore.contains("std::string") || task.contextBefore.contains("std::string")) {
            return "    // [AGENTIC-AUTO] String implementation\n    return std::string();";
        }
        if (task.contextBefore.contains("void ", CaseInsensitive) && 
            !task.contextBefore.contains("void *")) {
            return "    // [AGENTIC-AUTO] Void implementation\n    // TODO: Add logic";
        }
        if (task.fullContext.contains("class") && task.fullContext.contains("virtual")) {
            return "    // [AGENTIC-AUTO] Override implementation\n    // Call base or implement specific logic";
        }
    } else if (lang.name == "Python") {
        if (task.contextBefore.contains("def ", CaseInsensitive)) {
            if (task.contextBefore.contains("-> bool")) return "    return True";
            if (task.contextBefore.contains("-> int")) return "    return 0";
            if (task.contextBefore.contains("-> str")) return "    return \"\"";
            return "    pass";
        }
    }
    
    return std::string(); // Empty = use default
}

bool DigestionReverseEngineeringSystem::applyAgenticFix(const std::string &filePath, const AgenticTask &task, const DigestionConfig &config) {
    if (config.createBackups) {
        const std::string backupId = task.backupId.empty()
            ? std::string::number(// DateTime::currentMSecsSinceEpoch())
            : task.backupId;
        createBackup(filePath, backupId);
    }
    
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) return false;
    
    std::string content = std::string::fromUtf8(file.readAll());
    file.close();
    
    std::stringList lines = content.split('\n');
    if (task.lineNumber < 1 || task.lineNumber > lines.size()) return false;
    
    int idx = task.lineNumber - 1;
    std::string replacement = task.suggestedFix;
    
    if (replacement.empty()) {
        // Generic replacement
        replacement = lines[idx] + "\n    // [AGENTIC] Implementation required";
    }
    
    lines[idx] = replacement;
    
    QSaveFile out(filePath);
    if (!out.open(std::iostream::WriteOnly)) return false;
    out.write(lines.join('\n').toUtf8());
    return out.commit();
}

void DigestionReverseEngineeringSystem::createBackup(const std::string &filePath, const std::string &backupId) {
    const std::string resolvedBackupId = backupId.empty()
        ? std::string::number(// DateTime::currentDateTime().toMSecsSinceEpoch())
        : backupId;
    const std::string effectiveDir = m_backupDir.empty() ? DigestionConfig().backupDir : m_backupDir;
    // backupDir(effectiveDir);
    if (!backupDir.exists()) backupDir.mkpath(".");
    std::string backupPath = backupDir.filePath(
        std::string("%1_%2.bak").fileName(), resolvedBackupId)
    );
    
    if (std::filesystem::copy(filePath, backupPath)) {
        std::mutexLocker lock(&m_backupMutex);
        m_backupRegistry[resolvedBackupId] = filePath;
        m_backupTimes[resolvedBackupId] = // DateTime::currentMSecsSinceEpoch();
        backupCreated(filePath, backupPath);
        rollbackAvailable(resolvedBackupId);
    }
}

std::vector<uint8_t> DigestionReverseEngineeringSystem::computeFileHash(const std::string &filePath) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly)) return std::vector<uint8_t>();
    
    QCryptographicHash hash(QCryptographicHash::Blake2s_256);
    hash.addData(&file);
    return hash.result();
}

bool DigestionReverseEngineeringSystem::shouldProcessFile(const std::string &filePath, const DigestionConfig &config) {
    // Check exclude patterns
    for (const std::string &pattern : config.excludePatterns) {
        std::regex re(pattern);
        if (re.match(filePath).hasMatch()) return false;
    }
    
    // Check gitignore if in git mode
    if (config.useGitMode) {
        // Simple check - could be enhanced with actual git check-ignore
        if (filePath.contains("/.git/") || filePath.contains("/build/") || filePath.contains("/.vs/"))
            return false;
    }
    
    // Check extension
    std::string ext = // FileInfo: filePath).suffix().toLower();
    return kDigestionExtensions.contains(ext);
}

std::string DigestionReverseEngineeringSystem::detectLanguage(const std::string &filePath) {
    // Info info(filePath);
    std::string ext = info.suffix().toLower();
    
    // Check cache first
    {
        std::mutexLocker lock(&m_mutex);
        if (m_profileCache.contains(filePath)) return m_profileCache.object(filePath)->name;
    }
    
    std::string mapped = languageFromExtension(ext, filePath);
    if (mapped.empty()) return std::string();

    auto profileIt = std::find_if(m_profiles.begin(), m_profiles.end(), [&mapped](const LanguageProfile &p) {
        return p.name == mapped;
    });
    if (profileIt == m_profiles.end()) return std::string();

    {
        std::mutexLocker lock(&m_mutex);
        m_profileCache.insert(filePath, new LanguageProfile(*profileIt));
    }
    return mapped;
}

std::stringList DigestionReverseEngineeringSystem::getGitModifiedFiles(const std::string &rootDir) {
    // Process removed
    git.setWorkingDirectory(rootDir);
    git.start("git", std::stringList() << "diff" << "--name-only" << "HEAD");
    git.waitForFinished();
    
    std::string output = std::string::fromUtf8(git.readAllStandardOutput());
    std::stringList files = output.split('\n', SkipEmptyParts);
    
    // Convert to absolute paths
    for (std::string &file : files) {
        file = // (rootDir).absoluteFilePath(file);
    }
    return files;
}

void DigestionReverseEngineeringSystem::updateProgress() {
    int total = m_stats.totalFiles.loadAcquire();
    int scanned = m_stats.scannedFiles.loadAcquire();
    int stubs = m_stats.stubsFound.loadAcquire();
    int percent = total > 0 ? (scanned * 100 / total) : 0;
    
    progressUpdate(scanned, total, stubs, percent);
}

void DigestionReverseEngineeringSystem::generateFinalReport() {
    void* report;
    report["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    report["root_directory"] = m_rootDir;
    report["elapsed_ms"] = m_timer.elapsed();
    report["statistics"] = void*{
        {"total_files", m_stats.totalFiles.loadAcquire()},
        {"scanned_files", m_stats.scannedFiles.loadAcquire()},
        {"stubs_found", m_stats.stubsFound.loadAcquire()},
        {"extensions_applied", m_stats.extensionsApplied.loadAcquire()},
        {"errors", m_stats.errors.loadAcquire()},
        {"skipped_large_files", m_stats.skippedLargeFiles.loadAcquire()},
        {"cache_hits", m_stats.cacheHits.loadAcquire()},
        {"bytes_processed", (int64_t)m_stats.bytesProcessed}
    };
    report["files"] = m_results;
    
    m_lastReport = report;
    
    // File operation removed;
    if (out.open(std::iostream::WriteOnly)) {
        out.write(void*(report).toJson(void*::Indented));
    }
    
    pipelineFinished(report, m_timer.elapsed());
}

void DigestionReverseEngineeringSystem::stop() {
    m_stopRequested.storeRelease(1);
}

bool DigestionReverseEngineeringSystem::isRunning() const {
    return m_running.loadAcquire();
}

DigestionStats DigestionReverseEngineeringSystem::stats() const {
    return m_stats;
}

void* DigestionReverseEngineeringSystem::lastReport() const {
    std::mutexLocker lock(&m_mutex);
    return m_lastReport;
}

void* DigestionReverseEngineeringSystem::generateIncrementalReport(const std::stringList &changedFiles) {
    void* report;
    void* fileResults;
    int totalFiles = 0;
    int stubsFound = 0;
    int64_t bytesProcessed = 0;
    
    DigestionConfig config;
    for (const std::string &filePath : changedFiles) {
        if (filePath.empty() || !// Info::exists(filePath)) continue;
        if (!shouldProcessFile(filePath, config)) continue;
        
        // File operation removed;
        if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) continue;
        std::vector<uint8_t> rawData = file.readAll();
        file.close();
        
        bytesProcessed += rawData.size();
        std::string content = std::string::fromUtf8(rawData);
        std::string lang = detectLanguage(filePath);
        if (lang.empty()) continue;
        
        auto profileIt = std::find_if(m_profiles.begin(), m_profiles.end(),
            [&lang](const LanguageProfile &p) { return p.name == lang; });
        if (profileIt == m_profiles.end()) continue;
        
        FileDigest digest;
        digest.path = filePath;
        digest.hash = computeFileHash(filePath);
        
        auto tasks = findStubs(content, *profileIt, digest, 0);
        stubsFound += tasks.size();
        totalFiles++;
        
        void* fileResult;
        fileResult["file"] = filePath;
        fileResult["language"] = lang;
        fileResult["size_bytes"] = rawData.size();
        fileResult["stubs_found"] = tasks.size();
        fileResult["hash"] = std::string(digest.hash.toHex());
        
        void* taskArray;
        for (const auto &task : tasks) {
            void* t;
            t["line"] = task.lineNumber;
            t["type"] = task.stubType;
            t["context"] = task.fullContext;
            t["suggested_fix"] = task.suggestedFix;
            t["confidence"] = task.confidence;
            t["backup_id"] = task.backupId;
            taskArray.append(t);
        }
        fileResult["tasks"] = taskArray;
        fileResults.append(fileResult);
    }
    
    report["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    report["files"] = fileResults;
    report["statistics"] = void*{
        {"total_files", totalFiles},
        {"stubs_found", stubsFound},
        {"bytes_processed", bytesProcessed}
    };
    
    return report;
}

void DigestionReverseEngineeringSystem::loadHashCache(const std::string &cacheFile) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly)) return;
    
    void* doc = void*::fromJson(file.readAll());
    void* obj = doc.object();
    
    std::mutexLocker lock(&m_mutex);
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        m_hashCache[it.key()] = std::vector<uint8_t>::fromHex(it.value().toString().toUtf8());
    }
}

void DigestionReverseEngineeringSystem::saveHashCache(const std::string &cacheFile) {
    std::mutexLocker lock(&m_mutex);
    void* obj;
    for (auto it = m_hashCache.begin(); it != m_hashCache.end(); ++it) {
        obj[it.key()] = std::string(it.value().toHex());
    }
    lock.unlock();
    
    // File operation removed;
    if (file.open(std::iostream::WriteOnly)) {
        file.write(void*(obj).toJson());
    }
}

bool DigestionReverseEngineeringSystem::rollbackFile(const std::string &backupId) {
    std::mutexLocker lock(&m_backupMutex);
    if (!m_backupRegistry.contains(backupId)) return false;
    
    std::string original = m_backupRegistry[backupId];
    std::string backupPath = // (m_backupDir.empty() ? DigestionConfig().backupDir : m_backupDir)
        .filePath(std::string("%1_%2.bak").fileName(), backupId));
    lock.unlock();
    
    return std::filesystem::copy(backupPath, original);
}

bool DigestionReverseEngineeringSystem::rollbackAll(const // DateTime &timestamp) {
    const int64_t threshold = timestamp.isValid() ? timestamp.toMSecsSinceEpoch() : 0;
    const std::string effectiveDir = m_backupDir.empty() ? DigestionConfig().backupDir : m_backupDir;
    
    std::map<std::string, std::string> registrySnapshot;
    std::map<std::string, int64_t> timeSnapshot;
    {
        std::mutexLocker lock(&m_backupMutex);
        registrySnapshot = m_backupRegistry;
        timeSnapshot = m_backupTimes;
    }
    
    bool success = true;
    for (auto it = registrySnapshot.begin(); it != registrySnapshot.end(); ++it) {
        const std::string backupId = it.key();
        const int64_t backupTime = timeSnapshot.value(backupId, backupId.toLongLong());
        if (timestamp.isValid() && backupTime < threshold) continue;
        
        const std::string original = it.value();
        const std::string backupPath = // (effectiveDir)
            .filePath(std::string("%1_%2.bak").fileName(), backupId));
        if (!std::filesystem::exists(backupPath)) {
            success = false;
            continue;
        }
        std::filesystem::remove(original);
        if (!std::filesystem::copy(backupPath, original)) success = false;
    }
    
    return success;
}

void DigestionReverseEngineeringSystem::clearCache() {
    std::mutexLocker lock(&m_mutex);
    m_hashCache.clear();
    m_profileCache.clear();
}

std::vector<uint8_t> DigestionReverseEngineeringSystem::fastHash(const std::vector<uint8_t> &data) {
#if defined(_WIN32) && defined(_M_X64)
    if (!data.empty()) {
        std::vector<uint8_t> outHash(32, Uninitialized);
        DigestionHashChunk(data.constData(), static_cast<size_t>(data.size()), outHash.data());
        return outHash;
    }
#endif
    QCryptographicHash hash(QCryptographicHash::Blake2s_256);
    hash.addData(data);
    return hash.result();
}

bool DigestionReverseEngineeringSystem::asmOptimizedScan(const std::vector<uint8_t> &data, const char *pattern) {
    if (!pattern || pattern[0] == '\0' || data.empty()) return false;
#if defined(_WIN32) && defined(_M_X64)
    const size_t patLen = std::strlen(pattern);
    return DigestionFastScan(data.constData(), static_cast<size_t>(data.size()), pattern, patLen) != 0;
#endif
    return data.contains(pattern);
}

