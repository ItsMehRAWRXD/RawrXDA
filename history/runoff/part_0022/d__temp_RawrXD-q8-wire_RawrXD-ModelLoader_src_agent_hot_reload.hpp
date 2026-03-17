#pragma once
#include <string>

class HotReload {
public:
    explicit HotReload();
    
    // Reload quantization library on-the-fly
    bool reloadQuant(const std::string& quantType);
    
    // Reload specific module
    bool reloadModule(const std::string& moduleName);
};
