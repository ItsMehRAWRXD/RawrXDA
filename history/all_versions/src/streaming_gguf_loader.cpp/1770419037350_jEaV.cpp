#include "streaming_gguf_loader.h"
#include "model_loader/GGUFConstants.hpp"
#include "utils/Diagnostics.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace RawrXD {

StreamingGGUFLoader::StreamingGGUFLoader()
    : is_open_(false), current_zone_memory_(0), max_zone_memory_mb_(GGUFConstants::DEFAULT_ZONE_MEMORY_MB) {
    std::memset(&header_, 0, sizeof(GGUFHeader));
}

StreamingGGUFLoader::~StreamingGGUFLoader() {
    Close();
}

bool StreamingGGUFLoader::Open(const std::string& filepath) {
    filepath_ = filepath;
    file_.open(filepath, std::ios::binary);
    if (!file_.is_open()) {
        std::cerr << "❌ Failed to open GGUF file: " << filepath << std::endl;
        return false;
    }
    
    is_open_ = true;
    
    // Parse header first
    if (!ParseHeader()) {
        Close();
        return false;
    }
    
    // Parse metadata
    if (!ParseMetadata()) {
        Close();
        return false;
    }
    
    // Build tensor index (no data loaded yet!)
    if (!BuildTensorIndex()) {
        Close();
        return false;
    }
    
    // Assign tensors to zones
    AssignTensorsToZones();
    
    std::cout << "✅ GGUF Model opened in streaming mode" << std::endl;
    std::cout << "   File: " << filepath << std::endl;
    std::cout << "   Tensors: " << tensor_index_.size() << std::endl;
    std::cout << "   Zones: " << zones_.size() << std::endl;
    std::cout << "   Memory (header+index): ~" << ((tensor_index_.size() * 100) / (1024*1024)) << " MB" << std::endl;
    
    return true;
}

bool StreamingGGUFLoader::Close() {
    if (file_.is_open()) {
        file_.close();
    }
    is_open_ = false;
    tensor_index_.clear();
    zones_.clear();
    active_zones_.clear();
    current_zone_ = "";
    return true;
}

bool StreamingGGUFLoader::ParseHeader() {
    if (!file_.is_open()) return false;
    
    file_.seekg(0);
    
    // Read magic
    if (!ReadValue(header_.magic)) return false;
    if (header_.magic != GGUFConstants::GGUF_MAGIC) {
        std::cerr << "❌ Invalid GGUF magic: 0x" << std::hex << header_.magic << std::endl;
        Diagnostics::error("Invalid GGUF magic number", "StreamingGGUFLoader");
        return false;
    }
    
    // Read version
    if (!ReadValue(header_.version)) return false;
    if (header_.version != GGUFConstants::GGUF_VERSION) {
        std::cerr << "❌ Unsupported GGUF version: " << header_.version << std::endl;
        Diagnostics::error("Unsupported GGUF version: " + std::to_string(header_.version), "StreamingGGUFLoader");
        return false;
    }
    
    // Read tensor count
    if (!ReadValue(header_.tensor_count)) return false;
    
    // Read metadata KV count
    if (!ReadValue(header_.metadata_kv_count)) return false;
    
    // Calculate metadata offset
    header_.metadata_offset = file_.tellg();
    
    return true;
}

GGUFHeader StreamingGGUFLoader::GetHeader() const {
    return header_;
}

