#pragma once
#include <string>
#include <functional>

class HotReload {
public:
    HotReload() = default;
    bool reloadQuant(const std::string& quantType);
    bool reloadModule(const std::string& moduleName);

    // Callbacks (replace Qt signals)
    std::function<void(const std::string&)> onQuantReloaded;
    std::function<void(const std::string&)> onModuleReloaded;
    std::function<void(const std::string&)> onReloadFailed;
};
