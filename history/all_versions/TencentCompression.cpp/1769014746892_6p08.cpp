#include "RawrXD/TencentCompression.hpp"
#include <cmath>
#include <queue>
#include <functional>
#include <QDebug>

namespace RawrXD {

TencentCompressionProvider::TencentCompressionProvider(const Config& config)
    : config_(config) {
    
    if (config.quantization_bits < 2 || config.quantization_bits > 8) {
        throw std::invalid_argument("quantization_bits must be 2-8");
    }
    if (config.block_size < 32 || config.block_size > 1024) {
        throw std::invalid_argument("block_size must be 32-1024");
    }
    if (config.sparsity_threshold < 0.0f || config.sparsity_threshold > 1.0f) {
        throw std::invalid_argument("sparsity_threshold must be 0.0-1.0");
    }
}

std::vector<uint8_t> TencentCompressionProvider::Compress(
    const float* data,
    size_t count,
    double* out_ratio
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    const size_t num_blocks = (count + config_.block_size - 1) / config_.block_size;
    std::vector<int8_t> quantized(count);
    std::vector<BlockMetadata> metadata(num_blocks);
    std::vector<uint8_t> compressed_data;
    
    stats_.original_bytes.fetch_add(count * sizeof(float));
    stats_.blocks_processed.fetch_add(num_blocks);
    
    // Process each block
    #pragma omp parallel for schedule(dynamic, 4)
    for (size_t block_idx = 0; block_idx < num_blocks; ++block_idx) {
        const size_t block_start = block_idx * config_.block_size;
        const size_t block_count = std::min(config_.block_size, count - block_start);
        
        BlockMetadata& meta = metadata[block_idx];
        meta.original_size = block_count * sizeof(float);
        
        // Step 1: Quantize
        float scale = 0.0f, zero_point = 0.0f;
        if (config_.quantization_bits == 4) {
            QuantizeBlock_Q4_0_AVX512(
                &data[block_start],
                &quantized[block_start],
                block_count,
                scale,
                zero_point
            );
        }
        
        meta.scale = scale;
        meta.zero_point = zero_point;
        
        // Step 2: Detect sparsity
        std::vector<bool> is_nonzero(block_count);
        float sparsity_ratio = 0.0f;
        if (config_.use_sparse_representation) {
            DetectSparsity_AVX512(
                &data[block_start],
                block_count,
                is_nonzero.data(),
                sparsity_ratio
            );
        }
        
        meta.sparsity_ratio = sparsity_ratio;
        
        // Step 3: Encode
        std::vector<uint8_t> block_compressed;
        if (sparsity_ratio > 0.3f) {
            if (config_.use_huffman_coding) {
                HuffmanNode* root = nullptr;
                std::unordered_map<int, std::vector<bool>> codes;
                BuildHuffmanTree(
                    &quantized[block_start],
                    block_count,
                    root,
                    codes
                );
                
                HuffmanEncode(
                    &quantized[block_start],
                    block_count,
                    codes,
                    block_compressed
                );
                
                meta.encoding_type = 1;  // Huffman
                if (root) delete root;
            } else {
                block_compressed.assign(
                    reinterpret_cast<const uint8_t*>(&quantized[block_start]),
                    reinterpret_cast<const uint8_t*>(&quantized[block_start] + block_count)
                );
                meta.encoding_type = 0;
            }
        } else {
            SparseEncode(
                &quantized[block_start],
                block_count,
                is_nonzero.data(),
                block_compressed
            );
        }
        
        meta.compressed_size = static_cast<uint32_t>(block_compressed.size());
        
        #pragma omp critical
        {
            compressed_data.insert(
                compressed_data.end(),
                block_compressed.begin(),
                block_compressed.end()
            );
        }
    }
    
    // Serialize metadata
    std::vector<uint8_t> metadata_bytes = SerializeMetadata(metadata);
    
    // Final format: [metadata_size(4)][metadata][compressed_blocks...]
    std::vector<uint8_t> final_compressed;
    const uint32_t meta_size = static_cast<uint32_t>(metadata_bytes.size());
    
    final_compressed.reserve(sizeof(meta_size) + meta_size + compressed_data.size());
    
    final_compressed.insert(
        final_compressed.end(),
        reinterpret_cast<const uint8_t*>(&meta_size),
        reinterpret_cast<const uint8_t*>(&meta_size) + sizeof(meta_size)
    );
    
    final_compressed.insert(
        final_compressed.end(),
        metadata_bytes.begin(),
        metadata_bytes.end()
    );
    
    final_compressed.insert(
        final_compressed.end(),
        compressed_data.begin(),
        compressed_data.end()
    );
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time
    );
    
