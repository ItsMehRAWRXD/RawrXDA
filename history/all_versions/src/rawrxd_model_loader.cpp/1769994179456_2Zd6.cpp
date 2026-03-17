#include "rawrxd_model_loader.h"
#include <iostream>
#include <algorithm>

RawrXDModelLoader::~RawrXDModelLoader() {
    if (mappedView) UnmapViewOfFile(mappedView);
    if (hMapping) CloseHandle(hMapping);
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    
    // Cleanup GPU memory
    if (device != VK_NULL_HANDLE) {
        for (auto& [name, tensor] : tensors) {
            if (tensor.gpuBuffer) vkDestroyBuffer(device, tensor.gpuBuffer, nullptr);
            if (tensor.gpuMemory) vkFreeMemory(device, tensor.gpuMemory, nullptr);
        }
    }
}

bool RawrXDModelLoader::Load(const wchar_t* path, VkDevice vkDevice, VkPhysicalDevice physDevice) {
    this->device = vkDevice;
    vkGetPhysicalDeviceMemoryProperties(physDevice, &memProps);
    
    hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    LARGE_INTEGER fs;
    GetFileSizeEx(hFile, &fs);
    fileSize = static_cast<size_t>(fs.QuadPart);
    
    hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) return false;
    
    mappedView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!mappedView) return false;
    
    // Parse GGUF
    uint8_t* ptr = (uint8_t*)mappedView;
    GGUFHeader* header = (GGUFHeader*)ptr;
    
    if (header->magic != 0x46554747) { // "GGUF" in LE
        // Try BE check if needed, but Windows is usually LE
        return false;
    }
    
    if (header->version < 2 || header->version > 3) {
        // Only support v2/v3 for now
        // return false; 
    }
    
    ptr += sizeof(GGUFHeader);
    
    // Parse KV pairs
    ptr = ParseMetadata(ptr, header->kv_count);
    
    // Parse Tensor Info
    for (uint64_t i = 0; i < header->tensor_count; i++) {
        // Name
        uint64_t nameLen = *(uint64_t*)ptr; ptr += 8;
        std::string name((char*)ptr, nameLen);
        ptr += nameLen;
        
        // Dims
        uint32_t n_dims = *(uint32_t*)ptr; ptr += 4;
        std::vector<uint64_t> dims;
        for (uint32_t d = 0; d < n_dims; d++) {
            dims.push_back(*(uint64_t*)ptr); ptr += 8;
        }
        
        // Type
        uint32_t type = *(uint32_t*)ptr; ptr += 4;
        
        // Offset
        uint64_t offset = *(uint64_t*)ptr; ptr += 8;
        
        Tensor t;
        t.name = name;
        t.type = type;
        t.dims = dims;
        t.offset = offset;
        
        // Base pointer calculation
        // GGUF tensor data starts aligned after all headers
        // But the "offset" is relative to the start of the data block?
        // Note: GGUF specific alignment rules.
        // We defer data pointer calculation until we find the start of data block.
        tensors[name] = t;
    }
    
    // Align to 32 bytes for data block
    uintptr_t currentAddr = (uintptr_t)ptr;
    uintptr_t alignedAddr = (currentAddr + 31) & ~31;
    uint8_t* dataStart = (uint8_t*)alignedAddr;
    
    // Fixup pointers
    for (auto& [name, t] : tensors) {
        t.data = dataStart + t.offset;
    }
    
    return true;
}

uint8_t* RawrXDModelLoader::ParseMetadata(uint8_t* ptr, uint64_t kv_count) {
    for (uint64_t i = 0; i < kv_count; i++) {
        // Key
        uint64_t keyLen = *(uint64_t*)ptr; ptr += 8;
        std::string key((char*)ptr, keyLen);
        ptr += keyLen;
        
        // Value Type
        uint32_t valType = *(uint32_t*)ptr; ptr += 4;
        
        // Read Value based on type
        // 0=UINT8, 1=INT8, ... 4=UINT32, 5=INT32, ... 8=UINT64, ... 10=BOOL, 11=STRING
        switch (valType) {
            case 4: // UINT32
            case 5: // INT32
            {
                 uint32_t val = *(uint32_t*)ptr; ptr += 4;
                 intMetadata[key] = val;
                 break;
            }
            case 8: // UINT64
            case 9: // INT64
            {
                 uint64_t val = *(uint64_t*)ptr; ptr += 8;
                 intMetadata[key] = (uint32_t)val; // Truncate for simplicity
                 break;
            }
            case 11: // STRING
            {
                 uint64_t len = *(uint64_t*)ptr; ptr += 8;
                 ptr += len; // Skip string content for now
                 break;
            }
            default:
            {
                // Simple skip logic (incomplete for arrays)
                // Assuming simple scalar types or known layout
                // Getting array length is tricky without full switch
                // NOTE: Critical for parsing, if we skip wrong, we crash.
                // Assuming minimal GGUF metadata for now: mostly strings and ints.
                // If bool(10): 1 byte
                if (valType == 10) ptr += 1;
                else if (valType <= 1) ptr += 1;
                else if (valType <= 3) ptr += 2;
                else if (valType <= 7) ptr += 4;
                else if (valType <= 9) ptr += 8;
                break;
            }
        }
    }
    return ptr;
}

uint32_t RawrXDModelLoader::GetMetadataInt(const std::string& key) {
    if (intMetadata.count(key)) return intMetadata[key];
    return 0;
}
