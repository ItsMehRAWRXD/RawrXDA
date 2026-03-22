// Linker fallbacks for Win32IDE when ASM model loader / beacon symbols are absent.
// Signatures must match src/universal_model_router.cpp extern "C" declarations.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

namespace
{
std::mutex g_modelLoaderMutex;
std::wstring g_loadedModelPath;
std::atomic<bool> g_modelLoaderInitialized{false};
std::atomic<unsigned long long> g_modelLoadTimestampMs{0};

int loadModelImpl(const wchar_t* path)
{
    if (path == nullptr || *path == L'\0')
    {
        return -1;
    }
    std::error_code ec;
    const std::filesystem::path p(path);
    if (!std::filesystem::exists(p, ec) || ec)
    {
        return -1;
    }
    if (!std::filesystem::is_regular_file(p, ec) || ec)
    {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    g_loadedModelPath = p.native();
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    g_modelLoadTimestampMs.store(
        static_cast<unsigned long long>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()),
        std::memory_order_relaxed);
    return 0;
}
}  // namespace

extern "C" int ModelLoaderInit(void)
{
    g_modelLoaderInitialized.store(true, std::memory_order_relaxed);
    return 0;
}

extern "C" int LoadModel(const wchar_t* path)
{
    if (!g_modelLoaderInitialized.load(std::memory_order_relaxed))
    {
        return -1;
    }
    return loadModelImpl(path);
}

extern "C" void UnloadModel(void)
{
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    g_loadedModelPath.clear();
}

// UniversalModelRouter::hotSwapModel expects return value 1 on success.
extern "C" int HotSwapModel(const wchar_t* newPath, char /*preserveKV*/)
{
    if (!g_modelLoaderInitialized.load(std::memory_order_relaxed))
    {
        return 0;
    }
    if (loadModelImpl(newPath) != 0)
    {
        return 0;
    }
    return 1;
}

extern "C" void* GetTensor(const char* /*name*/)
{
    return nullptr;
}

extern "C" const wchar_t* GetCurrentModelPath(void)
{
    static thread_local std::wstring s_tlsPath;
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    s_tlsPath = g_loadedModelPath;
    return s_tlsPath.c_str();
}

extern "C" unsigned long long GetModelLoadTimestamp(void)
{
    return g_modelLoadTimestampMs.load(std::memory_order_relaxed);
}

extern "C" bool ModelLoaderShutdown(void)
{
    g_modelLoaderInitialized.store(false, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_modelLoaderMutex);
    g_loadedModelPath.clear();
    return true;
}

extern "C" int BeaconRouterInit(void)
{
    return 0;
}

extern "C" int BeaconSend(int /*beaconID*/, void* /*pData*/, int /*dataLen*/)
{
    return 0;
}

extern "C" int BeaconRecv(int /*beaconID*/, void** /*ppData*/, int* /*pLen*/)
{
    return -1;
}

extern "C" int TryBeaconRecv(int /*beaconID*/, void** /*ppData*/, int* /*pLen*/)
{
    return -1;
}

extern "C" int RegisterAgent(int /*agentID*/, int /*beaconSlot*/)
{
    return 0;
}

// Enterprise_DevUnlock: canonical C ABI is extern "C" int64_t Enterprise_DevUnlock() in
// enterprise_devunlock_bridge.cpp — do not duplicate here (LNK2005).
