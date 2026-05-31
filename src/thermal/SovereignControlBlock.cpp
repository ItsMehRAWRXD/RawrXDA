/**
 * @file SovereignControlBlock.cpp
 * @brief Shared Memory Manager Implementation
 * 
 * Full production implementation of the shared memory interface
 * for C++/MASM thermal control communication.
 * 
 * @copyright RawrXD IDE 2026
 */

#include "SovereignControlBlock.h"
#include <chrono>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// SharedMemoryManager Implementation
// ═══════════════════════════════════════════════════════════════════════════════

SharedMemoryManager::SharedMemoryManager()
    : m_controlBlock(nullptr)
{
#ifdef _WIN32
    m_hMapping = nullptr;
#else
    m_fd = -1;
#endif
}

SharedMemoryManager::~SharedMemoryManager()
{
    close();
}

bool SharedMemoryManager::create(const std::string& name)
{
    if (isOpen()) {
        close();
    }
    
    m_name = name;
    
#ifdef _WIN32
    // Create named shared memory on Windows
    std::wstring wname(name.begin(), name.end());
    
    m_hMapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        SOVEREIGN_CONTROL_BLOCK_SIZE,
        wname.c_str()
    );
    
    if (m_hMapping == nullptr) {
        return false;
    }
    
    bool alreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);
    
    m_controlBlock = static_cast<SovereignControlBlock*>(
        MapViewOfFile(
            m_hMapping,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            SOVEREIGN_CONTROL_BLOCK_SIZE
        )
    );
    
    if (m_controlBlock == nullptr) {
        CloseHandle(m_hMapping);
        m_hMapping = nullptr;
        return false;
    }
    
    // Initialize if newly created
    if (!alreadyExists) {
        initialize();
    }
    
#else
    // Create POSIX shared memory
    std::string shmName = "/" + name;
    
    m_fd = shm_open(shmName.c_str(), O_CREAT | O_RDWR, 0666);
    if (m_fd == -1) {
        return false;
    }
    
    if (ftruncate(m_fd, SOVEREIGN_CONTROL_BLOCK_SIZE) == -1) {
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    
    m_controlBlock = static_cast<SovereignControlBlock*>(
        mmap(nullptr, SOVEREIGN_CONTROL_BLOCK_SIZE,
             PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0)
    );
    
    if (m_controlBlock == MAP_FAILED) {
        ::close(m_fd);
        m_fd = -1;
        m_controlBlock = nullptr;
        return false;
    }
    
    // Check if needs initialization
    if (m_controlBlock->magic != SovereignControlBlock::MAGIC) {
        initialize();
    }
#endif
    
    return true;
}

bool SharedMemoryManager::open(const std::string& name)
{
    if (isOpen()) {
        close();
    }
    
    m_name = name;
    
#ifdef _WIN32
    std::wstring wname(name.begin(), name.end());
    
    m_hMapping = OpenFileMappingW(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        wname.c_str()
    );
    
    if (m_hMapping == nullptr) {
        return false;
    }
    
    m_controlBlock = static_cast<SovereignControlBlock*>(
        MapViewOfFile(
            m_hMapping,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            SOVEREIGN_CONTROL_BLOCK_SIZE
        )
    );
    
    if (m_controlBlock == nullptr) {
        CloseHandle(m_hMapping);
        m_hMapping = nullptr;
        return false;
    }
    
#else
    std::string shmName = "/" + name;
    
    m_fd = shm_open(shmName.c_str(), O_RDWR, 0666);
    if (m_fd == -1) {
        return false;
    }
    
    m_controlBlock = static_cast<SovereignControlBlock*>(
        mmap(nullptr, SOVEREIGN_CONTROL_BLOCK_SIZE,
             PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0)
    );
    
    if (m_controlBlock == MAP_FAILED) {
        ::close(m_fd);
        m_fd = -1;
        m_controlBlock = nullptr;
        return false;
    }
#endif
    
    // Verify magic number
    if (m_controlBlock->magic != SovereignControlBlock::MAGIC) {
        close();
        return false;
    }
    
    return true;
}

