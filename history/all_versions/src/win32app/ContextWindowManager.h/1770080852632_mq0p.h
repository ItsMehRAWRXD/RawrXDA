#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace RawrXD {

// ============================================================================
// CONTEXT WINDOW MANAGER
// Handles dynamic context allocation from 4K to 1M tokens
// ============================================================================

class ContextWindowManager {
public:
    enum class ContextSize {
        CTX_4K = 4096,
        CTX_8K = 8192,
        CTX_16K = 16384,
        CTX_32K = 32768,
        CTX_64K = 65536,
        CTX_128K = 131072,
        CTX_256K = 262144,
        CTX_512K = 524288,
        CTX_1M = 1048576
    };
    
    struct ContextConfig {
        ContextSize size;
        size_t bytesPerToken; // typically 4 bytes for int32 tokens
        bool compressionEnabled;
        float targetCompressionRatio;
    };
    
    ContextWindowManager();
    ~ContextWindowManager();
    
    bool allocateContext(ContextSize size);
    void freeContext();
    bool isAllocated() const { return m_buffer != nullptr; }
    size_t getCurrentSize() const { return m_currentSize; }
    size_t getMaxTokens() const { return m_maxTokens; }
    
    // Memory mapped file for > 256K contexts
    bool useMemoryMappedFile(bool enable);
    bool isUsingMemoryMap() const { return m_useMemoryMap; }
    
    // Context buffer access
    void* getBuffer() { return m_buffer; }
    const void* getBuffer() const { return m_buffer; }
    
    // Statistics
    size_t getBytesAllocated() const { return m_bytesAllocated; }
    float getUtilization() const;
    
private:
    void* m_buffer;
    size_t m_currentSize;
    size_t m_maxTokens;
    size_t m_bytesAllocated;
    bool m_useMemoryMap;
    HANDLE m_hMapFile;
    std::string m_mapFilePath;
};

// ============================================================================
// PLUGIN SYSTEM
// Converts VSIX extensions to native IDE plugins
// ============================================================================

class PluginManager {
public:
    struct PluginManifest {
        std::string id;
        std::string name;
        std::string version;
        std::string publisher;
        std::string description;
        std::vector<std::string> categories;
        std::map<std::string, std::string> contributes;
        std::vector<std::string> activationEvents;
    };
    
    struct LoadedPlugin {
        PluginManifest manifest;
        HMODULE hModule;
        bool active;
        std::map<std::string, void*> exports;
    };
    
    PluginManager();
    ~PluginManager();
    
    // VSIX Installation
    bool installVSIX(const std::string& vsixPath);
    bool uninstallPlugin(const std::string& pluginId);
    
    // Native Plugin Loading
    bool loadPlugin(const std::string& pluginPath);
    bool unloadPlugin(const std::string& pluginId);
    bool isPluginLoaded(const std::string& pluginId) const;
    
    // Plugin Execution
    bool executeCommand(const std::string& pluginId, const std::string& command, const std::string& args);
    void* getPluginExport(const std::string& pluginId, const std::string& functionName);
    
    // Plugin Query
    std::vector<LoadedPlugin> getLoadedPlugins() const;
    LoadedPlugin* getPlugin(const std::string& pluginId);
    
    // VSIX Conversion
    bool convertVSIXToNative(const std::string& vsixPath, const std::string& outputPath);
    
private:
    std::map<std::string, LoadedPlugin> m_plugins;
    std::string m_pluginDirectory;
    
    bool extractVSIX(const std::string& vsixPath, const std::string& extractPath);
    bool parseManifest(const std::string& manifestPath, PluginManifest& manifest);
    bool compilePlugin(const std::string& sourceDir, const std::string& outputPath);
};

// ============================================================================
// MEMORY MODULE LOADER
// Dynamically loads optimized memory modules for different context sizes
// ============================================================================

class MemoryModuleLoader {
public:
    struct ModuleInfo {
        int contextSize; // in K
        std::string modulePath;
        HMODULE hModule;
        bool loaded;
        
        // Function pointers exported by module
        typedef void* (*AllocateFunc)(size_t);
        typedef void (*FreeFunc)(void*);
        typedef bool (*OptimizeFunc)(void*, size_t);
        
        AllocateFunc allocate;
        FreeFunc free;
        OptimizeFunc optimize;
    };
    
    MemoryModuleLoader();
    ~MemoryModuleLoader();
    
    bool loadModule(int contextSizeK);
    void unloadModule();
    ModuleInfo* getCurrentModule() { return m_currentModule; }
    
    // Module operations
    void* allocateBuffer(size_t size);
    void freeBuffer(void* buffer);
    bool optimizeBuffer(void* buffer, size_t size);
    
private:
    std::map<int, ModuleInfo> m_modules;
    ModuleInfo* m_currentModule;
    std::string m_moduleDirectory;
    
    bool loadModuleFunctions(ModuleInfo& module);
    std::string getModulePath(int contextSizeK) const;
};

} // namespace RawrXD
