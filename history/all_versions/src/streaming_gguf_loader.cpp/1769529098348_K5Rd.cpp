#include "streaming_gguf_loader.h"
#include "gguf_robust_tools.hpp"
#include "metadata_guard.hpp"
#include "model_loader/GGUFConstants.hpp"
#include "utils/Diagnostics.hpp"
#define GGML_COMMON_DECL_CPP
#include "ggml-common.h"
#undef GGML_COMMON_DECL_CPP
#include "ggml.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <cstdlib>
#include <limits>

StreamingGGUFLoader::StreamingGGUFLoader()
    : is_open_(false), current_zone_memory_(0), max_zone_memory_mb_(GGUFConstants::DEFAULT_ZONE_MEMORY_MB) {
    std::memset(&header_, 0, sizeof(GGUFHeader));
    max_zone_memory_mb_ = ParseBudgetEnv();
    const char* verbose_env = std::getenv("RAWRXD_GGUF_VERBOSE");
    verbose_ = verbose_env && *verbose_env && std::string(verbose_env) != "0";
    // Initialize MASM-provided robust allocator and primitives (no-op if already init)
    Robust_Initialize();
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

    const char* meta_only_env = std::getenv("RAWRXD_METADATA_ONLY");
    const bool metadata_only = meta_only_env && *meta_only_env && std::string(meta_only_env) != "0";
    if (metadata_only) {
        AdjustMemoryBudget();
        std::cout << "✅ GGUF metadata opened (metadata-only mode)" << std::endl;
        std::cout << "   File: " << filepath << std::endl;
        return true;
    }
    
    // Build tensor index (no data loaded yet!)
    if (!BuildTensorIndex()) {
        Close();
        return false;
    }
    
    AdjustMemoryBudget();
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
    if (header_.version < GGUFConstants::GGUF_VERSION_MIN || header_.version > GGUFConstants::GGUF_VERSION_MAX) {
        std::cerr << "❌ Unsupported GGUF version: " << header_.version << std::endl;
        Diagnostics::error("Unsupported GGUF version: " + std::to_string(header_.version), "StreamingGGUFLoader");
        return false;
    }
    if (header_.version != GGUFConstants::GGUF_VERSION_MAX) {
        std::cerr << "⚠️  Loading legacy GGUF version: " << header_.version << std::endl;
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
    using namespace rawrxd::gguf;
    
    RobustFileStream rfile(filepath_);
    if (!rfile.Open()) {
        fprintf(stderr, "[GGUF] Failed to open file: %s\n", filepath_.c_str());
        return false;
    }
    
    // Validate header first
    rawrxd::gguf::GGUFHeader safe_header;
    std::string error;
    if (!HeaderValidator::Validate(rfile, safe_header, error)) {
        fprintf(stderr, "[GGUF] Header validation failed: %s\n", error.c_str());
        return false;
    }
    
    if (verbose_) {
        fprintf(stderr, "[GGUF] Validated header: %s, Tensors: %llu, Metadata pairs: %llu\n",
                safe_header.GetVersionString().c_str(),
                safe_header.tensor_count,
                safe_header.metadata_kv_count);
    }
    
    // Configure parser with specific skips
    SafeMetadataParser::Config cfg;
    cfg.skip_chat_template = true;      
    cfg.skip_tokenizer_merges = true;   
    cfg.verbose_diagnostics = verbose_;
    cfg.max_string_length = 50 * 1024 * 1024;  // 50MB hard limit
    
    SafeMetadataParser parser(rfile, cfg);
    
    // Callbacks for parsed values
    auto on_string = [this](const std::string& k, rawrxd::gguf::GGUFType, const std::string& v) {
        metadata_.kv_pairs[k] = v;
        TrackMetadataField(k, v);
    };
    
    // Essential: Seek back to metadata_kv_count (offset 16) so SafeMetadataParser can read it
    rfile.SeekAbsolute(16);

    auto result = parser.Parse(on_string, nullptr);
    
    if (verbose_) fprintf(stderr, "\n");  // Newline after progress
    
    if (!result.success) {
        fprintf(stderr, "[GGUF] Parse failed at key #%llu: %s\n", 
                result.keys_parsed, result.error_message.c_str());
        HexDump(rfile, rfile.Position() > 64 ? rfile.Position() - 64 : 0, 128);
        if (verbose_) {
            fprintf(stderr, "[GGUF] Falling back to metadata guard scanner\n");
        }
        return ParseMetadataGuarded();
    }
    
    if (verbose_) {
        fprintf(stderr, "[GGUF] Parsed %llu keys, skipped %zu items, total skipped bytes: %llu\n",
                result.keys_parsed,
                result.skipped_keys.size(),
                result.bytes_skipped);
        
        for (const auto& skipped : result.skipped_keys) {
            fprintf(stderr, "[GGUF]   - Skipped: %s\n", skipped.c_str());
        }
    }
    
    return true;
}

bool StreamingGGUFLoader::ParseMetadataGuarded() {
    rawrxd::gguf::FileView view(filepath_);
    if (!view.valid()) {
        return false;
    }

    rawrxd::gguf::MetadataScanner scanner(view);
    if (!scanner.seek(header_.metadata_offset)) {
        return false;
    }

    for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
        rawrxd::gguf::GStringView key;
        if (!scanner.read_key(key)) {
            return false;
        }

        rawrxd::gguf::ValueType type;
        if (!scanner.read_value_type(type)) {
            return false;
        }

        std::string key_str(key.data(), key.size());
        if (ShouldSkipMetadataKey(std::string_view(key.data(), key.size()))) {
            if (!scanner.skip_value(type)) {
                return false;
            }
            continue;
        }

        if (type == rawrxd::gguf::ValueType::STRING) {
            rawrxd::gguf::GStringView value;
            if (!scanner.read_string(value)) {
                return false;
            }
            std::string value_str(value.data(), value.size());
            metadata_.kv_pairs[key_str] = value_str;
            TrackMetadataField(key_str, value_str);
        } else {
            if (!scanner.skip_value(type)) {
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
    using namespace rawrxd::gguf;

    if (!file_.is_open()) {
        return false;
    }
    
    // START: Robust Metadata Skipping using SafeMetadataParser
    // We must skip metadata safely to handle corruptions/padding before reaching tensors.
    
    RobustFileStream rfile(filepath_);
    if (!rfile.Open()) {
        std::cerr << "❌ Failed to open file for robust indexing: " << filepath_ << std::endl;
        return false;
    }
    
    // Seek to MetadataKVCount (offset 16)
    // Note: HeaderValidator would leave us at 24. rfile.Open leaves at 0.
    rfile.SeekAbsolute(16);
    
    SafeMetadataParser::Config cfg;
    cfg.verbose_diagnostics = false; // Silent skip
    cfg.skip_chat_template = true;
    cfg.skip_tokenizer_merges = true;
    
    SafeMetadataParser parser(rfile, cfg);
    
    // Just skip, no callbacks
    auto result = parser.Parse(nullptr, nullptr);
    if (!result.success) {
        std::cerr << "❌ robust metadata skip failed: " << result.error_message << std::endl;
        return false;
    }
    
    // Sync main stream to tensor start position
    file_.seekg(rfile.Position(), std::ios::beg);
    
    // END: Robust Metadata Skipping
    
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
    const uint64_t budget_bytes = GetZoneBudgetBytes();

    std::vector<std::pair<std::string, TensorRef*>> ordered_refs;
    ordered_refs.reserve(tensor_index_.size());
    for (auto& entry : tensor_index_) {
        ordered_refs.push_back({entry.first, &entry.second});
    }
    std::sort(ordered_refs.begin(), ordered_refs.end(), [](const auto& a, const auto& b) {
        return a.second->offset < b.second->offset;
    });

    uint32_t layer_zone = 0;
    uint64_t layer_acc = 0;
    uint32_t embedding_chunk = 0, output_chunk = 0, misc_chunk = 0;
    uint64_t embedding_acc = 0, output_acc = 0, misc_acc = 0;

    auto assignChunk = [&](const std::string& base, uint64_t tensor_size, uint64_t& acc, uint32_t& idx) {
        if (budget_bytes && tensor_size > budget_bytes) {
            std::string name = base + "_oversize_" + std::to_string(idx++);
            acc = 0;
            return name;
        }
        if (budget_bytes && acc && acc + tensor_size > budget_bytes) {
            ++idx;
            acc = 0;
        }
        std::string name = base;
        if (idx > 0) {
            name += "_chunk" + std::to_string(idx);
        }
        acc += tensor_size;
        return name;
    };

    auto assignLayerZone = [&](uint64_t tensor_size) {
        if (budget_bytes && tensor_size > budget_bytes) {
            std::string name = "layers_oversize_" + std::to_string(layer_zone++);
            layer_acc = 0;
            return name;
        }
        if (budget_bytes && layer_acc && layer_acc + tensor_size > budget_bytes) {
            ++layer_zone;
            layer_acc = 0;
        }
        std::string name = "layers_" + std::to_string(layer_zone);
        layer_acc += tensor_size;
        return name;
    };

    for (auto& pair : ordered_refs) {
        const std::string& tensor_name = pair.first;
        TensorRef& tensor_ref = *pair.second;
        std::string zone;

        if (tensor_name.find("token_embd") != std::string::npos ||
            tensor_name.find("embedding") != std::string::npos) {
            zone = assignChunk("embedding", tensor_ref.size, embedding_acc, embedding_chunk);
        } else if (tensor_name.find("output.weight") != std::string::npos ||
                   tensor_name.find("lm_head") != std::string::npos ||
                   tensor_name.find("output_norm") != std::string::npos) {
            zone = assignChunk("output_head", tensor_ref.size, output_acc, output_chunk);
        } else if (tensor_name.find("blk.") != std::string::npos) {
            zone = assignLayerZone(tensor_ref.size);
        } else {
            zone = assignChunk("misc", tensor_ref.size, misc_acc, misc_chunk);
        }

        if (zones_.find(zone) == zones_.end()) {
            zones_[zone] = {zone, {}, 0, false, {}};
        }
        zones_[zone].tensors.push_back(tensor_name);
        zones_[zone].total_bytes += tensor_ref.size;

        tensor_ref.zone_name = zone;
    }

    std::cout << "\n📊 Zone Assignment Summary (budget ~" << (budget_bytes / (1024 * 1024))
              << " MB per zone):" << std::endl;
    for (const auto& [zone_name, zone_info] : zones_) {
        std::cout << "   " << zone_name << ": " << zone_info.tensors.size()
                  << " tensors, " << (zone_info.total_bytes / (1024 * 1024)) << " MB" << std::endl;
    }
    std::cout << std::endl;
}

void StreamingGGUFLoader::AdjustMemoryBudget() {
    // Honor explicit environment override if present
    max_zone_memory_mb_ = ParseBudgetEnv();

    // Tighten budget automatically for very large models
    if (max_zone_memory_mb_ == GGUFConstants::DEFAULT_ZONE_MEMORY_MB) {
        if (metadata_.layer_count >= 80 || GetTotalFileSize() > (20ull << 30)) {
            max_zone_memory_mb_ = 384; // narrower zones for 70B-120B class models
        }
    }
}

uint64_t StreamingGGUFLoader::GetZoneBudgetBytes(uint64_t override_mb) const {
    uint64_t mb = override_mb ? override_mb : max_zone_memory_mb_;
    if (mb == 0) {
        return 0;
    }
    if (mb > (std::numeric_limits<uint64_t>::max() / (1024ull * 1024ull))) {
        return GGUFConstants::DEFAULT_ZONE_MEMORY_MB * 1024ull * 1024ull;
    }
    return mb * 1024ull * 1024ull;
}

uint64_t StreamingGGUFLoader::ParseBudgetEnv() const {
    const char* raw = std::getenv("RAWRXD_STREAMING_ZONE_MB");
    if (!raw || *raw == '\0') {
        return GGUFConstants::DEFAULT_ZONE_MEMORY_MB;
    }
    char* end = nullptr;
    unsigned long long parsed = std::strtoull(raw, &end, 10);
    if (end == raw || parsed == 0) {
        return GGUFConstants::DEFAULT_ZONE_MEMORY_MB;
    }
    return static_cast<uint64_t>(parsed);
}

bool StreamingGGUFLoader::ReadTensorSlice(const TensorRef& ref, std::vector<uint8_t>& data) const {
    if (!file_.is_open()) {
        return false;
    }
    file_.seekg(ref.offset, std::ios::beg);
    data.resize(ref.size);
    file_.read(reinterpret_cast<char*>(data.data()), ref.size);
    return file_.good();
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

    const uint64_t budget_bytes = GetZoneBudgetBytes(max_memory_mb);
    if (budget_bytes && zone.total_bytes > budget_bytes) {
        std::cout << "⚠️ Zone exceeds budget, streaming on-demand: " << zone_name << " (~"
                  << (zone.total_bytes / (1024.0 * 1024.0)) << " MB vs budget "
                  << (budget_bytes / (1024.0 * 1024.0)) << " MB)" << std::endl;
        return true;
    }
    
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

    auto tensor_it = tensor_index_.find(tensor_name);
    if (tensor_it == tensor_index_.end()) {
        return false;
    }
    const TensorRef& ref = tensor_it->second;
    TensorZoneInfo& zone = zones_[zone_name];

    const uint64_t budget_bytes = GetZoneBudgetBytes();
    if (budget_bytes && zone.total_bytes > budget_bytes) {
        return ReadTensorSlice(ref, data);
    }
    
    // Load zone if not already loaded
    if (!zone.is_loaded) {
        if (!LoadZone(zone_name)) {
            return false;
        }
    }

    // Find offset within zone data
    uint64_t offset_in_zone = 0;
    for (const auto& other_name : zone.tensors) {
        if (other_name == tensor_name) {
            break;
        }
        offset_in_zone += tensor_index_.at(other_name).size;
    }
    
    // Copy tensor data
    if (offset_in_zone + ref.size > zone.data.size()) {
        std::cerr << "❌ Zone buffer too small for tensor: " << tensor_name << std::endl;
        return false;
    }
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

// ============================================================================
// ZERO-COPY ACCESS (Critical for 50μs target)
// ============================================================================

std::span<const std::byte> StreamingGGUFLoader::GetTensorView(
    const std::string& tensor_name,
    size_t offset,
    size_t length)
{
    // Find tensor metadata
    auto tensor_it = tensor_index_.find(tensor_name);
    if (tensor_it == tensor_index_.end()) {
        std::cerr << "❌ GetTensorView: Tensor not found: " << tensor_name << std::endl;
        return {};  // Empty span
    }
    const TensorRef& ref = tensor_it->second;
    
    // Get zone name
    std::string zone_name = GetTensorZone(tensor_name);
    if (zone_name.empty()) {
        return {};
    }
    
    auto zone_it = zones_.find(zone_name);
    if (zone_it == zones_.end()) {
        return {};
    }
    const TensorZoneInfo& zone = zone_it->second;
    
    // Check if zone is resident
    if (!zone.is_loaded || zone.data.empty()) {
        // Zone not loaded - caller should call LoadZone() first or use IsTensorResident()
        return {};
    }
    
    // Calculate offset within zone
    uint64_t offset_in_zone = 0;
    for (const auto& other_name : zone.tensors) {
        if (other_name == tensor_name) {
            break;
        }
        auto other_it = tensor_index_.find(other_name);
        if (other_it != tensor_index_.end()) {
            offset_in_zone += other_it->second.size;
        }
    }
    
    // Apply user offset
    offset_in_zone += offset;
    
    // Calculate actual length
    size_t actual_length = (length == SIZE_MAX) ? (ref.size - offset) : length;
    
    // Bounds check
    if (offset_in_zone + actual_length > zone.data.size()) {
        std::cerr << "❌ GetTensorView: Out of bounds for tensor: " << tensor_name << std::endl;
        return {};
    }
    
    // ZERO-COPY: Return pointer directly into zone memory
    const std::byte* ptr = reinterpret_cast<const std::byte*>(zone.data.data() + offset_in_zone);
    return std::span<const std::byte>(ptr, actual_length);
}

bool StreamingGGUFLoader::IsTensorResident(const std::string& tensor_name) const {
    std::string zone_name = GetTensorZone(tensor_name);
    if (zone_name.empty()) {
        return false;
    }
    
    auto zone_it = zones_.find(zone_name);
    if (zone_it == zones_.end()) {
        return false;
    }
    
    return zone_it->second.is_loaded && !zone_it->second.data.empty();
}

const std::byte* StreamingGGUFLoader::GetZonePointer(const std::string& zone_name) const {
    auto zone_it = zones_.find(zone_name);
    if (zone_it == zones_.end() || !zone_it->second.is_loaded || zone_it->second.data.empty()) {
        return nullptr;
    }
    return reinterpret_cast<const std::byte*>(zone_it->second.data.data());
}

uint64_t StreamingGGUFLoader::GetTotalFileSize() const {
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
        case GGMLType::F32: return "F32 (float32)";
        case GGMLType::F16: return "F16 (float16)";
        case GGMLType::BF16: return "BF16 (bfloat16)";
        case GGMLType::F64: return "F64 (float64)";
        case GGMLType::I8: return "I8 (int8)";
        case GGMLType::I16: return "I16 (int16)";
        case GGMLType::I32: return "I32 (int32)";
        case GGMLType::I64: return "I64 (int64)";
        case GGMLType::Q4_0: return "Q4_0 (quantized 4-bit, zero point)";
        case GGMLType::Q4_1: return "Q4_1 (quantized 4-bit with delta)";
        case GGMLType::Q5_0: return "Q5_0 (quantized 5-bit)";
        case GGMLType::Q5_1: return "Q5_1 (quantized 5-bit with delta)";
        case GGMLType::Q8_0: return "Q8_0 (quantized 8-bit, zero point)";
        case GGMLType::Q8_1: return "Q8_1 (quantized 8-bit with sum)";
        case GGMLType::Q2_K: return "Q2_K (gguf2 quantized 2-bit)";
        case GGMLType::Q3_K: return "Q3_K (gguf2 quantized 3-bit)";
        case GGMLType::Q4_K: return "Q4_K (gguf2 quantized 4-bit)";
        case GGMLType::Q5_K: return "Q5_K (gguf2 quantized 5-bit)";
        case GGMLType::Q6_K: return "Q6_K (gguf2 quantized 6-bit)";
        case GGMLType::Q8_K: return "Q8_K (gguf2 quantized 8-bit)";
        case GGMLType::IQ2_XXS: return "IQ2_XXS (importance-quantized)";
        case GGMLType::IQ2_XS: return "IQ2_XS (importance-quantized)";
        case GGMLType::IQ3_XXS: return "IQ3_XXS (importance-quantized)";
        case GGMLType::IQ1_S: return "IQ1_S (importance-quantized)";
        case GGMLType::IQ4_NL: return "IQ4_NL (importance-quantized)";
        case GGMLType::IQ3_S: return "IQ3_S (importance-quantized)";
        case GGMLType::IQ2_S: return "IQ2_S (importance-quantized)";
        case GGMLType::IQ4_XS: return "IQ4_XS (importance-quantized)";
        case GGMLType::IQ1_M: return "IQ1_M (importance-quantized)";
        case GGMLType::TQ1_0: return "TQ1_0 (ternary quantized)";
        case GGMLType::TQ2_0: return "TQ2_0 (ternary quantized)";
        case GGMLType::MXFP4: return "MXFP4 (mixed precision)";
        default: return "Unknown";
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

    constexpr uint64_t MAX_SAFE_STRING_LEN = 10ULL * 1024 * 1024;
    if (len > MAX_SAFE_STRING_LEN) {
        const auto skip_offset = static_cast<std::streamoff>(len);
        if (!file_.seekg(skip_offset, std::ios::cur)) {
            return false;
        }
        value = "[skipped: " + std::to_string(len) + " bytes]";
        return true;
    }

    // Try to allocate via the MASM slab allocator to avoid throwing std::bad_alloc
    // If assembly-provided allocator is not linked/available, this will still compile
    // but the linker must provide `Robust_Allocate`/`Robust_Free` symbols.
    void* masm_buf = nullptr;
    if (len > 0) {
        masm_buf = Robust_Allocate(len, 0);
    }

    if (masm_buf != nullptr) {
        // Read directly into MASM-provided buffer
        file_.read(reinterpret_cast<char*>(masm_buf), static_cast<std::streamsize>(len));
        if (!file_.good()) {
            Robust_Free(masm_buf);
            return false;
        }
        // Move into std::string and free MASM buffer
        value.assign(reinterpret_cast<char*>(masm_buf), static_cast<size_t>(len));
        Robust_Free(masm_buf);
        return true;
    }

    // Fallback to standard allocation with exception guard
    try {
        value.resize(len);
    } catch (const std::bad_alloc&) {
        const auto skip_offset = static_cast<std::streamoff>(len);
        file_.seekg(skip_offset, std::ios::cur);
        value = "[skipped: allocation failed]";
        return file_.good();
    }
    file_.read(&value[0], static_cast<std::streamsize>(len));
    return file_.good();
}

uint64_t StreamingGGUFLoader::CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const {
    uint64_t num_elements = 1;
    for (uint64_t dim : shape) {
        num_elements *= dim;
    }
    
    // Helper lambda for block-aligned size calculation
    auto block_aligned_size = [num_elements](uint64_t block_elements, uint64_t block_size) -> uint64_t {
        uint64_t num_blocks = (num_elements + block_elements - 1) / block_elements;
        return num_blocks * block_size;
    };

    switch (type) {
        case GGMLType::F32: return num_elements * sizeof(float);
        case GGMLType::F16: return num_elements * sizeof(ggml_half);
        case GGMLType::BF16: return num_elements * sizeof(ggml_bf16_t);
        case GGMLType::F64: return num_elements * sizeof(double);
        case GGMLType::I8: return num_elements * sizeof(int8_t);
        case GGMLType::I16: return num_elements * sizeof(int16_t);
        case GGMLType::I32: return num_elements * sizeof(int32_t);
        case GGMLType::I64: return num_elements * sizeof(int64_t);

        case GGMLType::Q4_0: return block_aligned_size(QK4_0, sizeof(block_q4_0));
        case GGMLType::Q4_1: return block_aligned_size(QK4_1, sizeof(block_q4_1));
        case GGMLType::Q5_0: return block_aligned_size(QK5_0, sizeof(block_q5_0));
        case GGMLType::Q5_1: return block_aligned_size(QK5_1, sizeof(block_q5_1));
        case GGMLType::Q8_0: return block_aligned_size(QK8_0, sizeof(block_q8_0));
        case GGMLType::Q8_1: return block_aligned_size(QK8_1, sizeof(block_q8_1));
        case GGMLType::MXFP4: return block_aligned_size(QK_MXFP4, sizeof(block_mxfp4));

        case GGMLType::Q2_K: return block_aligned_size(QK_K, sizeof(block_q2_K));
        case GGMLType::Q3_K: return block_aligned_size(QK_K, sizeof(block_q3_K));
        case GGMLType::Q4_K: return block_aligned_size(QK_K, sizeof(block_q4_K));
        case GGMLType::Q5_K: return block_aligned_size(QK_K, sizeof(block_q5_K));
        case GGMLType::Q6_K: return block_aligned_size(QK_K, sizeof(block_q6_K));
        case GGMLType::Q8_K: return block_aligned_size(QK_K, sizeof(block_q8_K));

        case GGMLType::IQ2_XXS: return block_aligned_size(QK_K, sizeof(block_iq2_xxs));
        case GGMLType::IQ2_XS: return block_aligned_size(QK_K, sizeof(block_iq2_xs));
        case GGMLType::IQ3_XXS: return block_aligned_size(QK_K, sizeof(block_iq3_xxs));
        case GGMLType::IQ1_S: return block_aligned_size(QK_K, sizeof(block_iq1_s));
        case GGMLType::IQ4_NL: return block_aligned_size(QK4_NL, sizeof(block_iq4_nl));
        case GGMLType::IQ3_S: return block_aligned_size(QK_K, sizeof(block_iq3_s));
        case GGMLType::IQ2_S: return block_aligned_size(QK_K, sizeof(block_iq2_s));
        case GGMLType::IQ4_XS: return block_aligned_size(QK_K, sizeof(block_iq4_xs));
        case GGMLType::IQ1_M: return block_aligned_size(QK_K, sizeof(block_iq1_m));

        case GGMLType::TQ1_0: return block_aligned_size(QK_K, sizeof(block_tq1_0));
        case GGMLType::TQ2_0: return block_aligned_size(QK_K, sizeof(block_tq2_0));

        default: return num_elements * sizeof(float);
    }
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
    return GetTotalFileSize();
}

