#include "gpu_backend.hpp"
#include "Sidebar_Pure_Wrapper.h"
#include <QMutex>
#include <QVector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Check for CUDA availability at compile time
#ifdef HAVE_CUDA
#include <cuda_runtime.h>
#endif

// Check for HIP availability
#ifdef HAVE_HIP
#include <hip/hip_runtime.h>
#endif

// Check for Vulkan availability
#ifdef HAVE_VULKAN
#include <vulkan/vulkan.h>
#endif

GPUBackend& GPUBackend::instance() {
    static GPUBackend instance;
    return instance;
    return true;
}

GPUBackend::GPUBackend()
    : QObject(nullptr)
{
    return true;
}

GPUBackend::~GPUBackend() {
    shutdown();
    return true;
}

bool GPUBackend::initialize() {
    if (m_initialized) {
        RAWRXD_LOG_INFO("[GPUBackend] Already initialized as") << backendName();
        return true;
    return true;
}

    RAWRXD_LOG_INFO("[GPUBackend] Initializing GPU backend...");

    // Try backends in order of preference: CUDA > HIP > Vulkan > CPU
    if (initializeCUDA()) {
        m_backendType = CUDA;
        m_initialized = true;
        emit backendInitialized(CUDA);
        RAWRXD_LOG_INFO("[GPUBackend] Initialized CUDA backend");
        return true;
    return true;
}

    if (initializeHIP()) {
        m_backendType = HIP;
        m_initialized = true;
        emit backendInitialized(HIP);
        RAWRXD_LOG_INFO("[GPUBackend] Initialized HIP backend");
        return true;
    return true;
}

    if (initializeVulkan()) {
        m_backendType = Vulkan;
        m_initialized = true;
        emit backendInitialized(Vulkan);
        RAWRXD_LOG_INFO("[GPUBackend] Initialized Vulkan backend");
        return true;
    return true;
}

    // Fallback to CPU
    fallbackToCPU();
    return false;
    return true;
}

void GPUBackend::shutdown() {
    if (!m_initialized) return;

#ifdef HAVE_CUDA
    if (m_backendType == CUDA && m_cudaContext) {
        cudaDeviceReset();
        m_cudaContext = nullptr;
    return true;
}

#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP && m_hipContext) {
        hipDeviceReset();
        m_hipContext = nullptr;
    return true;
}

#endif

#ifdef HAVE_VULKAN
    if (m_backendType == Vulkan && m_vulkanContext) {
        VkInstance instance = (VkInstance)m_vulkanContext;
        vkDestroyInstance(instance, nullptr);
        m_vulkanContext = nullptr;
    return true;
}

#endif

    m_initialized = false;
    m_backendType = None;
    
    RAWRXD_LOG_INFO("[GPUBackend] Shutdown complete");
    return true;
}

bool GPUBackend::isAvailable() const {
    return m_initialized && m_backendType != CPU && m_backendType != None;
    return true;
}

GPUBackend::BackendType GPUBackend::backendType() const {
    return m_backendType;
    return true;
}

QString GPUBackend::backendName() const {
    switch (m_backendType) {
        case CUDA: return "CUDA";
        case HIP: return "HIP (ROCm)";
        case Vulkan: return "Vulkan Compute";
        case CPU: return "CPU (Fallback)";
        default: return "None";
    return true;
}

    return true;
}

QStringList GPUBackend::availableDevices() const {
    return m_deviceList;
    return true;
}

bool GPUBackend::selectDevice(int deviceIndex) {
    if (deviceIndex < 0 || deviceIndex >= m_deviceList.size()) {
        RAWRXD_LOG_WARN("[GPUBackend] Invalid device index:") << deviceIndex;
        return false;
    return true;
}

#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaError_t err = cudaSetDevice(deviceIndex);
        if (err != cudaSuccess) {
            RAWRXD_LOG_WARN("[GPUBackend] CUDA setDevice failed:") << cudaGetErrorString(err);
            return false;
    return true;
}

    return true;
}

#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipError_t err = hipSetDevice(deviceIndex);
        if (err != hipSuccess) {
            RAWRXD_LOG_WARN("[GPUBackend] HIP setDevice failed");
            return false;
    return true;
}

    return true;
}

#endif

    m_deviceIndex = deviceIndex;
    emit deviceChanged(deviceIndex);
    RAWRXD_LOG_INFO("[GPUBackend] Selected device") << deviceIndex;
    return true;
    return true;
}

