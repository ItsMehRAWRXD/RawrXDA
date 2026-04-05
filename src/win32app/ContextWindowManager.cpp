#include "ContextWindowManager.h"
#include <windows.h>
#include <shlwapi.h>
#include <fstream>
#include <sstream>
#include <vector>

#pragma comment(lib, "shlwapi.lib")

namespace RawrXD {

namespace {
constexpr size_t kMaxManifestBytes = 512u * 1024u;
constexpr size_t kMaxManifestFieldBytes = 256u;

bool containsUnsafeShellChars(const std::string& s) {
    return s.find('"') != std::string::npos || s.find('`') != std::string::npos;
}
}

// ============================================================================
// CONTEXT WINDOW MANAGER IMPLEMENTATION
// ============================================================================

ContextWindowManager::ContextWindowManager()
    : m_buffer(nullptr)
    , m_currentSize(0)
    , m_maxTokens(0)
    , m_bytesAllocated(0)
    , m_useMemoryMap(false)
    , m_hMapFile(nullptr)
{
}

ContextWindowManager::~ContextWindowManager() {
    freeContext();
}

bool ContextWindowManager::allocateContext(ContextSize size) {
    freeContext();
    
    m_maxTokens = static_cast<size_t>(size);
    const size_t bytesPerToken = 4; // int32 tokens
    m_currentSize = m_maxTokens * bytesPerToken;
    
    // Use memory mapped file for large contexts (>= 256K)
    if (m_maxTokens >= 262144) { // 256K
        m_useMemoryMap = true;
        
        // Create temp directory for memory map
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        
        std::stringstream ss;
        ss << tempPath << "RawrXD_Context_" << GetCurrentProcessId() << ".tmp";
        m_mapFilePath = ss.str();
        
        // Create memory-mapped file
        m_hMapFile = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            (DWORD)(m_currentSize >> 32),
            (DWORD)(m_currentSize & 0xFFFFFFFF),
            nullptr
        );
        
        if (m_hMapFile == nullptr) {
            return false;
        }
        
        m_buffer = MapViewOfFile(
            m_hMapFile,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            m_currentSize
        );
        
        if (m_buffer == nullptr) {
            CloseHandle(m_hMapFile);
            m_hMapFile = nullptr;
            return false;
        }
        
    } else {
        // Standard heap allocation for smaller contexts
        m_useMemoryMap = false;
        m_buffer = VirtualAlloc(
            nullptr,
            m_currentSize,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE
        );
        
        if (m_buffer == nullptr) {
            return false;
        }
    }
    
    m_bytesAllocated = m_currentSize;
    
    // Zero out the buffer
    ZeroMemory(m_buffer, m_currentSize);
    
    return true;
}

void ContextWindowManager::freeContext() {
    if (m_buffer) {
        if (m_useMemoryMap) {
            UnmapViewOfFile(m_buffer);
            if (m_hMapFile) {
                CloseHandle(m_hMapFile);
                m_hMapFile = nullptr;
            }
            if (!m_mapFilePath.empty()) {
                DeleteFileA(m_mapFilePath.c_str());
                m_mapFilePath.clear();
            }
        } else {
            VirtualFree(m_buffer, 0, MEM_RELEASE);
        }
        m_buffer = nullptr;
    }
    
    m_currentSize = 0;
    m_maxTokens = 0;
    m_bytesAllocated = 0;
    m_useMemoryMap = false;
}

bool ContextWindowManager::useMemoryMappedFile(bool enable) {
    if (m_buffer) {
        // Cannot change mode while buffer is allocated
        return false;
    }
    m_useMemoryMap = enable;
    return true;
}

