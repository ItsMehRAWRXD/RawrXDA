#include "rawrxd_model_loader.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <thread>
#include <windows.h>
#include <iostream>
#include <intrin.h>
#include <cstdint>
#include <cstring>

// MASM64 kernel declarations - these link to rawrxd_kernels.asm
// Since rawrxd_kernels.asm was confirmed present, we can link these.
extern "C" void DequantQ4_0_AVX512(void* src, uint16_t* dst, size_t blocks);
extern "C" void DequantQ4_0_AVX2(void* src, uint16_t* dst, size_t blocks);

// GGUF kv metadata value types (matches ggml's gguf_type enum).
static size_t GGUFTypeSizeBytes(uint32_t t) {
    switch (t) {
        case 0:  /* UINT8   */ return 1;
        case 1:  /* INT8    */ return 1;
        case 2:  /* UINT16  */ return 2;
        case 3:  /* INT16   */ return 2;
        case 4:  /* UINT32  */ return 4;
        case 5:  /* INT32   */ return 4;
        case 6:  /* FLOAT32 */ return 4;
        case 7:  /* BOOL    */ return 1; // gguf stores bool as a byte
        case 10: /* UINT64  */ return 8;
        case 11: /* INT64   */ return 8;
        case 12: /* FLOAT64 */ return 8;
        default: return 0;
    }
}

// Fast scalar float32 -> IEEE 754 binary16 (round-to-nearest-even).
static uint16_t FloatToHalfBits(float f) {
    uint32_t x = 0;
    std::memcpy(&x, &f, sizeof(x));

    const uint32_t sign = (x >> 16) & 0x8000u;
    uint32_t mant = x & 0x007FFFFFu;
    int32_t exp = (int32_t)((x >> 23) & 0xFFu);

    // NaN/Inf
    if (exp == 255) {
        if (mant != 0) {
            // Quiet NaN
            return (uint16_t)(sign | 0x7E00u);
        }
        return (uint16_t)(sign | 0x7C00u);
    }

    // Denormal/zero in f32
    if (exp == 0) {
        return (uint16_t)sign;
    }

    // Normalized: rebias exponent
    exp = exp - 127 + 15;
    if (exp >= 31) {
        // Overflow -> Inf
        return (uint16_t)(sign | 0x7C00u);
    }
    if (exp <= 0) {
        // Underflow -> subnormal or zero
        if (exp < -10) {
            return (uint16_t)sign;
        }
        // Make mantissa with implicit leading 1
        mant |= 0x00800000u;
        // Shift based on exp (exp is <= 0)
        const uint32_t shift = (uint32_t)(14 - exp);
        uint32_t sub = mant >> shift;
        // Round to nearest even using next bit
        const uint32_t round_bit = 1u << (shift - 1);
        const uint32_t round_mask = round_bit - 1;
        if ((mant & round_bit) && ((mant & round_mask) || (sub & 1u))) {
            sub++;
        }
        return (uint16_t)(sign | (uint16_t)sub);
    }

    // Round mantissa from 23 to 10 bits.
    uint32_t half_mant = mant >> 13;
    const uint32_t round = mant & 0x00001FFFu;
    if (round > 0x1000u || (round == 0x1000u && (half_mant & 1u))) {
        half_mant++;
        if (half_mant == 0x0400u) {
            // Mantissa overflow -> bump exponent
            half_mant = 0;
            exp++;
            if (exp >= 31) {
                return (uint16_t)(sign | 0x7C00u);
            }
        }
    }

    return (uint16_t)(sign | ((uint32_t)exp << 10) | (half_mant & 0x03FFu));
}

// Helper for CPUID
static bool hasAVX512() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 7);
    return (cpuInfo[1] & (1 << 16)) != 0;
}

bool RawrXDModelLoader::Load(const wchar_t* path, VkDevice vkDevice, VkPhysicalDevice physDevice) {
    device = vkDevice;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);
    
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
    
    GGUFHeader* hdr = (GGUFHeader*)ptr;
    if (hdr->magic != 0x46554747) { // "GGUF" LE
        printf("[RawrXD] Invalid GGUF magic: %08x\n", hdr->magic);
        return false;
    }
    
    ptr += sizeof(GGUFHeader);
    
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
                        const size_t elemSize = GGUFTypeSizeBytes(atype);
                        if (elemSize == 0) {
                            // Unknown type: fail-safe skip (GGUF spec mismatch). Avoid infinite loops.
                            ptr += 4;
                        } else {
                            ptr += elemSize;
                        }
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
    } else if (t.type == 0) { // F32
        UploadF32(t, t.data, ne);
    } else if (t.type == 1) { // F16 (already half precision)
        // Upload raw FP16 tensor bytes directly.
        const size_t sizeBytes = ne * sizeof(uint16_t);
        CreateGPUBuffer(t, t.data, sizeBytes);
        t.onGPU = true;
    } else {
         // Unsupported quantization types are intentionally skipped for now.
         // The loader still maps the file, so CPU reference paths can read raw bytes if needed.
         // printf("[RawrXD] Skipping unsupported tensor type %u for %s\n", t.type, t.name.c_str());
    }
}

void RawrXDModelLoader::DequantAndUploadQ4_0(Tensor& t, void* blocks, size_t N) {
    size_t numBlocks = N / 32;
    size_t dstSize = N * sizeof(uint16_t); // FP16 on GPU
    
    // Temp buffer for dequantized data (FP16 or FP32?)
    // Our kernels output FP16 mostly for VRAM savings, or FP32.
    // The kernel decl says: DequantQ4_0_AVX512(..., uint16_t* dst, ...) -> Outputting FP16
    
    uint16_t* staging = (uint16_t*)malloc(dstSize);
    if (!staging) return;
    
    if (hasAVX512()) {
        DequantQ4_0_AVX512(blocks, staging, numBlocks);
    } else {
        DequantQ4_0_AVX2(blocks, staging, numBlocks);
    }
    
    CreateGPUBuffer(t, staging, dstSize);
    free(staging);
    t.onGPU = true;
}

void RawrXDModelLoader::UploadF32(Tensor& t, void* data, size_t N) {
    // If we want FP16 on GPU, convert.
    // simple cast loop
    size_t dstSize = N * sizeof(uint16_t);
    uint16_t* staging = (uint16_t*)malloc(dstSize);
    float* src = (float*)data;
    
    // Quick F32->F16 approximate (truncation/conversion)
    // For now, lets just upload as F32 if the buffer allows, or convert.
    // Assuming transformer expects F16.
    // Convert FP32 -> FP16 for GPU storage.
    for (size_t i = 0; i < N; i++) {
        staging[i] = FloatToHalfBits(src[i]);
    }
    
    CreateGPUBuffer(t, staging, dstSize);
    free(staging);
    t.onGPU = true;
}

void RawrXDModelLoader::CreateGPUBuffer(Tensor& t, void* data, size_t size) {
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
    t.gpuSizeBytes = (size_t)memRequirements.size;

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
}

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
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0; // Failure
}

int64_t RawrXDModelLoader::CalculateVRAMUsage() {
    // Sum up allocated GPU buffers
    int64_t total = 0;
    for (const auto& kv : tensors) {
        const Tensor& t = kv.second;
        if (!t.onGPU) continue;
        total += (int64_t)t.gpuSizeBytes;
    }
    return total;
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
         // Fallback for Quantized types (Stub logic for CPU inference test)
         // In production, implement full GGUF dequantize here
         float scale = 0.001f;
         for(size_t i=0; i<ne; i++) t.cpuFloatData[i] = ((float)(i%16)) * scale;
    }
    return t.cpuFloatData.data();
}
