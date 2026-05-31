#pragma once
// ============================================================================
// model_puller.h — Unified Model Puller API
// ============================================================================
// High-level API used by both CLI and GUI to pull models.
// Parses user input, routes to HF / Ollama / URL / Local sources,
// manages download + verification + registry.
// ============================================================================

#ifndef RAWRXD_MODEL_PULLER_H
#define RAWRXD_MODEL_PULLER_H

#include "model_puller/download_manager.h"
#include "model_puller/hf_client.h"
#include "model_puller/ollama_registry.h"
#include "model_puller/model_index.h"

#include <string>
#include <vector>
#include <functional>
#include <mutex>

namespace RawrXD {

// -------------------------------------------------------
// Model source type detection
// -------------------------------------------------------
struct ModelSource {
    enum Type {
        HUGGING_FACE,  // "bartowski/Qwen2.5-Coder-32B-Instruct-GGUF"
        OLLAMA,        // "llama3.2:3b"
        URL,           // "https://..."
        LOCAL          // "C:\models\model.gguf"
    };

    Type        type       = URL;
    std::string identifier;     // repo id or URL
    std::string filename;       // specific file (HF) or empty
    std::string quantization;   // "Q4_K_M" hint (from :quant suffix)
    std::string tag;            // Ollama tag
};

// -------------------------------------------------------
// Pull result
// -------------------------------------------------------
struct PullResult {
    bool        success = false;
    std::string modelId;        // registry ID
    std::string filePath;       // absolute path to GGUF
    std::string error;
    uint64_t    sizeBytes = 0;
    std::string sha256;
};

// -------------------------------------------------------
// Pull step reporting
// -------------------------------------------------------
enum class PullStep {
    Parsing,
    FetchingFileList,
    ResolvingQuantization,
    Downloading,
    Verifying,
    Registering,
    Complete,
    Failed
};

struct PullStatus {
    PullStep step = PullStep::Parsing;
    int      stepNumber = 0;
    int      totalSteps = 4;
    std::string stepDescription;
    DownloadProgress downloadProgress;
};

using PullStatusCallback = std::function<void(const PullStatus&)>;

// -------------------------------------------------------
// ModelPuller — unified high-level API
// -------------------------------------------------------
class ModelPuller {
public:
    static ModelPuller& Instance();

    // ---- Parse user input into ModelSource ----
    ModelSource Parse(const std::string& input) const;

    // ---- Pull a model (blocking) ----
    PullResult Pull(const std::string& input,
                    PullStatusCallback onStatus = nullptr);

    PullResult Pull(const ModelSource& source,
                    PullStatusCallback onStatus = nullptr);

    // ---- List quantizations for a HF repo ----
    std::vector<HFFileInfo> ListQuantizations(const std::string& repoId);

    // ---- Search HF for models ----
    std::vector<HFRepoInfo> Search(const std::string& query, int limit = 20);

    // ---- Cancel any active download ----
    void Cancel();

    // ---- Local model management ----
    bool RegisterLocalModel(const std::string& filePath,
                            const std::string& name = "",
                            const std::string& tags = "");

    bool RemoveModel(const std::string& id, bool deleteFile = false);

    std::vector<ModelEntry> ListLocalModels();
    std::vector<ModelEntry> SearchLocalModels(const std::string& query);

    bool SetActiveModel(const std::string& id);

    // ---- Config ----
    void SetHFToken(const std::string& token);
    void SetModelsBasePath(const std::string& path);
    ModelIndex& GetIndex() { return m_index; }

    // ---- Auto-scan: discover and register untracked GGUF files ----
    int AutoScanAndRegister();
    int AutoScanDirectory(const std::string& dirPath);

private:
    ModelPuller();
    ~ModelPuller();
    ModelPuller(const ModelPuller&) = delete;
    ModelPuller& operator=(const ModelPuller&) = delete;

    PullResult PullFromHF(const ModelSource& src, PullStatusCallback cb);
    PullResult PullFromOllama(const ModelSource& src, PullStatusCallback cb);
    PullResult PullFromURL(const ModelSource& src, PullStatusCallback cb);
    PullResult PullFromLocal(const ModelSource& src, PullStatusCallback cb);

    void ReportStep(PullStatusCallback& cb, PullStep step, int num, const std::string& desc);

    HuggingFaceClient      m_hfClient;
    OllamaRegistryClient   m_ollamaClient;
    DownloadManager        m_urlDownloader;
    ModelIndex             m_index;
    std::mutex             m_pullMutex;
};

} // namespace RawrXD

#endif // RAWRXD_MODEL_PULLER_H
