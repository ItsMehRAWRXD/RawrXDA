#include "gguf_parser.h"
#include "gguf_loader.h"
#include <stdexcept>

namespace RawrXD {

Expected<ParsedGGUFModel, std::string> GGUFParser::parse(const std::string& path) {
    GGUFLoader loader;
    if (!loader.Open(path)) {
        return Unexpected(std::string("Failed to open file: ") + path);
    }
    
    if (!loader.ParseHeader()) { // Removed ParseMetadata check here as it's separate in loader logic usually, or implicitly called?
        return Unexpected(std::string("Failed to parse GGUF header"));
    }
    
    // Explicitly parse metadata to access vocab
    if (!loader.ParseMetadata()) {
        return Unexpected(std::string("Failed to parse GGUF metadata"));
    }
    
    ParsedGGUFModel model;
    GGUFMetadata meta = loader.GetMetadata();
    
    model.vocab = meta.tokens;
    model.token_scores = meta.token_scores;
    model.token_types = meta.token_types;
    
    // Retrieve Tensor Info to populate metadata
    std::vector<TensorInfo> infos = loader.GetTensorInfo();
    if (infos.empty()) {
        // Some models might theoretically be empty or metadata-only, but usually this is an error for inference
        // We'll proceed but totalSize will be 0.
    }
    
    // Load all tensors tightly packed into the weights buffer
    // GGUFLoader::LoadTensorRange handles the file reading and packing
    if (!loader.LoadTensorRange(0, infos.size(), model.weights)) {
        return Unexpected(std::string("Failed to load tensor data"));
    }
    
    // Calculate total size and populate internal metadata with IN-MEMORY offsets
    model.totalSize = model.weights.size();
    
    size_t currentOffset = 0;
    for (const auto& info : infos) {
        ParsedGGUFModel::TensorMetadata tensorMeta;
        tensorMeta.name = info.name;
        tensorMeta.offset = currentOffset;
        tensorMeta.size = info.size_bytes; // Use size_bytes as per GGUFLoader logic
        tensorMeta.shape = info.shape;
        tensorMeta.type = (int)info.type;
        
        model.tensors.push_back(tensorMeta);
        
        currentOffset += info.size_bytes;
    }
    
    loader.Close();
    return model;
}

}
