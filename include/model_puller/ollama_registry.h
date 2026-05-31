#pragma once
// ============================================================================
// ollama_registry.h — Ollama Registry Client (Docker Registry v2)
// ============================================================================
// Pulls models directly from registry.ollama.ai using the Docker Registry
// v2 API. Fetches manifests, identifies the GGUF blob layer, downloads it.
//   - No dependency on `ollama` CLI
//   - Resume-capable
//   - Extracts the GGUF model layer from OCI manifests
// ============================================================================

#ifndef RAWRXD_OLLAMA_REGISTRY_H
#define RAWRXD_OLLAMA_REGISTRY_H

#include "model_puller/download_manager.h"
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {

// -------------------------------------------------------
// Ollama manifest layer info
// -------------------------------------------------------
struct OllamaLayer {
    std::string digest;       // "sha256:abc123..."
    std::string mediaType;    // "application/vnd.ollama.image.model"
    uint64_t    sizeBytes = 0;
};

struct OllamaManifest {
    std::string schemaVersion;
    std::string mediaType;
    std::vector<OllamaLayer> layers;

    // Convenience: find the model (GGUF) layer
    const OllamaLayer* FindModelLayer() const;
};

// -------------------------------------------------------
// Parsed model reference: "llama3.2:3b" → namespace="library", model="llama3.2", tag="3b"
// -------------------------------------------------------
struct OllamaModelRef {
    std::string ns;    // "library" by default
    std::string model;
    std::string tag;   // "latest" by default

    static OllamaModelRef Parse(const std::string& input);
    std::string ManifestUrl() const;
    std::string BlobUrl(const std::string& digest) const;
};

// -------------------------------------------------------
// OllamaRegistryClient
// -------------------------------------------------------
class OllamaRegistryClient {
public:
    OllamaRegistryClient();
    ~OllamaRegistryClient();

    // Fetch the manifest for a model (e.g. "llama3.2:3b")
    bool GetManifest(const std::string& modelSpec, OllamaManifest& manifestOut);

    // Pull the model GGUF blob to destDir
    //   Returns the final file path on success
    bool Pull(const std::string& modelSpec,
              const std::string& destDir,
              std::string& outFilePath,
              ProgressCallback onProgress = nullptr,
              bool resume = true);

    // Cancel active download
    void Cancel();

    // Get the expected model size without downloading
    bool GetModelSize(const std::string& modelSpec, uint64_t& sizeOut);

private:
    DownloadManager m_downloader;
};

} // namespace RawrXD

#endif // RAWRXD_OLLAMA_REGISTRY_H