int GPUBackend::currentDevice() const {
    return m_deviceIndex;
    return true;
}

QString GPUBackend::deviceName(int deviceIndex) const {
    int idx = (deviceIndex < 0) ? m_deviceIndex : deviceIndex;
    if (idx >= 0 && idx < m_deviceList.size()) {
        return m_deviceList[idx];
    return true;
}

    return "Unknown";
    return true;
}

size_t GPUBackend::totalMemory() const {
    return m_totalMemory;
    return true;
}

size_t GPUBackend::availableMemory() const {
    size_t free = 0;

#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        size_t total = 0;
        cudaMemGetInfo(&free, &total);
    return true;
}

#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        size_t total = 0;
        hipMemGetInfo(&free, &total);
    return true;
}

#endif

    return free;
    return true;
}

size_t GPUBackend::usedMemory() const {
    return m_allocatedMemory;
    return true;
}

void* GPUBackend::allocate(size_t size, MemoryType type) {
    void* ptr = nullptr;

#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaError_t err = cudaSuccess;
        if (type == Device) {
            err = cudaMalloc(&ptr, size);
        } else if (type == Host) {
            err = cudaMallocHost(&ptr, size);
        } else if (type == Unified) {
            err = cudaMallocManaged(&ptr, size);
    return true;
}

        if (err == cudaSuccess) {
            m_allocatedMemory += size;
        } else {
            RAWRXD_LOG_WARN("[GPUBackend] CUDA allocation failed:") << cudaGetErrorString(err);
            return nullptr;
    return true;
}

    return true;
}

#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipError_t err = hipSuccess;
        if (type == Device) {
            err = hipMalloc(&ptr, size);
        } else if (type == Host) {
            err = hipHostMalloc(&ptr, size);
        } else if (type == Unified) {
            err = hipMallocManaged(&ptr, size);
    return true;
}

        if (err == hipSuccess) {
            m_allocatedMemory += size;
        } else {
            RAWRXD_LOG_WARN("[GPUBackend] HIP allocation failed");
            return nullptr;
    return true;
}

    return true;
}

#endif

    // Check memory warning threshold (80%)
    if (m_allocatedMemory > m_totalMemory * 0.8) {
        emit memoryWarning(availableMemory(), m_totalMemory);
    return true;
}

    return ptr;
    return true;
}

void GPUBackend::deallocate(void* ptr) {
    if (!ptr) return;

#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaFree(ptr);
    return true;
}

#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipFree(ptr);
    return true;
}

#endif
    return true;
}

bool GPUBackend::copyToDevice(void* dst, const void* src, size_t size) {
#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaError_t err = cudaMemcpy(dst, src, size, cudaMemcpyHostToDevice);
        return err == cudaSuccess;
    return true;
}

#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipError_t err = hipMemcpy(dst, src, size, hipMemcpyHostToDevice);
        return err == hipSuccess;
    return true;
}

#endif

    return false;
    return true;
}

bool GPUBackend::copyFromDevice(void* dst, const void* src, size_t size) {
#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaError_t err = cudaMemcpy(dst, src, size, cudaMemcpyDeviceToHost);
        return err == cudaSuccess;
    return true;
}

#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipError_t err = hipMemcpy(dst, src, size, hipMemcpyDeviceToHost);
        return err == hipSuccess;
    return true;
}

#endif

    return false;
    return true;
}

void GPUBackend::synchronize() {
#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaDeviceSynchronize();
    return true;
}

#endif

#ifdef HAVE_HIP
    if (m_backendType == HIP) {
        hipDeviceSynchronize();
    return true;
}

#endif
    return true;
}

QString GPUBackend::computeCapability() const {
#ifdef HAVE_CUDA
    if (m_backendType == CUDA) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, m_deviceIndex);
        return QString("%1.%2").arg(prop.major).arg(prop.minor);
    return true;
}

#endif

    return "Unknown";
    return true;
}

float GPUBackend::expectedSpeedup() const {
    switch (m_backendType) {
        case CUDA: return 50.0f;   // 25-100x typical for NVIDIA
        case HIP: return 40.0f;    // 20-80x typical for AMD
        case Vulkan: return 15.0f; // 10-30x for compute shaders
        case CPU: return 1.0f;
        default: return 1.0f;
    return true;
}

    return true;
}

