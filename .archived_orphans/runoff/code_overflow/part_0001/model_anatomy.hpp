#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

// Use canonical GGUF types from gguf_loader (no redefinition).
#include "../gguf_loader.h"

namespace RawrXD {

/// Single tensor entry in the anatomy manifest (for hotpatch/IDE visibility).
struct TensorEntry {
    std::string name;
    uint32_t    tensorType = 0;   // GGMLType as int
    int         category = 0;     // Embedding=0, AttnQ=1, AttnK=2, AttnV=3, AttnO=4, FFNGate=5, FFNUp=6, FFNDown=7, Norm=8, Output=9, Misc=10
    int         layerId = -1;     // -1 for shared/embedding
    uint64_t    byteOffset = 0;
    uint64_t    byteSize = 0;
    uint64_t    elementCount = 0;
    std::vector<uint64_t> shape;
};

/// Full model anatomy: header + tensor manifest (load once, then tune/hotpatch).
struct ModelAnatomy {
    std::string modelName;
    uint32_t    ggufVersion = 0;
    uint64_t    tensorCount = 0;
    uint64_t    metadataKvCount = 0;
    uint64_t    totalParams = 0;  // Approx from tensor sizes
    std::vector<TensorEntry> tensors;
};

/// Build anatomy from an open GGUF loader (after Open + ParseHeader + ParseMetadata).
/// Streams progress to optional ostream (e.g. stdout) when streamOut is non-null.
bool BuildAnatomyFromLoader(
    void* loader,  // IGGUFLoader* or GGUFLoader*
    ModelAnatomy& out,
    std::ostream* streamOut = nullptr
);

/// Classify tensor name into category and optional layer id.
void ClassifyTensor(const std::string& name, int& category, int& layerId);

/// Export anatomy to JSON (one line or pretty).
std::string ExportAnatomyToJson(const ModelAnatomy& a, bool pretty = true);

} // namespace RawrXD
