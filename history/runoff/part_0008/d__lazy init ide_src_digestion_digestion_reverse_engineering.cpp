#include "digestion_reverse_engineering.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>
#include <QDateTime>
#include <QtConcurrent>
        {"C", {"c"}, {
            QRegularExpression(R"(TODO\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(FIXME\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(STUB\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(NOT_IMPLEMENTED)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"C++", {"cpp", "hpp", "h", "cc", "cxx", "c++", "inl"}, {
            QRegularExpression(R"(TODO\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(FIXME\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(STUB\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(NOT_IMPLEMENTED)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(throw\s+std::(?:runtime_error|exception|logic_error)\s*\(\s*[\"']Not implemented)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(Q_UNIMPLEMENTED\(\))"),
            QRegularExpression(R"(\{\s*//\s*TODO\s*\n\s*\})"),
            QRegularExpression(R"(assert\(false\s*&&\s*[\"']Not implemented[\"']\))")
        }, "//", "/*", "*/", true},

#ifdef Q_OS_WIN
#include <windows.h>
#include <intrin.h>
#include <memoryapi.h>
#endif

#if defined(Q_OS_WIN) && defined(_M_X64)
extern "C" {
    int DigestionFastScan(const char* data, size_t len, const char* pattern, size_t patLen);
    void DigestionHashChunk(const void* data, size_t len, void* outHash);
}
#endif

namespace {

const QSet<QString> kDigestionExtensions = {
    QStringLiteral("c"), QStringLiteral("cpp"), QStringLiteral("cxx"), QStringLiteral("cc"), QStringLiteral("c++"),
    QStringLiteral("h"), QStringLiteral("hpp"), QStringLiteral("hh"), QStringLiteral("hxx"),
    QStringLiteral("rs"), QStringLiteral("go"),
    QStringLiteral("py"), QStringLiteral("pyw"), QStringLiteral("pyi"),
    QStringLiteral("js"), QStringLiteral("ts"), QStringLiteral("jsx"), QStringLiteral("tsx"), QStringLiteral("mjs"),
    QStringLiteral("java"), QStringLiteral("kt"),
    QStringLiteral("asm"), QStringLiteral("inc"), QStringLiteral("s"), QStringLiteral("masm"),
    QStringLiteral("cs"), QStringLiteral("swift"), QStringLiteral("zig"),
    QStringLiteral("cmake"), QStringLiteral("txt")
};

const QHash<QString, QString> kExtensionToLanguage = {
    {QStringLiteral("c"), QStringLiteral("C")},
    {QStringLiteral("cpp"), QStringLiteral("C++")},
    {QStringLiteral("cxx"), QStringLiteral("C++")},
    {QStringLiteral("cc"), QStringLiteral("C++")},
    {QStringLiteral("c++"), QStringLiteral("C++")},
    {QStringLiteral("hpp"), QStringLiteral("C++")},
    {QStringLiteral("hh"), QStringLiteral("C++")},
    {QStringLiteral("hxx"), QStringLiteral("C++")},
    {QStringLiteral("rs"), QStringLiteral("Rust")},
    {QStringLiteral("go"), QStringLiteral("Go")},
    {QStringLiteral("py"), QStringLiteral("Python")},
    {QStringLiteral("pyw"), QStringLiteral("Python")},
    {QStringLiteral("pyi"), QStringLiteral("Python")},
    {QStringLiteral("js"), QStringLiteral("JavaScript/TypeScript")},
    {QStringLiteral("mjs"), QStringLiteral("JavaScript/TypeScript")},
    {QStringLiteral("ts"), QStringLiteral("JavaScript/TypeScript")},
    {QStringLiteral("tsx"), QStringLiteral("JavaScript/TypeScript")},
    {QStringLiteral("jsx"), QStringLiteral("JavaScript/TypeScript")},
    {QStringLiteral("java"), QStringLiteral("Java")},
    {QStringLiteral("kt"), QStringLiteral("Kotlin")},
    {QStringLiteral("cs"), QStringLiteral("C#")},
    {QStringLiteral("swift"), QStringLiteral("Swift")},
    {QStringLiteral("zig"), QStringLiteral("Zig")},
    {QStringLiteral("asm"), QStringLiteral("MASM")},
    {QStringLiteral("inc"), QStringLiteral("MASM")},
    {QStringLiteral("s"), QStringLiteral("MASM")},
    {QStringLiteral("masm"), QStringLiteral("MASM")},
    {QStringLiteral("cmake"), QStringLiteral("CMake")},
    {QStringLiteral("txt"), QStringLiteral("CMake")}
};

QString detectHeaderLanguage(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("C");
    }

    QByteArray sample = file.read(1024).toLower();
    if (sample.contains("class ") || sample.contains("template") || sample.contains("namespace") || sample.contains("std::")) {
        return QStringLiteral("C++");
    }
    return QStringLiteral("C");
}

QString languageFromExtension(const QString& ext, const QString& path) {
    if (ext == QLatin1String("h")) {
        return detectHeaderLanguage(path);
    }
    auto it = kExtensionToLanguage.constFind(ext);
    if (it != kExtensionToLanguage.constEnd()) {
        return *it;
    }
    return QString();
}

#ifdef Q_OS_WIN
struct ScopedFileMap {
    HANDLE fileHandle{INVALID_HANDLE_VALUE};
    HANDLE mapHandle{nullptr};
    const char* view{nullptr};
    qint64 fileSize{0};

    bool open(const QString& path) {
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

    QByteArray asByteArray() const {
        return view && fileSize > 0 ? QByteArray::fromRawData(view, static_cast<int>(fileSize)) : QByteArray();
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

DigestionReverseEngineeringSystem::DigestionReverseEngineeringSystem(QObject *parent)
    : QObject(parent), m_profileCache(1000) {
    initializeLanguageProfiles();
    m_threadPool = std::make_unique<QThreadPool>();
    m_backupDir = DigestionConfig().backupDir;
}

DigestionReverseEngineeringSystem::~DigestionReverseEngineeringSystem() {
    stop();
    m_threadPool->waitForDone(5000);
}

void DigestionReverseEngineeringSystem::initializeLanguageProfiles() {
    m_profiles = {
        {"C++", {"cpp", "hpp", "h", "cc", "cxx", "c++", "inl"}, {
            QRegularExpression(R"(TODO\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(FIXME\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(STUB\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(NOT_IMPLEMENTED)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(throw\s+std::(?:runtime_error|exception|logic_error)\s*\(\s*[\"']Not implemented)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(Q_UNIMPLEMENTED\(\))"),
            QRegularExpression(R"(\{\s*//\s*TODO\s*\n\s*\})"),
            QRegularExpression(R"(assert\(false\s*&&\s*[\"']Not implemented[\"']\))")
        }, "//", "/*", "*/", true},
        
        {"MASM", {"asm", "inc", "masm"}, {
            QRegularExpression(R"(;\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(;\s*STUB)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(;\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(invoke\s+ExitProcess.*;\s*stub)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(ret\s*;\s*unimplemented)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(db\s+['\"]NOT_IMPLEMENTED['\"])", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(;\s*PLACEHOLDER)"),
            QRegularExpression(R"(xor\s+(?:eax|rax),\s+(?:eax|rax)\s*;\s*stub)")
        }, ";", "/*", "*/", true},
        
        {"Python", {"py", "pyw", "pyi"}, {
            QRegularExpression(R"(#\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(#\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(pass\s*#\s*stub)"),
            QRegularExpression(R"(raise\s+NotImplementedError)"),
            QRegularExpression(R"(return\s+None\s*#\s*stub)"),
            QRegularExpression(R"(\.\.\.)")
        }, "#", "\"\"\"", "\"\"\"", false},
        
        {"JavaScript/TypeScript", {"js", "jsx", "ts", "tsx", "mjs"}, {
            QRegularExpression(R"(//\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(//\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(throw\s+new\s+Error\s*\(\s*['\"]Not implemented)"),
            QRegularExpression(R"(return\s+(?:null|undefined)\s*;\s*//\s*stub)"),
            QRegularExpression(R"(/\*\s*STUB\s*\*/)"),
            QRegularExpression(R"(TODO\([^)]+\):\s*)")
        }, "//", "/*", "*/", false},
        
        {"CMake", {"cmake", "txt"}, {
            QRegularExpression(R"(#\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(#\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(message\s*\(\s*FATAL_ERROR\s+[\"']Not implemented)"),
            QRegularExpression(R"(#\s*STUB)")
        }, "#", "#[[", "]]", false},
        
        {"Rust", {"rs"}, {
                {"C", {"c"}, {
                    QRegularExpression(R"(TODO\s*:)", QRegularExpression::CaseInsensitiveOption),
                    QRegularExpression(R"(FIXME\s*:)", QRegularExpression::CaseInsensitiveOption),
                    QRegularExpression(R"(STUB\s*:)", QRegularExpression::CaseInsensitiveOption),
                    QRegularExpression(R"(NOT_IMPLEMENTED)", QRegularExpression::CaseInsensitiveOption),
                    QRegularExpression(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption)
                }, "//", "/*", "*/", false},

            QRegularExpression(R"(//\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(//\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(todo!\(\))"),
            QRegularExpression(R"(unimplemented!\(\))"),
            QRegularExpression(R"(panic!\(\s*[\"']Not implemented)"),
            QRegularExpression(R"(return\s+;\s*//\s*stub)")
        }, "//", "/*", "*/", true},
        
        {"Go", {"go"}, {
            QRegularExpression(R"(//\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(//\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+(?:nil|false|0)\s*,?\s*(?:nil|err)?\s*//\s*stub)")
        }, "//", "/*", "*/", false},

        {"Java", {"java"}, {
            QRegularExpression(R"(TODO\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(FIXME\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(throw\s+new\s+UnsupportedOperationException)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+null\s*;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"Kotlin", {"kt"}, {
            QRegularExpression(R"(TODO\s*\(")", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(NotImplementedError)", QRegularExpression::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"C#", {"cs"}, {
            QRegularExpression(R"(TODO\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(FIXME\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(throw\s+new\s+NotImplementedException)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+default\(\)\s*;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"Swift", {"swift"}, {
            QRegularExpression(R"(TODO\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(fatalError\(\s*\"Not implemented\")", QRegularExpression::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"Zig", {"zig"}, {
            QRegularExpression(R"(TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(@panic\(\s*\"TODO\")", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(unreachable;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption)
        }, "//", "/*", "*/", false}
    };
}

void DigestionReverseEngineeringSystem::runFullDigestionPipeline(const QString &rootDir, const DigestionConfig &config) {
    if (m_running.loadAcquire()) return;
    m_running.storeRelease(1);
    m_stopRequested.storeRelease(0);

                {"Java", {"java"}, {
                    QRegularExpression(R"(TODO\s*:)", QRegularExpression::CaseInsensitiveOption),
                    QRegularExpression(R"(FIXME\s*:)", QRegularExpression::CaseInsensitiveOption),
                    QRegularExpression(R"(throw\s+new\s+UnsupportedOperationException)", QRegularExpression::CaseInsensitiveOption),
                    QRegularExpression(R"(return\s+null\s*;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption)
                }, "//", "/*", "*/", false},

                {"Kotlin", {"kt"}, {
                    QRegularExpression(R"(TODO\s*\(")", QRegularExpression::CaseInsensitiveOption),
                    QRegularExpression(R"(NotImplementedError)", QRegularExpression::CaseInsensitiveOption)
                }, "//", "/*", "*/", false},
    
    m_rootDir = rootDir;
    m_profiles = {
        {"C", {"c"}, {
            QRegularExpression(R"(TODO\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(FIXME\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(STUB\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(NOT_IMPLEMENTED)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption)
        }, "//", "/*", "*/", false},

        {"C++", {"cpp", "hpp", "h", "cc", "cxx", "c++", "inl"}, {
            QRegularExpression(R"(TODO\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(FIXME\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(STUB\s*:)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(NOT_IMPLEMENTED)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(throw\s+std::(?:runtime_error|exception|logic_error)\s*\(\s*[\"']Not implemented)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(Q_UNIMPLEMENTED\(\))"),
            QRegularExpression(R"(\{\s*//\s*TODO\s*\n\s*\})"),
            QRegularExpression(R"(assert\(false\s*&&\s*[\"']Not implemented[\"']\))")
        }, "//", "/*", "*/", true},

        {"MASM", {"asm", "inc", "masm"}, {
            QRegularExpression(R"(;\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(;\s*STUB)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(;\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(invoke\s+ExitProcess.*;\s*stub)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(ret\s*;\s*unimplemented)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(db\s+['\"]NOT_IMPLEMENTED['\"])", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(;\s*PLACEHOLDER)"),
            QRegularExpression(R"(xor\s+(?:eax|rax),\s+(?:eax|rax)\s*;\s*stub)")
        }, ";", "/*", "*/", true},

        {"Python", {"py", "pyw", "pyi"}, {
            QRegularExpression(R"(#\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(#\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(pass\s*#\s*stub)"),
            QRegularExpression(R"(raise\s+NotImplementedError)"),
            QRegularExpression(R"(return\s+None\s*#\s*stub)"),
            QRegularExpression(R"(\.\.\.)")
        }, "#", "\"\"\"", "\"\"\"", false},

        {"JavaScript/TypeScript", {"js", "jsx", "ts", "tsx", "mjs"}, {
            QRegularExpression(R"(//\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(//\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(throw\s+new\s+Error\s*\(\s*['\"]Not implemented)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+(?:null|undefined)\s*;\s*//\s*stub)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(/\*\s*STUB\s*\*/)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(TODO\([^)]+\):\s*)")
        }, "//", "/*", "*/", false},

        {"CMake", {"cmake", "txt"}, {
            QRegularExpression(R"(#\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(#\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(message\s*\(\s*FATAL_ERROR\s+[\"']Not implemented)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(#\s*STUB)")
        }, "#", "#[[", "]]", false},

        {"Rust", {"rs"}, {
            QRegularExpression(R"(//\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(//\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(todo!\(\))"),
            QRegularExpression(R"(unimplemented!\(\))"),
            QRegularExpression(R"(panic!\(\s*[\"']Not implemented)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+;\s*//\s*stub)")
        }, "//", "/*", "*/", true},

        {"Go", {"go"}, {
            QRegularExpression(R"(//\s*TODO)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(//\s*FIXME)", QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(R"(return\s+(?:nil|false|0)\s*,?\s*(?:nil|err)?\s*//\s*stub)")
        }, "//", "/*", "*/", false}
    };
        digest.lastModified = info.lastModified().toMSecsSinceEpoch();
        digest.lineCount = 0; // Will fill during scan
        
        if (config.incremental) {
            QByteArray currentHash = computeFileHash(path);
            digest.hash = currentHash;
            
            QMutexLocker lock(&m_mutex);
            if (m_hashCache.contains(path) && m_hashCache[path] == currentHash) {
                m_stats.cacheHits.ref();
                continue; // Skip unchanged
            }
        }
        
        filesToProcess.append(digest);
        if (config.maxFiles > 0 && filesToProcess.size() >= config.maxFiles) break;
    }
    
    m_stats.totalFiles.store(filesToProcess.size());
    emit pipelineStarted(rootDir, filesToProcess.size());
    
    // Process in chunks
    int chunkCount = (filesToProcess.size() + config.chunkSize - 1) / config.chunkSize;
    QFutureSynchronizer<void> synchronizer;
    
    for (int i = 0; i < filesToProcess.size() && !m_stopRequested.loadAcquire(); i += config.chunkSize) {
        int end = qMin(i + config.chunkSize, filesToProcess.size());
        QVector<FileDigest> chunk = filesToProcess.mid(i, end - i);
        int chunkId = i / config.chunkSize;
        
        QFuture<void> future = QtConcurrent::run(m_threadPool.get(), [this, chunk, chunkId, config]() {
            processChunk(chunk, chunkId, config);
        });
        synchronizer.addFuture(future);
    }
    
    synchronizer.waitForFinished();
    m_running.storeRelease(0);
    
    // Update hash cache
    if (config.incremental) {
        QMutexLocker lock(&m_mutex);
        for (const auto &file : filesToProcess) {
            if (!file.hash.isEmpty()) m_hashCache[file.path] = file.hash;
        }
    }
    
    generateFinalReport();
}

void DigestionReverseEngineeringSystem::scanDirectory(const QString &rootDir) {
    runFullDigestionPipeline(rootDir, DigestionConfig());
}

void DigestionReverseEngineeringSystem::processChunk(const QVector<FileDigest> &files, int chunkId, const DigestionConfig &config) {
    for (const auto &file : files) {
        if (m_stopRequested.loadAcquire()) return;
        scanSingleFile(file, config);
    }
    emit chunkCompleted(chunkId + 1, (files.size() + config.chunkSize - 1) / config.chunkSize);
}

void DigestionReverseEngineeringSystem::scanSingleFile(const FileDigest &fileDigest, const DigestionConfig &config) {
    QByteArray rawData;
#ifdef Q_OS_WIN
    ScopedFileMap mapped;
    if (mapped.open(fileDigest.path)) {
        rawData = mapped.asByteArray();
        m_stats.bytesProcessed += mapped.fileSize;
    }
#endif

    QFile file(fileDigest.path);
    if (rawData.isEmpty()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            emit errorOccurred(fileDigest.path, "Cannot open file: " + file.errorString());
            m_stats.errors.ref();
            return;
        }
        rawData = file.readAll();
        file.close();
        m_stats.bytesProcessed += rawData.size();
    }
    
    QString content = QString::fromUtf8(rawData);
    QString lang = detectLanguage(fileDigest.path);
    if (lang.isEmpty()) {
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
    
    emit fileScanned(fileDigest.path, lang, tasks.size());
    
    // Apply fixes if requested
    if (config.applyExtensions && !tasks.isEmpty()) {
        QString modifiedContent = content;
        bool modified = false;
        
        // Sort by line descending to avoid offset issues
        QList<AgenticTask> sortedTasks = tasks;
        std::sort(sortedTasks.begin(), sortedTasks.end(), 
                  [](const AgenticTask &a, const AgenticTask &b) { return a.lineNumber > b.lineNumber; });
        
        for (auto &task : sortedTasks) {
            if (applyAgenticFix(fileDigest.path, task, config)) {
                m_stats.extensionsApplied.ref();
                emit extensionApplied(fileDigest.path, task.lineNumber, task.stubType);
                modified = true;
            } else {
                emit extensionFailed(fileDigest.path, task.lineNumber, "Apply failed");
            }
        }
    }
    
    // Record results
    QJsonObject fileResult;
    fileResult["file"] = fileDigest.path;
    fileResult["language"] = lang;
    fileResult["size_bytes"] = rawData.size();
    fileResult["stubs_found"] = tasks.size();
    fileResult["hash"] = QString(fileDigest.hash.toHex());
    
    QJsonArray taskArray;
    for (const auto &task : tasks) {
        QJsonObject t;
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
    
    QMutexLocker lock(&m_mutex);
    m_results.append(fileResult);
    lock.unlock();
    
    updateProgress();
}

QList<AgenticTask> DigestionReverseEngineeringSystem::findStubs(
    const QString &content, const LanguageProfile &lang, const FileDigest &file, int maxTasks) {
    
    QList<AgenticTask> tasks;
    QStringList lines = content.split('\n');
    
    for (int i = 0; i < lines.size() && (maxTasks == 0 || tasks.size() < maxTasks); ++i) {
        const QString &line = lines[i];
        
        for (const auto &pattern : lang.stubPatterns) {
            if (pattern.match(line).hasMatch()) {
                AgenticTask task;
                task.filePath = file.path;
                task.lineNumber = i + 1;
                task.stubType = pattern.pattern();
                task.timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
                task.backupId = QString("%1_%2").arg(task.timestamp).arg(task.lineNumber);
                
                // Extract context (5 lines before/after for better AI context)
                int start = qMax(0, i - 5);
                int end = qMin(lines.size() - 1, i + 5);
                task.contextBefore = lines.mid(start, i - start).join('\n');
                task.contextAfter = lines.mid(i + 1, end - i).join('\n');
                task.fullContext = lines.mid(start, end - start + 1).join('\n');
                
                // Determine confidence based on context
                if (line.contains("TODO:", Qt::CaseInsensitive) && line.length() < 50) {
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

QString DigestionReverseEngineeringSystem::generateIntelligentFix(const AgenticTask &task, const LanguageProfile &lang) {
    // Context-aware fix generation based on surrounding code patterns
    if (lang.name == "MASM") {
        if (task.stubType.contains("ExitProcess", Qt::CaseInsensitive)) {
            if (task.contextBefore.contains("proc", Qt::CaseInsensitive)) {
                return "; [AGENTIC-AUTO] Proper function epilogue\n    mov rsp, rbp\n    pop rbp\n    ret";
            }
            return "; [AGENTIC-AUTO] Safe exit with cleanup\n    xor ecx, ecx\n    call ExitProcess";
        }
        if (task.contextBefore.contains("proc", Qt::CaseInsensitive) && task.contextAfter.contains("endp", Qt::CaseInsensitive)) {
            return "    ; [AGENTIC-AUTO] Function implementation\n    xor eax, eax\n    ret";
        }
        if (task.fullContext.contains("memcpy", Qt::CaseInsensitive) || task.fullContext.contains("movs")) {
            return "    ; [AGENTIC-AUTO] Optimized memory copy\n    cld\n    rep movsb";
        }
    } else if (lang.name == "C++") {
        // Check return type from context
        if (task.contextBefore.contains("bool ", Qt::CaseInsensitive) || 
            task.contextBefore.contains("-> bool")) {
            return "    // [AGENTIC-AUTO] Boolean implementation\n    return true;";
        }
        if (task.contextBefore.contains("int ", Qt::CaseInsensitive) || 
            task.contextBefore.contains("size_t") ||
            task.contextBefore.contains("-> int")) {
            return "    // [AGENTIC-AUTO] Integer implementation\n    return 0;";
        }
        if (task.contextBefore.contains("QString") || task.contextBefore.contains("std::string")) {
            return "    // [AGENTIC-AUTO] String implementation\n    return QString();";
        }
        if (task.contextBefore.contains("void ", Qt::CaseInsensitive) && 
            !task.contextBefore.contains("void *")) {
            return "    // [AGENTIC-AUTO] Void implementation\n    // TODO: Add logic";
        }
        if (task.fullContext.contains("class") && task.fullContext.contains("virtual")) {
            return "    // [AGENTIC-AUTO] Override implementation\n    // Call base or implement specific logic";
        }
    } else if (lang.name == "Python") {
        if (task.contextBefore.contains("def ", Qt::CaseInsensitive)) {
            if (task.contextBefore.contains("-> bool")) return "    return True";
            if (task.contextBefore.contains("-> int")) return "    return 0";
            if (task.contextBefore.contains("-> str")) return "    return \"\"";
            return "    pass";
        }
    }
    
    return QString(); // Empty = use default
}

bool DigestionReverseEngineeringSystem::applyAgenticFix(const QString &filePath, const AgenticTask &task, const DigestionConfig &config) {
    if (config.createBackups) {
        const QString backupId = task.backupId.isEmpty()
            ? QString::number(QDateTime::currentMSecsSinceEpoch())
            : task.backupId;
        createBackup(filePath, backupId);
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    QStringList lines = content.split('\n');
    if (task.lineNumber < 1 || task.lineNumber > lines.size()) return false;
    
    int idx = task.lineNumber - 1;
    QString replacement = task.suggestedFix;
    
    if (replacement.isEmpty()) {
        // Generic replacement
        replacement = lines[idx] + "\n    // [AGENTIC] Implementation required";
    }
    
    lines[idx] = replacement;
    
    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly)) return false;
    out.write(lines.join('\n').toUtf8());
    return out.commit();
}

void DigestionReverseEngineeringSystem::createBackup(const QString &filePath, const QString &backupId) {
    const QString resolvedBackupId = backupId.isEmpty()
        ? QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch())
        : backupId;
    const QString effectiveDir = m_backupDir.isEmpty() ? DigestionConfig().backupDir : m_backupDir;
    QDir backupDir(effectiveDir);
    if (!backupDir.exists()) backupDir.mkpath(".");
    QString backupPath = backupDir.filePath(
        QString("%1_%2.bak").arg(QFileInfo(filePath).fileName(), resolvedBackupId)
    );
    
    if (QFile::copy(filePath, backupPath)) {
        QMutexLocker lock(&m_backupMutex);
        m_backupRegistry[resolvedBackupId] = filePath;
        m_backupTimes[resolvedBackupId] = QDateTime::currentMSecsSinceEpoch();
        emit backupCreated(filePath, backupPath);
        emit rollbackAvailable(resolvedBackupId);
    }
}

QByteArray DigestionReverseEngineeringSystem::computeFileHash(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return QByteArray();
    
    QCryptographicHash hash(QCryptographicHash::Blake2s_256);
    hash.addData(&file);
    return hash.result();
}

bool DigestionReverseEngineeringSystem::shouldProcessFile(const QString &filePath, const DigestionConfig &config) {
    // Check exclude patterns
    for (const QString &pattern : config.excludePatterns) {
        QRegularExpression re(pattern);
        if (re.match(filePath).hasMatch()) return false;
    }
    
    // Check gitignore if in git mode
    if (config.useGitMode) {
        // Simple check - could be enhanced with actual git check-ignore
        if (filePath.contains("/.git/") || filePath.contains("/build/") || filePath.contains("/.vs/"))
            return false;
    }
    
    // Check extension
    QString ext = QFileInfo(filePath).suffix().toLower();
    return kDigestionExtensions.contains(ext);
}

QString DigestionReverseEngineeringSystem::detectLanguage(const QString &filePath) {
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    
    // Check cache first
    {
        QMutexLocker lock(&m_mutex);
        if (m_profileCache.contains(filePath)) return m_profileCache.object(filePath)->name;
    }
    
    QString mapped = languageFromExtension(ext, filePath);
    if (mapped.isEmpty()) return QString();

    auto profileIt = std::find_if(m_profiles.begin(), m_profiles.end(), [&mapped](const LanguageProfile &p) {
        return p.name == mapped;
    });
    if (profileIt == m_profiles.end()) return QString();

    {
        QMutexLocker lock(&m_mutex);
        m_profileCache.insert(filePath, new LanguageProfile(*profileIt));
    }
    return mapped;
}

QStringList DigestionReverseEngineeringSystem::getGitModifiedFiles(const QString &rootDir) {
    QProcess git;
    git.setWorkingDirectory(rootDir);
    git.start("git", QStringList() << "diff" << "--name-only" << "HEAD");
    git.waitForFinished();
    
    QString output = QString::fromUtf8(git.readAllStandardOutput());
    QStringList files = output.split('\n', Qt::SkipEmptyParts);
    
    // Convert to absolute paths
    for (QString &file : files) {
        file = QDir(rootDir).absoluteFilePath(file);
    }
    return files;
}

void DigestionReverseEngineeringSystem::updateProgress() {
    int total = m_stats.totalFiles.loadAcquire();
    int scanned = m_stats.scannedFiles.loadAcquire();
    int stubs = m_stats.stubsFound.loadAcquire();
    int percent = total > 0 ? (scanned * 100 / total) : 0;
    
    emit progressUpdate(scanned, total, stubs, percent);
}

void DigestionReverseEngineeringSystem::generateFinalReport() {
    QJsonObject report;
    report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    report["root_directory"] = m_rootDir;
    report["elapsed_ms"] = m_timer.elapsed();
    report["statistics"] = QJsonObject{
        {"total_files", m_stats.totalFiles.loadAcquire()},
        {"scanned_files", m_stats.scannedFiles.loadAcquire()},
        {"stubs_found", m_stats.stubsFound.loadAcquire()},
        {"extensions_applied", m_stats.extensionsApplied.loadAcquire()},
        {"errors", m_stats.errors.loadAcquire()},
        {"skipped_large_files", m_stats.skippedLargeFiles.loadAcquire()},
        {"cache_hits", m_stats.cacheHits.loadAcquire()},
        {"bytes_processed", (qint64)m_stats.bytesProcessed}
    };
    report["files"] = m_results;
    
    m_lastReport = report;
    
    QFile out("digestion_report.json");
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    }
    
    emit pipelineFinished(report, m_timer.elapsed());
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

QJsonObject DigestionReverseEngineeringSystem::lastReport() const {
    QMutexLocker lock(&m_mutex);
    return m_lastReport;
}

QJsonObject DigestionReverseEngineeringSystem::generateIncrementalReport(const QStringList &changedFiles) {
    QJsonObject report;
    QJsonArray fileResults;
    int totalFiles = 0;
    int stubsFound = 0;
    qint64 bytesProcessed = 0;
    
    DigestionConfig config;
    for (const QString &filePath : changedFiles) {
        if (filePath.isEmpty() || !QFileInfo::exists(filePath)) continue;
        if (!shouldProcessFile(filePath, config)) continue;
        
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QByteArray rawData = file.readAll();
        file.close();
        
        bytesProcessed += rawData.size();
        QString content = QString::fromUtf8(rawData);
        QString lang = detectLanguage(filePath);
        if (lang.isEmpty()) continue;
        
        auto profileIt = std::find_if(m_profiles.begin(), m_profiles.end(),
            [&lang](const LanguageProfile &p) { return p.name == lang; });
        if (profileIt == m_profiles.end()) continue;
        
        FileDigest digest;
        digest.path = filePath;
        digest.hash = computeFileHash(filePath);
        
        auto tasks = findStubs(content, *profileIt, digest, 0);
        stubsFound += tasks.size();
        totalFiles++;
        
        QJsonObject fileResult;
        fileResult["file"] = filePath;
        fileResult["language"] = lang;
        fileResult["size_bytes"] = rawData.size();
        fileResult["stubs_found"] = tasks.size();
        fileResult["hash"] = QString(digest.hash.toHex());
        
        QJsonArray taskArray;
        for (const auto &task : tasks) {
            QJsonObject t;
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
    
    report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    report["files"] = fileResults;
    report["statistics"] = QJsonObject{
        {"total_files", totalFiles},
        {"stubs_found", stubsFound},
        {"bytes_processed", bytesProcessed}
    };
    
    return report;
}

void DigestionReverseEngineeringSystem::loadHashCache(const QString &cacheFile) {
    QFile file(cacheFile);
    if (!file.open(QIODevice::ReadOnly)) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject obj = doc.object();
    
    QMutexLocker lock(&m_mutex);
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        m_hashCache[it.key()] = QByteArray::fromHex(it.value().toString().toUtf8());
    }
}

void DigestionReverseEngineeringSystem::saveHashCache(const QString &cacheFile) {
    QMutexLocker lock(&m_mutex);
    QJsonObject obj;
    for (auto it = m_hashCache.begin(); it != m_hashCache.end(); ++it) {
        obj[it.key()] = QString(it.value().toHex());
    }
    lock.unlock();
    
    QFile file(cacheFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson());
    }
}

bool DigestionReverseEngineeringSystem::rollbackFile(const QString &backupId) {
    QMutexLocker lock(&m_backupMutex);
    if (!m_backupRegistry.contains(backupId)) return false;
    
    QString original = m_backupRegistry[backupId];
    QString backupPath = QDir(m_backupDir.isEmpty() ? DigestionConfig().backupDir : m_backupDir)
        .filePath(QString("%1_%2.bak").arg(QFileInfo(original).fileName(), backupId));
    lock.unlock();
    
    return QFile::copy(backupPath, original);
}

bool DigestionReverseEngineeringSystem::rollbackAll(const QDateTime &timestamp) {
    const qint64 threshold = timestamp.isValid() ? timestamp.toMSecsSinceEpoch() : 0;
    const QString effectiveDir = m_backupDir.isEmpty() ? DigestionConfig().backupDir : m_backupDir;
    
    QHash<QString, QString> registrySnapshot;
    QHash<QString, qint64> timeSnapshot;
    {
        QMutexLocker lock(&m_backupMutex);
        registrySnapshot = m_backupRegistry;
        timeSnapshot = m_backupTimes;
    }
    
    bool success = true;
    for (auto it = registrySnapshot.begin(); it != registrySnapshot.end(); ++it) {
        const QString backupId = it.key();
        const qint64 backupTime = timeSnapshot.value(backupId, backupId.toLongLong());
        if (timestamp.isValid() && backupTime < threshold) continue;
        
        const QString original = it.value();
        const QString backupPath = QDir(effectiveDir)
            .filePath(QString("%1_%2.bak").arg(QFileInfo(original).fileName(), backupId));
        if (!QFile::exists(backupPath)) {
            success = false;
            continue;
        }
        QFile::remove(original);
        if (!QFile::copy(backupPath, original)) success = false;
    }
    
    return success;
}

void DigestionReverseEngineeringSystem::clearCache() {
    QMutexLocker lock(&m_mutex);
    m_hashCache.clear();
    m_profileCache.clear();
}

QByteArray DigestionReverseEngineeringSystem::fastHash(const QByteArray &data) {
#if defined(Q_OS_WIN) && defined(_M_X64)
    if (!data.isEmpty()) {
        QByteArray outHash(32, Qt::Uninitialized);
        DigestionHashChunk(data.constData(), static_cast<size_t>(data.size()), outHash.data());
        return outHash;
    }
#endif
    QCryptographicHash hash(QCryptographicHash::Blake2s_256);
    hash.addData(data);
    return hash.result();
}

bool DigestionReverseEngineeringSystem::asmOptimizedScan(const QByteArray &data, const char *pattern) {
    if (!pattern || pattern[0] == '\0' || data.isEmpty()) return false;
#if defined(Q_OS_WIN) && defined(_M_X64)
    const size_t patLen = std::strlen(pattern);
    return DigestionFastScan(data.constData(), static_cast<size_t>(data.size()), pattern, patLen) != 0;
#endif
    return data.contains(pattern);
}
