// ============================================================================
// model_puller.cpp — Unified Model Puller Implementation
// ============================================================================
// Routes pulls to HF / Ollama / URL / Local, coordinates download +
// verification + registry registration.
// ============================================================================

#include "model_puller/model_puller.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>

namespace RawrXD {

// ============================================================================
// GGUF format constants (inline — avoids linking CPUInference::GGUFLoader)
// ============================================================================
static constexpr uint32_t GGUF_MAGIC = 0x46554747u; // "GGUF" LE

// GGUF metadata value types (v2/v3 spec)
enum GGUFValueType : uint32_t {
    GGUF_TYPE_UINT8    = 0,
    GGUF_TYPE_INT8     = 1,
    GGUF_TYPE_UINT16   = 2,
    GGUF_TYPE_INT16    = 3,
    GGUF_TYPE_UINT32   = 4,
    GGUF_TYPE_INT32    = 5,
    GGUF_TYPE_FLOAT32  = 6,
    GGUF_TYPE_BOOL     = 7,
    GGUF_TYPE_STRING   = 8,
    GGUF_TYPE_ARRAY    = 9,
    GGUF_TYPE_UINT64   = 10,
    GGUF_TYPE_INT64    = 11,
    GGUF_TYPE_FLOAT64  = 12,
};

// Read a GGUF-format string: uint64 length + raw bytes (no null terminator)
static bool ReadGGUFString(std::ifstream& f, std::string& out) {
    uint64_t len = 0;
    if (!f.read(reinterpret_cast<char*>(&len), 8)) return false;
    if (len > 1024 * 1024) return false; // sanity cap: 1 MB
    out.resize(static_cast<size_t>(len));
    return !!f.read(out.data(), static_cast<std::streamsize>(len));
}

// Skip over a typed GGUF metadata value without reading it
static bool SkipGGUFValue(std::ifstream& f, uint32_t valueType) {
    switch (valueType) {
        case GGUF_TYPE_UINT8:   case GGUF_TYPE_INT8:  case GGUF_TYPE_BOOL: f.seekg(1, std::ios::cur); break;
        case GGUF_TYPE_UINT16:  case GGUF_TYPE_INT16:  f.seekg(2, std::ios::cur); break;
        case GGUF_TYPE_UINT32:  case GGUF_TYPE_INT32:  case GGUF_TYPE_FLOAT32: f.seekg(4, std::ios::cur); break;
        case GGUF_TYPE_UINT64:  case GGUF_TYPE_INT64:  case GGUF_TYPE_FLOAT64: f.seekg(8, std::ios::cur); break;
        case GGUF_TYPE_STRING: { std::string s; if (!ReadGGUFString(f, s)) return false; break; }
        case GGUF_TYPE_ARRAY: {
            uint32_t elemType = 0;
            uint64_t elemCount = 0;
            if (!f.read(reinterpret_cast<char*>(&elemType), 4)) return false;
            if (!f.read(reinterpret_cast<char*>(&elemCount), 8)) return false;
            for (uint64_t i = 0; i < elemCount; ++i)
                if (!SkipGGUFValue(f, elemType)) return false;
            break;
        }
        default: return false;
    }
    return f.good();
}

// ============================================================================
// ValidateGGUF — post-download GGUF header + metadata validation (inline)
// ============================================================================
// Returns true if file is a valid GGUF. Populates entry metadata on success.
static bool ValidateGGUF(const std::string& filePath, ModelEntry& entry, std::string& errorOut) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) {
        errorOut = "Cannot open GGUF file: " + filePath;
        return false;
    }

    // Read header: magic(4) + version(4) + tensor_count(8) + metadata_kv_count(8)
    uint32_t magic = 0, version = 0;
    uint64_t tensorCount = 0, kvCount = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);
    f.read(reinterpret_cast<char*>(&version), 4);
    f.read(reinterpret_cast<char*>(&tensorCount), 8);
    f.read(reinterpret_cast<char*>(&kvCount), 8);
    if (!f.good()) { errorOut = "GGUF too short: cannot read header"; return false; }

    if (magic != GGUF_MAGIC) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%08X", magic);
        errorOut = std::string("Bad GGUF magic: expected 0x46554747, got ") + buf;
        return false;
    }

    if (version < 2 || version > 3) {
        errorOut = "Unsupported GGUF version: " + std::to_string(version);
        return false;
    }

    // Walk metadata KV pairs to find "general.architecture"
    // Limit to first 200 KV pairs to avoid unbounded reads
    uint64_t kvLimit = (kvCount < 200) ? kvCount : 200;
    for (uint64_t i = 0; i < kvLimit; ++i) {
        std::string key;
        if (!ReadGGUFString(f, key)) break;

        uint32_t valueType = 0;
        if (!f.read(reinterpret_cast<char*>(&valueType), 4)) break;

        if (key == "general.architecture" && valueType == GGUF_TYPE_STRING) {
            std::string arch;
            if (ReadGGUFString(f, arch) && !arch.empty())
                entry.architecture = arch;
            return true; // found what we need — done
        }

        // Skip this value
        if (!SkipGGUFValue(f, valueType)) break;
    }

    return true; // valid GGUF even if architecture not found
}

