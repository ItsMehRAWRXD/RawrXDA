#pragma once
#include <string>

class SelfPatch {
public:
    explicit SelfPatch();
    
    // Add Vulkan kernel from template
    bool addKernel(const std::string& name, const std::string& templateName);
    
    // Add C++ wrapper for kernel
    bool addCpp(const std::string& name, const std::string& deps);
    
    // Hot-reload the binary (rebuild + restart)
    bool hotReload();
    
    // Patch existing file
    bool patchFile(const std::string& filename, const std::string& patch);
};