void SharedMemoryManager::close()
{
#ifdef _WIN32
    if (m_controlBlock != nullptr) {
        UnmapViewOfFile(m_controlBlock);
        m_controlBlock = nullptr;
    }
    
    if (m_hMapping != nullptr) {
        CloseHandle(m_hMapping);
        m_hMapping = nullptr;
    }
#else
    if (m_controlBlock != nullptr) {
        munmap(m_controlBlock, SOVEREIGN_CONTROL_BLOCK_SIZE);
        m_controlBlock = nullptr;
    }
    
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

void SharedMemoryManager::initialize()
{
    if (m_controlBlock == nullptr) return;
    
    // Zero out entire block
    std::memset(m_controlBlock, 0, SOVEREIGN_CONTROL_BLOCK_SIZE);
    
    // Set magic and version
    m_controlBlock->magic = SovereignControlBlock::MAGIC;
    m_controlBlock->version = SovereignControlBlock::VERSION;
    m_controlBlock->sequenceNumber = 1;
    
    // Set defaults
    m_controlBlock->burstMode = static_cast<int32_t>(BurstMode::ADAPTIVE_HYBRID);
    m_controlBlock->activeDrives = 0;
    m_controlBlock->thermalThreshold = 60.0;
    m_controlBlock->thermalHeadroom = 30.0;
    
    // Initialize throttle
    m_controlBlock->throttle.throttlePercent = 0;
    m_controlBlock->throttle.flags = 0;
    m_controlBlock->throttle.pauseLoopCount = 0;
    
    // Initialize drive command
    m_controlBlock->driveCmd.selectedDrive = -1;
    m_controlBlock->driveCmd.previousDrive = -1;
    m_controlBlock->driveCmd.thermalHeadroom = 0.0;
    
    // Initialize auth
    m_controlBlock->auth.sessionKey = 0;
    m_controlBlock->auth.keyValid = 0;
    m_controlBlock->auth.authAttempts = 0;
    
    updateTimestamp();
}

void SharedMemoryManager::incrementSequence()
{
    if (m_controlBlock != nullptr) {
#ifdef _WIN32
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&m_controlBlock->sequenceNumber));
#else
        __sync_add_and_fetch(&m_controlBlock->sequenceNumber, 1);
#endif
    }
}