// ============================================================================
// CheckDiskSpace — pre-download free space validation
// ============================================================================
// Requires at least 1.5x model size free to account for temp files / partial writes.
static bool CheckDiskSpace(const std::string& destPath, uint64_t requiredBytes, std::string& errorOut) {
    if (requiredBytes == 0) return true; // unknown size, skip check

    std::filesystem::path destDir = std::filesystem::path(destPath).parent_path();
    if (destDir.empty()) destDir = ".";

    std::error_code ec;
    std::filesystem::create_directories(destDir, ec);

    auto spaceInfo = std::filesystem::space(destDir, ec);
    if (ec) {
        // Can't determine space — allow the download to proceed
        return true;
    }

    // Require 1.5x the model size
    uint64_t needed = requiredBytes + (requiredBytes / 2);
    if (spaceInfo.available < needed) {
        double availGB = static_cast<double>(spaceInfo.available) / (1024.0 * 1024.0 * 1024.0);
        double needGB  = static_cast<double>(needed) / (1024.0 * 1024.0 * 1024.0);
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Insufficient disk space: %.2f GB available, %.2f GB needed (%.2f GB model + 50%% margin)",
                 availGB, needGB,
                 static_cast<double>(requiredBytes) / (1024.0 * 1024.0 * 1024.0));
        errorOut = buf;
        return false;
    }
    return true;
}

// ============================================================================
// CleanupCorruptPartial — remove zero-byte or corrupt partial downloads
// ============================================================================
static void CleanupCorruptPartial(const std::string& filePath) {
    std::error_code ec;
    if (std::filesystem::exists(filePath, ec)) {
        auto sz = std::filesystem::file_size(filePath, ec);
        if (!ec && sz == 0) {
            std::filesystem::remove(filePath, ec);
        }
    }
}

// ============================================================================
// Singleton
// ============================================================================
ModelPuller& ModelPuller::Instance() {
    static ModelPuller instance;
    return instance;
}

ModelPuller::ModelPuller() {
    m_index.Initialize();
}

ModelPuller::~ModelPuller() = default;