float ContextWindowManager::getUtilization() const {
    if (!m_buffer || m_bytesAllocated == 0 || m_maxTokens == 0) {
        return 0.0f;
    }

    // Sample token buffer occupancy by counting non-zero int32 slots.
    const int32_t* tokens = static_cast<const int32_t*>(m_buffer);
    const size_t totalTokens = m_bytesAllocated / sizeof(int32_t);
    const size_t sampleCount = (totalTokens > 4096) ? 4096 : totalTokens;
    if (sampleCount == 0) return 0.0f;

    size_t nonZero = 0;
    const size_t stride = (totalTokens > sampleCount) ? (totalTokens / sampleCount) : 1;
    for (size_t i = 0, idx = 0; i < sampleCount && idx < totalTokens; ++i, idx += stride) {
        if (tokens[idx] != 0) {
            ++nonZero;
        }
    }

    return static_cast<float>(nonZero) / static_cast<float>(sampleCount);
}

// ============================================================================
// PLUGIN MANAGER IMPLEMENTATION
// ============================================================================

PluginManager::PluginManager() {
    char modulePath[MAX_PATH];
    GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    PathRemoveFileSpecA(modulePath);
    m_pluginDirectory = std::string(modulePath) + "\\plugins";
    
    // Create plugins directory if it doesn't exist
    CreateDirectoryA(m_pluginDirectory.c_str(), nullptr);
}

PluginManager::~PluginManager() {
    // Unload all plugins
    for (auto& pair : m_plugins) {
        if (pair.second.hModule) {
            FreeLibrary(pair.second.hModule);
        }
    }
}

bool PluginManager::installVSIX(const std::string& vsixPath) {
    // Create temp extraction directory
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    
    std::stringstream ss;
    ss << tempPath << "RawrXD_VSIX_" << GetTickCount64();
    std::string extractPath = ss.str();
    
    if (!CreateDirectoryA(extractPath.c_str(), nullptr)) {
        return false;
    }
    
    // Extract VSIX (ZIP format)
    if (!extractVSIX(vsixPath, extractPath)) {
        RemoveDirectoryA(extractPath.c_str());
        return false;
    }
    
    // Parse manifest
    std::string manifestPath = extractPath + "\\extension\\package.json";
    PluginManifest manifest;
    if (!parseManifest(manifestPath, manifest)) {
        return false;
    }
    
    // Convert to native plugin
    std::string outputPath = m_pluginDirectory + "\\" + manifest.id + ".dll";
    if (!convertVSIXToNative(extractPath, outputPath)) {
        return false;
    }
    
    // Load the plugin
    bool success = loadPlugin(outputPath);
    
    // Cleanup temp directory
    RemoveDirectoryA(extractPath.c_str());
    
    return success;
}

