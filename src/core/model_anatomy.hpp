#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace RawrXD
{

struct TensorEntry
{
    std::string name;
    uint32_t tensorType{0};  // ggml_type raw
    int category{10};         // see kCategoryNames in .cpp
    int layerId{-1};
    uint64_t byteOffset{0};   // absolute file offset to payload
    uint64_t byteSize{0};
    uint64_t elementCount{0};
    std::vector<uint64_t> shape;
};

struct ModelAnatomy
{
    std::string modelPath;
    std::string modelName;
    uint32_t ggufVersion{0};
    uint64_t tensorCount{0};
    uint64_t totalParams{0};
    std::vector<TensorEntry> tensors;
};

void ClassifyTensor(const std::string& name, int& category, int& layerId);

/// Stub: optional IGGUFLoader integration when available.
bool BuildAnatomyFromLoader(void* loader, ModelAnatomy& out, std::ostream* streamOut);

/// Parse GGUF tensor table from disk (metadata only; uses aligned tensor-data base offsets).
bool BuildAnatomyFromGgufPath(const std::string& filePath, ModelAnatomy& out, std::ostream* streamOut,
                              std::string* errorOut = nullptr);

std::string ExportAnatomyToJson(const ModelAnatomy& a, bool pretty);

}  // namespace RawrXD
