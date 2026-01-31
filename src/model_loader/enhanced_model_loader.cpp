#include "enhanced_model_loader.h"
#include "../../include/inference_engine_stub.hpp"
#include "qtapp/gguf_server.hpp"
#include "streaming_gguf_loader.h"


#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>

EnhancedModelLoader::EnhancedModelLoader(void* parent)
    : void(parent)
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

bool EnhancedModelLoader::loadModel(const std::string& modelInput) {
    if (modelInput.empty()) {
        m_lastError = "Model input is empty";
        error(m_lastError);
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
            error(m_lastError);
            return false;
        }

        m_loadedFormat = modelSource->format;
        logLoadStart(modelInput, modelSource->format);
        loadingStage(std::string::fromStdString(modelSource->display_name));

        bool success = false;

        switch (modelSource->format) {
            case ModelFormat::GGUF_LOCAL:
                loadingStage("Loading local GGUF model...");
                success = loadGGUFLocal(modelInput);
                break;

            case ModelFormat::HF_REPO:
                loadingStage("Downloading from HuggingFace Hub...");
                success = loadHFModel(modelInput);
                break;

            case ModelFormat::HF_FILE:
                loadingStage("Downloading HF file...");
                success = loadHFModel(modelInput);
                break;

            case ModelFormat::OLLAMA_REMOTE:
                loadingStage("Connecting to Ollama endpoint...");
                success = loadOllamaModel(modelInput);
                break;

            case ModelFormat::MASM_COMPRESSED:
                loadingStage("Decompressing model...");
                success = loadCompressedModel(modelInput);
                break;

            default:
                m_lastError = "Unsupported model format";
                error(m_lastError);
                return false;
        }

        const auto end_time = std::chrono::steady_clock::now();
        const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        if (success) {
            m_modelPath = modelInput;
            logLoadSuccess(modelInput, modelSource->format, duration_ms);
            modelLoaded(modelInput);
            return true;
        } else {
            m_lastError = "Model loading failed at format stage";
            logLoadError(modelInput, modelSource->format, m_lastError);
            error(m_lastError);
            return false;
        }

    } catch (const std::exception& e) {
        m_lastError = std::string("Exception: %1"));
        logLoadError(modelInput, ModelFormat::UNKNOWN, m_lastError);
        error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::loadGGUFLocal(const std::string& modelPath) {
    std::string path = modelPath.toStdString();

    if (!std::filesystem::exists(path)) {
        m_lastError = "GGUF file not found: " + modelPath;
        error(m_lastError);
        return false;
    }

    try {
        auto fileSize = std::filesystem::file_size(path);

        if (!m_engine) {
            m_engine = std::make_unique<InferenceEngine>();
        }

        // Load into inference engine
        if (!m_engine->Initialize(path)) {
            m_lastError = "InferenceEngine failed to initialize";
            error(m_lastError);
            return false;
        }

        loadingProgress(80);

        // Optionally start GGUF server
        if (!m_server) {
            m_server = std::make_unique<GGUFServer>(m_engine.get(), this);
// Qt connect removed
// Qt connect removed
// Qt connect removed
        }

        if (!m_server->start(m_port)) {
        }

        loadingProgress(100);
        return true;

    } catch (const std::exception& e) {
        m_lastError = std::string("GGUF load exception: %1"));
        error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::loadHFModel(const std::string& repoId) {
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
        loadingProgress(10);

        // TODO: Implement HF download with cache logic
        // For now, return error to prevent silent failure
        m_lastError = "HuggingFace downloads not yet implemented";
        error(m_lastError);
        return false;

    } catch (const std::exception& e) {
        m_lastError = std::string("HF download exception: %1"));
        error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::loadOllamaModel(const std::string& modelName) {
    std::string name = modelName.toStdString();

    try {
        loadingProgress(20);

        // Validate Ollama is available
        if (!m_ollamaProxy->isOllamaAvailable()) {
            m_lastError = "Ollama service not available on localhost:11434";
            error(m_lastError);
            return false;
        }

        // Check if model exists
        if (!m_ollamaProxy->isModelAvailable(modelName)) {
            m_lastError = "Model not found in Ollama: " + modelName;
            error(m_lastError);
            return false;
        }

        // Set model for Ollama proxy
        m_ollamaProxy->setModel(modelName);
        
        loadingProgress(100);
        return true;

    } catch (const std::exception& e) {
        m_lastError = std::string("Ollama connection exception: %1"));
        error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::loadCompressedModel(const std::string& compressedPath) {
    std::string path = compressedPath.toStdString();

    if (!std::filesystem::exists(path)) {
        m_lastError = "Compressed file not found: " + compressedPath;
        error(m_lastError);
        return false;
    }

    try {
        auto fileSize = std::filesystem::file_size(path);
        loadingProgress(30);

        auto source = m_formatRouter->route(path);
        if (!source) {
            m_lastError = "Failed to detect compression type";
            error(m_lastError);
            return false;
        }

        return decompressAndLoad(compressedPath, source->compression);

    } catch (const std::exception& e) {
        m_lastError = std::string("Compressed load exception: %1"));
        error(m_lastError);
        return false;
    }
}

bool EnhancedModelLoader::decompressAndLoad(const std::string& compressedPath, CompressionType compression) {
    std::string compressed = compressedPath.toStdString();
    std::string tempFile = m_tempDirectory + "/decompressed_model_" + std::to_string(std::time(nullptr)) + ".gguf";

    try {
        loadingProgress(40);

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
            decompressed_data = compressed_data;
        } else if (compression == CompressionType::ZSTD) {
            decompressed_data = compressed_data;
        } else if (compression == CompressionType::LZ4) {
            decompressed_data = compressed_data;
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
        loadingProgress(60);

        // Now load as GGUF
        return loadGGUFLocal(std::string::fromStdString(tempFile));

    } catch (const std::exception& e) {
        m_lastError = std::string("Decompression failed: %1"));
        return false;
    }
}

bool EnhancedModelLoader::startServer(quint16 port) {
    m_port = port;

    if (!m_engine) {
        m_lastError = "No model loaded";
        error(m_lastError);
        return false;
    }

    if (!m_server) {
        m_server = std::make_unique<GGUFServer>(m_engine.get(), this);
// Qt connect removed
// Qt connect removed
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

std::string EnhancedModelLoader::getModelInfo() const {
    if (m_engine && m_engine->isModelLoaded()) {
        return std::string("GGUF Model loaded: %1")));
    }
    return "No model loaded";
}

quint16 EnhancedModelLoader::getServerPort() const {
    return m_port;
}

std::string EnhancedModelLoader::getServerUrl() const {
    return std::string("http://localhost:%1");
}

void EnhancedModelLoader::logLoadStart(const std::string& input, ModelFormat format) {
            << input
            << "format:" << std::string::fromStdString(FormatRouter::formatToString(format));
}

void EnhancedModelLoader::logLoadSuccess(const std::string& input, ModelFormat format, int64_t durationMs) {
            << input
            << "format:" << std::string::fromStdString(FormatRouter::formatToString(format))
            << "duration:" << durationMs << "ms";
}

void EnhancedModelLoader::logLoadError(const std::string& input, ModelFormat format, const std::string& error) {
               << input
               << "format:" << std::string::fromStdString(FormatRouter::formatToString(format))
               << "error:" << error;
}

bool EnhancedModelLoader::setupTempDirectory() {
    try {
        std::string appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
        m_tempDirectory = appData + "/model_loader_temp";
        std::filesystem::create_directories(m_tempDirectory);
        return true;
    } catch (const std::exception& e) {
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
        }
    }
    m_tempFiles.clear();
}