bool PluginManager::extractVSIX(const std::string& vsixPath, const std::string& extractPath) {
    if (vsixPath.empty() || extractPath.empty() ||
        containsUnsafeShellChars(vsixPath) || containsUnsafeShellChars(extractPath)) {
        return false;
    }

    // VSIX files are ZIP archives
    // For simplicity, we'll shell out to PowerShell for extraction
    std::stringstream cmd;
    cmd << "powershell.exe -NoProfile -Command \"& {";
    cmd << "Expand-Archive -Path '" << vsixPath << "' ";
    cmd << "-DestinationPath '" << extractPath << "' -Force";
    cmd << "}\"";
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    
    // CreateProcessA requires a writable buffer; create a copy to avoid const issues
    std::string cmdStr = cmd.str();
    
    if (!CreateProcessA(nullptr, cmdStr.data(), nullptr, nullptr, FALSE, 
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return exitCode == 0;
}

bool PluginManager::parseManifest(const std::string& manifestPath, PluginManifest& manifest) {
    if (manifestPath.empty()) {
        return false;
    }

    std::ifstream file(manifestPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    const std::streamoff sz = file.tellg();
    if (sz <= 0 || static_cast<size_t>(sz) > kMaxManifestBytes) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    
    // Simple JSON parsing (real implementation would use a JSON library)
    std::string content(static_cast<size_t>(sz), '\0');
    file.read(content.data(), sz);
    if (!file) {
        return false;
    }
    file.close();
    
    // Extract basic fields (simplified parsing)
    auto extractField = [&content](const std::string& field) -> std::string {
        size_t pos = content.find("\"" + field + "\"");
        if (pos == std::string::npos) return "";
        
        size_t colonPos = content.find(":", pos);
        if (colonPos == std::string::npos) return "";
        
        size_t quoteStart = content.find("\"", colonPos);
        if (quoteStart == std::string::npos) return "";
        
        size_t quoteEnd = content.find("\"", quoteStart + 1);
        if (quoteEnd == std::string::npos) return "";
        
        std::string out = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        if (out.size() > kMaxManifestFieldBytes) {
            out.resize(kMaxManifestFieldBytes);
        }
        return out;
    };
    
    manifest.name = extractField("name");
    manifest.version = extractField("version");
    manifest.description = extractField("description");
    manifest.publisher = extractField("publisher");
    manifest.id = manifest.publisher + "." + manifest.name;
    
    return !manifest.name.empty();
}

bool PluginManager::convertVSIXToNative(const std::string& vsixPath, const std::string& outputPath) {
    // This is a complex operation that would require:
    // 1. JavaScript/TypeScript to C++ transpilation
    // 2. API bridging for VS Code extension API
    // 3. Compilation to native DLL
    
    // For now, we'll create a stub DLL that can be loaded
    // Real implementation would need a full transpiler and compiler toolchain
    
    return false; // Not fully implemented in this stub
}

bool PluginManager::loadPlugin(const std::string& pluginPath) {
    HMODULE hModule = LoadLibraryA(pluginPath.c_str());
    if (!hModule) {
        return false;
    }
    
    // Extract plugin ID from path
    std::string filename = pluginPath.substr(pluginPath.find_last_of("\\/") + 1);
    std::string pluginId = filename.substr(0, filename.find_last_of("."));
    
    LoadedPlugin plugin;
    plugin.hModule = hModule;
    plugin.active = true;
    plugin.manifest.id = pluginId;
    
    // Load standard exports
    plugin.exports["activate"] = (void*)GetProcAddress(hModule, "activate");
    plugin.exports["deactivate"] = (void*)GetProcAddress(hModule, "deactivate");
    
    m_plugins[pluginId] = plugin;
    
    // Call activate if available
    if (plugin.exports["activate"]) {
        typedef void (*ActivateFunc)();
        ((ActivateFunc)plugin.exports["activate"])();
    }
    
    return true;
}

bool PluginManager::unloadPlugin(const std::string& pluginId) {
    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        return false;
    }
    
    // Call deactivate
    if (it->second.exports["deactivate"]) {
        typedef void (*DeactivateFunc)();
        ((DeactivateFunc)it->second.exports["deactivate"])();
    }
    
    FreeLibrary(it->second.hModule);
    m_plugins.erase(it);
    
    return true;
}

bool PluginManager::isPluginLoaded(const std::string& pluginId) const {
    return m_plugins.find(pluginId) != m_plugins.end();
}

bool PluginManager::executeCommand(const std::string& pluginId, const std::string& command, const std::string& args) {
    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        return false;
    }
    
    void* cmdFunc = it->second.exports[command];
    if (!cmdFunc) {
        cmdFunc = GetProcAddress(it->second.hModule, command.c_str());
        if (!cmdFunc) {
            return false;
        }
        it->second.exports[command] = cmdFunc;
    }
    
    typedef bool (*CommandFunc)(const char*);
    return ((CommandFunc)cmdFunc)(args.c_str());
}

void* PluginManager::getPluginExport(const std::string& pluginId, const std::string& functionName) {
    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        return nullptr;
    }
    
    auto exportIt = it->second.exports.find(functionName);
    if (exportIt != it->second.exports.end()) {
        return exportIt->second;
    }
    
    void* func = GetProcAddress(it->second.hModule, functionName.c_str());
    if (func) {
        it->second.exports[functionName] = func;
    }
    
    return func;
}