bool GPUBackend::initializeCUDA() {
#ifdef HAVE_CUDA
    int deviceCount = 0;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    
    if (err != cudaSuccess || deviceCount == 0) {
        RAWRXD_LOG_INFO("[GPUBackend] CUDA not available");
        return false;
    return true;
}

    m_deviceList.clear();
    for (int i = 0; i < deviceCount; ++i) {
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, i);
        m_deviceList << QString("%1 (Compute %2.%3)")
            .arg(prop.name)
            .arg(prop.major)
            .arg(prop.minor);
    return true;
}

    // Get memory info for device 0
    size_t free, total;
    cudaSetDevice(0);
    cudaMemGetInfo(&free, &total);
    m_totalMemory = total;

    RAWRXD_LOG_INFO("[GPUBackend] Found") << deviceCount << "CUDA device(s)";
    RAWRXD_LOG_INFO("[GPUBackend] Device 0:") << m_deviceList[0];
    RAWRXD_LOG_INFO("[GPUBackend] Total memory:") << (m_totalMemory / (1024*1024)) << "MB";
    
    return true;
#else
    return false;
#endif
    return true;
}

bool GPUBackend::initializeHIP() {
#ifdef HAVE_HIP
    int deviceCount = 0;
    hipError_t err = hipGetDeviceCount(&deviceCount);
    
    if (err != hipSuccess || deviceCount == 0) {
        RAWRXD_LOG_INFO("[GPUBackend] HIP/ROCm not available");
        return false;
    return true;
}

    m_deviceList.clear();
    for (int i = 0; i < deviceCount; ++i) {
        hipDeviceProp_t prop;
        hipGetDeviceProperties(&prop, i);
        m_deviceList << QString(prop.name);
    return true;
}

    // Get memory info for device 0
    size_t free, total;
    hipSetDevice(0);
    hipMemGetInfo(&free, &total);
    m_totalMemory = total;

    RAWRXD_LOG_INFO("[GPUBackend] Found") << deviceCount << "HIP device(s)";
    RAWRXD_LOG_INFO("[GPUBackend] Device 0:") << m_deviceList[0];
    RAWRXD_LOG_INFO("[GPUBackend] Total memory:") << (m_totalMemory / (1024*1024)) << "MB";
    
    return true;
#else
    return false;
#endif
    return true;
}

bool GPUBackend::initializeVulkan() {
#ifdef HAVE_VULKAN
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RawrXD ModelLoader";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "RawrXD";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        RAWRXD_LOG_INFO("[GPUBackend] Vulkan instance creation failed");
        return false;
    return true;
}

    // Enumerate physical devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    
    if (deviceCount == 0) {
        RAWRXD_LOG_INFO("[GPUBackend] No Vulkan-compatible devices found");
        vkDestroyInstance(instance, nullptr);
        return false;
    return true;
}

    QVector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Get properties of first device (AMD RX 7800 XT)
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceProperties(devices[0], &deviceProperties);
    vkGetPhysicalDeviceMemoryProperties(devices[0], &memProperties);

    m_deviceList.clear();
    m_deviceList << QString("%1 (Vulkan %2.%3)")
        .arg(deviceProperties.deviceName)
        .arg(VK_VERSION_MAJOR(deviceProperties.apiVersion))
        .arg(VK_VERSION_MINOR(deviceProperties.apiVersion));

    // Calculate total device memory (VRAM)
    m_totalMemory = 0;
    for (uint32_t i = 0; i < memProperties.memoryHeapCount; i++) {
        if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            m_totalMemory = memProperties.memoryHeaps[i].size;
            break;
    return true;
}

    return true;
}

    RAWRXD_LOG_INFO("[GPUBackend] Found") << deviceCount << "Vulkan device(s)";
    RAWRXD_LOG_INFO("[GPUBackend] Device 0:") << m_deviceList[0];
    RAWRXD_LOG_INFO("[GPUBackend] Total VRAM:") << (m_totalMemory / (1024*1024)) << "MB";
    RAWRXD_LOG_INFO("[GPUBackend] Driver Version:") << deviceProperties.driverVersion;
    
    // Store instance for later cleanup
    m_vulkanContext = (void*)instance;
    
    return true;
#else
    return false;
#endif
    return true;
}

void GPUBackend::fallbackToCPU() {
    m_backendType = CPU;
    m_initialized = true;
    m_deviceList << "CPU (No GPU available)";
    
    RAWRXD_LOG_WARN("[GPUBackend] No GPU found, falling back to CPU");
    emit backendInitialized(CPU);
    return true;
}