// ============================================================================
// Parse — classify user input
// ============================================================================
ModelSource ModelPuller::Parse(const std::string& input) const {
    ModelSource src;
    src.identifier = input;

    // Trim whitespace
    std::string trimmed = input;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) trimmed.pop_back();

    if (trimmed.empty()) {
        src.type = ModelSource::LOCAL;
        return src;
    }

    // 1) Direct URL: starts with http:// or https://
    if (trimmed.rfind("http://", 0) == 0 || trimmed.rfind("https://", 0) == 0) {
        src.type = ModelSource::URL;
        src.identifier = trimmed;
        // Extract filename from URL path
        auto lastSlash = trimmed.rfind('/');
        if (lastSlash != std::string::npos) {
            src.filename = trimmed.substr(lastSlash + 1);
            // Remove query params
            auto qpos = src.filename.find('?');
            if (qpos != std::string::npos) src.filename = src.filename.substr(0, qpos);
        }
        src.quantization = HuggingFaceClient::ExtractQuantization(src.filename);
        return src;
    }

    // 2) Local file path (exists on disk)
    if (std::filesystem::exists(trimmed)) {
        src.type = ModelSource::LOCAL;
        src.identifier = trimmed;
        src.filename = std::filesystem::path(trimmed).filename().string();
        src.quantization = HuggingFaceClient::ExtractQuantization(src.filename);
        return src;
    }

    // 3) Windows-style path (drive letter) — even if doesn't exist yet
    if (trimmed.size() >= 3 && std::isalpha(static_cast<unsigned char>(trimmed[0]))
        && trimmed[1] == ':' && (trimmed[2] == '\\' || trimmed[2] == '/')) {
        src.type = ModelSource::LOCAL;
        src.identifier = trimmed;
        return src;
    }

    // 4) HuggingFace format: "user/repo" or "user/repo:quant"
    //    Heuristic: contains exactly one slash, no colon before slash
    //    Or: "user/repo:Q4_K_M" where colon is after the slash
    {
        auto slashPos = trimmed.find('/');
        if (slashPos != std::string::npos && slashPos > 0) {
            // Has a slash — likely HF
            src.type = ModelSource::HUGGING_FACE;

            // Check for :quant suffix (after the slash)
            std::string afterSlash = trimmed.substr(slashPos + 1);
            auto colonPos = afterSlash.rfind(':');
            if (colonPos != std::string::npos && colonPos > 0) {
                src.quantization = afterSlash.substr(colonPos + 1);
                src.identifier = trimmed.substr(0, slashPos + 1 + colonPos);
            } else {
                src.identifier = trimmed;
            }
            return src;
        }
    }

    // 5) Ollama format: "modelname" or "modelname:tag"
    //    No slash, possibly a colon for tag
    {
        src.type = ModelSource::OLLAMA;
        auto colonPos = trimmed.find(':');
        if (colonPos != std::string::npos) {
            src.identifier = trimmed.substr(0, colonPos);
            src.tag = trimmed.substr(colonPos + 1);
        } else {
            src.identifier = trimmed;
            src.tag = "latest";
        }
        return src;
    }
}

// ============================================================================
// Pull (string input)
// ============================================================================
PullResult ModelPuller::Pull(const std::string& input, PullStatusCallback onStatus) {
    ModelSource src = Parse(input);
    return Pull(src, onStatus);
}

// ============================================================================
// Pull (ModelSource)
// ============================================================================
PullResult ModelPuller::Pull(const ModelSource& source, PullStatusCallback onStatus) {
    std::lock_guard<std::mutex> lock(m_pullMutex);

    switch (source.type) {
        case ModelSource::HUGGING_FACE:
            return PullFromHF(source, onStatus);
        case ModelSource::OLLAMA:
            return PullFromOllama(source, onStatus);
        case ModelSource::URL:
            return PullFromURL(source, onStatus);
        case ModelSource::LOCAL:
            return PullFromLocal(source, onStatus);
        default: {
            PullResult r;
            r.error = "Unknown source type";
            return r;
        }
    }
}

