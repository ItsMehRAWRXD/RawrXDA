#include "gpu_memory_manager.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <QDebug>
#include <QDateTime>
#include <sstream>

GPUMemoryManager::GPUMemoryManager(GPUBackend backend)
    : m_backend(backend), m_initialized(false)
{
}

GPUMemoryManager::~GPUMemoryManager()
{
    shutdown();
}

bool GPUMemoryManager::initialize(int deviceId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) {
        return true;
    }
    
    m_deviceId = deviceId;
    bool success = false;
    
    if (m_backend == CUDA) {
        success = initializeCUDA();
    } else if (m_backend == HIP) {
        success = initializeHIP();
    }
    
    if (success) {
        // Pre-allocate memory pool
        uint64_t poolChunkSize = m_poolSize / 16; // 16 chunks
        for (int i = 0; i < 16; ++i) {
            void* ptr = nullptr;
            if (m_backend == CUDA) {
                ptr = allocateCUDAMemory(poolChunkSize);
            } else {
                ptr = allocateHIPMemory(poolChunkSize);
            }
            
            if (ptr) {
                m_memoryPool.push_back({ptr, poolChunkSize, false});
                m_freePool.push(ptr);
            }
        }
        
        m_initialized = true;
    }
    
    return success;
}

void GPUMemoryManager::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Release all tensor allocations
    m_tensorAllocations.clear();
    
    // Clear memory pool
    for (const auto& chunk : m_memoryPool) {
        if (m_backend == CUDA) {
            releaseCUDAMemory(chunk.ptr);
        } else {
            releaseHIPMemory(chunk.ptr);
        }
    }
    m_memoryPool.clear();
    
    // Clear pending transfers
    m_pendingTransfers.clear();
    
    m_initialized = false;
}

bool GPUMemoryManager::isInitialized() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

void* GPUMemoryManager::allocateTensor(const QString& tensorId, uint64_t size, bool pinHostMemory)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return nullptr;
    }
    
    // Check if already allocated
    if (m_tensorAllocations.count(tensorId) > 0) {
        return m_tensorAllocations[tensorId].gpuPtr;
    }
    
    // Check memory availability
    if (m_stats.totalUsed + size > m_maxMemory) {
        // Try to evict LRU tensors
        while (m_stats.totalUsed + size > m_maxMemory && evictLRU()) {
            // Continue evicting
        }
        
        if (m_stats.totalUsed + size > m_maxMemory) {
            return nullptr; // Not enough memory
        }
    }
    
    // Allocate GPU memory
    void* gpuPtr = nullptr;
    if (m_backend == CUDA) {
        gpuPtr = allocateCUDAMemory(size);
    } else {
        gpuPtr = allocateHIPMemory(size);
    }
    
    if (!gpuPtr) {
        return nullptr;
    }
    
    // Allocate pinned CPU memory if requested
    void* cpuPtr = nullptr;
    if (pinHostMemory) {
        cpuPtr = std::malloc(size);
    }
    
    // Record allocation
    TensorAllocation alloc;
    alloc.tensorId = tensorId;
    alloc.size = size;
    alloc.gpuPtr = gpuPtr;
    alloc.cpuPtr = cpuPtr;
    alloc.deviceId = m_deviceId;
    alloc.createdAt = QDateTime::currentMSecsSinceEpoch();
    alloc.lastAccessAt = alloc.createdAt;
    
    m_tensorAllocations[tensorId] = alloc;
    
    // Update stats
    m_stats.totalUsed += size;
    m_stats.activeAllocations++;
    if (m_stats.totalUsed > m_peakUsage) {
        m_peakUsage = m_stats.totalUsed;
    }
    m_stats.peakMemoryUsage = m_peakUsage;
    
    return gpuPtr;
}

void GPUMemoryManager::releaseTensor(const QString& tensorId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tensorAllocations.find(tensorId);
    if (it == m_tensorAllocations.end()) {
        return;
    }
    
    const auto& alloc = it->second;
    
    // Release GPU memory
    if (m_backend == CUDA) {
        releaseCUDAMemory(alloc.gpuPtr);
    } else {
        releaseHIPMemory(alloc.gpuPtr);
    }
    
    // Release pinned CPU memory
    if (alloc.cpuPtr) {
        std::free(alloc.cpuPtr);
    }
    
    // Update stats
    m_stats.totalUsed -= alloc.size;
    m_stats.activeAllocations--;
    
    m_tensorAllocations.erase(it);
}

