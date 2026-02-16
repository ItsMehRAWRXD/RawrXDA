/**
 * @file model_name_utils.h
 * @brief Automatic model name normalizer and resolver — ensures loaded models are READ correctly.
 *
 * Handles names like "BigDaddyG-F32-FROM-Q4" that may be rejected by strict validation.
 * Provides:
 *   - Derive model name from GGUF path (filename stem)
 *   - Normalize for API (alphanumeric, hyphens, underscores, colons, dots)
 *   - Fuzzy/case-insensitive match against known model list (Ollama manifest)
 *   - Validation that accepts valid model names (no over-strict format rejection)
 */

#pragma once

#include <string>
#include <vector>

namespace RawrXD {

/**
 * Model name utilities for automatic naming and resolution.
 */
class ModelNameUtils {
public:
    /**
     * Derive a canonical model name from a GGUF file path.
     * e.g. "C:/Models/BigDaddyG-F32-FROM-Q4.gguf" -> "BigDaddyG-F32-FROM-Q4"
     */
    static std::string deriveFromPath(const std::string& path);

    /**
     * Normalize a model name for API use.
     * Keeps alphanumeric, hyphens, underscores, colons, dots; trims whitespace.
     * Returns empty if invalid (empty, only whitespace, or disallowed chars).
     */
    static std::string normalize(const std::string& name);

    /**
     * Check if a model name is valid for API use.
     * Accepts alphanumeric, hyphens, underscores, colons, dots.
     */
    static bool isValid(const std::string& name);

    /**
     * Resolve display/input name to the exact API name used by Ollama.
     * If the name exists in knownModels (exact match), returns it as-is.
     * If case-insensitive match exists in knownModels, returns the exact known name.
     * Otherwise returns normalized input (so we try it anyway).
     */
    static std::string resolveToApiName(const std::string& input,
                                        const std::vector<std::string>& knownModels);

    /**
     * Get well-known BigDaddyG and related model name variants for fallback lists.
     * Includes common quant suffixes (Q4, Q4_K_M, F32, FROM-Q4, etc.).
     */
    static std::vector<std::string> getBigDaddyGVariants();

    /**
     * Add model names derived from filesystem scan (Ollama models dir, etc.)
     * to an existing list, avoiding duplicates.
     */
    static void addDerivedNamesFromDirs(const std::vector<std::string>& dirPaths,
                                        std::vector<std::string>& outNames);

private:
    static bool charAllowed(char c);
    static std::string toLower(const std::string& s);
};

} // namespace RawrXD
