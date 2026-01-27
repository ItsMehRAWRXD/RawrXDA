#pragma once

#include <string>
#include <memory>
#include <vector>

// Forward declarations
class StreamingGGUFLoader;
class GGUFLoader;
struct GGUFMetadata;

/**
 * @brief Centralized GGUF model loader.
 *
 * Handles all GGUF file parsing, metadata extraction, and tensor indexing.
 * Replaces inline GGUF loading logic in Win32IDE to improve modularity.
 */
class ModelLoader
{
public:
    ModelLoader();
    ~ModelLoader();

    /**
     * Load a GGUF model from the specified file path.
     * @param filepath Absolute path to the .gguf file.
     * @param useStreaming If true, use StreamingGGUFLoader; otherwise, use GGUFLoader.
     * @return true on success, false on error.
     */
    [[nodiscard]] bool loadModel(const std::string& filepath, bool useStreaming = true);

    /**
     * Close the currently loaded model and free resources.
     */
    void closeModel();

    /**
     * Get metadata from the loaded model.
     * @return Metadata structure or empty if no model loaded.
     */
    [[nodiscard]] GGUFMetadata getMetadata() const;

    /**
     * Get the current memory usage (in bytes) for the loaded model.
     */
    [[nodiscard]] size_t getCurrentMemoryUsage() const;

    /**
     * Check if a model is currently loaded.
     */
    [[nodiscard]] bool isModelLoaded() const;

    /**
     * Get the path of the currently loaded model.
     */
    [[nodiscard]] std::string getLoadedModelPath() const;

    /**
     * List all available tensors in the loaded model.
     */
    [[nodiscard]] std::vector<std::string> getAvailableTensors() const;

    /**
     * Get tensor shape by name.
     */
    [[nodiscard]] std::vector<size_t> getTensorShape(const std::string& tensorName) const;

    /**
     * Pre-load a specific zone into memory.
     */
    bool preloadZone(const std::string& zoneName);

    /**
     * Unload a specific zone from memory.
     */
    void unloadZone(const std::string& zoneName);

private:
    std::unique_ptr<StreamingGGUFLoader> m_streamingLoader = nullptr;
    std::string m_loadedModelPath;
    bool m_usingStreaming = false;
    bool m_isLoaded = false;
    size_t m_totalMemoryUsage = 0;
};