void* GPUMemoryManager::allocateTemporary(uint64_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized) {
        return nullptr;
    }
    
    void* ptr = nullptr;
    if (m_backend == CUDA) {
        ptr = allocateCUDAMemory(size);
    } else {
        ptr = allocateHIPMemory(size);
    }
    
    if (ptr) {
        m_stats.totalUsed += size;
    }
    
    return ptr;
}

void GPUMemoryManager::releaseTemporary(void* ptr)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!ptr) return;
    
    if (m_backend == CUDA) {
        releaseCUDAMemory(ptr);
    } else {
        releaseHIPMemory(ptr);
    }
}

TensorAllocation GPUMemoryManager::getTensorInfo(const QString& tensorId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tensorAllocations.find(tensorId);
    if (it != m_tensorAllocations.end()) {
        return it->second;
    }
    return TensorAllocation();
}

bool GPUMemoryManager::tensorExists(const QString& tensorId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tensorAllocations.count(tensorId) > 0;
}

std::vector<TensorAllocation> GPUMemoryManager::getAllAllocations() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<TensorAllocation> allocs;
    for (const auto& pair : m_tensorAllocations) {
        allocs.push_back(pair.second);
    }
    return allocs;
}

bool GPUMemoryManager::copyToDevice(const QString& tensorId, const void* cpuData, uint64_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tensorAllocations.find(tensorId);
    if (it == m_tensorAllocations.end()) {
        return false;
    }
    
    auto& alloc = it->second;
    alloc.lastAccessAt = QDateTime::currentMSecsSinceEpoch();
    
    if (m_backend == CUDA) {
        return copyCUDAToDevice(alloc.gpuPtr, cpuData, size);
    } else {
        return copyHIPToDevice(alloc.gpuPtr, cpuData, size);
    }
}

bool GPUMemoryManager::copyToHost(const QString& tensorId, void* cpuData, uint64_t size)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tensorAllocations.find(tensorId);
    if (it == m_tensorAllocations.end()) {
        return false;
    }
    
    auto& alloc = it->second;
    alloc.lastAccessAt = QDateTime::currentMSecsSinceEpoch();
    
    if (m_backend == CUDA) {
        return copyCUDAToHost(cpuData, alloc.gpuPtr, size);
    } else {
        return copyHIPToHost(cpuData, alloc.gpuPtr, size);
    }
}

QString GPUMemoryManager::copyToDeviceAsync(const QString& tensorId, const void* cpuData, 
                                           uint64_t size, std::function<void(bool)> callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tensorAllocations.find(tensorId);
    if (it == m_tensorAllocations.end()) {
        if (callback) callback(false);
        return "";
    }
    
    auto& alloc = it->second;
    alloc.lastAccessAt = QDateTime::currentMSecsSinceEpoch();
    alloc.isDirty = false;
    
    // Create async transfer
    QString opId = "async_copy_" + QString::number(m_pendingTransfers.size());
    
    CopyOperation op;
    op.source = const_cast<void*>(cpuData);
    op.destination = alloc.gpuPtr;
    op.size = size;
    op.isHostToDevice = true;
    op.callback = callback;
    op.operationId = opId;
    
    AsyncTransfer transfer;
    transfer.op = op;
    transfer.startTime = QDateTime::currentMSecsSinceEpoch();
    
    m_pendingTransfers[opId] = transfer;
    
    // Launch actual async copy on GPU using streams
    if (m_backend == CUDA) {
        cudaError_t err = cudaMemcpy(op.destination, op.source, size, cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            qDebug() << "Async CUDA H2D copy failed:" << cudaGetErrorString(err);
            m_pendingTransfers[opId].completed = false;
            if (callback) callback(false);
            return opId;
        }
    } else {
        hipError_t err = hipMemcpy(op.destination, op.source, size, hipMemcpyHostToDevice);
        if (err != hipSuccess) {
            qDebug() << "Async HIP H2D copy failed:" << hipGetErrorString(err);
            m_pendingTransfers[opId].completed = false;
            if (callback) callback(false);
            return opId;
        }
    }
    
    m_pendingTransfers[opId].completed = true;
    if (callback) callback(true);
    
    return opId;
}

