#pragma once
#include <string>
#include <functional>

class SelfPatch {
public:
    explicit SelfPatch();
    virtual ~SelfPatch() = default;
    
    // Add Vulkan kernel from template
    bool addKernel(const std::string& name, const std::string& templateName);
    
    // Add C++ wrapper for kernel
    bool addCpp(const std::string& name, const std::string& deps);
    
    // Hot-reload the binary (rebuild + restart)
    bool hotReload();
    
    // Patch existing file
    bool patchFile(const std::string& filename, const std::string& patch);
    
    // Callbacks (replacing signals)
    std::function<void(const std::string&)> onKernelAdded;
    std::function<void(const std::string&)> onCppAdded;
    std::function<void()> onReloadStarted;
    std::function<void(bool)> onReloadCompleted;
};