bool StreamingGGUFLoader::ParseMetadata() {
    if (!file_.is_open() || header_.metadata_kv_count == 0) {
        return false;
    }
    
    file_.seekg(header_.metadata_offset);
    
    for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
        std::string key, value;
        
        if (!ReadString(key)) {
            std::cerr << "❌ Failed to read metadata key at index " << i << std::endl;
            return false;
        }
        
        uint32_t value_type;
        if (!ReadValue(value_type)) {
            std::cerr << "❌ Failed to read metadata value type for key: " << key << std::endl;
            return false;
        }
        
        // ================================================================
        // GGUF v3 complete value type handling (all 13 types)
        // Ported from Qt streaming_gguf_loader_qt for full format support
        // ================================================================
        switch (value_type) {
            case GGUFConstants::GGUF_VALUE_TYPE_UINT8: {
                uint8_t val;
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = std::to_string(val);
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_INT8: {
                int8_t val;
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = std::to_string(val);
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_UINT16: {
                uint16_t val;
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = std::to_string(val);
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_INT16: {
                int16_t val;
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = std::to_string(val);
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_UINT32: {
                uint32_t val;
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = std::to_string(val);
                
                // Parse important numeric metadata
                if (key == GGUFConstants::META_LLAMA_BLOCK_COUNT) {
                    metadata_.layer_count = val;
                } else if (key == GGUFConstants::META_LLAMA_CONTEXT_LENGTH) {
                    metadata_.context_length = val;
                } else if (key == GGUFConstants::META_LLAMA_EMBEDDING_LENGTH) {
                    metadata_.embedding_dim = val;
                } else if (key == GGUFConstants::META_LLAMA_VOCAB_SIZE) {
                    metadata_.vocab_size = val;
                } else if (key == GGUFConstants::META_LLAMA_HEAD_COUNT) {
                    metadata_.head_count = val;
                } else if (key == GGUFConstants::META_LLAMA_HEAD_COUNT_KV) {
                    metadata_.head_count_kv = val;
                } else if (key == GGUFConstants::META_LLAMA_FFN_LENGTH) {
                    metadata_.feed_forward_length = val;
                }
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_INT32: {
                int32_t val;
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = std::to_string(val);
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_FLOAT32: {
                float val;
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = std::to_string(val);
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_BOOL: {
                uint8_t val;  // GGUF spec: bool is stored as uint8
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = val ? "true" : "false";
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_STRING: {
                if (!ReadString(value)) {
                    std::cerr << "❌ Failed to read metadata string value for key: " << key << std::endl;
                    return false;
                }
                metadata_.kv_pairs[key] = value;
                
                // Parse important string metadata
                if (key == GGUFConstants::META_GENERAL_ARCHITECTURE) {
                    if (value == "llama") metadata_.architecture_type = 1;
                    else if (value == "mistral") metadata_.architecture_type = 2;
                    else if (value == "phi") metadata_.architecture_type = 3;
                    else if (value == "gemma") metadata_.architecture_type = 4;
                    else if (value == "qwen2") metadata_.architecture_type = 5;
                    else if (value == "starcoder") metadata_.architecture_type = 6;
                }
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_ARRAY: {
                // Array type: element_type (uint32) + count (uint64) + elements
                uint32_t element_type;
                uint64_t arr_count;
                if (!ReadValue(element_type)) return false;
                if (!ReadValue(arr_count)) return false;
                
                // Parse array elements based on element type
                // For string arrays (tokenizer tokens), collect them
                bool is_token_array = (key == GGUFConstants::META_TOKENIZER_TOKENS);
                bool is_score_array = (key == GGUFConstants::META_TOKENIZER_SCORES);
                bool is_type_array = (key == GGUFConstants::META_TOKENIZER_TOKEN_TYPE);
                
                for (uint64_t a = 0; a < arr_count; ++a) {
                    switch (element_type) {
                        case GGUFConstants::GGUF_VALUE_TYPE_UINT8: {
                            uint8_t v; if (!ReadValue(v)) return false;
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_INT8: {
                            int8_t v; if (!ReadValue(v)) return false;
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_UINT16: {
                            uint16_t v; if (!ReadValue(v)) return false;
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_INT16: {
                            int16_t v; if (!ReadValue(v)) return false;
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_UINT32: {
                            uint32_t v; if (!ReadValue(v)) return false;
                            if (is_type_array) metadata_.token_types.push_back(v);
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_INT32: {
                            int32_t v; if (!ReadValue(v)) return false;
                            if (is_type_array) metadata_.token_types.push_back(static_cast<uint32_t>(v));
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_FLOAT32: {
                            float v; if (!ReadValue(v)) return false;
                            if (is_score_array) metadata_.token_scores.push_back(v);
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_BOOL: {
                            uint8_t v; if (!ReadValue(v)) return false;
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_STRING: {
                            std::string s;
                            if (!ReadString(s)) return false;
                            if (is_token_array) {
                                metadata_.tokens.push_back(s);
                                m_vocab.push_back(s);
                            }
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_UINT64: {
                            uint64_t v; if (!ReadValue(v)) return false;
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_INT64: {
                            int64_t v; if (!ReadValue(v)) return false;
                            break;
                        }
                        case GGUFConstants::GGUF_VALUE_TYPE_FLOAT64: {
                            double v; if (!ReadValue(v)) return false;
                            break;
                        }
                        default: {
                            std::cerr << "❌ Unknown array element type " << element_type 
                                      << " in key: " << key << std::endl;
                            return false;
                        }
                    }
                }
                
                metadata_.kv_pairs[key] = "[array:" + std::to_string(arr_count) + "]";
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_UINT64: {
                uint64_t val;
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = std::to_string(val);
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_INT64: {
                int64_t val;
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = std::to_string(val);
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_FLOAT64: {
                double val;
                if (!ReadValue(val)) return false;
                metadata_.kv_pairs[key] = std::to_string(val);
                break;
            }
            default: {
                std::cerr << "❌ Unknown metadata value type " << value_type 
                          << " for key: " << key << " at index " << i << std::endl;
                return false;
            }
        }
    }
    
    return true;
}

GGUFMetadata StreamingGGUFLoader::GetMetadata() const {
    return metadata_;
}

bool StreamingGGUFLoader::BuildTensorIndex() {
    if (!file_.is_open()) {
        return false;
    }
    
    // Skip metadata to get to tensor info
    file_.seekg(header_.metadata_offset);
    
    // Skip metadata entries — must handle ALL 13 GGUF v3 value types
    for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
        std::string key, value;
        if (!ReadString(key)) return false;
        
        uint32_t value_type;
        if (!ReadValue(value_type)) return false;
        
        switch (value_type) {
            case GGUFConstants::GGUF_VALUE_TYPE_UINT8:  { uint8_t v; if (!ReadValue(v)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_INT8:   { int8_t v; if (!ReadValue(v)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_UINT16: { uint16_t v; if (!ReadValue(v)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_INT16:  { int16_t v; if (!ReadValue(v)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_UINT32: { uint32_t v; if (!ReadValue(v)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_INT32:  { int32_t v; if (!ReadValue(v)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_FLOAT32:{ float v; if (!ReadValue(v)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_BOOL:   { uint8_t v; if (!ReadValue(v)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_STRING: { if (!ReadString(value)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_ARRAY: {
                // Must skip the entire array: element_type + count + elements
                uint32_t elem_type;
                uint64_t arr_count;
                if (!ReadValue(elem_type)) return false;
                if (!ReadValue(arr_count)) return false;
                
                for (uint64_t a = 0; a < arr_count; ++a) {
                    switch (elem_type) {
                        case GGUFConstants::GGUF_VALUE_TYPE_UINT8:  { uint8_t v; if (!ReadValue(v)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_INT8:   { int8_t v; if (!ReadValue(v)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_UINT16: { uint16_t v; if (!ReadValue(v)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_INT16:  { int16_t v; if (!ReadValue(v)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_UINT32: { uint32_t v; if (!ReadValue(v)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_INT32:  { int32_t v; if (!ReadValue(v)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_FLOAT32:{ float v; if (!ReadValue(v)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_BOOL:   { uint8_t v; if (!ReadValue(v)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_STRING: { std::string s; if (!ReadString(s)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_UINT64: { uint64_t v; if (!ReadValue(v)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_INT64:  { int64_t v; if (!ReadValue(v)) return false; break; }
                        case GGUFConstants::GGUF_VALUE_TYPE_FLOAT64:{ double v; if (!ReadValue(v)) return false; break; }
                        default: {
                            std::cerr << "❌ Unknown array element type " << elem_type << " while skipping metadata" << std::endl;
                            return false;
                        }
                    }
                }
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_UINT64: { uint64_t v; if (!ReadValue(v)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_INT64:  { int64_t v; if (!ReadValue(v)) return false; break; }
            case GGUFConstants::GGUF_VALUE_TYPE_FLOAT64:{ double v; if (!ReadValue(v)) return false; break; }
            default: {
                std::cerr << "❌ Unknown metadata type " << value_type << " while skipping key: " << key << std::endl;
                return false;
            }
        }
    }
    
    // Now read tensor info (no data!)
    for (uint64_t i = 0; i < header_.tensor_count; ++i) {
        TensorRef ref;
        
        if (!ReadString(ref.name)) {
            std::cerr << "❌ Failed to read tensor name at index " << i << std::endl;
            return false;
        }
        
        uint32_t n_dims;
        if (!ReadValue(n_dims)) return false;
        
        ref.shape.resize(n_dims);
        for (uint32_t d = 0; d < n_dims; ++d) {
            if (!ReadValue(ref.shape[d])) return false;
        }
        
        uint32_t type_val;
        if (!ReadValue(type_val)) return false;
        ref.type = static_cast<GGMLType>(type_val);
        
        if (!ReadValue(ref.offset)) return false;
        
        ref.size = CalculateTensorSize(ref.shape, ref.type);
        ref.zone_name = "";  // Will be assigned later
        
        tensor_index_[ref.name] = ref;
    }
    
    return true;
}

std::vector<TensorRef> StreamingGGUFLoader::GetTensorIndex() const {
    std::vector<TensorRef> result;
    for (const auto& [name, ref] : tensor_index_) {
        result.push_back(ref);
    }
    return result;
}

void StreamingGGUFLoader::AssignTensorsToZones() {
    // Zone assignment strategy (like a game engine):
    // Group tensors into zones (8 layers per zone)
    
    for (auto& [tensor_name, tensor_ref] : tensor_index_) {
        std::string zone;
        
        // Pattern matching to assign zones
        if (tensor_name.find("token_embd") != std::string::npos ||
            tensor_name.find("embedding") != std::string::npos) {
            zone = "embedding";
        }
        else if (tensor_name.find("output.weight") != std::string::npos ||
                 tensor_name.find("lm_head") != std::string::npos ||
                 tensor_name.find("output_norm") != std::string::npos) {
            zone = "output_head";
        }
        else if (tensor_name.find("blk.") != std::string::npos) {
            // Extract layer number: blk.0.attn → layer 0
            int layer = ExtractLayerNumber(tensor_name);
            int zone_num = layer / 8;  // 8 layers per zone
            zone = "layers_" + std::to_string(zone_num);
        }
        else {
            zone = "misc";
        }
        
        // Add to zone
        if (zones_.find(zone) == zones_.end()) {
            zones_[zone] = {zone, {}, 0, false, {}};
        }
        zones_[zone].tensors.push_back(tensor_name);
        zones_[zone].total_bytes += tensor_ref.size;
        
        tensor_ref.zone_name = zone;
    }
    
    // Print zone info
    std::cout << "\n📊 Zone Assignment Summary:" << std::endl;
    for (const auto& [zone_name, zone_info] : zones_) {
        std::cout << "   " << zone_name << ": " << zone_info.tensors.size() 
                  << " tensors, " << (zone_info.total_bytes / (1024*1024)) << " MB" << std::endl;
    }
    std::cout << std::endl;
}

int32_t StreamingGGUFLoader::ExtractLayerNumber(const std::string& tensor_name) const {
    // Look for "blk.N" pattern
    size_t pos = tensor_name.find("blk.");
    if (pos == std::string::npos) return 0;
    
    pos += 4;  // Skip "blk."
    size_t end = tensor_name.find_first_not_of("0123456789", pos);
    if (end == std::string::npos) end = tensor_name.length();
    
    try {
        return std::stoi(tensor_name.substr(pos, end - pos));
    } catch (...) {
        return 0;
    }
}

std::string StreamingGGUFLoader::GetZoneForTensor(const std::string& tensor_name) const {
    auto it = tensor_index_.find(tensor_name);
    if (it != tensor_index_.end()) {
        return it->second.zone_name;
    }
    return "";
}

std::string StreamingGGUFLoader::GetTensorZone(const std::string& tensor_name) const {
    return GetZoneForTensor(tensor_name);
}

bool StreamingGGUFLoader::LoadZone(const std::string& zone_name, uint64_t max_memory_mb) {
    auto zone_it = zones_.find(zone_name);
    if (zone_it == zones_.end()) {
        std::cerr << "❌ Zone not found: " << zone_name << std::endl;
        return false;
    }
    
    TensorZoneInfo& zone = zone_it->second;
    
    // Already loaded?
    if (zone.is_loaded) {
        std::cout << "✓ Zone already loaded: " << zone_name << std::endl;
        return true;
    }
    
    // Unload previous zone if needed
    if (!current_zone_.empty() && current_zone_ != zone_name) {
        UnloadZone(current_zone_);
    }
    
    // Check file is open
    if (!is_open_ || !file_.is_open()) {
        std::cerr << "❌ File not open for streaming" << std::endl;
        return false;
    }
    
    // Stream from disk
    zone.data.clear();
    zone.data.reserve(zone.total_bytes);
    
    uint64_t total_loaded = 0;
    
    std::cout << "📥 Loading zone: " << zone_name << " (" << (zone.total_bytes / (1024.0*1024.0)) << " MB)..." << std::endl;
    
    for (const auto& tensor_name : zone.tensors) {
        // Get tensor metadata from index
        auto tensor_it = tensor_index_.find(tensor_name);
        if (tensor_it == tensor_index_.end()) {
            std::cerr << "❌ Tensor not in index: " << tensor_name << std::endl;
            return false;
        }
        
        const TensorRef& ref = tensor_it->second;
        
        // Seek to tensor offset in file
        file_.seekg(ref.offset, std::ios::beg);
        
        // Read from disk into zone buffer
        size_t old_size = zone.data.size();
        zone.data.resize(old_size + ref.size);
        
        file_.read(reinterpret_cast<char*>(zone.data.data() + old_size), ref.size);
        
        if (!file_.good()) {
            std::cerr << "❌ Failed to read tensor: " << tensor_name << std::endl;
            zone.data.resize(old_size);
            return false;
        }
        
        total_loaded += ref.size;
    }
    
    zone.is_loaded = true;
    current_zone_ = zone_name;
    current_zone_memory_ = total_loaded;
    
    std::cout << "✅ Zone loaded: " << zone_name << " (" << (total_loaded / (1024.0*1024.0)) << " MB)" << std::endl;
    
    return true;
}

bool StreamingGGUFLoader::UnloadZone(const std::string& zone_name) {
    auto zone_it = zones_.find(zone_name);
    if (zone_it == zones_.end()) {
        return false;
    }
    
    TensorZoneInfo& zone = zone_it->second;
    
    if (zone.is_loaded) {
        zone.data.clear();
        zone.data.shrink_to_fit();
        zone.is_loaded = false;
        std::cout << "📤 Zone unloaded: " << zone_name << std::endl;
    }
    
    return true;
}

bool StreamingGGUFLoader::GetTensorData(const std::string& tensor_name, std::vector<uint8_t>& data) {
    // Find which zone this tensor belongs to
    std::string zone_name = GetTensorZone(tensor_name);
    if (zone_name.empty()) {
        std::cerr << "❌ Tensor not found: " << tensor_name << std::endl;
        return false;
    }
    
    // Load zone if not already loaded
    if (!zones_[zone_name].is_loaded) {
        if (!LoadZone(zone_name)) {
            return false;
        }
    }
    
    // Find tensor in zone
    TensorZoneInfo& zone = zones_[zone_name];
    
    auto tensor_it = tensor_index_.find(tensor_name);
    if (tensor_it == tensor_index_.end()) {
        return false;
    }
    
    const TensorRef& ref = tensor_it->second;
    
    // Find offset within zone data
    uint64_t offset_in_zone = 0;
    for (const auto& other_name : zone.tensors) {
        if (other_name == tensor_name) {
            break;
        }
        offset_in_zone += tensor_index_[other_name].size;
    }
    
    // Copy tensor data
    data.resize(ref.size);
    std::memcpy(data.data(), zone.data.data() + offset_in_zone, ref.size);
    
    return true;
}

TensorZoneInfo StreamingGGUFLoader::GetZoneInfo(const std::string& zone_name) const {
    auto it = zones_.find(zone_name);
    if (it != zones_.end()) {
        return it->second;
    }
    return {};
}

uint64_t StreamingGGUFLoader::GetTotalFileSize() {
    if (!file_.is_open()) return 0;
    
    std::streampos current = file_.tellg();
    file_.seekg(0, std::ios::end);
    uint64_t size = file_.tellg();
    file_.seekg(current);
    return size;
}

uint64_t StreamingGGUFLoader::GetCurrentMemoryUsage() const {
    uint64_t usage = 0;
    
    // Header + metadata + index
    usage += 100 * 1024 * 1024;  // ~100 MB for overhead
    
    // Active zones
    for (const auto& [zone_name, zone_info] : zones_) {
        if (zone_info.is_loaded) {
            usage += zone_info.data.size();
        }
    }
    
    return usage;
}

std::vector<std::string> StreamingGGUFLoader::GetLoadedZones() const {
    std::vector<std::string> result;
    for (const auto& [zone_name, zone_info] : zones_) {
        if (zone_info.is_loaded) {
            result.push_back(zone_name);
        }
    }
    return result;
}

std::vector<std::string> StreamingGGUFLoader::GetAllZones() const {
    std::vector<std::string> result;
    for (const auto& [zone_name, zone_info] : zones_) {
        result.push_back(zone_name);
    }
    return result;
}

std::vector<TensorInfo> StreamingGGUFLoader::GetAllTensorInfo() const {
    std::vector<TensorInfo> result;
    for (const auto& [name, ref] : tensor_index_) {
        TensorInfo info;
        info.name = name;
        info.shape = ref.shape;
        info.type = ref.type;
        info.offset = ref.offset;
        info.size_bytes = ref.size;
        result.push_back(info);
    }
    return result;
}

std::vector<TensorInfo> StreamingGGUFLoader::GetTensorInfo() const {
    return GetAllTensorInfo();
}

// ============================================================================
// TYPE STRING CONVERSION
// ============================================================================

std::string StreamingGGUFLoader::GetTypeString(GGMLType type) const {
    switch (type) {
        case GGMLType::F32:       return "F32 (float32)";
        case GGMLType::F16:       return "F16 (float16)";
        case GGMLType::Q4_0:      return "Q4_0 (quantized 4-bit, zero point)";
        case GGMLType::Q4_1:      return "Q4_1 (quantized 4-bit with delta)";
        case GGMLType::F16_HALF:  return "F16_HALF (half precision)";
        case GGMLType::Q5_0:      return "Q5_0 (quantized 5-bit, zero point)";
        case GGMLType::Q5_1:      return "Q5_1 (quantized 5-bit with delta)";
        case GGMLType::Q8_0:      return "Q8_0 (quantized 8-bit, zero point)";
        case GGMLType::Q8_1:      return "Q8_1 (quantized 8-bit with delta)";
        case GGMLType::Q2_K:      return "Q2_K (k-quant 2-bit)";
        case GGMLType::Q4_K:      return "Q4_K (k-quant 4-bit)";
        case GGMLType::Q5_K:      return "Q5_K (k-quant 5-bit)";
        case GGMLType::Q3_K:      return "Q3_K (k-quant 3-bit)";
        case GGMLType::Q6_K:      return "Q6_K (k-quant 6-bit)";
        case GGMLType::Q8_K:      return "Q8_K (k-quant 8-bit)";
        case GGMLType::IQ2_XXS:   return "IQ2_XXS (importance 2-bit ultra)";
        case GGMLType::IQ2_XS:    return "IQ2_XS (importance 2-bit extra small)";
        case GGMLType::IQ3_XXS:   return "IQ3_XXS (importance 3-bit ultra)";
        case GGMLType::IQ1_S:     return "IQ1_S (importance 1-bit small)";
        case GGMLType::IQ4_NL:    return "IQ4_NL (importance 4-bit non-linear)";
        case GGMLType::IQ3_S:     return "IQ3_S (importance 3-bit small)";
        case GGMLType::IQ2_S:     return "IQ2_S (importance 2-bit small)";
        case GGMLType::IQ4_XS:    return "IQ4_XS (importance 4-bit extra small)";
        case GGMLType::I8:        return "I8 (integer 8-bit)";
        case GGMLType::I16:       return "I16 (integer 16-bit)";
        case GGMLType::I32:       return "I32 (integer 32-bit)";
        case GGMLType::I64:       return "I64 (integer 64-bit)";
        case GGMLType::F64:       return "F64 (double precision)";
        case GGMLType::IQ1_M:     return "IQ1_M (importance 1-bit medium)";
        default:                  return "Unknown (type " + std::to_string(static_cast<uint32_t>(type)) + ")";
    }
}

// ============================================================================
// PRIVATE TEMPLATE FUNCTIONS
template<typename T>
bool StreamingGGUFLoader::ReadValue(T& value) {
    file_.read(reinterpret_cast<char*>(&value), sizeof(T));
    return file_.good();
}

bool StreamingGGUFLoader::ReadString(std::string& value) {
    uint64_t len;
    if (!ReadValue(len)) return false;
    
    value.resize(len);
    file_.read(&value[0], len);
    return file_.good();
}

uint64_t StreamingGGUFLoader::CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const {
    uint64_t num_elements = 1;
    for (uint64_t dim : shape) {
        num_elements *= dim;
    }
    
    // Bytes per element for each GGMLType (complete v3 coverage)
    // Quantized types use block-based sizes; we approximate per-element for zone sizing.
    float bytes_per_element = 4.0f;  // Default F32
    switch (type) {
        case GGMLType::F32:       bytes_per_element = 4.0f; break;
        case GGMLType::F16:       bytes_per_element = 2.0f; break;
        case GGMLType::F16_HALF:  bytes_per_element = 2.0f; break;
        case GGMLType::Q4_0:      bytes_per_element = 0.5f + (2.0f / 32); break;  // 18 bytes per 32 elements
        case GGMLType::Q4_1:      bytes_per_element = 0.5f + (4.0f / 32); break;  // 20 bytes per 32 elements
        case GGMLType::Q5_0:      bytes_per_element = 0.625f + (4.0f / 32); break;
        case GGMLType::Q5_1:      bytes_per_element = 0.625f + (4.0f / 32); break;
        case GGMLType::Q8_0:      bytes_per_element = 1.0f + (2.0f / 32); break;
        case GGMLType::Q8_1:      bytes_per_element = 1.0f + (8.0f / 32); break;
        case GGMLType::Q2_K:      bytes_per_element = 0.3125f; break;
        case GGMLType::Q3_K:      bytes_per_element = 0.4375f; break;
        case GGMLType::Q4_K:      bytes_per_element = 0.5f; break;
        case GGMLType::Q5_K:      bytes_per_element = 0.625f; break;
        case GGMLType::Q6_K:      bytes_per_element = 0.75f; break;
        case GGMLType::Q8_K:      bytes_per_element = 1.0625f; break;
        case GGMLType::IQ2_XXS:   bytes_per_element = 0.25f; break;
        case GGMLType::IQ2_XS:    bytes_per_element = 0.28125f; break;
        case GGMLType::IQ3_XXS:   bytes_per_element = 0.375f; break;
        case GGMLType::IQ1_S:     bytes_per_element = 0.1875f; break;
        case GGMLType::IQ4_NL:    bytes_per_element = 0.5f; break;
        case GGMLType::IQ3_S:     bytes_per_element = 0.4375f; break;
        case GGMLType::IQ2_S:     bytes_per_element = 0.3125f; break;
        case GGMLType::IQ4_XS:    bytes_per_element = 0.5f; break;
        case GGMLType::I8:        bytes_per_element = 1.0f; break;
        case GGMLType::I16:       bytes_per_element = 2.0f; break;
        case GGMLType::I32:       bytes_per_element = 4.0f; break;
        case GGMLType::I64:       bytes_per_element = 8.0f; break;
        case GGMLType::F64:       bytes_per_element = 8.0f; break;
        case GGMLType::IQ1_M:     bytes_per_element = 0.21875f; break;
        default:                  bytes_per_element = 4.0f; break;  // Conservative default
    }
    
    return static_cast<uint64_t>(num_elements * bytes_per_element);
}

// ============================================================================
// INTERFACE IMPLEMENTATIONS (IGGUFLoader required methods)
// ============================================================================

bool StreamingGGUFLoader::LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) {
    // Delegate to GetTensorData which handles zone loading
    return GetTensorData(tensor_name, data);
}

bool StreamingGGUFLoader::LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) {
    // Get all tensors and load the requested range
    data.clear();
    
    std::vector<std::string> tensor_names;
    for (const auto& [name, ref] : tensor_index_) {
        tensor_names.push_back(name);
    }
    
    // Sort by offset to get consistent ordering
    std::sort(tensor_names.begin(), tensor_names.end(), [this](const std::string& a, const std::string& b) {
        return tensor_index_.at(a).offset < tensor_index_.at(b).offset;
    });
    
    if (start_idx >= tensor_names.size()) {
        return false;
    }
    
    size_t end_idx = std::min(start_idx + count, tensor_names.size());
    
    for (size_t i = start_idx; i < end_idx; ++i) {
        std::vector<uint8_t> tensor_data;
        if (!GetTensorData(tensor_names[i], tensor_data)) {
            return false;
        }
        data.insert(data.end(), tensor_data.begin(), tensor_data.end());
    }
    
    return true;
}

size_t StreamingGGUFLoader::GetTensorByteSize(const TensorInfo& tensor) const {
    return static_cast<size_t>(CalculateTensorSize(tensor.shape, tensor.type));
}

uint64_t StreamingGGUFLoader::GetFileSize() const {
    return const_cast<StreamingGGUFLoader*>(this)->GetTotalFileSize();
}

bool StreamingGGUFLoader::StreamZoneFromDisk(const std::string& zone_name) {
    auto zone_it = zones_.find(zone_name);
    if (zone_it == zones_.end()) {
        std::cerr << "[StreamingGGUFLoader] StreamZoneFromDisk: zone '" << zone_name << "' not found" << std::endl;
        return false;
    }

    if (!is_open_ || !file_.is_open()) {
        std::cerr << "[StreamingGGUFLoader] StreamZoneFromDisk: file not open" << std::endl;
        return false;
    }

    TensorZoneInfo& zone = zone_it->second;

    // Allocate buffer for the entire zone
    zone.data.clear();
    zone.data.reserve(static_cast<size_t>(zone.total_bytes));

    // Stream each tensor in the zone from disk
    for (const auto& tensor_name : zone.tensors) {
        auto ref_it = tensor_index_.find(tensor_name);
        if (ref_it == tensor_index_.end()) {
            std::cerr << "[StreamingGGUFLoader] StreamZoneFromDisk: tensor '" 
                      << tensor_name << "' not in index" << std::endl;
            continue;
        }

        const TensorRef& ref = ref_it->second;
        file_.seekg(static_cast<std::streamoff>(ref.offset));
        if (!file_.good()) {
            std::cerr << "[StreamingGGUFLoader] StreamZoneFromDisk: seek failed for tensor '" 
                      << tensor_name << "' at offset " << ref.offset << std::endl;
            zone.data.clear();
            return false;
        }

        size_t prev_size = zone.data.size();
        zone.data.resize(prev_size + static_cast<size_t>(ref.size));
        file_.read(reinterpret_cast<char*>(zone.data.data() + prev_size),
                   static_cast<std::streamsize>(ref.size));

        if (!file_.good()) {
            std::cerr << "[StreamingGGUFLoader] StreamZoneFromDisk: read failed for tensor '" 
                      << tensor_name << "' (" << ref.size << " bytes)" << std::endl;
            zone.data.clear();
            return false;
        }
    }

    zone.is_loaded = true;
    active_zones_[zone_name] = true;
    std::cout << "[StreamingGGUFLoader] Streamed zone '" << zone_name 
              << "' from disk: " << zone.data.size() << " bytes, "
              << zone.tensors.size() << " tensors" << std::endl;
    return true;
}

// GetVocabulary is defined inline in header
} // namespace RawrXD
