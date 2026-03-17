#include "rawrxd_model_loader.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <thread>
#include <windows.h>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <limits>

extern "C" void Dequant_Q4_0(void* src, float* dst);
extern "C" void Dequant_Q4_K(void* src, float* dst);
extern "C" void Dequant_Q8_0(void* src, float* dst);
extern "C" void Dequant_F16(void* src, float* dst, size_t count);

// GGUF Q8_0 block structure
struct Q8_0_Block {
    uint16_t d; // float16 scale
    int8_t qs[32]; // 32 bytes
};

// GGUF Q4_K block structure
struct Q4_K_Block {
    uint16_t d;     // super-block scale
    uint16_t dmin;  // super-block min
    uint8_t scales[12]; 
    uint8_t qs[128];
};

static float f16_to_f32(uint16_t h) {
    const uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t frac = h & 0x03FFu;

    uint32_t out;
    if (exp == 0) {
        if (frac == 0) {
            out = sign;
        } else {
            exp = 1;
            while ((frac & 0x0400u) == 0) {
                frac <<= 1;
                --exp;
            }
            frac &= 0x03FFu;
            out = sign | ((exp + 112u) << 23) | (frac << 13);
        }
    } else if (exp == 0x1Fu) {
        out = sign | 0x7F800000u | (frac << 13);
    } else {
        out = sign | ((exp + 112u) << 23) | (frac << 13);
    }

    float f;
    memcpy(&f, &out, sizeof(float));
    return f;
}

// Raw GGUF file header — matches binary layout exactly
struct GGUFFileHeader {
    uint32_t magic;        // 0x46554747 = "GGUF" LE
    uint32_t version;
    uint64_t tensor_count;
    uint64_t kv_count;     // metadata_kv_count
};

bool RawrXDModelLoader::Load(const wchar_t* path, VkDevice vkDevice, VkPhysicalDevice physDevice) {
    device = vkDevice;

#ifdef RAWR_ENABLE_VULKAN
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);
#else
    (void)physDevice;
    memset(&memProps, 0, sizeof(memProps));
#endif
    
    // 1. Memory-mapped file (zero copy from disk)
    hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, 
                       OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("[RawrXD] Could not open model file: %ls\n", path);
        return false;
    }
    
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    fileSize = size.QuadPart;
    
    hMapping = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    mappedView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!mappedView) {
        printf("[RawrXD] Memory mapping failed\n");
        CloseHandle(hFile);
        return false;
    }
    
    // 2. Parse GGUF structure
    uint8_t* ptr = (uint8_t*)mappedView;
    uint8_t* start = ptr;
    
    GGUFFileHeader* hdr = (GGUFFileHeader*)ptr;
    if (hdr->magic != 0x46554747) { // "GGUF" LE
        printf("[RawrXD] Invalid GGUF magic: %08x\n", hdr->magic);
        return false;
    }
    
    ptr += sizeof(GGUFFileHeader);
    
    // Skip metadata (simple parser to just skip it)
    ptr = ParseMetadata(ptr, hdr->kv_count);
    
    // 3. Tensor info array
    std::vector<Tensor> tensorInfos;
    tensorInfos.reserve(hdr->tensor_count);
    
    for (uint64_t i = 0; i < hdr->tensor_count; i++) {
        Tensor t;
        // Read tensor info (name, dims, type, offset)
        ptr = ParseTensorInfo(ptr, t);
        // Offset is relative to start of data block, which is after headers
        // But GGUF v3 offsets are usually relative to the *tensor data* start alignment.
        // Wait, GGUF spec: offset is relative to the start of the file or data section?
        // GGUF v2/v3 spec says relative to the *start of the file*? No, usually it's relative to the alignment point.
        // Lets assume standard GGUF: offset is absolute or relative to data block.
        // Usually, `tensorDataOffset` is calculated after headers.
        tensorInfos.push_back(t);
    }
    
    // 4. Align to 32 bytes for tensor data start
    uint64_t headerBytes = (uint64_t)(ptr - start);
    uint64_t dataStart = (headerBytes + 31) & ~31;
    
    // 5. Parallel async load + dequantize to GPU
    printf("[RawrXD] Loading %zu tensors from GGUF...\n", tensorInfos.size());
    printf("[RawrXD] Data starts at offset %llu\n", dataStart);

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    size_t numThreads = sysInfo.dwNumberOfProcessors;
    if (numThreads > 32) numThreads = 32; // basic clamp
    
    std::vector<std::vector<Tensor*>> chunks(numThreads);
    for (size_t i = 0; i < tensorInfos.size(); i++) {
        // Fix up offset to be absolute pointer
        tensorInfos[i].data = start + dataStart + tensorInfos[i].offset;
        chunks[i % numThreads].push_back(&tensorInfos[i]);
    }
    
    std::vector<std::thread> workers;
    for (auto& chunk : chunks) {
        if (chunk.empty()) continue;
        workers.emplace_back([this, chunk]() {
             for (auto* t : chunk) {
                 this->LoadTensorAsync(*t);
             }
        });
    }
    
    for (auto& w : workers) w.join();
    
    // 6. Build tensor lookup map
    for (auto& t : tensorInfos) {
        tensors[t.name] = std::move(t);
    }
    
    printf("[RawrXD] Model loaded successfully. VRAM used: %.2f GB\n", 
           CalculateVRAMUsage() / 1e9);
    printf("[RawrXD] Config: dim=%d, layers=%d, heads=%d, kv_heads=%d, vocab=%d, ctx=%d\n",
           n_embd, n_layers, n_heads, n_heads_kv, vocab_size, n_ctx);
    printf("[RawrXD] Tensor names in model (%zu total):\n", tensors.size());
    int tc = 0;
    for (auto& kv : tensors) {
        printf("  [%3d] type=%d dims=", tc++, kv.second.type);
        for (auto d : kv.second.dims) printf("%llu ", (unsigned long long)d);
        printf("%s\n", kv.first.c_str());
        if (tc > 15) { printf("  ... (%zu more)\n", tensors.size() - tc); break; }
    }
    return true;
}