    stats_.compression_time_us.fetch_add(duration.count());
    stats_.compressed_bytes.fetch_add(final_compressed.size());
    
    if (out_ratio) {
        *out_ratio = static_cast<double>(count * sizeof(float)) / final_compressed.size();
    }
    
    qDebug() << "Tencent compression: ratio =" 
             << static_cast<double>(count * sizeof(float)) / final_compressed.size();
    
    return final_compressed;
}

bool TencentCompressionProvider::DecompressToQuantized(
    const std::vector<uint8_t>& compressed,
    int8_t* quantized_output,
    size_t count
) {
    if (compressed.size() < sizeof(uint32_t)) {
        return false;
    }
    
    // Read metadata size
    uint32_t meta_size;
    memcpy(&meta_size, compressed.data(), sizeof(meta_size));
    
    if (compressed.size() < sizeof(meta_size) + meta_size) {
        return false;
    }
    
    // Deserialize metadata
    std::vector<uint8_t> metadata_bytes(
        compressed.begin() + sizeof(meta_size),
        compressed.begin() + sizeof(meta_size) + meta_size
    );
    
    std::vector<BlockMetadata> metadata;
    if (!DeserializeMetadata(metadata_bytes, metadata)) {
        return false;
    }
    
    // Decompress each block
    size_t out_pos = 0;
    size_t comp_pos = sizeof(meta_size) + meta_size;
    
    for (const auto& meta : metadata) {
        if (comp_pos + meta.compressed_size > compressed.size()) {
            break;
        }
        
        const uint8_t* block_data = compressed.data() + comp_pos;
        
        // For simplicity, assume sparse or raw for now
        size_t block_count = std::min((size_t)meta.original_size, count - out_pos);
        memcpy(&quantized_output[out_pos], block_data, std::min((size_t)meta.compressed_size, block_count));
        
        out_pos += block_count;
        comp_pos += meta.compressed_size;
    }
    
    return true;
}

bool TencentCompressionProvider::DecompressToFloat(
    const std::vector<uint8_t>& compressed,
    float* float_output,
    size_t count
) {
    // Allocate temp quantized buffer
    std::vector<int8_t> quantized(count);
    
    if (!DecompressToQuantized(compressed, quantized.data(), count)) {
        return false;
    }
    
    // Dequantize (stub - real impl would use stored scale/zero_point)
    for (size_t i = 0; i < count; ++i) {
        float_output[i] = static_cast<float>(quantized[i]) / 127.0f;
    }
    
    return true;
}

void TencentCompressionProvider::QuantizeBlock_Q4_0_AVX512(
    const float* input,
    int8_t* output,
    size_t count,
    float& scale,
    float& zero_point
) {
    const size_t simd_width = 16;
    __m512 absmax = _mm512_set1_ps(0.0f);
    
    // Find absolute maximum
    size_t i = 0;
    for (; i <= count - simd_width; i += simd_width) {
        __m512 val = _mm512_loadu_ps(&input[i]);
        __m512 abs_val = _mm512_abs_ps(val);
        absmax = _mm512_max_ps(absmax, abs_val);
    }
    
    // Reduce to scalar
    __m256 max256 = _mm256_max_ps(
        _mm512_extractf32x8_ps(absmax, 0),
        _mm512_extractf32x8_ps(absmax, 1)
    );
    
    __m128 max128 = _mm_max_ps(
        _mm256_extractf128_ps(max256, 0),
        _mm256_extractf128_ps(max256, 1)
    );
    
    max128 = _mm_max_ps(max128, _mm_movehl_ps(max128, max128));
    max128 = _mm_max_ss(max128, _mm_shuffle_ps(max128, max128, 1));
    
    float max_val = _mm_cvtss_f32(max128);
    scale = max_val / 7.0f;
    zero_point = 0.0f;
    
    if (scale < 1e-6f) scale = 1.0f;
    
    // Quantize
    i = 0;
    for (; i <= count - simd_width; i += simd_width) {
        __m512 val = _mm512_loadu_ps(&input[i]);
        __m512 scaled = _mm512_div_ps(val, _mm512_set1_ps(scale));
        __m512i q32 = _mm512_cvttps_epi32(scaled);
        
        // Clamp and convert to int8
        for (int j = 0; j < 16; ++j) {
            int32_t q = _mm512_extract_epi32(q32, j);
            output[i + j] = static_cast<int8_t>(std::max(-128, std::min(127, q)));
        }
    }
    
    // Remainder
    for (; i < count; ++i) {
        float scaled = input[i] / scale;
        output[i] = static_cast<int8_t>(std::max(-128.0f, std::min(127.0f, scaled)));
    }
}

