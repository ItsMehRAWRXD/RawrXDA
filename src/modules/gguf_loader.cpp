#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t kv_count;
};

class GGUFLoader {
    HANDLE hFile;
    HANDLE hMapping;
    void* mapped_view;
    size_t file_size;

public:
    GGUFLoader() : hFile(INVALID_HANDLE_VALUE), hMapping(NULL), mapped_view(NULL), file_size(0) {}
    
    ~GGUFLoader() {
        if (mapped_view) UnmapViewOfFile(mapped_view);
        if (hMapping) CloseHandle(hMapping);
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    }
    
    bool Load(const std::string& path) {
        hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        
        LARGE_INTEGER size;
        GetFileSizeEx(hFile, &size);
        file_size = size.QuadPart;
        
        hMapping = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!hMapping) return false;
        
        mapped_view = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        return mapped_view != NULL;
    }
    
    void* get_data() const { return mapped_view; }
    size_t get_size() const { return file_size; }
};