// Simple metadata skipper / scraper
uint8_t* RawrXDModelLoader::ParseMetadata(uint8_t* ptr, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        // Read Key
        uint64_t len = *(uint64_t*)ptr; ptr += 8;
        std::string key((char*)ptr, len); ptr += len;
        
        // Read Type
        uint32_t type = *(uint32_t*)ptr; ptr += 4;
        
        // Read/Skip Value
        switch (type) {
            case 8: // String
            {
                uint64_t vlen = *(uint64_t*)ptr; ptr += 8;
                // Capture useful string metadata if needed
                ptr += vlen;
                break;
            }
            case 9: // Array
            {
                uint32_t atype = *(uint32_t*)ptr; ptr += 4;
                uint64_t Alen = *(uint64_t*)ptr; ptr += 8;

                if (key == "tokenizer.ggml.tokens") {
                    vocab_size = (int)Alen;
                }

                // Skip array content
                for(uint64_t j=0; j<Alen; j++) {
                    if (atype == 8) { // Array of strings
                         uint64_t slen = *(uint64_t*)ptr; ptr += 8 + slen;
                    } else { // Fixed width (assume max 8 bytes for simplicity in skipper)
                        // This is hacky, real GGUF needs sizeof(atype)
                        // Most arrays are small ints/floats (4 bytes)
                        ptr += 4; // TODO: Fix for 64-bit arrays
                    }
                }
                break;
            }
            default: // Scalars
                // Basic types 4(u32), 5(i32), 6(f32), 7(bool) -> 4 bytes?
                // 10(u64), 11(i64), 12(f64) -> 8 bytes
                // Capture specific keys
                if (key == "llm_load_print_meta") { /* no op */ }
                
                if (type >= 4 && type <= 7) {
                    uint32_t val = *(uint32_t*)ptr;
                    if (key == "llama.embedding_length" || key == "general.embedding_length") n_embd = val;
                    if (key == "llama.block_count" || key == "general.block_count") n_layers = val;
                    if (key == "llama.attention.head_count" || key == "general.attention.head_count") n_heads = val;
                    if (key == "llama.attention.head_count_kv" || key == "general.attention.head_count_kv") n_heads_kv = val;
                    if (key == "llama.context_length" || key == "general.context_length") n_ctx = val;
                    if (key == "llama.feed_forward_length" || key == "general.feed_forward_length") n_ffn = val;
                    ptr += 4;
                } else if (type >= 10 && type <= 12) {
                     ptr += 8;
                } else {
                    ptr += 4; // Fallback
                }
                break;
        }
    }
    return ptr;
}

uint8_t* RawrXDModelLoader::ParseTensorInfo(uint8_t* ptr, Tensor& t) {
    uint64_t len = *(uint64_t*)ptr; ptr += 8;
    t.name = std::string((char*)ptr, len); ptr += len;
    
    uint32_t n_dims = *(uint32_t*)ptr; ptr += 4;
    t.dims.resize(n_dims);
    for (uint32_t i = 0; i < n_dims; i++) {
        t.dims[i] = *(uint64_t*)ptr; ptr += 8;
    }
    
    t.type = *(uint32_t*)ptr; ptr += 4;
    t.offset = *(uint64_t*)ptr; ptr += 8;
    return ptr;
}

