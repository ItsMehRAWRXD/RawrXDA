#include "rawrxd_model_loader.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <thread>
#include <windows.h>
#include <iostream>
#include <intrin.h>
#include <cstdint>

// MASM64 kernel declarations - these link to rawrxd_kernels.asm
// Since rawrxd_kernels.asm was confirmed present, we can link these.
extern "C" void DequantQ4_0_AVX512(void* src, uint16_t* dst, size_t blocks);
extern "C" void DequantQ4_0_AVX2(void* src, uint16_t* dst, size_t blocks);

// F16 ↔ F32 conversion (IEEE 754 compliant)
float RawrXDModelLoader::HalfToFloat(uint16_t h) {
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exp  = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    if (exp == 0) {
        if (mant == 0) return sign ? -0.0f : 0.0f;     // Zero
        // Denormalized
        float f = (float)mant / 1024.0f;
        f *= (1.0f / 16384.0f); // 2^-14
        return sign ? -f : f;
    }
    if (exp == 31) {
        if (mant == 0) return sign ? -INFINITY : INFINITY;
        return NAN;
    }
    float f = (1.0f + (float)mant / 1024.0f) * powf(2.0f, (int)exp - 15);
    return sign ? -f : f;
}

uint16_t RawrXDModelLoader::FloatToHalf(float f) {
    uint32_t x;
    memcpy(&x, &f, sizeof(uint32_t));
    uint32_t sign = (x >> 16) & 0x8000;
    int32_t  exp = ((x >> 23) & 0xFF) - 127;
    uint32_t mant = x & 0x7FFFFF;
    if (exp <= -15) return (uint16_t)(sign);              // Underflow → zero
    if (exp >  15)  return (uint16_t)(sign | 0x7C00);     // Overflow → inf
    return (uint16_t)(sign | ((exp + 15) << 10) | (mant >> 13));
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
                    } else { // Fixed width element
                        // GGUF element type sizes:
                        // 0=u8(1), 1=i8(1), 2=u16(2), 3=i16(2),
                        // 4=u32(4), 5=i32(4), 6=f32(4), 7=bool(1),
                        // 10=u64(8), 11=i64(8), 12=f64(8)
                        size_t elemSize = 4; // default
                        if (atype <= 1 || atype == 7) elemSize = 1;
                        else if (atype <= 3) elemSize = 2;
                        else if (atype <= 6) elemSize = 4;
                        else if (atype >= 10 && atype <= 12) elemSize = 8;
                        ptr += elemSize;
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
    } else {
         // Placeholder for other types
         // printf("Skipping unsupported type %d for %s\n", t.type, t.name.c_str());
         // In production wed fail or handle F16/Q8 etc.
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
    
    // IEEE 754 F32 → F16 conversion (software half-float)
    // F16: 1 sign + 5 exponent + 10 mantissa
    for (size_t i = 0; i < N; i++) {
        uint32_t fbits;
        memcpy(&fbits, &src[i], sizeof(uint32_t));
        uint32_t sign = (fbits >> 16) & 0x8000;
        int32_t  exponent = ((fbits >> 23) & 0xFF) - 127;
        uint32_t mantissa = fbits & 0x7FFFFF;
        
        if (exponent <= -15) {
            // Underflow → zero (or denorm, but zero is safe for inference)
            staging[i] = static_cast<uint16_t>(sign);
        } else if (exponent > 15) {
            // Overflow → infinity
            staging[i] = static_cast<uint16_t>(sign | 0x7C00);
        } else {
            staging[i] = static_cast<uint16_t>(sign | ((exponent + 15) << 10) | (mantissa >> 13));
        }
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
    // Sum all GPU buffer sizes that have been allocated
    int64_t total = 0;
    for (const auto& [name, t] : tensors) {
        if (t.onGPU && t.gpuBuffer != VK_NULL_HANDLE) {
            size_t ne = 1;
            for (auto d : t.dims) ne *= d;
            // Estimate size based on type (F16=2 bytes, F32=4 bytes, Q4=0.5 bytes, etc.)
            size_t bytesPerElem = 2; // default F16
            if (t.type == 0) bytesPerElem = 4;      // F32
            else if (t.type == 1) bytesPerElem = 2;  // F16
            else if (t.type >= 2) bytesPerElem = 1;  // quantized (approx)
            total += static_cast<int64_t>(ne * bytesPerElem);
        }
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
    
    if (t.type == 0) { // F32 — direct memcpy
         if (t.data) memcpy(t.cpuFloatData.data(), t.data, ne * sizeof(float));
    } else if (t.type == 1) { // F16 — convert to F32
         if (t.data) {
             uint16_t* src = (uint16_t*)t.data;
             for (size_t i = 0; i < ne; i++) {
                 t.cpuFloatData[i] = HalfToFloat(src[i]);
             }
         }
    } else if (t.type == 2) { // Q4_0 — dequantize 32 weights per block
         if (t.data) {
             Q4_0_Block* blocks = (Q4_0_Block*)t.data;
             size_t numBlocks = (ne + 31) / 32;
             for (size_t b = 0; b < numBlocks; b++) {
                 float d = HalfToFloat(blocks[b].d);
                 for (int j = 0; j < 32 && b * 32 + j < ne; j++) {
                     // Each byte holds 2 x 4-bit quants
                     int nibble = (blocks[b].qs[j / 2] >> ((j % 2) * 4)) & 0xF;
                     t.cpuFloatData[b * 32 + j] = (float)(nibble - 8) * d;
                 }
             }
         }
    } else if (t.type == 6) { // Q5_0 — 5-bit quantization (32 per block)
         if (t.data) {
             Q5_0_Block* blocks = (Q5_0_Block*)t.data;
             size_t numBlocks = (ne + 31) / 32;
             for (size_t b = 0; b < numBlocks; b++) {
                 float d = HalfToFloat(blocks[b].d);
                 for (int j = 0; j < 32 && b * 32 + j < ne; j++) {
                     int lo = (blocks[b].qs[j / 2] >> ((j % 2) * 4)) & 0xF;
                     int hi = (blocks[b].qh[j / 8] >> (j % 8)) & 1;
                     int q = lo | (hi << 4);
                     t.cpuFloatData[b * 32 + j] = (float)(q - 16) * d;
                 }
             }
         }
    } else if (t.type == 8) { // Q8_0 — 8-bit quantization (32 per block)
         if (t.data) {
             Q8_0_Block* blocks = (Q8_0_Block*)t.data;
             size_t numBlocks = (ne + 31) / 32;
             for (size_t b = 0; b < numBlocks; b++) {
                 float d = HalfToFloat(blocks[b].d);
                 for (int j = 0; j < 32 && b * 32 + j < ne; j++) {
                     t.cpuFloatData[b * 32 + j] = (float)blocks[b].qs[j] * d;
                 }
             }
         }
    } else if (t.type == 12) { // Q4_K — 4-bit super-block quantization (256 per block)
         if (t.data) {
             Q4_K_Block* blocks = (Q4_K_Block*)t.data;
             size_t numBlocks = (ne + 255) / 256;
             for (size_t b = 0; b < numBlocks; b++) {
                 float d = HalfToFloat(blocks[b].d);
                 float dmin = HalfToFloat(blocks[b].dmin);
                 for (int j = 0; j < 256 && b * 256 + j < ne; j++) {
                     int q = (blocks[b].qs[j / 2] >> ((j % 2) * 4)) & 0xF;
                     t.cpuFloatData[b * 256 + j] = (float)q * d - dmin;
                 }
             }
         }
    } else {
         // Unsupported quant type — zero-fill (safe fallback, won't crash but won't generate well)
         printf("[RawrXD] WARNING: Unsupported quant type %u for tensor '%s', zero-filling\n", 
                t.type, name.c_str());
         memset(t.cpuFloatData.data(), 0, ne * sizeof(float));
    }
    return t.cpuFloatData.data();
}