std::vector<PluginManager::LoadedPlugin> PluginManager::getLoadedPlugins() const {
    std::vector<LoadedPlugin> plugins;
    for (const auto& pair : m_plugins) {
        plugins.push_back(pair.second);
    }
    return plugins;
}

PluginManager::LoadedPlugin* PluginManager::getPlugin(const std::string& pluginId) {
    auto it = m_plugins.find(pluginId);
    if (it == m_plugins.end()) {
        return nullptr;
    }
    return &it->second;
}

// ============================================================================
// MEMORY MODULE LOADER IMPLEMENTATION
// ============================================================================

MemoryModuleLoader::MemoryModuleLoader()
    : m_currentModule(nullptr)
{
    char modulePath[MAX_PATH];
    GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    PathRemoveFileSpecA(modulePath);
    m_moduleDirectory = std::string(modulePath) + "\\memory_modules";
    
    CreateDirectoryA(m_moduleDirectory.c_str(), nullptr);
}

MemoryModuleLoader::~MemoryModuleLoader() {
    unloadModule();
}

std::string MemoryModuleLoader::getModulePath(int contextSizeK) const {
    std::stringstream ss;
    ss << m_moduleDirectory << "\\memory_" << contextSizeK << "k.dll";
    return ss.str();
}

bool MemoryModuleLoader::loadModule(int contextSizeK) {
    // Check if already loaded
    auto it = m_modules.find(contextSizeK);
    if (it != m_modules.end() && it->second.loaded) {
        m_currentModule = &it->second;
        return true;
    }
    
    // Get module path
    std::string modulePath = getModulePath(contextSizeK);
    
    // Load the DLL
    HMODULE hModule = LoadLibraryA(modulePath.c_str());
    if (!hModule) {
        // Module doesn't exist, use default allocation
        return false;
    }
    
    ModuleInfo module;
    module.contextSize = contextSizeK;
    module.modulePath = modulePath;
    module.hModule = hModule;
    module.loaded = true;
    
    // Load function pointers
    if (!loadModuleFunctions(module)) {
        FreeLibrary(hModule);
        return false;
    }
    
    m_modules[contextSizeK] = module;
    m_currentModule = &m_modules[contextSizeK];
    
    return true;
}

bool MemoryModuleLoader::loadModuleFunctions(ModuleInfo& module) {
    module.allocate = (ModuleInfo::AllocateFunc)GetProcAddress(module.hModule, "AllocateContextBuffer");
    module.free = (ModuleInfo::FreeFunc)GetProcAddress(module.hModule, "FreeContextBuffer");
    module.optimize = (ModuleInfo::OptimizeFunc)GetProcAddress(module.hModule, "OptimizeContextBuffer");
    
    return module.allocate && module.free && module.optimize;
}

void MemoryModuleLoader::unloadModule() {
    if (m_currentModule && m_currentModule->hModule) {
        FreeLibrary(m_currentModule->hModule);
        m_currentModule->loaded = false;
        m_currentModule = nullptr;
    }
}

void* MemoryModuleLoader::allocateBuffer(size_t size) {
    if (m_currentModule && m_currentModule->allocate) {
        return m_currentModule->allocate(size);
    }
    // Fallback to standard allocation
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void MemoryModuleLoader::freeBuffer(void* buffer) {
    if (m_currentModule && m_currentModule->free) {
        m_currentModule->free(buffer);
    } else {
        VirtualFree(buffer, 0, MEM_RELEASE);
    }
}

bool MemoryModuleLoader::optimizeBuffer(void* buffer, size_t size) {
    if (m_currentModule && m_currentModule->optimize) {
        return m_currentModule->optimize(buffer, size);
    }
    return false;
}

} // namespace RawrXD