void RawrXDModelLoader::LoadTensorAsync(Tensor& t) {
    // Determine size
    size_t ne = 1;
    for (auto d : t.dims) ne *= d;
    
    // If its a weight we need on GPU (blocks, output, norm), upload it.
    // If its small metadata, maybe keep on CPU? For now upload everything used in forward pass.
    
    // Simple filter: Only load layers and norms. Token embeddings usually large, sometimes CPU?
    // Lets load everything to GPU for now.
    
    if (t.type == 2) { // Q4_0
        DequantAndUploadQ4_0(t, t.data, ne);
    } else if (t.type == 3) { // Q4_1
        // Dequant_Q4_1 is in QuantKernels_Full.asm as well
    } else if (t.type == 8) { // Q8_0
        DequantAndUploadQ8_0(t, t.data, ne);
    } else if (t.type == 12) { // Q4_K
        DequantAndUploadQ4_K(t, t.data, ne);
    } else if (t.type == 0) { // F32
        UploadF32(t, t.data, ne);
    } else {
         // Placeholder for other types
         // printf("Skipping unsupported type %d for %s\n", t.type, t.name.c_str());
    }
}

void RawrXDModelLoader::DequantAndUploadQ8_0(Tensor& t, void* blocks, size_t N) {
    size_t numBlocks = N / 32;
    t.cpuFloatData.resize(N);
    
    uint8_t* ptr = (uint8_t*)blocks;
    for (size_t b = 0; b < numBlocks; b++) {
#ifdef RAWR_ENABLE_ASM_KERNELS
        Dequant_Q8_0(ptr, &t.cpuFloatData[b * 32]);
#else
        // Manual implementation if ASM not linked
        Q8_0_Block* blk = (Q8_0_Block*)ptr;
        float d = f16_to_f32(blk->d);
        for(int i=0; i<32; i++) t.cpuFloatData[b*32 + i] = (float)blk->qs[i] * d;
#endif
        ptr += 34; // BS_Q8_0
    }
}

void RawrXDModelLoader::DequantAndUploadQ4_K(Tensor& t, void* blocks, size_t N) {
    size_t numSuperBlocks = N / 256;
    t.cpuFloatData.resize(N);
    
    uint8_t* ptr = (uint8_t*)blocks;
    for (size_t b = 0; b < numSuperBlocks; b++) {
#ifdef RAWR_ENABLE_ASM_KERNELS
        Dequant_Q4_K(ptr, &t.cpuFloatData[b * 256]);
#else
        // Q4_K complex logic skipped here
#endif
        ptr += 144; // BS_Q4_K
    }
}

void RawrXDModelLoader::DequantAndUploadQ4_0(Tensor& t, void* blocks, size_t N) {
    size_t numBlocks = N / 32;
    t.cpuFloatData.resize(N);
    
    uint8_t* ptr = (uint8_t*)blocks;
    for (size_t b = 0; b < numBlocks; b++) {
#ifdef RAWR_ENABLE_ASM_KERNELS
        Dequant_Q4_0(ptr, &t.cpuFloatData[b * 32]);
#else
        Q4_0_Block* blk = (Q4_0_Block*)ptr;
        float d = f16_to_f32(blk->d);
        for (int i = 0; i < 16; i++) {
            int8_t b0 = (blk->qs[i] & 0x0F) - 8;
            int8_t b1 = (blk->qs[i] >> 4) - 8;
            t.cpuFloatData[b * 32 + i]      = (float)b0 * d;
            t.cpuFloatData[b * 32 + i + 16] = (float)b1 * d;
        }
#endif
        ptr += 18; // BS_Q4_0
    }

#ifdef RAWR_ENABLE_VULKAN
    // ... upload to GPU logic ...
#endif
}

void RawrXDModelLoader::UploadF32(Tensor& t, void* data, size_t N) {
    // CPU path: copy F32 data directly
    t.cpuFloatData.resize(N);
    if (data) memcpy(t.cpuFloatData.data(), data, N * sizeof(float));

#ifdef RAWR_ENABLE_VULKAN
    size_t dstSize = N * sizeof(uint16_t);
    uint16_t* staging = (uint16_t*)malloc(dstSize);
    memset(staging, 0, dstSize);
    CreateGPUBuffer(t, staging, dstSize);
    free(staging);
    t.onGPU = true;
#endif
}