// ============================================================================
// PullFromHF
// ============================================================================
PullResult ModelPuller::PullFromHF(const ModelSource& src, PullStatusCallback cb) {
    PullResult result;

    // Step 1: Fetch file list
    ReportStep(cb, PullStep::FetchingFileList, 1, "Fetching file list from Hugging Face...");

    std::vector<HFFileInfo> files;
    if (!m_hfClient.ListGGUFFiles(src.identifier, files)) {
        result.error = "Failed to list GGUF files for " + src.identifier;
        ReportStep(cb, PullStep::Failed, 1, result.error);
        return result;
    }

    if (files.empty()) {
        result.error = "No GGUF files found in " + src.identifier;
        ReportStep(cb, PullStep::Failed, 1, result.error);
        return result;
    }

    // Step 2: Resolve quantization
    ReportStep(cb, PullStep::ResolvingQuantization, 2, "Resolving quantization...");

    const HFFileInfo* selectedFile = nullptr;

    if (!src.quantization.empty()) {
        // Match by quantization
        std::string upperQ = src.quantization;
        std::transform(upperQ.begin(), upperQ.end(), upperQ.begin(), ::toupper);

        for (auto& f : files) {
            std::string fq = f.quantization;
            std::transform(fq.begin(), fq.end(), fq.begin(), ::toupper);
            if (fq == upperQ) {
                selectedFile = &f;
                break;
            }
        }

        if (!selectedFile) {
            // Fuzzy match: user typed "q4" but file has "Q4_K_M"
            for (auto& f : files) {
                std::string fq = f.quantization;
                std::transform(fq.begin(), fq.end(), fq.begin(), ::toupper);
                if (fq.find(upperQ) != std::string::npos) {
                    selectedFile = &f;
                    break;
                }
            }
        }
    }

    if (!selectedFile && !src.filename.empty()) {
        // Match by exact filename
        for (auto& f : files) {
            if (f.filename == src.filename || f.rfilename == src.filename) {
                selectedFile = &f;
                break;
            }
        }
    }

    if (!selectedFile) {
        // Default: pick the Q4_K_M if available, else the first file
        for (auto& f : files) {
            if (f.quantization == "Q4_K_M") {
                selectedFile = &f;
                break;
            }
        }
        if (!selectedFile) selectedFile = &files[0];
    }

    std::cout << "Found: " << selectedFile->filename
              << " (" << (selectedFile->sizeBytes / (1024ULL * 1024 * 1024))
              << "." << ((selectedFile->sizeBytes / (1024ULL * 1024 * 100)) % 10)
              << " GB)\n";

    // Build destination path
    std::string safeName = src.identifier;
    for (auto& c : safeName) {
        if (c == '/' || c == '\\') c = '_';
    }

    std::filesystem::path destDir = std::filesystem::path(m_index.GetModelsBasePath())
                                    / "huggingface" / safeName;
    std::error_code ec;
    std::filesystem::create_directories(destDir, ec);

    std::string destPath = (destDir / selectedFile->filename).string();

    // Disk space pre-check
    {
        std::string spaceErr;
        if (!CheckDiskSpace(destPath, selectedFile->sizeBytes, spaceErr)) {
            result.error = spaceErr;
            ReportStep(cb, PullStep::Failed, 2, result.error);
            return result;
        }
    }

    // Step 3: Download
    ReportStep(cb, PullStep::Downloading, 3, "Downloading " + selectedFile->filename + "...");

    bool dlOk = m_hfClient.Download(src.identifier, selectedFile->filename, destPath,
        [&cb](const DownloadProgress& p) {
            if (cb) {
                PullStatus s;
                s.step = PullStep::Downloading;
                s.stepNumber = 3;
                s.totalSteps = 4;
                s.stepDescription = "Downloading...";
                s.downloadProgress = p;
                cb(s);
            }
        });

    if (!dlOk) {
        result.error = "Download failed for " + selectedFile->filename;
        ReportStep(cb, PullStep::Failed, 3, result.error);
        CleanupCorruptPartial(destPath);
        return result;
    }

    // Step 4: Verify SHA256
    ReportStep(cb, PullStep::Verifying, 4, "Verifying SHA256...");

    std::string sha256 = DownloadManager::ComputeSHA256(destPath);
    if (!selectedFile->sha256.empty()) {
        if (!DownloadManager::VerifySHA256(destPath, selectedFile->sha256)) {
            result.error = "SHA256 verification failed! Expected " + selectedFile->sha256
                         + " got " + sha256;
            ReportStep(cb, PullStep::Failed, 4, result.error);
            return result;
        }
        std::cout << "Verifying SHA256... OK\n";
    } else {
        std::cout << "Verifying SHA256... (no hash available, computed: " << sha256 << ")\n";
    }

    // Step 4b: GGUF header + metadata validation
    {
        std::string ggufErr;
        ModelEntry tmpEntry;
        if (!ValidateGGUF(destPath, tmpEntry, ggufErr)) {
            result.error = "GGUF validation failed: " + ggufErr;
            ReportStep(cb, PullStep::Failed, 4, result.error);
            return result;
        }
        // Will use tmpEntry.architecture below
        if (!tmpEntry.architecture.empty()) {
            std::cout << "GGUF validated: arch=" << tmpEntry.architecture << "\n";
        }

        // Step 5: Register
        ReportStep(cb, PullStep::Registering, 4, "Registering model...");

        // Extract model name from filename (remove quant and extension)
        std::string modelName = selectedFile->filename;
        auto dotPos = modelName.rfind(".gguf");
        if (dotPos != std::string::npos) modelName = modelName.substr(0, dotPos);
        // Try to remove quantization suffix for a cleaner name
        auto dashPos = modelName.rfind('-');
        if (dashPos != std::string::npos && !selectedFile->quantization.empty()) {
            std::string tail = modelName.substr(dashPos + 1);
            std::string upperTail = tail;
            std::transform(upperTail.begin(), upperTail.end(), upperTail.begin(), ::toupper);
            std::string upperQ = selectedFile->quantization;
            std::transform(upperQ.begin(), upperQ.end(), upperQ.begin(), ::toupper);
            if (upperTail == upperQ) {
                modelName = modelName.substr(0, dashPos);
            }
        }

        ModelEntry entry;
        entry.id           = ModelIndex::GenerateId(modelName, selectedFile->quantization);
        entry.name         = modelName;
        entry.quantization = selectedFile->quantization;
        entry.path         = std::filesystem::relative(destPath, m_index.GetModelsBasePath()).string();
        entry.absolutePath = std::filesystem::absolute(destPath).string();
        entry.sizeBytes    = selectedFile->sizeBytes;
        entry.sha256       = sha256;
        entry.source       = "hf://" + src.identifier;
        entry.downloadedAt = ModelIndex::NowISO8601();
        entry.architecture = tmpEntry.architecture;

        m_index.AddModel(entry);

        result.success   = true;
        result.modelId   = entry.id;
        result.filePath  = entry.absolutePath;
        result.sizeBytes = entry.sizeBytes;
        result.sha256    = sha256;
    } // end GGUF validation block

    ReportStep(cb, PullStep::Complete, 4, "Done.");
    std::cout << "\nModel ready. Use: rawrxd run " << result.modelId << "\n";

    return result;
}

