// ================================================================
// Auto Model Loader - Automatic model discovery and loading
// ================================================================
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>

class AutoModelLoader {
public:
    static std::vector<std::string> DiscoverModels(const char* search_path) {
        std::vector<std::string> models;
        
        WIN32_FIND_DATAA findData;
        char pattern[MAX_PATH];
        sprintf_s(pattern, "%s\\*.exec", search_path);
        
        HANDLE hFind = FindFirstFileA(pattern, &findData);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    char fullPath[MAX_PATH];
                    sprintf_s(fullPath, "%s\\%s", search_path, findData.cFileName);
                    models.push_back(fullPath);
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }
        
        return models;
    }
};