void SharedMemoryManager::updateTimestamp()
{
    if (m_controlBlock != nullptr) {
        using namespace std::chrono;
        m_controlBlock->lastUpdateTime = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count();
        incrementSequence();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Setter Methods
// ═══════════════════════════════════════════════════════════════════════════════

void SharedMemoryManager::setNVMeTemperature(int driveIndex, double temp)
{
    if (m_controlBlock != nullptr && driveIndex >= 0 && driveIndex < 5) {
        m_controlBlock->nvmeTemps[driveIndex] = temp;
        updateTimestamp();
    }
}

void SharedMemoryManager::setGPUTemperature(double temp)
{
    if (m_controlBlock != nullptr) {
        m_controlBlock->gpuTemp = temp;
        updateTimestamp();
    }
}

void SharedMemoryManager::setCPUTemperature(double temp)
{
    if (m_controlBlock != nullptr) {
        m_controlBlock->cpuTemp = temp;
        updateTimestamp();
    }
}

void SharedMemoryManager::setThrottlePercent(int percent)
{
    if (m_controlBlock != nullptr) {
        m_controlBlock->throttle.throttlePercent = percent;
        
        // Calculate PAUSE loop count based on throttle percentage
        // Higher throttle = more PAUSE iterations = slower throughput
        m_controlBlock->throttle.pauseLoopCount = percent * 10;  // Scale factor
        
        updateTimestamp();
    }
}

void SharedMemoryManager::setSoftThrottle(double value)
{
    if (m_controlBlock != nullptr) {
        m_controlBlock->softThrottleValue = value;
        m_controlBlock->throttle.flags |= static_cast<uint32_t>(ThrottleFlags::SOFT_THROTTLE);
        updateTimestamp();
    }
}

void SharedMemoryManager::setBurstMode(BurstMode mode)
{
    if (m_controlBlock != nullptr) {
        m_controlBlock->burstMode = static_cast<int32_t>(mode);
        updateTimestamp();
    }
}

void SharedMemoryManager::setSelectedDrive(int driveIndex, double headroom)
{
    if (m_controlBlock != nullptr) {
        m_controlBlock->driveCmd.previousDrive = m_controlBlock->driveCmd.selectedDrive;
        m_controlBlock->driveCmd.selectedDrive = driveIndex;
        m_controlBlock->driveCmd.thermalHeadroom = headroom;
        
        using namespace std::chrono;
        m_controlBlock->driveCmd.selectionTime = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count();
        
        m_controlBlock->throttle.flags |= static_cast<uint32_t>(ThrottleFlags::DRIVE_SWITCH_PENDING);
        updateTimestamp();
    }
}

void SharedMemoryManager::setPrediction(double predictedTemp, double confidence, bool valid)
{
    if (m_controlBlock != nullptr) {
        m_controlBlock->prediction.predictedTemp = predictedTemp;
        m_controlBlock->prediction.confidence = confidence;
        m_controlBlock->prediction.isValid = valid ? 1 : 0;
        m_controlBlock->predictionValid = valid ? 1 : 0;
        
        if (valid) {
            m_controlBlock->throttle.flags |= static_cast<uint32_t>(ThrottleFlags::PREDICTIVE_ENABLED);
        } else {
            m_controlBlock->throttle.flags &= ~static_cast<uint32_t>(ThrottleFlags::PREDICTIVE_ENABLED);
        }
        
        updateTimestamp();
    }
}

void SharedMemoryManager::setSessionKey(uint64_t key)
{
    if (m_controlBlock != nullptr) {
        m_controlBlock->auth.sessionKey = key;
        m_controlBlock->auth.keyValid = (key != 0) ? 1 : 0;
        
        using namespace std::chrono;
        m_controlBlock->auth.timestamp = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()
        ).count();
        
        if (key != 0) {
            m_controlBlock->throttle.flags |= static_cast<uint32_t>(ThrottleFlags::SESSION_KEY_VALID);
        } else {
            m_controlBlock->throttle.flags &= ~static_cast<uint32_t>(ThrottleFlags::SESSION_KEY_VALID);
        }
        
        updateTimestamp();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Getter Methods
// ═══════════════════════════════════════════════════════════════════════════════

double SharedMemoryManager::getNVMeTemperature(int driveIndex) const
{
    if (m_controlBlock != nullptr && driveIndex >= 0 && driveIndex < 5) {
        return m_controlBlock->nvmeTemps[driveIndex];
    }
    return 0.0;
}

double SharedMemoryManager::getGPUTemperature() const
{
    return m_controlBlock ? m_controlBlock->gpuTemp : 0.0;
}

double SharedMemoryManager::getCPUTemperature() const
{
    return m_controlBlock ? m_controlBlock->cpuTemp : 0.0;
}

int SharedMemoryManager::getThrottlePercent() const
{
    return m_controlBlock ? m_controlBlock->throttle.throttlePercent : 0;
}

BurstMode SharedMemoryManager::getBurstMode() const
{
    return m_controlBlock ? static_cast<BurstMode>(m_controlBlock->burstMode) 
                          : BurstMode::ADAPTIVE_HYBRID;
}

int SharedMemoryManager::getSelectedDrive() const
{
    return m_controlBlock ? m_controlBlock->driveCmd.selectedDrive : -1;
}

uint64_t SharedMemoryManager::getSessionKey() const
{
    return m_controlBlock ? m_controlBlock->auth.sessionKey : 0;
}

} // namespace rawrxd::thermal