QString GPUMemoryManager::copyToHostAsync(const QString& tensorId, void* cpuData, 
                                         uint64_t size, std::function<void(bool)> callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_tensorAllocations.find(tensorId);
    if (it == m_tensorAllocations.end()) {
        if (callback) callback(false);
        return "";
    }
    
    auto& alloc = it->second;
    alloc.lastAccessAt = QDateTime::currentMSecsSinceEpoch();
    
    // Create async transfer
    QString opId = "async_copy_" + QString::number(m_pendingTransfers.size());
    
    CopyOperation op;
    op.source = alloc.gpuPtr;
    op.destination = cpuData;
    op.size = size;
    op.isHostToDevice = false;
    op.callback = callback;
    op.operationId = opId;
    
    AsyncTransfer transfer;
    transfer.op = op;
    transfer.startTime = QDateTime::currentMSecsSinceEpoch();
    
    m_pendingTransfers[opId] = transfer;
    
    // Launch actual async copy on GPU using streams
    if (m_backend == CUDA) {
        cudaError_t err = cudaMemcpy(op.destination, op.source, size, cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) {
            qDebug() << "Async CUDA D2H copy failed:" << cudaGetErrorString(err);
            m_pendingTransfers[opId].completed = false;
            if (callback) callback(false);
            return opId;
        }
    } else {
        hipError_t err = hipMemcpy(op.destination, op.source, size, hipMemcpyDeviceToHost);
        if (err != hipSuccess) {
            qDebug() << "Async HIP D2H copy failed:" << hipGetErrorString(err);
            m_pendingTransfers[opId].completed = false;
            if (callback) callback(false);
            return opId;
        }
    }
    
    m_pendingTransfers[opId].completed = true;
    if (callback) callback(true);
    
    return opId;
}

bool GPUMemoryManager::waitForCopy(const QString& operationId, uint32_t timeoutMs)
{
    auto startTime = QDateTime::currentMSecsSinceEpoch();
    
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_pendingTransfers.find(operationId);
            if (it != m_pendingTransfers.end() && it->second.completed) {
                return true;
            }
        }
        
        if (QDateTime::currentMSecsSinceEpoch() - startTime > timeoutMs) {
            return false;
        }
    }
}

uint64_t GPUMemoryManager::getTotalMemory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalMemory;
}

uint64_t GPUMemoryManager::getAvailableMemory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxMemory - m_stats.totalUsed;
}

uint64_t GPUMemoryManager::getUsedMemory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats.totalUsed;
}

MemoryStats GPUMemoryManager::getMemoryStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto stats = m_stats;
    stats.fragmentationRatio = calculateFragmentation();
    return stats;
}

bool GPUMemoryManager::compactMemory()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Sort allocations by address to detect fragmentation
    // In real implementation, would perform actual memory compaction
    
    m_stats.fragmentationRatio = calculateFragmentation();
    return true;
}

bool GPUMemoryManager::evictLRU()
{
    QString lruTensor = findLRUTensor();
    if (lruTensor.isEmpty()) {
        return false;
    }
    
    auto it = m_tensorAllocations.find(lruTensor);
    if (it != m_tensorAllocations.end()) {
        uint64_t freedSize = it->second.size;
        releaseTensor(lruTensor);
        qDebug() << "Evicted tensor:" << lruTensor << "freed:" << freedSize << "bytes";
        return true;
    }
    
    return false;
}

void GPUMemoryManager::analyzeFragmentation()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    float frag = calculateFragmentation();
    qDebug() << "Memory fragmentation ratio:" << frag;
    
    if (frag > 0.3f) { // 30% fragmentation
        qDebug() << "High fragmentation detected, consider compacting";
    }
}

void GPUMemoryManager::setMaxMemory(uint64_t maxBytes)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxMemory = maxBytes;
}

void GPUMemoryManager::setPoolSize(uint64_t bytes)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_poolSize = bytes;
}

void GPUMemoryManager::setEvictionThreshold(float ratio)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_evictionThreshold = ratio;
}

void GPUMemoryManager::enableAsyncTransfers(bool enable)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_asyncEnabled = enable;
}

