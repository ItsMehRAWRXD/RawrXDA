#pragma once
#include <vector>
#include <memory>
#include <list>
#include <iostream>

namespace TitanKV {

// Block size (number of tokens per block)
constexpr int BLOCK_SIZE = 16;
// Hidden dimension
constexpr int HIDDEN_DIM = 4096;
// Number of heads
constexpr int NUM_HEADS = 32;
// Head dimension
constexpr int HEAD_DIM = HIDDEN_DIM / NUM_HEADS;

// KVCacheBlock: Stores K and V for a chunk of tokens
struct KVCacheBlock {
    // Layout: [BLOCK_SIZE, NUM_HEADS, HEAD_DIM]
    // Using simple flat vector for demonstration, aligned Memory in production
    std::vector<float> k_data;
    std::vector<float> v_data;
    int token_count = 0; // Usage within this block

    KVCacheBlock() {
        k_data.resize(BLOCK_SIZE * NUM_HEADS * HEAD_DIM);
        v_data.resize(BLOCK_SIZE * NUM_HEADS * HEAD_DIM);
    }
};

// Logical to Physical mapping
using BlockTable = std::vector<int>;

class BlockManager {
private:
    std::vector<std::unique_ptr<KVCacheBlock>> physical_blocks;
    std::list<int> free_blocks;

public:
    BlockManager(int num_blocks) {
        physical_blocks.reserve(num_blocks);
        for (int i = 0; i < num_blocks; ++i) {
            physical_blocks.push_back(std::make_unique<KVCacheBlock>());
            free_blocks.push_back(i);
        }
    }

    int allocate() {
        if (free_blocks.empty()) {
            throw std::runtime_error("OOM: No free KV cache blocks");
        }
        int id = free_blocks.front();
        free_blocks.pop_front();
        physical_blocks[id]->token_count = 0;
        return id;
    }

    void free(int block_id) {
        free_blocks.push_back(block_id);
    }

    KVCacheBlock* get(int block_id) {
        if (block_id >= 0 && block_id < physical_blocks.size()) {
            return physical_blocks[block_id].get();
        }
        return nullptr;
    }
};

class PagedKVCache {
private:
    BlockManager& manager;
    BlockTable block_table; // Maps logical block index -> physical block index
    int current_logical_len = 0;

public:
    PagedKVCache(BlockManager& mgr) : manager(mgr) {}

    // Append a token's KV to the cache
    void append_token(const std::vector<float>& k, const std::vector<float>& v) {
        int logical_block_idx = current_logical_len / BLOCK_SIZE;
        int offset_in_block = current_logical_len % BLOCK_SIZE;

        if (block_table.size() <= logical_block_idx) {
            // Allocate new block
            int new_physical = manager.allocate();
            block_table.push_back(new_physical);
        }

        int physical_id = block_table[logical_block_idx];
        KVCacheBlock* block = manager.get(physical_id);
        
        // Copy data into block (Simulated memcpy)
        // [offset, :, :]
        size_t matrix_size = NUM_HEADS * HEAD_DIM;
        // Simple copy for demonstration
        // In AVX-512 version, this would be vectorized
        // We assume k/v are [NUM_HEADS * HEAD_DIM] flat
        
        // Destination offset: offset_in_block * matrix_size
        size_t dest_offset = offset_in_block * matrix_size;
        
        // Safety check
        if (dest_offset + matrix_size <= block->k_data.size()) {
            std::copy(k.begin(), k.end(), block->k_data.begin() + dest_offset);
            std::copy(v.begin(), v.end(), block->v_data.begin() + dest_offset);
            block->token_count++;
        }
        
        current_logical_len++;
    }

    // Get current KV data for attention (Logical view)
    // In optimized inference, the attention kernel reads from the block table directly
    // This function returns the metadata needed for Paged Attention
    const BlockTable& get_block_table() const {
        return block_table;
    }

    int get_context_len() const { return current_logical_len; }
};

} // namespace TitanKV