// ============================================================================
// PullFromOllama
// ============================================================================
PullResult ModelPuller::PullFromOllama(const ModelSource& src, PullStatusCallback cb) {
    PullResult result;

    std::string spec = src.identifier;
    if (!src.tag.empty() && src.tag != "latest") {
        spec += ":" + src.tag;
    }

    // Step 1: Fetch manifest
    ReportStep(cb, PullStep::FetchingFileList, 1, "Fetching manifest from Ollama registry...");

    uint64_t modelSize = 0;
    m_ollamaClient.GetModelSize(spec, modelSize);
    if (modelSize > 0) {
        std::cout << "Model size: " << (modelSize / (1024ULL * 1024 * 1024))
                  << "." << ((modelSize / (1024ULL * 1024 * 100)) % 10) << " GB\n";

        // Disk space pre-check
        std::string spaceErr;
        std::string ollamaDir = (std::filesystem::path(m_index.GetModelsBasePath()) / "native").string();
        if (!CheckDiskSpace(ollamaDir + "\\check", modelSize, spaceErr)) {
            result.error = spaceErr;
            ReportStep(cb, PullStep::Failed, 1, result.error);
            return result;
        }
    }

    // Step 2: Download
    ReportStep(cb, PullStep::Downloading, 2, "Downloading from Ollama registry...");

    std::string outFilePath;
    bool dlOk = m_ollamaClient.Pull(spec, m_index.GetModelsBasePath(), outFilePath,
        [&cb](const DownloadProgress& p) {
            if (cb) {
                PullStatus s;
                s.step = PullStep::Downloading;
                s.stepNumber = 2;
                s.totalSteps = 4;
                s.downloadProgress = p;
                cb(s);
            }
        });

    if (!dlOk || outFilePath.empty()) {
        result.error = "Failed to pull " + spec + " from Ollama registry";
        ReportStep(cb, PullStep::Failed, 2, result.error);
        if (!outFilePath.empty()) CleanupCorruptPartial(outFilePath);
        return result;
    }

    // Step 3: Verify
    ReportStep(cb, PullStep::Verifying, 3, "Computing SHA256...");
    std::string sha256 = DownloadManager::ComputeSHA256(outFilePath);

    // Step 3b: GGUF validation
    std::string ggufArch;
    {
        std::string ggufErr;
        ModelEntry tmpEntry;
        if (ValidateGGUF(outFilePath, tmpEntry, ggufErr)) {
            ggufArch = tmpEntry.architecture;
            if (!ggufArch.empty()) {
                std::cout << "GGUF validated: arch=" << ggufArch << "\n";
            }
        } else {
            // Ollama may produce non-GGUF formats (e.g. safetensors). Warn but don't fail.
            std::cout << "[warn] GGUF validation skipped: " << ggufErr << "\n";
        }
    }

    // Step 4: Register
    ReportStep(cb, PullStep::Registering, 4, "Registering model...");

    std::error_code ec;
    uint64_t fileSize = std::filesystem::file_size(outFilePath, ec);

    std::string safeName = src.identifier;
    for (auto& c : safeName) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') c = '-';
    }

    ModelEntry entry;
    entry.id           = ModelIndex::GenerateId(safeName, src.tag);
    entry.name         = src.identifier;
    entry.quantization = src.tag;
    entry.path         = std::filesystem::relative(outFilePath, m_index.GetModelsBasePath()).string();
    entry.absolutePath = std::filesystem::absolute(outFilePath).string();
    entry.sizeBytes    = fileSize;
    entry.architecture = ggufArch;
    entry.sha256       = sha256;
    entry.source       = "native://" + spec;
    entry.downloadedAt = ModelIndex::NowISO8601();

    m_index.AddModel(entry);

    result.success   = true;
    result.modelId   = entry.id;
    result.filePath  = entry.absolutePath;
    result.sizeBytes = fileSize;
    result.sha256    = sha256;

    ReportStep(cb, PullStep::Complete, 4, "Done.");
    std::cout << "\nModel ready. Use: rawrxd run " << entry.id << "\n";

    return result;
}