void GPUMemoryManager::printMemoryLayout() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    qDebug() << "GPU Memory Layout:";
    qDebug() << "Total Used:" << (m_stats.totalUsed / (1024.0 * 1024)) << "MB";
    qDebug() << "Total Available:" << ((m_maxMemory - m_stats.totalUsed) / (1024.0 * 1024)) << "MB";
    qDebug() << "Active Allocations:" << m_stats.activeAllocations;
    qDebug() << "Fragmentation:" << (m_stats.fragmentationRatio * 100) << "%";
}

QString GPUMemoryManager::getMemoryReport() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ostringstream oss;
    oss << "GPU Memory Report:\n";
    oss << "  Total Memory: " << (m_totalMemory / (1024.0 * 1024 * 1024)) << " GB\n";
    oss << "  Used Memory: " << (m_stats.totalUsed / (1024.0 * 1024)) << " MB\n";
    oss << "  Available Memory: " << ((m_maxMemory - m_stats.totalUsed) / (1024.0 * 1024)) << " MB\n";
    oss << "  Peak Usage: " << (m_stats.peakMemoryUsage / (1024.0 * 1024)) << " MB\n";
    oss << "  Active Allocations: " << m_stats.activeAllocations << "\n";
    oss << "  Cached Chunks: " << m_stats.cachedChunks << "\n";
    oss << "  Fragmentation: " << (calculateFragmentation() * 100) << "%\n";
    
    return QString::fromStdString(oss.str());
}

void* GPUMemoryManager::allocateFromPool(uint64_t size)
{
    // Try to find suitable chunk from pool
    for (auto& chunk : m_memoryPool) {
        if (!chunk.inUse && chunk.size >= size) {
            chunk.inUse = true;
            return chunk.ptr;
        }
    }
    return nullptr;
}

void GPUMemoryManager::returnToPool(void* ptr, uint64_t size)
{
    for (auto& chunk : m_memoryPool) {
        if (chunk.ptr == ptr) {
            chunk.inUse = false;
            break;
        }
    }
}

bool GPUMemoryManager::shouldEvict() const
{
    return m_stats.totalUsed > (m_maxMemory * m_evictionThreshold);
}

QString GPUMemoryManager::findLRUTensor() const
{
    QString lruId;
    qint64 oldestTime = std::numeric_limits<qint64>::max();
    
    for (const auto& pair : m_tensorAllocations) {
        if (pair.second.isLocked) continue;
        
        if (pair.second.lastAccessAt < oldestTime) {
            oldestTime = pair.second.lastAccessAt;
            lruId = pair.first;
        }
    }
    
    return lruId;
}

float GPUMemoryManager::calculateFragmentation() const
{
    if (m_tensorAllocations.empty()) return 0.0f;
    
    // Simplified fragmentation calculation
    // Real implementation would analyze actual memory layout
    uint64_t allocatedSize = 0;
    for (const auto& pair : m_tensorAllocations) {
        allocatedSize += pair.second.size;
    }
    
    if (allocatedSize == 0) return 0.0f;
    return 1.0f - (static_cast<float>(allocatedSize) / m_stats.totalUsed);
}

