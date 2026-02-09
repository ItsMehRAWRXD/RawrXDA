#include "enhanced_model_loader.h"
#include "../../include/inference_engine_stub.hpp"
#include "qtapp/gguf_server.hpp"
#include "streaming_gguf_loader.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <QUrl>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>
#include "codec/compression.h"

#ifdef _WIN32
#include <Windows.h>
#endif

EnhancedModelLoader::EnhancedModelLoader(QObject* parent)
    : QObject(parent)
    , m_engine(nullptr)
    , m_server(nullptr)
    , m_formatRouter(std::make_unique<FormatRouter>())
    , m_hfDownloader(std::make_unique<HFDownloader>())
    , m_ollamaProxy(std::make_unique<OllamaProxy>()) {
    
    setupTempDirectory();
}

EnhancedModelLoader::~EnhancedModelLoader() {
    if (m_server) {
        m_server->stop();
    }
    cleanupTempFiles();
}

bool EnhancedModelLoader::loadModel(const QString& modelInput) {
    if (modelInput.isEmpty()) {
        m_lastError = "Model input is empty";
        emit error(m_lastError);
        return false;
    }

    const auto start_time = std::chrono::steady_clock::now();
    const std::string input = modelInput.toStdString();

    try {
        // Route the input
        auto modelSource = m_formatRouter->route(input);
        if (!modelSource) {
            m_lastError = "Failed to determine model format";
            logLoadError(modelInput, ModelFormat::UNKNOWN, m_lastError);
            emit error(m_lastError);
            return false;
        }

        m_loadedFormat = modelSource->format;
        logLoadStart(modelInput, modelSource->format);
        emit loadingStage(QString::fromStdString(modelSource->display_name));

        bool success = false;

        switch (modelSource->format) {
            case ModelFormat::GGUF_LOCAL:
                emit loadingStage("Loading local GGUF model...");
                success = loadGGUFLocal(modelInput);
                break;

            case ModelFormat::HF_REPO:
                emit loadingStage("Downloading from HuggingFace Hub...");
                success = loadHFModel(modelInput);
                break;

            case ModelFormat::HF_FILE:
                emit loadingStage("Downloading HF file...");
                success = loadHFModel(modelInput);
                break;

            case ModelFormat::OLLAMA_REMOTE:
                emit loadingStage("Connecting to Ollama endpoint...");
                success = loadOllamaModel(modelInput);
                break;

            case ModelFormat::MASM_COMPRESSED:
                emit loadingStage("Decompressing model...");
                success = loadCompressedModel(modelInput);
                break;

            default:
                m_lastError = "Unsupported model format";
                emit error(m_lastError);
                return false;
        }

        const auto end_time = std::chrono::steady_clock::now();
        const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        if (success) {
            m_modelPath = modelInput;
            logLoadSuccess(modelInput, modelSource->format, duration_ms);
            emit modelLoaded(modelInput);
            return true;
        } else {
            m_lastError = "Model loading failed at format stage";
            logLoadError(modelInput, modelSource->format, m_lastError);
            emit error(m_lastError);
            return false;
        }

    } catch (const std::exception& e) {
        m_lastError = QString("Exception: %1").arg(e.what());
        logLoadError(modelInput, ModelFormat::UNKNOWN, m_lastError);
        emit error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::loadGGUFLocal(const QString& modelPath) {
    std::string path = modelPath.toStdString();

    if (!std::filesystem::exists(path)) {
        m_lastError = "GGUF file not found: " + modelPath;
        emit error(m_lastError);
        return false;
    }

    try {
        auto fileSize = std::filesystem::file_size(path);
        qInfo() << "Loading GGUF:" << modelPath << "(" << (fileSize / 1024 / 1024) << "MB)";

        if (!m_engine) {
            m_engine = std::make_unique<InferenceEngine>();
        }

        // Load into inference engine
        if (!m_engine->Initialize(path)) {
            m_lastError = "InferenceEngine failed to initialize";
            emit error(m_lastError);
            return false;
        }

        emit loadingProgress(80);

        // Optionally start GGUF server
        if (!m_server) {
            m_server = std::make_unique<GGUFServer>(m_engine.get(), this);
            connect(m_server.get(), &GGUFServer::serverStarted, this, &EnhancedModelLoader::serverStarted);
            connect(m_server.get(), &GGUFServer::serverStopped, this, &EnhancedModelLoader::serverStopped);
            connect(m_server.get(), QOverload<const QString&>::of(&GGUFServer::error),
                    this, &EnhancedModelLoader::error);
        }

        if (!m_server->start(m_port)) {
            qWarning() << "Failed to start GGUF server on port" << m_port << "- continuing without server";
        }

        emit loadingProgress(100);
        return true;

    } catch (const std::exception& e) {
        m_lastError = QString("GGUF load exception: %1").arg(e.what());
        emit error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::loadHFModel(const QString& repoId) {
    std::string repo = repoId.toStdString();

    // Extract revision if specified (repo:revision format)
    std::string repo_name = repo;
    std::string revision = "main";
    size_t colonPos = repo.find(':');
    if (colonPos != std::string::npos) {
        repo_name = repo.substr(0, colonPos);
        revision = repo.substr(colonPos + 1);
    }

    try {
        qInfo() << "Downloading HF model:" << QString::fromStdString(repo_name) << "revision:" << QString::fromStdString(revision);
        emit loadingProgress(10);

        // Determine cache directory
        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/rawrxd/models/hf";
        QDir().mkpath(cacheDir);
        
        // Sanitize repo name for filesystem
        QString sanitizedRepo = QString::fromStdString(repo_name);
        sanitizedRepo.replace('/', '_');
        QString modelDir = cacheDir + "/" + sanitizedRepo;
        QDir().mkpath(modelDir);
        
        emit loadingProgress(20);
        
        // Step 1: Query HuggingFace API for model files
        // Look for .gguf files in the repo
        QString apiUrl = QString("https://huggingface.co/api/models/%1/tree/%2")
            .arg(QString::fromStdString(repo_name), QString::fromStdString(revision));
        
        QNetworkAccessManager nam;
        QNetworkRequest request(QUrl(apiUrl));
        request.setRawHeader("Accept", "application/json");
        
        // Check for HF token
        QString hfToken = qEnvironmentVariable("HF_TOKEN", qEnvironmentVariable("HUGGING_FACE_HUB_TOKEN"));
        if (!hfToken.isEmpty()) {
            request.setRawHeader("Authorization", ("Bearer " + hfToken).toUtf8());
        }
        
        QEventLoop loop;
        QNetworkReply* reply = nam.get(request);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        
        if (reply->error() != QNetworkReply::NoError) {
            m_lastError = QString("HuggingFace API error: %1").arg(reply->errorString());
            reply->deleteLater();
            emit error(m_lastError);
            return false;
        }
        
        QByteArray responseData = reply->readAll();
        reply->deleteLater();
        
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        if (!doc.isArray()) {
            m_lastError = "Invalid HuggingFace API response (expected array of files)";
            emit error(m_lastError);
            return false;
        }
        
        emit loadingProgress(30);
        
        // Step 2: Find GGUF files to download
        QJsonArray files = doc.array();
        QString ggufFile;
        qint64 ggufSize = 0;
        
        for (const auto& fileVal : files) {
            QJsonObject fileObj = fileVal.toObject();
            QString filename = fileObj["path"].toString();
            if (filename.endsWith(".gguf", Qt::CaseInsensitive)) {
                // Prefer Q4_K_M or Q5_K_M quantization if multiple exist
                if (ggufFile.isEmpty() || 
                    filename.contains("Q4_K_M", Qt::CaseInsensitive) ||
                    filename.contains("Q5_K_M", Qt::CaseInsensitive)) {
                    ggufFile = filename;
                    ggufSize = fileObj["size"].toVariant().toLongLong();
                }
            }
        }
        
        if (ggufFile.isEmpty()) {
            m_lastError = QString("No .gguf files found in HuggingFace repo: %1").arg(QString::fromStdString(repo_name));
            emit error(m_lastError);
            return false;
        }
        
        qInfo() << "Found GGUF file:" << ggufFile << "(" << (ggufSize / (1024*1024)) << "MB)";
        
        // Step 3: Download the GGUF file
        QString localPath = modelDir + "/" + QFileInfo(ggufFile).fileName();
        
        // Check if already cached
        if (QFile::exists(localPath) && QFileInfo(localPath).size() == ggufSize) {
            qInfo() << "Using cached model:" << localPath;
            emit loadingProgress(90);
        } else {
            // Download from HuggingFace
            QString downloadUrl = QString("https://huggingface.co/%1/resolve/%2/%3")
                .arg(QString::fromStdString(repo_name), QString::fromStdString(revision), ggufFile);
            
            QNetworkRequest dlRequest(QUrl(downloadUrl));
            if (!hfToken.isEmpty()) {
                dlRequest.setRawHeader("Authorization", ("Bearer " + hfToken).toUtf8());
            }
            
            QNetworkReply* dlReply = nam.get(dlRequest);
            
            QFile outputFile(localPath);
            if (!outputFile.open(QIODevice::WriteOnly)) {
                m_lastError = QString("Cannot write to: %1").arg(localPath);
                dlReply->deleteLater();
                emit error(m_lastError);
                return false;
            }
            
            qint64 totalBytes = ggufSize;
            qint64 receivedBytes = 0;
            
            // Stream download to disk
            connect(dlReply, &QNetworkReply::readyRead, [&]() {
                QByteArray chunk = dlReply->readAll();
                outputFile.write(chunk);
                receivedBytes += chunk.size();
                
                if (totalBytes > 0) {
                    int progress = 30 + static_cast<int>((receivedBytes * 60) / totalBytes);
                    emit loadingProgress(std::min(progress, 90));
                }
            });
            
            QEventLoop dlLoop;
            connect(dlReply, &QNetworkReply::finished, &dlLoop, &QEventLoop::quit);
            dlLoop.exec();
            
            outputFile.close();
            
            if (dlReply->error() != QNetworkReply::NoError) {
                m_lastError = QString("Download failed: %1").arg(dlReply->errorString());
                QFile::remove(localPath); // Clean up partial download
                dlReply->deleteLater();
                emit error(m_lastError);
                return false;
            }
            
            dlReply->deleteLater();
            qInfo() << "Downloaded" << (receivedBytes / (1024*1024)) << "MB to" << localPath;
        }
        
        emit loadingProgress(90);
        
        // Step 4: Load the downloaded GGUF file using the standard GGUF loader
        return loadGGUFModel(localPath);

    } catch (const std::exception& e) {
        m_lastError = QString("HF download exception: %1").arg(e.what());
        emit error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::loadOllamaModel(const QString& modelName) {
    std::string name = modelName.toStdString();

    try {
        qInfo() << "Connecting to Ollama:" << modelName;
        emit loadingProgress(20);

        // Validate Ollama is available
        if (!m_ollamaProxy->isOllamaAvailable()) {
            m_lastError = "Ollama service not available on localhost:11434";
            emit error(m_lastError);
            return false;
        }

        // Check if model exists
        if (!m_ollamaProxy->isModelAvailable(modelName)) {
            m_lastError = "Model not found in Ollama: " + modelName;
            emit error(m_lastError);
            return false;
        }

        // Set model for Ollama proxy
        m_ollamaProxy->setModel(modelName);
        
        emit loadingProgress(100);
        qInfo() << "Ollama model connected:" << modelName;
        return true;

    } catch (const std::exception& e) {
        m_lastError = QString("Ollama connection exception: %1").arg(e.what());
        emit error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::loadCompressedModel(const QString& compressedPath) {
    std::string path = compressedPath.toStdString();

    if (!std::filesystem::exists(path)) {
        m_lastError = "Compressed file not found: " + compressedPath;
        emit error(m_lastError);
        return false;
    }

    try {
        auto fileSize = std::filesystem::file_size(path);
        qInfo() << "Decompressing:" << compressedPath << "(" << (fileSize / 1024 / 1024) << "MB)";
        emit loadingProgress(30);

        auto source = m_formatRouter->route(path);
        if (!source) {
            m_lastError = "Failed to detect compression type";
            emit error(m_lastError);
            return false;
        }

        return decompressAndLoad(compressedPath, source->compression);

    } catch (const std::exception& e) {
        m_lastError = QString("Compressed load exception: %1").arg(e.what());
        emit error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::decompressAndLoad(const QString& compressedPath, CompressionType compression) {
    std::string compressed = compressedPath.toStdString();
    std::string tempFile = m_tempDirectory + "/decompressed_model_" + std::to_string(std::time(nullptr)) + ".gguf";

    try {
        emit loadingProgress(40);

        // Decompress based on type
        std::ifstream infile(compressed, std::ios::binary);
        if (!infile.is_open()) {
            m_lastError = "Cannot open compressed file";
            return false;
        }

        // Read compressed data
        infile.seekg(0, std::ios::end);
        auto size = infile.tellg();
        infile.seekg(0, std::ios::beg);

        std::vector<uint8_t> compressed_data(size);
        infile.read(reinterpret_cast<char*>(compressed_data.data()), size);
        infile.close();

        std::vector<uint8_t> decompressed_data;

        if (compression == CompressionType::GZIP) {
            // GZIP decompression using Qt's zlib (qUncompress expects qCompress format)
            // For standard gzip files, use the raw inflate path via codec
            QByteArray compressedQBA(reinterpret_cast<const char*>(compressed_data.data()),
                                     static_cast<int>(compressed_data.size()));
            
            // Try qUncompress first (handles Qt-compressed format with 4-byte size header)
            QByteArray decompressedQBA = qUncompress(compressedQBA);
            
            if (decompressedQBA.isEmpty() && !compressed_data.empty()) {
                // Fallback: skip gzip header (10 bytes min) and inflate raw deflate stream
                // GZIP header: 1f 8b 08 ... (RFC 1952)
                if (compressed_data.size() >= 10 && 
                    compressed_data[0] == 0x1f && compressed_data[1] == 0x8b) {
                    // Skip the gzip header, find the start of deflate stream
                    size_t offset = 10;
                    uint8_t flags = compressed_data[3];
                    if (flags & 0x04) { // FEXTRA
                        if (offset + 2 <= compressed_data.size()) {
                            uint16_t xlen = compressed_data[offset] | (compressed_data[offset+1] << 8);
                            offset += 2 + xlen;
                        }
                    }
                    if (flags & 0x08) { // FNAME
                        while (offset < compressed_data.size() && compressed_data[offset]) offset++;
                        offset++;
                    }
                    if (flags & 0x10) { // FCOMMENT
                        while (offset < compressed_data.size() && compressed_data[offset]) offset++;
                        offset++;
                    }
                    if (flags & 0x02) offset += 2; // FHCRC
                    
                    // Use codec::inflate on the raw deflate data (minus 8 byte trailer)
                    if (offset < compressed_data.size() - 8) {
                        std::vector<uint8_t> deflateData(compressed_data.begin() + offset,
                                                          compressed_data.end() - 8);
                        bool ok = false;
                        decompressed_data = codec::inflate(deflateData, &ok);
                        if (!ok || decompressed_data.empty()) {
                            m_lastError = "GZIP decompression failed (inflate error)";
                            return false;
                        }
                    } else {
                        m_lastError = "GZIP file has invalid header structure";
                        return false;
                    }
                } else {
                    m_lastError = "Invalid GZIP magic bytes";
                    return false;
                }
            } else {
                decompressed_data.assign(decompressedQBA.constData(),
                                         decompressedQBA.constData() + decompressedQBA.size());
            }
            qInfo() << "GZIP decompression: " << compressed_data.size() 
                     << " -> " << decompressed_data.size() << " bytes";
                     
        } else if (compression == CompressionType::ZSTD) {
            // ZSTD decompression: ZSTD frame starts with magic 0xFD2FB528
            // Use dynamic loading of zstd library if available
            typedef unsigned long long (*ZSTD_getFrameContentSize_t)(const void*, size_t);
            typedef size_t (*ZSTD_decompress_t)(void*, size_t, const void*, size_t);
            typedef unsigned (*ZSTD_isError_t)(size_t);
            
            HMODULE hZstd = LoadLibraryA("libzstd.dll");
            if (!hZstd) hZstd = LoadLibraryA("zstd.dll");
            
            if (hZstd) {
                auto fnGetSize = (ZSTD_getFrameContentSize_t)GetProcAddress(hZstd, "ZSTD_getFrameContentSize");
                auto fnDecompress = (ZSTD_decompress_t)GetProcAddress(hZstd, "ZSTD_decompress");
                auto fnIsError = (ZSTD_isError_t)GetProcAddress(hZstd, "ZSTD_isError");
                
                if (fnGetSize && fnDecompress && fnIsError) {
                    unsigned long long decompSize = fnGetSize(compressed_data.data(), compressed_data.size());
                    if (decompSize == 0xFFFFFFFFFFFFFFFFULL || decompSize == 0) {
                        // Unknown size — estimate 4x expansion
                        decompSize = compressed_data.size() * 4;
                    }
                    
                    decompressed_data.resize(static_cast<size_t>(decompSize));
                    size_t result = fnDecompress(decompressed_data.data(), decompressed_data.size(),
                                                 compressed_data.data(), compressed_data.size());
                    if (fnIsError(result)) {
                        FreeLibrary(hZstd);
                        m_lastError = "ZSTD decompression failed";
                        return false;
                    }
                    decompressed_data.resize(result);
                    qInfo() << "ZSTD decompression: " << compressed_data.size() 
                             << " -> " << decompressed_data.size() << " bytes";
                } else {
                    FreeLibrary(hZstd);
                    m_lastError = "ZSTD library found but missing required functions";
                    return false;
                }
                FreeLibrary(hZstd);
            } else {
                m_lastError = "ZSTD decompression requires libzstd.dll or zstd.dll in PATH";
                return false;
            }
            
        } else if (compression == CompressionType::LZ4) {
            // LZ4 decompression: LZ4 frame starts with magic 0x184D2204
            typedef int (*LZ4F_getFrameInfo_t)(void*, void*, const void*, size_t*);
            typedef int (*LZ4_decompress_safe_t)(const char*, char*, int, int);
            
            HMODULE hLz4 = LoadLibraryA("liblz4.dll");
            if (!hLz4) hLz4 = LoadLibraryA("lz4.dll");
            
            if (hLz4) {
                auto fnDecomp = (LZ4_decompress_safe_t)GetProcAddress(hLz4, "LZ4_decompress_safe");
                
                if (fnDecomp) {
                    // LZ4 block format: first 4 bytes (LE) = original size
                    // LZ4 frame format: magic + descriptor
                    size_t origSize = compressed_data.size() * 4; // conservative estimate
                    
                    // Check for LZ4 frame magic (0x184D2204)
                    if (compressed_data.size() >= 4 &&
                        compressed_data[0] == 0x04 && compressed_data[1] == 0x22 &&
                        compressed_data[2] == 0x4D && compressed_data[3] == 0x18) {
                        // LZ4 frame format — use frame API
                        typedef size_t (*LZ4F_createDecompressionContext_t)(void**, unsigned);
                        typedef size_t (*LZ4F_decompress_t)(void*, void*, size_t*, const void*, size_t*, void*);
                        typedef size_t (*LZ4F_freeDecompressionContext_t)(void*);
                        
                        auto fnCreate = (LZ4F_createDecompressionContext_t)GetProcAddress(hLz4, "LZ4F_createDecompressionContext");
                        auto fnFrameDecomp = (LZ4F_decompress_t)GetProcAddress(hLz4, "LZ4F_decompress");
                        auto fnFree = (LZ4F_freeDecompressionContext_t)GetProcAddress(hLz4, "LZ4F_freeDecompressionContext");
                        
                        if (fnCreate && fnFrameDecomp && fnFree) {
                            void* dctx = nullptr;
                            fnCreate(&dctx, 1 /* LZ4F_VERSION */);
                            if (dctx) {
                                decompressed_data.resize(origSize);
                                size_t dstSize = decompressed_data.size();
                                size_t srcSize = compressed_data.size();
                                size_t totalOut = 0;
                                
                                const uint8_t* srcPtr = compressed_data.data();
                                size_t srcRemaining = srcSize;
                                
                                while (srcRemaining > 0) {
                                    size_t dstCap = decompressed_data.size() - totalOut;
                                    size_t srcConsumed = srcRemaining;
                                    size_t ret = fnFrameDecomp(dctx, 
                                        decompressed_data.data() + totalOut, &dstCap,
                                        srcPtr, &srcConsumed, nullptr);
                                    
                                    totalOut += dstCap;
                                    srcPtr += srcConsumed;
                                    srcRemaining -= srcConsumed;
                                    
                                    if (ret == 0) break; // Done
                                    
                                    // Need more output space
                                    if (totalOut >= decompressed_data.size()) {
                                        decompressed_data.resize(decompressed_data.size() * 2);
                                    }
                                }
                                
                                decompressed_data.resize(totalOut);
                                fnFree(dctx);
                            }
                        }
                    } else {
                        // LZ4 block format
                        decompressed_data.resize(origSize);
                        int decompLen = fnDecomp(
                            reinterpret_cast<const char*>(compressed_data.data()),
                            reinterpret_cast<char*>(decompressed_data.data()),
                            static_cast<int>(compressed_data.size()),
                            static_cast<int>(decompressed_data.size()));
                        
                        if (decompLen <= 0) {
                            FreeLibrary(hLz4);
                            m_lastError = "LZ4 block decompression failed";
                            return false;
                        }
                        decompressed_data.resize(decompLen);
                    }
                    
                    qInfo() << "LZ4 decompression: " << compressed_data.size()
                             << " -> " << decompressed_data.size() << " bytes";
                } else {
                    FreeLibrary(hLz4);
                    m_lastError = "LZ4 library found but missing LZ4_decompress_safe";
                    return false;
                }
                FreeLibrary(hLz4);
            } else {
                m_lastError = "LZ4 decompression requires liblz4.dll or lz4.dll in PATH";
                return false;
            }
        } else {
            m_lastError = "Unknown compression type";
            return false;
        }

        // Write decompressed data
        std::ofstream outfile(tempFile, std::ios::binary);
        if (!outfile.is_open()) {
            m_lastError = "Cannot write temp file";
            return false;
        }

        outfile.write(reinterpret_cast<const char*>(decompressed_data.data()), decompressed_data.size());
        outfile.close();

        m_tempFiles.push_back(tempFile);
        emit loadingProgress(60);

        // Now load as GGUF
        return loadGGUFLocal(QString::fromStdString(tempFile));

    } catch (const std::exception& e) {
        m_lastError = QString("Decompression failed: %1").arg(e.what());
        return false;
    }
}

bool EnhancedModelLoader::startServer(quint16 port) {
    m_port = port;

    if (!m_engine) {
        m_lastError = "No model loaded";
        emit error(m_lastError);
        return false;
    }

    if (!m_server) {
        m_server = std::make_unique<GGUFServer>(m_engine.get(), this);
        connect(m_server.get(), &GGUFServer::serverStarted, this, &EnhancedModelLoader::serverStarted);
        connect(m_server.get(), &GGUFServer::serverStopped, this, &EnhancedModelLoader::serverStopped);
    }

    return m_server->start(port);
}

void EnhancedModelLoader::stopServer() {
    if (m_server && m_server->isRunning()) {
        m_server->stop();
    }
}

bool EnhancedModelLoader::isServerRunning() const {
    return m_server && m_server->isRunning();
}

QString EnhancedModelLoader::getModelInfo() const {
    if (m_engine && m_engine->isModelLoaded()) {
        return QString("GGUF Model loaded: %1").arg(QString::fromStdString(m_engine->modelPath()));
    }
    return "No model loaded";
}

quint16 EnhancedModelLoader::getServerPort() const {
    return m_port;
}

QString EnhancedModelLoader::getServerUrl() const {
    return QString("http://localhost:%1").arg(m_port);
}

void EnhancedModelLoader::logLoadStart(const QString& input, ModelFormat format) {
    qInfo() << "📥 Model load started:"
            << input
            << "format:" << QString::fromStdString(FormatRouter::formatToString(format));
}

void EnhancedModelLoader::logLoadSuccess(const QString& input, ModelFormat format, qint64 durationMs) {
    qInfo() << "✅ Model load completed:"
            << input
            << "format:" << QString::fromStdString(FormatRouter::formatToString(format))
            << "duration:" << durationMs << "ms";
}

void EnhancedModelLoader::logLoadError(const QString& input, ModelFormat format, const QString& error) {
    qWarning() << "❌ Model load failed:"
               << input
               << "format:" << QString::fromStdString(FormatRouter::formatToString(format))
               << "error:" << error;
}

bool EnhancedModelLoader::setupTempDirectory() {
    try {
        std::string appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
        m_tempDirectory = appData + "/model_loader_temp";
        std::filesystem::create_directories(m_tempDirectory);
        qDebug() << "Temp directory:" << QString::fromStdString(m_tempDirectory);
        return true;
    } catch (const std::exception& e) {
        qWarning() << "Failed to setup temp directory:" << e.what();
        return false;
    }
}

void EnhancedModelLoader::cleanupTempFiles() {
    for (const auto& file : m_tempFiles) {
        try {
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
            }
        } catch (const std::exception& e) {
            qWarning() << "Failed to delete temp file:" << QString::fromStdString(file) << e.what();
        }
    }
    m_tempFiles.clear();
}