// ============================================================================
// PullFromURL
// ============================================================================
PullResult ModelPuller::PullFromURL(const ModelSource& src, PullStatusCallback cb) {
    PullResult result;

    // Extract filename from URL
    std::string filename = src.filename;
    if (filename.empty()) {
        filename = "model.gguf";
    }

    // Step 1: Connect
    ReportStep(cb, PullStep::Downloading, 1, "Downloading from URL...");

    std::filesystem::path destDir = std::filesystem::path(m_index.GetModelsBasePath()) / "url";
    std::error_code ec;
    std::filesystem::create_directories(destDir, ec);

    std::string destPath = (destDir / filename).string();

    bool dlOk = m_urlDownloader.Download(src.identifier, destPath,
        [&cb](const DownloadProgress& p) {
            if (cb) {
                PullStatus s;
                s.step = PullStep::Downloading;
                s.stepNumber = 1;
                s.totalSteps = 3;
                s.downloadProgress = p;
                cb(s);
            }
        });

    if (!dlOk) {
        result.error = "Download failed from " + src.identifier;
        ReportStep(cb, PullStep::Failed, 1, result.error);
        CleanupCorruptPartial(destPath);
        return result;
    }

    // Step 2: Verify
    ReportStep(cb, PullStep::Verifying, 2, "Computing SHA256...");
    std::string sha256 = DownloadManager::ComputeSHA256(destPath);

    // Step 2b: GGUF validation (if it's a .gguf file)
    std::string ggufArch;
    if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".gguf") {
        std::string ggufErr;
        ModelEntry tmpEntry;
        if (ValidateGGUF(destPath, tmpEntry, ggufErr)) {
            ggufArch = tmpEntry.architecture;
            if (!ggufArch.empty()) {
                std::cout << "GGUF validated: arch=" << ggufArch << "\n";
            }
        } else {
            result.error = "GGUF validation failed: " + ggufErr;
            ReportStep(cb, PullStep::Failed, 2, result.error);
            return result;
        }
    }

    // Step 3: Register
    ReportStep(cb, PullStep::Registering, 3, "Registering model...");

    uint64_t fileSize = std::filesystem::file_size(destPath, ec);
    std::string quant = HuggingFaceClient::ExtractQuantization(filename);
    std::string modelName = filename;
    auto dotPos = modelName.rfind(".gguf");
    if (dotPos != std::string::npos) modelName = modelName.substr(0, dotPos);

    ModelEntry entry;
    entry.id           = ModelIndex::GenerateId(modelName, quant);
    entry.name         = modelName;
    entry.quantization = quant;
    entry.path         = std::filesystem::relative(destPath, m_index.GetModelsBasePath()).string();
    entry.absolutePath = std::filesystem::absolute(destPath).string();
    entry.sizeBytes    = fileSize;
    entry.sha256       = sha256;
    entry.source       = src.identifier;
    entry.downloadedAt = ModelIndex::NowISO8601();
    entry.architecture = ggufArch;

    m_index.AddModel(entry);

    result.success   = true;
    result.modelId   = entry.id;
    result.filePath  = entry.absolutePath;
    result.sizeBytes = fileSize;
    result.sha256    = sha256;

    ReportStep(cb, PullStep::Complete, 3, "Done.");
    std::cout << "\nModel ready. Use: rawrxd run " << entry.id << "\n";

    return result;
}