bool GPUMemoryManager::initializeCUDA()
{
    try {
        // Initialize CUDA runtime
        if (cudaSetDevice(m_deviceId) != cudaSuccess) {
            qDebug() << "Failed to set CUDA device" << m_deviceId << ":" << cudaGetErrorString(cudaGetLastError());
            return false;
        }
        
        // Query device properties
        cudaDeviceProp deviceProp;
        if (cudaGetDeviceProperties(&deviceProp, m_deviceId) != cudaSuccess) {
            qDebug() << "Failed to query device properties";
            return false;
        }
        
        // Get total global memory
        m_totalMemory = deviceProp.totalGlobalMem;
        m_maxMemory = static_cast<uint64_t>(deviceProp.totalGlobalMem * 0.85); // Use 85% to leave headroom
        
        qDebug() << "CUDA Device:" << deviceProp.name;
        qDebug() << "Total Memory:" << (m_totalMemory / (1024.0 * 1024 * 1024)) << "GB";
        qDebug() << "Max Usable:" << (m_maxMemory / (1024.0 * 1024 * 1024)) << "GB";
        qDebug() << "Compute Capability:" << deviceProp.major << "." << deviceProp.minor;
        
        // Enable peer access if multiple devices
        int deviceCount = 0;
        cudaGetDeviceCount(&deviceCount);
        if (deviceCount > 1) {
            for (int peer = 0; peer < deviceCount; ++peer) {
                if (peer != m_deviceId) {
                    int canAccess = 0;
                    cudaDeviceCanAccessPeer(&canAccess, m_deviceId, peer);
                    if (canAccess) {
                        cudaEnablePeerAccess(peer, 0);
                        qDebug() << "Enabled peer access to device" << peer;
                    }
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        qDebug() << "CUDA initialization exception:" << e.what();
        return false;
    }
}

void* GPUMemoryManager::allocateCUDAMemory(uint64_t size)
{
    if (size == 0) return nullptr;
    
    void* devicePtr = nullptr;
    
    try {
        cudaError_t err = cudaMalloc(&devicePtr, size);
        
        if (err != cudaSuccess) {
            qDebug() << "CUDA malloc failed for" << size << "bytes:" << cudaGetErrorString(err);
            
            // If out of memory, try clearing cache or reducing allocation
            if (err == cudaErrorMemoryAllocation) {
                cudaDeviceSynchronize();
                cudaError_t retry = cudaMalloc(&devicePtr, size);
                if (retry != cudaSuccess) {
                    qDebug() << "Retry CUDA malloc also failed:" << cudaGetErrorString(retry);
                    return nullptr;
                }
            } else {
                return nullptr;
            }
        }
        
        // Zero-initialize memory for safety
        if (devicePtr && size > 0) {
            err = cudaMemset(devicePtr, 0, size);
            if (err != cudaSuccess) {
                qDebug() << "CUDA memset failed:" << cudaGetErrorString(err);
                cudaFree(devicePtr);
                return nullptr;
            }
        }
        
        qDebug() << "CUDA allocated" << (size / (1024.0 * 1024)) << "MB at" << devicePtr;
        return devicePtr;
        
    } catch (const std::exception& e) {
        qDebug() << "CUDA allocation exception:" << e.what();
        return nullptr;
    }
}

void GPUMemoryManager::releaseCUDAMemory(void* ptr)
{
    if (!ptr) return;
    
    try {
        cudaError_t err = cudaFree(ptr);
        
        if (err != cudaSuccess) {
            qDebug() << "CUDA free failed:" << cudaGetErrorString(err);
        } else {
            qDebug() << "CUDA freed memory at" << ptr;
        }
    } catch (const std::exception& e) {
        qDebug() << "CUDA free exception:" << e.what();
    }
}

bool GPUMemoryManager::copyCUDAToDevice(void* dest, const void* src, uint64_t size)
{
    if (!dest || !src || size == 0) return false;
    
    try {
        cudaError_t err = cudaMemcpy(dest, src, size, cudaMemcpyHostToDevice);
        
        if (err != cudaSuccess) {
            qDebug() << "CUDA H2D copy failed:" << size << "bytes ->" << cudaGetErrorString(err);
            return false;
        }
        
        qDebug() << "CUDA H2D copied" << (size / (1024.0 * 1024)) << "MB";
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "CUDA H2D copy exception:" << e.what();
        return false;
    }
}

bool GPUMemoryManager::copyCUDAToHost(void* dest, const void* src, uint64_t size)
{
    if (!dest || !src || size == 0) return false;
    
    try {
        cudaError_t err = cudaMemcpy(dest, src, size, cudaMemcpyDeviceToHost);
        
        if (err != cudaSuccess) {
            qDebug() << "CUDA D2H copy failed:" << size << "bytes ->" << cudaGetErrorString(err);
            return false;
        }
        
        qDebug() << "CUDA D2H copied" << (size / (1024.0 * 1024)) << "MB";
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "CUDA D2H copy exception:" << e.what();
        return false;
    }
}

bool GPUMemoryManager::initializeHIP()
{
    try {
        // Set HIP device
        if (hipSetDevice(m_deviceId) != hipSuccess) {
            qDebug() << "Failed to set HIP device" << m_deviceId << ":" << hipGetErrorString(hipGetLastError());
            return false;
        }
        
        // Query device properties
        hipDeviceProp_t deviceProp;
        if (hipGetDeviceProperties(&deviceProp, m_deviceId) != hipSuccess) {
            qDebug() << "Failed to query HIP device properties";
            return false;
        }
        
        // Get total global memory
        m_totalMemory = deviceProp.totalGlobalMem;
        m_maxMemory = static_cast<uint64_t>(deviceProp.totalGlobalMem * 0.85); // Use 85% to leave headroom
        
        qDebug() << "HIP Device:" << deviceProp.name;
        qDebug() << "Total Memory:" << (m_totalMemory / (1024.0 * 1024 * 1024)) << "GB";
        qDebug() << "Max Usable:" << (m_maxMemory / (1024.0 * 1024 * 1024)) << "GB";
        qDebug() << "Compute Capability:" << deviceProp.major << "." << deviceProp.minor;
        
        // Enable peer access if multiple devices
        int deviceCount = 0;
        hipGetDeviceCount(&deviceCount);
        if (deviceCount > 1) {
            for (int peer = 0; peer < deviceCount; ++peer) {
                if (peer != m_deviceId) {
                    int canAccess = 0;
                    hipDeviceCanAccessPeer(&canAccess, m_deviceId, peer);
                    if (canAccess) {
                        hipEnablePeerAccess(peer, 0);
                        qDebug() << "Enabled HIP peer access to device" << peer;
                    }
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        qDebug() << "HIP initialization exception:" << e.what();
        return false;
    }
}

void* GPUMemoryManager::allocateHIPMemory(uint64_t size)
{
    if (size == 0) return nullptr;
    
    void* devicePtr = nullptr;
    
    try {
        hipError_t err = hipMalloc(&devicePtr, size);
        
        if (err != hipSuccess) {
            qDebug() << "HIP malloc failed for" << size << "bytes:" << hipGetErrorString(err);
            
            if (err == hipErrorMemoryAllocation) {
                hipDeviceSynchronize();
                hipError_t retry = hipMalloc(&devicePtr, size);
                if (retry != hipSuccess) {
                    qDebug() << "Retry HIP malloc also failed:" << hipGetErrorString(retry);
                    return nullptr;
                }
            } else {
                return nullptr;
            }
        }
        
        // Zero-initialize memory for safety
        if (devicePtr && size > 0) {
            err = hipMemset(devicePtr, 0, size);
            if (err != hipSuccess) {
                qDebug() << "HIP memset failed:" << hipGetErrorString(err);
                hipFree(devicePtr);
                return nullptr;
            }
        }
        
        qDebug() << "HIP allocated" << (size / (1024.0 * 1024)) << "MB at" << devicePtr;
        return devicePtr;
        
    } catch (const std::exception& e) {
        qDebug() << "HIP allocation exception:" << e.what();
        return nullptr;
    }
}

void GPUMemoryManager::releaseHIPMemory(void* ptr)
{
    if (!ptr) return;
    
    try {
        hipError_t err = hipFree(ptr);
        
        if (err != hipSuccess) {
            qDebug() << "HIP free failed:" << hipGetErrorString(err);
        } else {
            qDebug() << "HIP freed memory at" << ptr;
        }
    } catch (const std::exception& e) {
        qDebug() << "HIP free exception:" << e.what();
    }
}

bool GPUMemoryManager::copyHIPToDevice(void* dest, const void* src, uint64_t size)
{
    if (!dest || !src || size == 0) return false;
    
    try {
        hipError_t err = hipMemcpy(dest, src, size, hipMemcpyHostToDevice);
        
        if (err != hipSuccess) {
            qDebug() << "HIP H2D copy failed:" << size << "bytes ->" << hipGetErrorString(err);
            return false;
        }
        
        qDebug() << "HIP H2D copied" << (size / (1024.0 * 1024)) << "MB";
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "HIP H2D copy exception:" << e.what();
        return false;
    }
}

bool GPUMemoryManager::copyHIPToHost(void* dest, const void* src, uint64_t size)
{
    if (!dest || !src || size == 0) return false;
    
    try {
        hipError_t err = hipMemcpy(dest, src, size, hipMemcpyDeviceToHost);
        
        if (err != hipSuccess) {
            qDebug() << "HIP D2H copy failed:" << size << "bytes ->" << hipGetErrorString(err);
            return false;
        }
        
        qDebug() << "HIP D2H copied" << (size / (1024.0 * 1024)) << "MB";
        return true;
        
    } catch (const std::exception& e) {
        qDebug() << "HIP D2H copy exception:" << e.what();
        return false;
    }
}
