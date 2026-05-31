// ============================================================================
// ollama_registry.cpp — Ollama Registry Client Implementation
// ============================================================================
// Direct Ollama registry pull using Docker Registry v2 protocol.
// No `ollama` CLI dependency — pure WinHTTP.
// ============================================================================

#include "model_puller/ollama_registry.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>
#include <filesystem>

using json = nlohmann::json;

namespace RawrXD {

static const char* OLLAMA_REGISTRY = "https://registry.ollama.ai";

// ============================================================================
// OllamaModelRef
// ============================================================================
OllamaModelRef OllamaModelRef::Parse(const std::string& input) {
    OllamaModelRef ref;
    ref.ns  = "library";
    ref.tag = "latest";

    std::string work = input;

    // Check for namespace/model format
    auto slashPos = work.find('/');
    if (slashPos != std::string::npos) {
        ref.ns    = work.substr(0, slashPos);
        work      = work.substr(slashPos + 1);
    }

    // Check for model:tag format
    auto colonPos = work.find(':');
    if (colonPos != std::string::npos) {
        ref.model = work.substr(0, colonPos);
        ref.tag   = work.substr(colonPos + 1);
    } else {
        ref.model = work;
    }

    return ref;
}

std::string OllamaModelRef::ManifestUrl() const {
    // GET https://registry.ollama.ai/v2/{ns}/{model}/manifests/{tag}
    return std::string(OLLAMA_REGISTRY) + "/v2/" + ns + "/" + model + "/manifests/" + tag;
}

std::string OllamaModelRef::BlobUrl(const std::string& digest) const {
    // GET https://registry.ollama.ai/v2/{ns}/{model}/blobs/{digest}
    return std::string(OLLAMA_REGISTRY) + "/v2/" + ns + "/" + model + "/blobs/" + digest;
}

// ============================================================================
// OllamaManifest
// ============================================================================
const OllamaLayer* OllamaManifest::FindModelLayer() const {
    // The model (GGUF) layer has mediaType "application/vnd.ollama.image.model"
    for (auto& layer : layers) {
        if (layer.mediaType == "application/vnd.ollama.image.model") {
            return &layer;
        }
    }
    // Fallback: largest layer is usually the model
    const OllamaLayer* largest = nullptr;
    for (auto& layer : layers) {
        if (!largest || layer.sizeBytes > largest->sizeBytes) {
            largest = &layer;
        }
    }
    return largest;
}

// ============================================================================
// OllamaRegistryClient
// ============================================================================
OllamaRegistryClient::OllamaRegistryClient()  = default;
OllamaRegistryClient::~OllamaRegistryClient() = default;

// ---------------------------------------------------------------------------
// GetManifest
// ---------------------------------------------------------------------------
bool OllamaRegistryClient::GetManifest(const std::string& modelSpec,
                                         OllamaManifest& manifestOut) {
    manifestOut = {};
    OllamaModelRef ref = OllamaModelRef::Parse(modelSpec);
    std::string url = ref.ManifestUrl();

    std::string body;
    if (!m_downloader.FetchJSON(url, body)) {
        std::cerr << "[Ollama] Failed to fetch manifest for " << modelSpec << "\n";
        return false;
    }

    try {
        json j = json::parse(body);

        manifestOut.schemaVersion = j.value("schemaVersion", "");
        manifestOut.mediaType     = j.value("mediaType", "");

        if (j.contains("layers") && j["layers"].is_array()) {
            for (auto& lj : j["layers"]) {
                OllamaLayer layer;
                layer.digest    = lj.value("digest", "");
                layer.mediaType = lj.value("mediaType", "");
                layer.sizeBytes = lj.value("size", uint64_t(0));
                manifestOut.layers.push_back(std::move(layer));
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Ollama] Manifest parse error: " << e.what() << "\n";
        return false;
    }

    return !manifestOut.layers.empty();
}

// ---------------------------------------------------------------------------
// Pull — download the GGUF blob
// ---------------------------------------------------------------------------
bool OllamaRegistryClient::Pull(const std::string& modelSpec,
                                  const std::string& destDir,
                                  std::string& outFilePath,
                                  ProgressCallback onProgress,
                                  bool resume) {
    outFilePath.clear();

    // 1. Fetch manifest
    OllamaManifest manifest;
    if (!GetManifest(modelSpec, manifest)) {
        return false;
    }

    // 2. Find the model layer
    const OllamaLayer* modelLayer = manifest.FindModelLayer();
    if (!modelLayer) {
        std::cerr << "[Ollama] No model layer found in manifest\n";
        return false;
    }

    // 3. Build blob download URL
    OllamaModelRef ref = OllamaModelRef::Parse(modelSpec);
    std::string blobUrl = ref.BlobUrl(modelLayer->digest);

    // 4. Build destination path
    //    ollama/{model}_{tag}/model.gguf
    std::string safeName = ref.model;
    for (auto& c : safeName) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    std::string safeTag = ref.tag;
    for (auto& c : safeTag) {
        if (c == '/' || c == '\\' || c == ':') c = '_';
    }

    std::filesystem::path dirPath = std::filesystem::path(destDir) / "native" / (safeName + "_" + safeTag);
    std::error_code ec;
    std::filesystem::create_directories(dirPath, ec);

    outFilePath = (dirPath / "model.gguf").string();

    // 5. Download
    return m_downloader.Download(blobUrl, outFilePath, onProgress, resume);
}

// ---------------------------------------------------------------------------
// GetModelSize
// ---------------------------------------------------------------------------
bool OllamaRegistryClient::GetModelSize(const std::string& modelSpec, uint64_t& sizeOut) {
    sizeOut = 0;
    OllamaManifest manifest;
    if (!GetManifest(modelSpec, manifest)) return false;

    const OllamaLayer* layer = manifest.FindModelLayer();
    if (!layer) return false;

    sizeOut = layer->sizeBytes;
    return sizeOut > 0;
}

void OllamaRegistryClient::Cancel() {
    m_downloader.Cancel();
}

} // namespace RawrXD