// ============================================================================
// PullFromLocal — register an existing file
// ============================================================================
PullResult ModelPuller::PullFromLocal(const ModelSource& src, PullStatusCallback cb) {
    PullResult result;

    if (!std::filesystem::exists(src.identifier)) {
        result.error = "File not found: " + src.identifier;
        ReportStep(cb, PullStep::Failed, 1, result.error);
        return result;
    }

    ReportStep(cb, PullStep::Verifying, 1, "Computing SHA256...");
    std::string sha256 = DownloadManager::ComputeSHA256(src.identifier);

    // GGUF validation (if it's a .gguf file)
    std::string ggufArch;
    std::string filename = std::filesystem::path(src.identifier).filename().string();
    if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".gguf") {
        std::string ggufErr;
        ModelEntry tmpEntry;
        if (ValidateGGUF(src.identifier, tmpEntry, ggufErr)) {
            ggufArch = tmpEntry.architecture;
            if (!ggufArch.empty()) {
                std::cout << "GGUF validated: arch=" << ggufArch << "\n";
            }
        } else {
            result.error = "GGUF validation failed: " + ggufErr;
            ReportStep(cb, PullStep::Failed, 1, result.error);
            return result;
        }
    }

    ReportStep(cb, PullStep::Registering, 2, "Registering model...");

    std::error_code ec;
    uint64_t fileSize = std::filesystem::file_size(src.identifier, ec);
    std::string quant = HuggingFaceClient::ExtractQuantization(filename);
    std::string modelName = filename;
    auto dotPos = modelName.rfind(".gguf");
    if (dotPos != std::string::npos) modelName = modelName.substr(0, dotPos);

    ModelEntry entry;
    entry.id           = ModelIndex::GenerateId(modelName, quant);
    entry.name         = modelName;
    entry.quantization = quant;
    entry.absolutePath = std::filesystem::absolute(src.identifier).string();
    entry.path         = entry.absolutePath; // local: absolute path
    entry.sizeBytes    = fileSize;
    entry.sha256       = sha256;
    entry.source       = "local://" + entry.absolutePath;
    entry.downloadedAt = ModelIndex::NowISO8601();
    entry.architecture = ggufArch;

    m_index.AddModel(entry);

    result.success   = true;
    result.modelId   = entry.id;
    result.filePath  = entry.absolutePath;
    result.sizeBytes = fileSize;
    result.sha256    = sha256;

    ReportStep(cb, PullStep::Complete, 2, "Done.");
    return result;
}

// ============================================================================
// ListQuantizations
// ============================================================================
std::vector<HFFileInfo> ModelPuller::ListQuantizations(const std::string& repoId) {
    std::vector<HFFileInfo> files;
    m_hfClient.ListGGUFFiles(repoId, files);
    return files;
}

// ============================================================================
// Search
// ============================================================================
std::vector<HFRepoInfo> ModelPuller::Search(const std::string& query, int limit) {
    std::vector<HFRepoInfo> results;
    m_hfClient.SearchModels(query, results, limit);
    return results;
}

