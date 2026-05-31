#include <iostream>
#include "Win32IDE_PathResolver.h"

int main() {
    std::cout << "RawrXD Path Resolution Test\n";
    std::cout << "=========================\n";
    
    std::cout << "Documents: " << RawrXDPathResolver::GetUserDocumentsPath() << "\n";
    std::cout << "Desktop: " << RawrXDPathResolver::GetUserDesktopPath() << "\n";
    std::cout << "AppData: " << RawrXDPathResolver::GetAppDataPath() << "\n";
    std::cout << "LocalAppData: " << RawrXDPathResolver::GetLocalAppDataPath() << "\n";
    std::cout << "Temp: " << RawrXDPathResolver::GetTempPath() << "\n";
    
    auto modelPaths = RawrXDPathResolver::GetDefaultModelPaths();
    std::cout << "\nModel Paths:\n";
    for (const auto& path : modelPaths) {
        std::cout << "  - " << path << "\n";
    }
    
    std::cout << "\nExtensions: " << RawrXDPathResolver::GetExtensionsPath() << "\n";
    std::cout << "Global Storage: " << RawrXDPathResolver::GetGlobalStoragePath() << "\n";
    
    return 0;
}