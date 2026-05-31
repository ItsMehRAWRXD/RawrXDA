// ============================================================================
// rawrxd_model_registry.h — Zero-dependency GGUF model scanner & registry
// ============================================================================
// Scans configured directories for .gguf files, parses headers for model
// metadata (arch, quant, param count, context length), and provides a
// queryable registry for the serve / list / rm commands.
// ============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace RawrXD {
namespace Serve {

// ============================================================================
// ModelEntry — one discovered .gguf file
// ============================================================================
struct ModelEntry {
    std::string name;           // display name  e.g. "phi3:mini-q4_0"
    std::string path;           // full filesystem path
    std::string architecture;   // e.g. "llama", "phi3", "mistral"
    std::string quantization;   // e.g. "Q4_0", "Q8_0", "F16"
    uint64_t    fileSizeBytes = 0;
    uint64_t    paramCount    = 0;
    uint32_t    contextLength = 0;
    uint32_t    vocabSize     = 0;
    uint64_t    modifiedAt    = 0; // FILETIME as epoch seconds
};

// ============================================================================
// ModelRegistry
// ============================================================================
class ModelRegistry {
public:
    ModelRegistry();
    ~ModelRegistry();

    // Add a directory to scan for .gguf files
    void addSearchPath(const std::string& dir);

    // Scan all search paths; returns number of models found
    size_t scan();

    // Look up by name (case-insensitive prefix match, Ollama-style)
    const ModelEntry* find(const std::string& nameOrPath) const;

    // List all discovered models
    const std::vector<ModelEntry>& models() const { return m_models; }

    // Remove a model file from disk + registry; returns true on success
    bool remove(const std::string& nameOrPath);

    // Default model directory (~/.rawrxd/models/ or %USERPROFILE%\.rawrxd\models)
    static std::string defaultModelDir();

private:
    void scanDirectory(const std::string& dir);
    bool probeGGUFHeader(const std::string& path, ModelEntry& out);
    std::string inferName(const std::string& filename) const;
    std::string inferQuant(const std::string& filename) const;

    std::vector<std::string>  m_searchPaths;
    std::vector<ModelEntry>   m_models;
    mutable std::mutex        m_mu;
};

} // namespace Serve
} // namespace RawrXD
