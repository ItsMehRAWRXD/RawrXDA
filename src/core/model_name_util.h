// ============================================================================
// model_name_util.h — Automatic Model Name Derivation
// ============================================================================
// Ensures loaded models are READ correctly as API-valid model names.
// Use this whenever deriving a display/API name from a model path or filename.
// Handles: BigDaddyG-F32-FROM-Q4.gguf -> BigDaddyG-F32-FROM-Q4
// ============================================================================

#pragma once

#include <string>
#include <algorithm>
#include <cctype>

namespace RawrXD {

/// Derive a canonical API model name from a path or raw name.
/// - "D:\\OllamaModels\\BigDaddyG-F32-FROM-Q4.gguf" -> "BigDaddyG-F32-FROM-Q4"
/// - "BigDaddyG-F32-FROM-Q4.gguf" -> "BigDaddyG-F32-FROM-Q4"
/// - "BigDaddyG-F32-FROM-Q4" -> "BigDaddyG-F32-FROM-Q4" (passthrough)
inline std::string DeriveModelNameFromPath(const std::string& pathOrName) {
    if (pathOrName.empty()) return "rawrxd";
    std::string name = pathOrName;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.empty()) return "rawrxd";
    // Strip .gguf (case-insensitive) first — common model extension
    if (name.size() >= 5) {
        std::string ext = name.substr(name.size() - 5);
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".gguf") name = name.substr(0, name.size() - 5);
    }
    // Fallback: strip from first dot (e.g. model.bin, model.safetensors)
    size_t dot = name.find('.');
    if (dot != std::string::npos && dot > 0) name = name.substr(0, dot);
    if (name.empty()) return "rawrxd";
    return name;
}

} // namespace RawrXD
