#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

// GGUF Constants
#define GGUF_MAGIC 0x46554747 // "GGUF"
#define GGUF_VERSION 3

struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t n_tensors;
    uint64_t n_kv;
};

void ProbeGGUF(const char* path) {
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open model: " << path << std::endl;
        return;
    }

    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    
    HANDLE hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMapping) {
        CloseHandle(hFile);
        std::cerr << "Map failed" << std::endl;
        return;
    }

    const uint8_t* base = (const uint8_t*)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!base) {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        return;
    }

    GGUFHeader* hdr = (GGUFHeader*)base;
    std::cout << "--- GGUF PROBE ---" << std::endl;
    std::cout << "Magic: " << std::hex << hdr->magic << (hdr->magic == GGUF_MAGIC ? " (OK)" : " (FAIL)") << std::dec << std::endl;
    std::cout << "Version: " << hdr->version << std::endl;
    std::cout << "Tensors: " << hdr->n_tensors << std::endl;
    std::cout << "KV Pairs: " << hdr->n_kv << std::endl;

    const uint8_t* ptr = base + sizeof(GGUFHeader);
    bool foundTokens = false;

    for (uint64_t i = 0; i < hdr->n_kv; i++) {
        uint64_t keyLen = *(uint64_t*)ptr;
        ptr += 8;
        std::string key((char*)ptr, (size_t)keyLen);
        ptr += keyLen;
        uint32_t vtype = *(uint32_t*)ptr;
        ptr += 4;

        if (key == "tokenizer.ggml.tokens") {
            foundTokens = true;
            uint32_t atype = *(uint32_t*)ptr;
            ptr += 4;
            uint64_t count = *(uint64_t*)ptr;
            std::cout << "FOUND tokenizer.ggml.tokens!" << std::endl;
            std::cout << "  Array Type: " << atype << std::endl;
            std::cout << "  Token Count: " << count << std::endl;
            
            const uint8_t* sptr = ptr + 8;
            for(int j=0; j<5; j++) {
                uint64_t slen = *(uint64_t*)sptr;
                sptr += 8;
                std::string s((char*)sptr, (size_t)slen);
                std::cout << "  Token[" << j << "]: " << s << std::endl;
                sptr += slen;
            }
            break;
        }

        // Skip logic
        if (vtype == 9) { // ARRAY
            uint32_t stype = *(uint32_t*)ptr;
            ptr += 4;
            uint64_t count = *(uint64_t*)ptr;
            ptr += 8;
            if (stype == 8) { // STRING
                for(uint64_t j=0; j<count; j++) {
                    uint64_t slen = *(uint64_t*)ptr;
                    ptr += 8 + slen;
                }
            } else {
                // Approximate skip for other array types (simplified for probe)
                ptr += count * 8; 
            }
        } else if (vtype == 8) { // STRING
            uint64_t slen = *(uint64_t*)ptr;
            ptr += 8 + slen;
        } else {
            ptr += 8; // simplified
        }
        
        if ((uint64_t)(ptr - base) > 1024 * 1024) {
            std::cout << "Reached 1MB scan limit, stopping." << std::endl;
            break;
        }
    }

    UnmapViewOfFile(base);
    CloseHandle(hMapping);
    CloseHandle(hFile);
}

int main() {
    ProbeGGUF("d:\\codestral22b.gguf");
    return 0;
}