void TencentCompressionProvider::DetectSparsity_AVX512(
    const float* input,
    size_t count,
    bool* is_nonzero,
    float& sparsity_ratio
) {
    const size_t simd_width = 16;
    size_t nonzero_count = 0;
    
    size_t i = 0;
    for (; i <= count - simd_width; i += simd_width) {
        __m512 val = _mm512_loadu_ps(&input[i]);
        __m512 abs = _mm512_abs_ps(val);
        __m512 threshold = _mm512_set1_ps(config_.sparsity_threshold);
        
        __mmask16 mask = _mm512_cmp_ps_mask(abs, threshold, _CMP_GT_OQ);
        
        for (int j = 0; j < 16; ++j) {
            if (mask & (1 << j)) {
                is_nonzero[i + j] = true;
                nonzero_count++;
            } else {
                is_nonzero[i + j] = false;
            }
        }
    }
    
    for (; i < count; ++i) {
        if (std::abs(input[i]) > config_.sparsity_threshold) {
            is_nonzero[i] = true;
            nonzero_count++;
        } else {
            is_nonzero[i] = false;
        }
    }
    
    sparsity_ratio = (float)nonzero_count / count;
}

void TencentCompressionProvider::SparseEncode(
    const int8_t* quantized,
    size_t count,
    const bool* is_nonzero,
    std::vector<uint8_t>& output
) {
    uint32_t nonzero_count = 0;
    for (size_t i = 0; i < count; ++i) {
        if (is_nonzero[i]) nonzero_count++;
    }
    
    size_t pos = output.size();
    output.resize(pos + sizeof(nonzero_count) + nonzero_count * sizeof(uint32_t) + nonzero_count);
    
    memcpy(output.data() + pos, &nonzero_count, sizeof(nonzero_count));
    pos += sizeof(nonzero_count);
    
    for (size_t i = 0; i < count; ++i) {
        if (is_nonzero[i]) {
            uint32_t index = static_cast<uint32_t>(i);
            memcpy(output.data() + pos, &index, sizeof(index));
            pos += sizeof(index);
            output[pos++] = static_cast<uint8_t>(quantized[i]);
        }
    }
}

void TencentCompressionProvider::BuildHuffmanTree(
    const int8_t* data,
    size_t count,
    HuffmanNode*& root,
    std::unordered_map<int, std::vector<bool>>& codes
) {
    std::unordered_map<int, uint64_t> frequencies;
    for (size_t i = 0; i < count; ++i) {
        frequencies[data[i]]++;
    }
    
    auto cmp = [](HuffmanNode* a, HuffmanNode* b) {
        return a->frequency > b->frequency;
    };
    std::priority_queue<HuffmanNode*, std::vector<HuffmanNode*>, decltype(cmp)> pq(cmp);
    
    for (const auto& [symbol, freq] : frequencies) {
        HuffmanNode* node = new HuffmanNode;
        node->symbol = symbol;
        node->frequency = freq;
        pq.push(node);
    }
    
    while (pq.size() > 1) {
        HuffmanNode* left = pq.top(); pq.pop();
        HuffmanNode* right = pq.top(); pq.pop();
        
        HuffmanNode* parent = new HuffmanNode;
        parent->frequency = left->frequency + right->frequency;
        parent->left = left;
        parent->right = right;
        
        pq.push(parent);
    }
    
    root = pq.empty() ? nullptr : pq.top();
    
    std::function<void(HuffmanNode*, std::vector<bool>&)> generate_codes = 
        [&](HuffmanNode* node, std::vector<bool>& code) {
            if (!node->left && !node->right) {
                codes[node->symbol] = code;
                return;
            }
            
            if (node->left) {
                code.push_back(false);
                generate_codes(node->left, code);
                code.pop_back();
            }
            
            if (node->right) {
                code.push_back(true);
                generate_codes(node->right, code);
                code.pop_back();
            }
        };
    
    if (root) {
        std::vector<bool> empty_code;
        generate_codes(root, empty_code);
    }
}

