#pragma once

#include <string>
#include <functional>

class HotReload {
public:
    explicit HotReload();
    
    // Reload quantization library on-the-fly
    bool reloadQuant(const std::string& quantType);
    
    // Reload specific module
    bool reloadModule(const std::string& moduleName);
    
    // Callbacks replacing signals
    std::function<void(const std::string&)> onQuantReloaded;
    std::function<void(const std::string&)> onModuleReloaded;
    std::function<void(const std::string&)> onReloadFailed;
};
