#include "streaming_gguf_loader.h"
#include "model_loader/GGUFConstants.hpp"
#include "utils/Diagnostics.hpp"
#include "compression_interface.h"
#include "streaming_gguf_loader_robust.cpp"
#include "telemetry_singleton.h"
#include "qtapp/settings_manager.h"
#include "gguf_robust_tools.hpp"
#include "gguf_robust_masm_bridge.hpp"
#include "RobustGGUFTools.hpp"
#include "RawrXD_HardenedMetadataParser.hpp"
#include "RawrXD_GGUF_Preflight.hpp"
#include "RawrXD_CorruptionDetector.hpp"
#include "RawrXD_MemoryMappedTensorStore.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <iostream>
#include <QDebug>

namespace {
    constexpr uint64_t kMaxTensorNameLen = 4096;

    bool IsEnvEnabled(const char* name) {
        const char* value = std::getenv(name);
        if (!value || !*value) return false;
        return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T';
    }

    bool GetEnvDefaultTrue(const char* name) {
        const char* value = std::getenv(name);
        if (!value || !*value) return true;
        return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' || value[0] == 'T';
    }

    bool TryCopyValue(const void* src, void* dst, size_t size) {
#if defined(_MSC_VER)
        __try {
            std::memcpy(dst, src, size);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
#else
        std::memcpy(dst, src, size);
        return true;
#endif
    }

    struct MasmParseContext {
        GGUFMetadata* metadata;
        bool verbose;
    };

    void MasmMetadataCallback(const rawrxd::gguf::masm::MetadataEntry* entry, void* userData) {
        if (!entry || !userData) return;

        auto* ctx = static_cast<MasmParseContext*>(userData);
        std::string key;
        if (entry->key_ptr && entry->key_len > 0) {
            key.assign(entry->key_ptr, entry->key_len);
        } else {
            key = "[unknown_key]";
        }

        if (entry->skipped) {
            (*ctx->metadata).kv_pairs[key] = "[SKIPPED]";
            return;
        }

        switch (entry->type) {
            case GGUFConstants::GGUF_VALUE_TYPE_STRING_SPEC:
            case GGUFConstants::GGUF_VALUE_TYPE_STRING: {
                if (!entry->value_ptr || entry->value_len == 0) {
                    (*ctx->metadata).kv_pairs[key] = "";
                    return;
                }
                std::string value;
                value.resize(static_cast<size_t>(entry->value_len));
                if (!TryCopyValue(entry->value_ptr, value.data(), static_cast<size_t>(entry->value_len))) {
                    (*ctx->metadata).kv_pairs[key] = "[COPY_FAILED]";
                    return;
                }
                (*ctx->metadata).kv_pairs[key] = std::move(value);
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_UINT32: {
                uint32_t v = 0;
                if (entry->value_ptr && TryCopyValue(entry->value_ptr, &v, sizeof(v))) {
                    (*ctx->metadata).kv_pairs[key] = std::to_string(v);
                }
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_INT32: {
                int32_t v = 0;
                if (entry->value_ptr && TryCopyValue(entry->value_ptr, &v, sizeof(v))) {
                    (*ctx->metadata).kv_pairs[key] = std::to_string(v);
                }
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_FLOAT32: {
                float v = 0.0f;
                if (entry->value_ptr && TryCopyValue(entry->value_ptr, &v, sizeof(v))) {
                    (*ctx->metadata).kv_pairs[key] = std::to_string(v);
                }
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_UINT64: {
                uint64_t v = 0;
                if (entry->value_ptr && TryCopyValue(entry->value_ptr, &v, sizeof(v))) {
                    (*ctx->metadata).kv_pairs[key] = std::to_string(v);
                }
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_INT64: {
                int64_t v = 0;
                if (entry->value_ptr && TryCopyValue(entry->value_ptr, &v, sizeof(v))) {
                    (*ctx->metadata).kv_pairs[key] = std::to_string(v);
                }
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_FLOAT64: {
                double v = 0.0;
                if (entry->value_ptr && TryCopyValue(entry->value_ptr, &v, sizeof(v))) {
                    (*ctx->metadata).kv_pairs[key] = std::to_string(v);
                }
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_BOOL: {
                uint8_t v = 0;
                if (entry->value_ptr && TryCopyValue(entry->value_ptr, &v, sizeof(v))) {
                    (*ctx->metadata).kv_pairs[key] = (v != 0) ? "true" : "false";
                }
                break;
            }
            case GGUFConstants::GGUF_VALUE_TYPE_ARRAY:
            default:
                (*ctx->metadata).kv_pairs[key] = "[UNHANDLED_TYPE]";
                break;
        }
    }
}

StreamingGGUFLoader::StreamingGGUFLoader()
    : current_zone_(nullptr)
    , is_open_(false)
    , robust_metadata_enabled_(GetEnvDefaultTrue("RAWRXD_GGUF_ROBUST_METADATA"))
    , robust_metadata_verbose_(IsEnvEnabled("RAWRXD_GGUF_ROBUST_VERBOSE"))
    , max_metadata_string_len_(rawrxd::GGUF_SAFETY_LIMITS::MAX_STRING_LEN)
    , masm_metadata_enabled_(IsEnvEnabled("RAWRXD_GGUF_MASM_METADATA"))
    , robust_metadata_peek_enabled_(GetEnvDefaultTrue("RAWRXD_GGUF_ROBUST_PEEK"))
    , robust_arena_(std::make_unique<gguf::RobustMemoryArena>(2048))
    , section_mapper_(std::make_unique<gguf::NtSectionMapper>())
    , string_pool_(std::make_unique<gguf::HardenedStringPool>(robust_arena_.get()))
    , raw_file_handle_(INVALID_HANDLE_VALUE)
{
    // Read compression preference from settings (default to 2 = BRUTAL_GZIP)
    uint32_t pref = static_cast<uint32_t>(SettingsManager::instance().compressionSettings().preferred_type);
    compression_provider_ = CompressionFactory::Create(pref);
    if (!compression_provider_) qWarning() << "[Streaming] No compression provider";
    else {
        qInfo() << "[Streaming] Compression provider active kernel:" << QString::fromStdString(compression_provider_->GetActiveKernel());
    }
}

StreamingGGUFLoader::~StreamingGGUFLoader() {
    Close();
}

bool StreamingGGUFLoader::Open(const std::string& filepath) {
    filepath_ = filepath;

    // [NEW] Added Preflight and Corruption detection for production-grade reliability
    if (robust_metadata_enabled_) {
        try {
            // 1. Preflight Analysis (Zero-allocation check)
            auto projection = RawrXD::Tools::GGUFInspector::Analyze(filepath);
            if (projection.predicted_heap_usage > 8ULL * 1024 * 1024 * 1024) // 8GB safety warning
                qWarning() << "[GGUFStream] Extreme memory pressure projected:" 
                           << (projection.predicted_heap_usage / 1024 / 1024) << "MB. Continuing with caution.";

            // 2. Corruption Detection
            auto report = RawrXD::Tools::GGUFCorruptionDetector::ScanFile(filepath);
            if (!report.is_valid) {
                qCritical() << "[GGUFStream] Aborting: Model file is corrupted or malicious.";
                return false;
            }
        } catch (const std::exception& e) {
            qWarning() << "[GGUFStream] Pre-flight/Scan failed:" << e.what();
        }
    }

    file_.open(filepath, std::ios::binary);
    if (!file_.is_open()) {
        std::cerr << "❌ Failed to open GGUF file: " << filepath << std::endl;
        return false;
    }

    if (raw_file_handle_ == INVALID_HANDLE_VALUE) {
        raw_file_handle_ = CreateFileA(filepath_.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                       nullptr, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                       nullptr);
        if (raw_file_handle_ == INVALID_HANDLE_VALUE && robust_metadata_peek_enabled_) {
            qWarning() << "[GGUFStream] Raw handle open failed, disabling robust peek";
            robust_metadata_peek_enabled_ = false;
        }
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
    
    // Telemetry: record open event
    QJsonObject meta;
    meta["file"] = QString::fromStdString(filepath);
    meta["tensor_count"] = static_cast<double>(tensor_index_.size());
    meta["zone_count"] = static_cast<double>(zones_.size());
    GetTelemetry().recordEvent("gguf_open_streaming", meta);

    return true;
}

bool StreamingGGUFLoader::Close() {
    if (file_.is_open()) {
        file_.close();
    }
    if (raw_file_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(raw_file_handle_);
        raw_file_handle_ = INVALID_HANDLE_VALUE;
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
    if (!ReadValue(header_.magic)) {
        qWarning() << "[GGUFStream] Failed to read GGUF magic";
        return false;
    }
    if (header_.magic != GGUFConstants::GGUF_MAGIC) {
        qCritical() << "[GGUFStream] Invalid GGUF magic: 0x" << QString::number(header_.magic, 16);
        Diagnostics::error("Invalid GGUF magic number", "StreamingGGUFLoader");
        return false;
    }
    
    // Read version
    if (!ReadValue(header_.version)) {
        qWarning() << "[GGUFStream] Failed to read GGUF version";
        return false;
    }
    if (header_.version != GGUFConstants::GGUF_VERSION) {
        qCritical() << "[GGUFStream] Unsupported GGUF version:" << header_.version;
        Diagnostics::error("Unsupported GGUF version: " + std::to_string(header_.version), "StreamingGGUFLoader");
        return false;
    }
    
    // Read tensor count
    if (!ReadValue(header_.tensor_count)) {
        qWarning() << "[GGUFStream] Failed to read tensor count";
        return false;
    }
    qInfo() << "[GGUFStream] Tensor count:" << header_.tensor_count;
    
    // Read metadata KV count
    if (!ReadValue(header_.metadata_kv_count)) {
        qWarning() << "[GGUFStream] Failed to read metadata KV count";
        return false;
    }
    
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
    
    auto start_time = std::chrono::steady_clock::now();
    
    // [NEW] Use the hardened metadata parser to eliminate bad_alloc crashes (Option A)
    try {
        if (robust_metadata_enabled_) {
            auto entries = RawrXD::Tools::HardenedGGUFMetadataParser::ParseMetadataRobust(
                filepath_, 
                true,  // skip_high_risk
                max_metadata_string_len_ > 0 ? max_metadata_string_len_ : 50*1024*1024);
            
            if (!entries.empty()) {
                for (const auto& entry : entries) {
                    metadata_.kv_pairs[entry.key] = entry.string_value;
                    
                    // Populate critical metadata fields
                    if (entry.key == "general.architecture") {
                        if (entry.string_value == "llama") metadata_.architecture_type = 1;
                    } else if (entry.key == "llama.block_count") {
                        metadata_.layer_count = static_cast<uint32_t>(entry.numeric_value);
                    } else if (entry.key == "llama.context_length") {
                        metadata_.context_length = static_cast<uint32_t>(entry.numeric_value);
                    } else if (entry.key == "llama.embedding_length") {
                        metadata_.embedding_dim = static_cast<uint32_t>(entry.numeric_value);
                    } else if (entry.key == "llama.vocab_size") {
                        metadata_.vocab_size = static_cast<uint32_t>(entry.numeric_value);
                    }
                }
                
                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                qInfo() << "[GGUFStream] Hardened metadata parse completed in" << duration_ms << "ms (" << entries.size() << "entries)";
                return true;
            }
        }
    } catch (const std::exception& e) {
        qWarning() << "[GGUFStream] Hardened parser failed, falling back to legacy paths:" << e.what();
    }

    // Pre-flight corruption scan if defensive scanner available
    if (robust_metadata_enabled_ && section_mapper_) {
        if (section_mapper_->open_mapped(filepath_)) {
            auto scan_result = defensive_scanner_.pre_flight_scan(
                section_mapper_->view(), section_mapper_->size());
            
            if (!scan_result.valid) {
                qWarning() << "[GGUFStream] Pre-flight scan failed:" 
                          << QString::fromStdString(scan_result.error_msg);
                section_mapper_->unmap();
                return false;
            }
            
            if (scan_result.has_poisoned_lengths) {
                qWarning() << "[GGUFStream] File has poisoned length fields";
            }
            
            if (scan_result.has_oversized_metadata) {
                qWarning() << "[GGUFStream] File contains oversized metadata (>16MB strings)";
            }
            
            section_mapper_->unmap();
        }
    }
    
    // Try robust memory-mapped parser first
    if (robust_metadata_enabled_) {
        std::wstring wide_path(filepath_.begin(), filepath_.end());
        RobustGGUFParser robust_parser(robust_metadata_verbose_);
        
        if (robust_parser.OpenMapped(wide_path.c_str())) {
            if (robust_parser.ParseMetadata()) {
                // Copy metadata from robust parser
                for (const auto& [key, value] : robust_parser.metadata) {
                    metadata_.kv_pairs[key] = value;
                    
                    // Parse critical metadata for backwards compatibility
                    if (key == "general.architecture" && value == "llama") {
                        metadata_.architecture_type = 1;
                    } else if (key == "llama.block_count") {
                        try { metadata_.layer_count = std::stoul(value); } catch(...) {}
                    } else if (key == "llama.context_length") {
                        try { metadata_.context_length = std::stoul(value); } catch(...) {}
                    } else if (key == "llama.embedding_length") {
                        try { metadata_.embedding_dim = std::stoul(value); } catch(...) {}
                    } else if (key == "llama.vocab_size") {
                        try { metadata_.vocab_size = std::stoul(value); } catch(...) {}
                    }
                }
                
                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                qInfo() << "[GGUFStream] Robust metadata parse completed in" << duration_ms << "ms (" << robust_parser.metadata.size() << "entries)";
                return true;
            }
        }
    }
    
    // Fallback to MASM parser if available
    file_.seekg(header_.metadata_offset);
    if (masm_metadata_enabled_) {
        HANDLE hFile = CreateFileA(filepath_.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            uint32_t flags = 0;
            if (robust_metadata_verbose_) flags |= 0x1; // FLAG_VERBOSE
            flags |= 0x4;  // FLAG_SKIP_TOKENS
            flags |= 0x8;  // FLAG_SKIP_MERGES
            flags |= 0x10; // FLAG_SKIP_TEMPLATE

            uint64_t count_offset = header_.metadata_offset >= 8 ? (header_.metadata_offset - 8) : 0;
            MasmParseContext ctx{&metadata_, robust_metadata_verbose_};
            uint64_t parsed = rawrxd::gguf::masm::Bridge::Instance().ParseMetadata(
                hFile, max_metadata_string_len_, flags, count_offset, MasmMetadataCallback, &ctx);
            CloseHandle(hFile);

            if (parsed > 0) {
                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                qInfo() << "[GGUFStream] MASM metadata parse completed in" << duration_ms << "ms (" << parsed << "entries)";
                return true;
            }
        }
    }

    for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
        std::string key, value;
        
        if (!ReadKeyString(key)) {
            std::cerr << "❌ Failed to read metadata key at index " << i << std::endl;
            return false;
        }
        
        uint32_t value_type;
        if (!ReadValue(value_type)) {
            std::cerr << "❌ Failed to read metadata value type for key: " << key << std::endl;
            return false;
        }

        if (ShouldSkipKey(key)) {
            if (!SkipValue(value_type)) {
                std::cerr << "❌ Failed to skip metadata value for key: " << key << std::endl;
                return false;
            }
            metadata_.kv_pairs[key] = "[SKIPPED]";
            if (robust_metadata_verbose_) {
                qInfo() << "[GGUFStream] Skipped oversized key:" << QString::fromStdString(key);
            }
            continue;
        }
        
        // Value type 1 = UTF-8 string
        if (value_type == GGUFConstants::GGUF_VALUE_TYPE_STRING ||
            value_type == GGUFConstants::GGUF_VALUE_TYPE_STRING_SPEC) {
            if (robust_metadata_peek_enabled_ && raw_file_handle_ != INVALID_HANDLE_VALUE) {
                const auto string_offset = file_.tellg();
                if (string_offset != std::streampos(-1)) {
                    RawrXD::GGUF::Robust::SafeStringParser parser(raw_file_handle_, false);
                    auto peek_len = parser.PeekLength(static_cast<int64_t>(string_offset));
                    if (peek_len && key == GGUFConstants::META_TOKENIZER_CHAT_TEMPLATE && *peek_len > 65536) {
                        char sig[256] = {};
                        RawrXD::GGUF::Robust::SafeStringParser::StringView view{
                            static_cast<uint64_t>(string_offset) + sizeof(uint64_t),
                            *peek_len,
                            true,
                            0};
                        parser.PeekHeader(view, sig, sizeof(sig));
                        parser.SkipString(static_cast<int64_t>(string_offset), *peek_len);
                        if (!file_.seekg(static_cast<std::streamoff>(sizeof(uint64_t) + *peek_len), std::ios::cur)) {
                            return false;
                        }
                        metadata_.kv_pairs[key] = "[skipped-template]";
                        if (robust_metadata_verbose_) {
                            qWarning() << "[GGUFStream] Skipped oversized chat_template (sig)"
                                       << QString::fromLatin1(sig);
                        }
                        continue;
                    }
                }
            }
            if (!ReadString(value)) {
                std::cerr << "❌ Failed to read metadata string value for key: " << key << std::endl;
                return false;
            }
            metadata_.kv_pairs[key] = value;
            
            // Parse important metadata
            if (key == GGUFConstants::META_GENERAL_ARCHITECTURE) {
                if (value == "llama") metadata_.architecture_type = 1;
            } else if (key == GGUFConstants::META_LLAMA_BLOCK_COUNT) {
                metadata_.layer_count = std::stoul(value);
            } else if (key == GGUFConstants::META_LLAMA_CONTEXT_LENGTH) {
                metadata_.context_length = std::stoul(value);
            } else if (key == GGUFConstants::META_LLAMA_EMBEDDING_LENGTH) {
                metadata_.embedding_dim = std::stoul(value);
            } else if (key == GGUFConstants::META_LLAMA_VOCAB_SIZE) {
                metadata_.vocab_size = std::stoul(value);
            }
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_UINT32) {  // uint32
            uint32_t uint_val;
            if (!ReadValue(uint_val)) return false;
            metadata_.kv_pairs[key] = std::to_string(uint_val);
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_INT32) {  // int32
            int32_t int_val;
            if (!ReadValue(int_val)) return false;
            metadata_.kv_pairs[key] = std::to_string(int_val);
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_FLOAT32) {  // float32
            float float_val;
            if (!ReadValue(float_val)) return false;
            metadata_.kv_pairs[key] = std::to_string(float_val);
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_ARRAY) {
            if (!SkipArray()) return false;
            metadata_.kv_pairs[key] = "[ARRAY_SKIPPED]";
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_UINT64) {
            uint64_t uint64_val;
            if (!ReadValue(uint64_val)) return false;
            metadata_.kv_pairs[key] = std::to_string(uint64_val);
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_INT64) {
            int64_t int64_val;
            if (!ReadValue(int64_val)) return false;
            metadata_.kv_pairs[key] = std::to_string(int64_val);
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_FLOAT64) {
            double float64_val;
            if (!ReadValue(float64_val)) return false;
            metadata_.kv_pairs[key] = std::to_string(float64_val);
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_BOOL) {
            uint8_t bool_val = 0;
            if (!ReadValue(bool_val)) return false;
            metadata_.kv_pairs[key] = (bool_val != 0) ? "true" : "false";
        } else {
            if (!SkipValue(value_type)) return false;
            metadata_.kv_pairs[key] = "[UNKNOWN_TYPE]";
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    if (robust_metadata_enabled_) {
        qInfo() << "[GGUFStream] Metadata parse completed in" << duration_ms << "ms";
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
    
    // Skip metadata entries
    for (uint64_t i = 0; i < header_.metadata_kv_count; ++i) {
        std::string key, value;
        if (!ReadKeyString(key)) return false;
        
        uint32_t value_type;
        if (!ReadValue(value_type)) return false;

        if (value_type == GGUFConstants::GGUF_VALUE_TYPE_STRING ||
            value_type == GGUFConstants::GGUF_VALUE_TYPE_STRING_SPEC) {
            if (!SkipString(max_metadata_string_len_)) return false;
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_ARRAY) {
            if (!SkipArray()) return false;
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_UINT32 ||
                   value_type == GGUFConstants::GGUF_VALUE_TYPE_INT32 ||
                   value_type == GGUFConstants::GGUF_VALUE_TYPE_FLOAT32) {
            uint32_t dummy;
            if (!ReadValue(dummy)) return false;
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_UINT64 ||
                   value_type == GGUFConstants::GGUF_VALUE_TYPE_INT64 ||
                   value_type == GGUFConstants::GGUF_VALUE_TYPE_FLOAT64) {
            uint64_t dummy;
            if (!ReadValue(dummy)) return false;
        } else if (value_type == GGUFConstants::GGUF_VALUE_TYPE_BOOL) {
            uint8_t dummy;
            if (!ReadValue(dummy)) return false;
        } else {
            if (!SkipValue(value_type)) return false;
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
        qWarning() << "[GGUFStream] Zone not found:" << QString::fromStdString(zone_name);
        return false;
    }
    
    TensorZoneInfo& zone = zone_it->second;
    
    // Already loaded?
    if (zone.is_loaded) {
        qInfo() << "[GGUFStream] Zone already loaded:" << QString::fromStdString(zone_name);
        return true;
    }
    
    // Unload previous zone if needed
    if (!current_zone_.empty() && current_zone_ != zone_name) {
        UnloadZone(current_zone_);
    }
    
    // Check file is open
    if (!is_open_ || !file_.is_open()) {
        qWarning() << "[GGUFStream] File not open for streaming";
        return false;
    }
    
    // Stream from disk
    zone.data.clear();
    zone.data.reserve(zone.total_bytes);
    
    uint64_t total_loaded = 0;
    
    qInfo() << "[GGUFStream] Loading zone:" << QString::fromStdString(zone_name) 
            << "(" << (zone.total_bytes / (1024.0*1024.0)) << "MB)...";
    
    for (const auto& tensor_name : zone.tensors) {
        auto tensor_it = tensor_index_.find(tensor_name);
        if (tensor_it == tensor_index_.end()) {
            qWarning() << "[GGUFStream] Tensor not in index:" << QString::fromStdString(tensor_name);
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
            qWarning() << "[GGUFStream] Failed to read tensor:" << QString::fromStdString(tensor_name);
            zone.data.resize(old_size);
            return false;
        }
        
        total_loaded += ref.size;
    }
    
    // new path: use compression interface if zone is compressed
    if (zone.compressed && compression_provider_) {
        std::vector<uint8_t> decompressed;
        if (compression_provider_->Decompress(zone.data, decompressed)) {
            zones_[zone_name].data = std::move(decompressed);
            zones_[zone_name].compressed = false;
            compression_stats_.total_decompressed_bytes += zones_[zone_name].data.size();
            qInfo() << "[Streaming] Zone" << QString::fromStdString(zone_name) 
                     << "decompressed via" << QString::fromStdString(compression_provider_->GetActiveKernel());
            
            // telemetry & safety
            GetTelemetry().recordEvent("gguf_zone_decompress", QJsonObject{{"zone", QString::fromStdString(zone_name)}});
            if (zones_[zone_name].data.size() > SettingsManager::instance().compressionSettings().max_decomp_bytes) {
                qWarning() << "[Streaming] Zone" << QString::fromStdString(zone_name).c_str() 
                           << "exceeds max decompression size";
                return false;
            }
            return true;
        }
    }

    // legacy path: inflate in-place
    if (zone.compressed) {
        qWarning() << "[Streaming] Zone" << QString::fromStdString(zone_name).c_str() 
                   << "is compressed but decompression failed or provider missing.";
        return false;
    }
    
    // IMPORTANT: For Ollama blobs, detect and auto-decompress using MASM kernels
    // This ensures brutal MASM compression is used throughout the Ollama pipeline
    std::vector<uint8_t> decompressed;
    if (compression_provider_ && compression_provider_->Decompress(zone.data, decompressed)) {
        zones_[zone_name].data = std::move(decompressed);
        compression_stats_.total_decompressed_bytes += zones_[zone_name].data.size();
        qInfo() << "[Streaming] Ollama zone auto-decompressed via" 
                 << QString::fromStdString(compression_provider_->GetActiveKernel());
        GetTelemetry().recordEvent("ollama_zone_autodecompress", QJsonObject{
            {"zone", QString::fromStdString(zone_name)},
            {"kernel", QString::fromStdString(compression_provider_->GetActiveKernel())}
        });
    }
    
    zone.is_loaded = true;
    current_zone_ = zone_name;
    current_zone_memory_ = total_loaded;
    
    qInfo() << "[GGUFStream] Zone loaded:" << QString::fromStdString(zone_name) 
            << "(" << (total_loaded / (1024.0*1024.0)) << "MB)";
    
    // Telemetry: zone memory
    GetTelemetry().recordEvent("gguf_zone_loaded", QJsonObject{
        {"zone", QString::fromStdString(zone_name)},
        {"bytes", static_cast<double>(total_loaded)}
    });
    
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
        case GGMLType::Q4_0: return "Q4_0 (quantized 4-bit, zero point)";
        case GGMLType::Q4_1: return "Q4_1 (quantized 4-bit with delta)";
        case GGMLType::Q2_K: return "Q2_K (gguf2 quantized 2-bit)";
        case GGMLType::Q3_K: return "Q3_K (gguf2 quantized 3-bit)";
        case GGMLType::Q4_K: return "Q4_K (gguf2 quantized 4-bit)";
        case GGMLType::Q5_K: return "Q5_K (gguf2 quantized 5-bit)";
        case GGMLType::Q6_K: return "Q6_K (gguf2 quantized 6-bit)";
        case GGMLType::Q8_0: return "Q8_0 (quantized 8-bit, zero point)";
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
    return ReadStringLimited(value, max_metadata_string_len_, true);
}

bool StreamingGGUFLoader::ReadKeyString(std::string& value) {
    return ReadStringLimited(value, kMaxTensorNameLen, false);
}

bool StreamingGGUFLoader::ReadStringLimited(std::string& value, uint64_t max_len, bool allow_skip) {
    const auto len_offset = file_.tellg();
    uint64_t len = 0;
    if (!ReadValue(len)) return false;

    bool length_valid = len <= max_len;
    bool flagged_corrupt = false;
    if (robust_metadata_peek_enabled_ && raw_file_handle_ != INVALID_HANDLE_VALUE && len_offset != std::streampos(-1)) {
        RawrXD::GGUF::Robust::SafeStringParser parser(raw_file_handle_, false);
        auto peek_len = parser.PeekLength(static_cast<int64_t>(len_offset));
        if (!peek_len || *peek_len != len || !parser.ValidateString(len, max_len)) {
            length_valid = false;
            flagged_corrupt = true;
        }
    } else if (len == std::numeric_limits<uint64_t>::max()) {
        length_valid = false;
        flagged_corrupt = true;
    }

    if (!length_valid) {
        if (robust_metadata_verbose_) {
            qWarning() << "[GGUFStream] String length" << static_cast<qulonglong>(len)
                       << "exceeds or fails validation (limit" << static_cast<qulonglong>(max_len) << ")";
        }
        if (!allow_skip) return false;
        if (len > static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max())) return false;
        if (!file_.seekg(static_cast<std::streamoff>(len), std::ios::cur)) return false;
        value = flagged_corrupt ? "[SKIPPED_CORRUPT]" : "[SKIPPED_OVERSIZE]";
        return file_.good();
    }

    try {
        value.resize(static_cast<size_t>(len));
    } catch (...) {
        if (allow_skip) {
            if (!file_.seekg(static_cast<std::streamoff>(len), std::ios::cur)) return false;
            value = "[SKIPPED_OOM]";
            return file_.good();
        }
        return false;
    }

    file_.read(&value[0], static_cast<std::streamsize>(len));
    return file_.good();
}

bool StreamingGGUFLoader::SkipString(uint64_t max_len) {
    uint64_t len = 0;
    if (!ReadValue(len)) return false;

    if (len > max_len && robust_metadata_verbose_) {
        qWarning() << "[GGUFStream] Skipping oversized string of" << static_cast<qulonglong>(len) << "bytes";
    }
    if (!file_.seekg(static_cast<std::streamoff>(len), std::ios::cur)) return false;
    return file_.good();
}

size_t StreamingGGUFLoader::GetScalarTypeSize(uint32_t value_type) const {
    switch (value_type) {
        case 0:  // UINT8
        case 1:  // INT8 (legacy string type is handled elsewhere)
            return 1;
        case 2:  // UINT16
        case 3:  // INT16
            return 2;
        case 4:  // UINT32
        case 5:  // INT32
        case 6:  // FLOAT32
            return 4;
        case 7:  // BOOL
            return 1;
        case 10: // UINT64
        case 11: // INT64
        case 12: // FLOAT64
            return 8;
        default:
            return 0;
    }
}

bool StreamingGGUFLoader::SkipArray(uint64_t max_depth) {
    if (max_depth == 0) return false;

    uint32_t elem_type = 0;
    uint64_t count = 0;
    if (!ReadValue(elem_type)) return false;
    if (!ReadValue(count)) return false;

    if (count > rawrxd::GGUF_SAFETY_LIMITS::MAX_ARRAY_ELEMENTS) {
        if (robust_metadata_verbose_) {
            qWarning() << "[GGUFStream] Array element count" << static_cast<qulonglong>(count)
                       << "exceeds max" << static_cast<qulonglong>(rawrxd::GGUF_SAFETY_LIMITS::MAX_ARRAY_ELEMENTS);
        }
        return false;
    }

    if (elem_type == GGUFConstants::GGUF_VALUE_TYPE_STRING ||
        elem_type == GGUFConstants::GGUF_VALUE_TYPE_STRING_SPEC) {
        for (uint64_t i = 0; i < count; ++i) {
            if (!SkipString(max_metadata_string_len_)) return false;
        }
        return true;
    }

    if (elem_type == GGUFConstants::GGUF_VALUE_TYPE_ARRAY) {
        for (uint64_t i = 0; i < count; ++i) {
            if (!SkipArray(max_depth - 1)) return false;
        }
        return true;
    }

    size_t elem_size = GetScalarTypeSize(elem_type);
    if (elem_size == 0) return false;

    if (count > (std::numeric_limits<uint64_t>::max() / elem_size)) return false;
    uint64_t bytes = count * elem_size;
    if (!file_.seekg(static_cast<std::streamoff>(bytes), std::ios::cur)) return false;
    return file_.good();
}

bool StreamingGGUFLoader::SkipValue(uint32_t value_type) {
    if (value_type == GGUFConstants::GGUF_VALUE_TYPE_STRING ||
        value_type == GGUFConstants::GGUF_VALUE_TYPE_STRING_SPEC) {
        return SkipString(max_metadata_string_len_);
    }
    if (value_type == GGUFConstants::GGUF_VALUE_TYPE_ARRAY) {
        return SkipArray();
    }

    size_t size = GetScalarTypeSize(value_type);
    if (size == 0) return false;
    if (!file_.seekg(static_cast<std::streamoff>(size), std::ios::cur)) return false;
    return file_.good();
}

bool StreamingGGUFLoader::ShouldSkipKey(const std::string& key) const {
    if (!robust_metadata_enabled_) return false;
    static const std::vector<std::string> skip_keys = {
        "tokenizer.chat_template",
        "tokenizer.ggml.tokens",
        "tokenizer.ggml.merges",
        "tokenizer.ggml.token_type",
        "tokenizer.ggml.token_scores"
    };
    return std::find(skip_keys.begin(), skip_keys.end(), key) != skip_keys.end();
}

uint64_t StreamingGGUFLoader::CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const {
    uint64_t num_elements = 1;
    for (uint64_t dim : shape) {
        num_elements *= dim;
    }
    
    float bytes_per_element = 4.0f;  // Default F32
    switch (type) {
        case GGMLType::F32: bytes_per_element = 4.0f; break;
        case GGMLType::F16: bytes_per_element = 2.0f; break;
        case GGMLType::Q4_0:
        case GGMLType::Q4_1: bytes_per_element = 0.5f; break;
        case GGMLType::Q2_K: bytes_per_element = 0.3125f; break;
        case GGMLType::Q3_K: bytes_per_element = 0.4375f; break;
        case GGMLType::Q4_K: bytes_per_element = 0.5f; break;
        case GGMLType::Q5_K: bytes_per_element = 0.625f; break;
        case GGMLType::Q6_K: bytes_per_element = 0.75f; break;
        case GGMLType::Q8_0: bytes_per_element = 1.0f; break;
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
    return GetTotalFileSize();
}