// ============================================================================
// Cancel
// ============================================================================
void ModelPuller::Cancel() {
    m_hfClient.Cancel();
    m_ollamaClient.Cancel();
    m_urlDownloader.Cancel();
}

// ============================================================================
// Local model management
// ============================================================================
bool ModelPuller::RegisterLocalModel(const std::string& filePath,
                                      const std::string& name,
                                      const std::string& tags) {
    ModelSource src;
    src.type = ModelSource::LOCAL;
    src.identifier = filePath;

    PullResult r = PullFromLocal(src, nullptr);
    if (r.success && !tags.empty()) {
        ModelEntry entry;
        if (m_index.GetModel(r.modelId, entry)) {
            entry.tags = tags;
            if (!name.empty()) entry.name = name;
            m_index.UpdateModel(entry);
        }
    }
    return r.success;
}

bool ModelPuller::RemoveModel(const std::string& id, bool deleteFile) {
    if (deleteFile) {
        ModelEntry entry;
        if (m_index.GetModel(id, entry) && !entry.absolutePath.empty()) {
            std::error_code ec;
            std::filesystem::remove(entry.absolutePath, ec);
        }
    }
    return m_index.RemoveModel(id);
}

std::vector<ModelEntry> ModelPuller::ListLocalModels() {
    return m_index.GetAllModels();
}

std::vector<ModelEntry> ModelPuller::SearchLocalModels(const std::string& query) {
    return m_index.Search(query);
}

bool ModelPuller::SetActiveModel(const std::string& id) {
    return m_index.SetActive(id);
}

void ModelPuller::SetHFToken(const std::string& token) {
    m_hfClient.SetToken(token);
}

void ModelPuller::SetModelsBasePath(const std::string& path) {
    m_index.Initialize(path);
}

// ============================================================================
// Step reporting helper
// ============================================================================
void ModelPuller::ReportStep(PullStatusCallback& cb, PullStep step, int num, const std::string& desc) {
    if (cb) {
        PullStatus s;
        s.step = step;
        s.stepNumber = num;
        s.totalSteps = 4;
        s.stepDescription = desc;
        cb(s);
    }
}

// ============================================================================
// Auto-scan & register — discover untracked GGUF files
// ============================================================================
int ModelPuller::AutoScanAndRegister() {
    return AutoScanDirectory(m_index.GetModelsBasePath());
}

int ModelPuller::AutoScanDirectory(const std::string& dirPath) {
    int registered = 0;

    if (dirPath.empty()) return 0;

    std::error_code ec;
    if (!std::filesystem::exists(dirPath, ec) || ec) return 0;

    try {
        for (auto& entry : std::filesystem::recursive_directory_iterator(
                 dirPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".gguf") continue;

            std::string absPath = std::filesystem::absolute(entry.path()).string();

            // Skip if already registered
            ModelEntry existing;
            if (m_index.GetModelByPath(absPath, existing)) continue;

            // Register it
            std::string filename = entry.path().filename().string();
            std::string quant = HuggingFaceClient::ExtractQuantization(filename);
            std::string modelName = filename;
            auto dotPos = modelName.rfind(".gguf");
            if (dotPos != std::string::npos) modelName = modelName.substr(0, dotPos);

            ModelEntry newEntry;
            newEntry.id           = ModelIndex::GenerateId(modelName, quant);
            newEntry.name         = modelName;
            newEntry.quantization = quant;
            newEntry.absolutePath = absPath;
            newEntry.path         = std::filesystem::relative(absPath, m_index.GetModelsBasePath()).string();
            newEntry.sizeBytes    = std::filesystem::file_size(entry.path(), ec);
            newEntry.source       = "auto-scan://" + absPath;
            newEntry.downloadedAt = ModelIndex::NowISO8601();

            // GGUF validation to extract architecture
            std::string ggufErr;
            ModelEntry tmpEntry;
            if (ValidateGGUF(absPath, tmpEntry, ggufErr)) {
                newEntry.architecture = tmpEntry.architecture;
            }

            m_index.AddModel(newEntry);
            ++registered;

            std::cout << "[auto-scan] Registered: " << modelName << " (" << absPath << ")\n";
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Skip directories we can't access
    }

    return registered;
}

} // namespace RawrXD