void TencentCompressionProvider::HuffmanEncode(
    const int8_t* data,
    size_t count,
    const std::unordered_map<int, std::vector<bool>>& codes,
    std::vector<uint8_t>& output
) {
    size_t pos = output.size();
    output.resize(pos + (count * 8 + 7) / 8 + 512);
    
    uint8_t* buffer = output.data() + pos;
    size_t bit_pos = 0;
    
    for (size_t i = 0; i < count; ++i) {
        auto it = codes.find(data[i]);
        if (it == codes.end()) continue;
        
        for (bool bit : it->second) {
            size_t byte_idx = bit_pos / 8;
            size_t bit_idx = bit_pos % 8;
            
            if (bit_idx == 0) {
                buffer[byte_idx] = 0;
            }
            
            if (bit) {
                buffer[byte_idx] |= (1 << bit_idx);
            }
            
            bit_pos++;
        }
    }
    
    output.resize(pos + (bit_pos + 7) / 8);
}

std::vector<uint8_t> TencentCompressionProvider::SerializeMetadata(
    const std::vector<BlockMetadata>& metadata
) {
    std::vector<uint8_t> serialized;
    serialized.reserve(metadata.size() * sizeof(BlockMetadata));
    
    for (const auto& meta : metadata) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&meta);
        serialized.insert(serialized.end(), bytes, bytes + sizeof(meta));
    }
    
    return serialized;
}

bool TencentCompressionProvider::DeserializeMetadata(
    const std::vector<uint8_t>& serialized,
    std::vector<BlockMetadata>& metadata
) {
    size_t num_blocks = serialized.size() / sizeof(BlockMetadata);
    metadata.resize(num_blocks);
    
    for (size_t i = 0; i < num_blocks; ++i) {
        memcpy(&metadata[i], serialized.data() + i * sizeof(BlockMetadata), sizeof(BlockMetadata));
    }
    
    return true;
}

void TencentCompressionProvider::DeltaEncode(int8_t* data, size_t count) {
    if (count == 0) return;
    for (size_t i = count - 1; i > 0; --i) {
        data[i] = data[i] - data[i-1];
    }
}

void TencentCompressionProvider::DeltaDecode(
    const int8_t* delta_encoded,
    size_t count,
    int8_t* output
) {
    if (count == 0) return;
    output[0] = delta_encoded[0];
    for (size_t i = 1; i < count; ++i) {
        output[i] = output[i-1] + delta_encoded[i];
    }
}

void TencentCompressionProvider::HuffmanDecode(
    const std::vector<uint8_t>& encoded,
    const HuffmanNode* root,
    size_t count,
    int8_t* output
) {
    if (!root) return;
    
    size_t out_idx = 0;
    const HuffmanNode* current = root;
    
    for (size_t byte_idx = 0; byte_idx < encoded.size() && out_idx < count; ++byte_idx) {
        uint8_t byte = encoded[byte_idx];
        
        for (int bit_idx = 0; bit_idx < 8 && out_idx < count; ++bit_idx) {
            bool bit = (byte >> bit_idx) & 1;
            
            if (bit) {
                current = current->right;
            } else {
                current = current->left;
            }
            
            if (current && !current->left && !current->right) {
                output[out_idx++] = static_cast<int8_t>(current->symbol);
                current = root;
            }
        }
    }
}

} // namespace RawrXD