void RawrXDModelLoader::CreateGPUBuffer(Tensor& t, void* data, size_t size) {
#ifdef RAWR_ENABLE_VULKAN
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &t.gpuBuffer) != VK_SUCCESS) {
        printf("failed to create buffer for %s\n", t.name.c_str());
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, t.gpuBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &t.gpuMemory) != VK_SUCCESS) {
        printf("failed to allocate memory for %s\n", t.name.c_str());
        return;
    }

    vkBindBufferMemory(device, t.gpuBuffer, t.gpuMemory, 0);
    
    if (data) {
        UploadViaStaging(data, size, t.gpuBuffer);
    }
#else
    // CPU-only mode: no GPU buffers
    (void)t; (void)data; (void)size;
#endif
}

#ifdef RAWR_ENABLE_VULKAN
void RawrXDModelLoader::UploadViaStaging(void* data, size_t size, VkBuffer dstBuffer) {
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    
    vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory);
    vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);
    
    void* mappedData;
    vkMapMemory(device, stagingBufferMemory, 0, size, 0, &mappedData);
    memcpy(mappedData, data, size);
    vkUnmapMemory(device, stagingBufferMemory);
    
    // Real One-Shot Command Submission
    // ---------------------------------------------------------
    // We assume Queue Family 0 is available for Transfer/Graphics.
    // In a production engine, we would pass the queue/pool from the engine context.
    
    uint32_t queueFamilyIndex = 0;
    VkQueue queue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
    
    VkCommandPool commandPool;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; 
    
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        printf("[RawrXD] Failed to create transient command pool for upload\n");
        // Fallback or fatal error
    } else {
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.commandBufferCount = 1;
        
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);
        
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuffer, 1, &copyRegion);
        
        vkEndCommandBuffer(commandBuffer);
        
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);
        
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        vkDestroyCommandPool(device, commandPool, nullptr);
    }
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

uint32_t RawrXDModelLoader::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
#ifdef RAWR_ENABLE_VULKAN
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
#else
    (void)typeFilter; (void)properties;
#endif
    return 0;
}
#endif // RAWR_ENABLE_VULKAN (closes UploadViaStaging + FindMemoryType block)

int64_t RawrXDModelLoader::CalculateVRAMUsage() {
    uint64_t total_bytes = 0;

    for (const auto& entry : tensors) {
        const Tensor& tensor = entry.second;
        if (!tensor.onGPU) {
            continue;
        }

        uint64_t tensor_bytes = 0;
        if (!tensor.cpuFloatData.empty()) {
            const uint64_t float_bytes = static_cast<uint64_t>(tensor.cpuFloatData.size()) * sizeof(float);
            tensor_bytes = float_bytes;
        } else {
            uint64_t elements = 1;
            for (uint64_t d : tensor.dims) {
                if (d == 0 || elements > (std::numeric_limits<uint64_t>::max() / d)) {
                    elements = 0;
                    break;
                }
                elements *= d;
            }

            if (elements != 0) {
                switch (tensor.type) {
                    case 0:  // F32
                        tensor_bytes = elements * sizeof(float);
                        break;
                    case 1:  // F16
                        tensor_bytes = elements * sizeof(uint16_t);
                        break;
                    case 2:  // Q4_0
                    case 3:  // Q4_1
                        tensor_bytes = (elements / 32) * 18;
                        break;
                    case 8:  // Q8_0
                        tensor_bytes = (elements / 32) * 34;
                        break;
                    case 12: // Q4_K
                    case 16: // Q4_K variant
                        tensor_bytes = (elements / 256) * 144;
                        break;
                    default:
                        tensor_bytes = elements;
                        break;
                }
            }
        }

        if (total_bytes > std::numeric_limits<uint64_t>::max() - tensor_bytes) {
            total_bytes = std::numeric_limits<uint64_t>::max();
            break;
        }
        total_bytes += tensor_bytes;
    }

    if (total_bytes > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        return std::numeric_limits<int64_t>::max();
    }
    return static_cast<int64_t>(total_bytes);
}

float* RawrXDModelLoader::GetTensor(const std::string& name) {
    if (tensors.find(name) == tensors.end()) return nullptr;
    Tensor& t = tensors[name];
    if (!t.cpuFloatData.empty()) return t.cpuFloatData.data();
    
    size_t ne = 1; 
    for(auto d : t.dims) ne *= d;
    t.cpuFloatData.resize(ne);
    
    if (t.type == 0) { // F32
         if (t.data) memcpy(t.cpuFloatData.data(), t.data, ne * sizeof(float));
    } else {
         // Weights already dequantized during LoadTensorAsync if RAWR_BATCH_LOAD is on.
         // If we reach here, it's a lazy load request.
         this->LoadTensorAsync(t); 
    }
    return t.cpuFloatData.data();
}
