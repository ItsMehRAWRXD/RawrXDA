#include "enhanced_model_loader.h"
#include "../../include/inference_engine_stub.hpp"
#include "qtapp/gguf_server.hpp"
#include "qtapp/universal_format_loader.hpp"
#include "streaming_gguf_loader.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>
#include "../utils/Logger.hpp"

EnhancedModelLoader::EnhancedModelLoader(QObject* parent)
    : QObject(parent)
    , m_engine(nullptr)
    , m_server(nullptr)
    , m_formatRouter(std::make_unique<FormatRouter>())
    , m_universalLoader(std::make_unique<UniversalFormatLoader>())
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
                // Try universal format loader (SafeTensors, PyTorch, TensorFlow, ONNX, etc.)
                emit loadingStage("Converting model format...");
                success = loadUniversalFormat(modelInput);
                break;
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

        // TODO: Implement HF download with cache logic
        // For now, return error to prevent silent failure
        m_lastError = "HuggingFace downloads not yet implemented";
        Logger::error(std::string("HF download not implemented for repo: ") + repo_name);
        emit error(m_lastError);
        return false;

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

        // For now, just copy the data (real decompression would use zstd/gzip libraries)
        // This is a placeholder that prevents silent failure
        if (compression == CompressionType::GZIP) {
            Logger::warn("GZIP decompression not yet implemented - using stored data as-is");
            decompressed_data = compressed_data;
        } else if (compression == CompressionType::ZSTD) {
            Logger::warn("ZSTD decompression not yet implemented - using stored data as-is");
            decompressed_data = compressed_data;
        } else if (compression == CompressionType::LZ4) {
            Logger::warn("LZ4 decompression not yet implemented - using stored data as-is");
            decompressed_data = compressed_data;
        } else {
            m_lastError = "Unknown compression type";
            Logger::error(std::string("Unknown compression type for file: ") + compressed);
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
        m_lastError = QString("Decompression exception: %1").arg(e.what());
        emit error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::loadUniversalFormat(const QString& modelPath) {
    if (!m_universalLoader) {
        m_lastError = "Universal format loader not initialized";
        emit error(m_lastError);
        return false;
    }

    qInfo() << "[EnhancedModelLoader] Loading non-GGUF format:" << modelPath;
    emit loadingProgress(20);

    try {
        // Use MASM-based universal loader to convert to GGUF
        QByteArray ggufData = m_universalLoader->load(modelPath);
        
        if (ggufData.isEmpty()) {
            m_lastError = QString("Universal loader failed: %1").arg(m_universalLoader->getLastError());
            emit error(m_lastError);
            return false;
        }

        emit loadingProgress(60);

        // Write converted GGUF to temp file
        QString tempFile = QString("%1/converted_model_%2.gguf")
            .arg(QString::fromStdString(m_tempDirectory))
            .arg(std::time(nullptr));

        QFile outfile(tempFile);
        if (!outfile.open(QIODevice::WriteOnly)) {
            m_lastError = "Cannot write converted GGUF file";
            emit error(m_lastError);
            return false;
        }

        outfile.write(ggufData);
        outfile.close();

        m_tempFiles.push_back(tempFile.toStdString());
        
        emit loadingProgress(80);
        qInfo() << "[EnhancedModelLoader] Successfully converted to GGUF, size:" << ggufData.size() << "bytes";

        // Load the converted GGUF
        return loadGGUFLocal(tempFile);

    } catch (const std::exception& e) {
        m_lastError = QString("Universal format load exception: %1").arg(e.what());
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

        // For now, just copy the data (real decompression would use zstd/gzip libraries)
        // This is a placeholder that prevents silent failure
        if (compression == CompressionType::GZIP) {
            Logger::warn("GZIP decompression not yet implemented - using stored data as-is");
            decompressed_data = compressed_data;
        } else if (compression == CompressionType::ZSTD) {
            Logger::warn("ZSTD decompression not yet implemented - using stored data as-is");
            decompressed_data = compressed_data;
        } else if (compression == CompressionType::LZ4) {
            Logger::warn("LZ4 decompression not yet implemented - using stored data as-is");
            decompressed_data = compressed_data;
        } else {
            m_lastError = "Unknown compression type";
            Logger::error(std::string("Unknown compression type for file: ") + compressed);
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
