// reverse_engineered_stubs.cpp — Minimal fallback implementations when RawrXD_Complete_ReverseEngineered.asm is not linked.
// Used by RawrXD-Win32IDE when the full ASM kernel is excluded. Implements minimal real logic for each exported symbol.

#include "../../include/reverse_engineered_bridge.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <thread>
#include <mutex>
#include <queue>
#include <map>
#include <condition_variable>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

void INFINITY_Shutdown(void) {
    (void)0;
}

// ----- Scheduler: minimal single-worker queue -----
typedef void (*TaskFn)(void*);
struct SchedTask { int64_t id; TaskFn fn; void* arg; };
static std::mutex s_schedMutex;
static std::queue<SchedTask> s_schedQueue;
static std::map<int64_t, void*> s_schedResults;
static std::condition_variable s_schedCv;
static std::atomic<int64_t> s_nextTaskId{1};
static std::atomic<bool> s_schedRunning{false};
static std::thread s_schedWorker;

static void schedWorkerLoop() {
    while (s_schedRunning.load()) {
        SchedTask t = {};
        {
            std::unique_lock<std::mutex> lock(s_schedMutex);
            s_schedCv.wait_for(lock, std::chrono::milliseconds(50), [&]() {
                return !s_schedRunning.load() || !s_schedQueue.empty();
            });
            if (!s_schedRunning.load()) break;
            if (s_schedQueue.empty()) continue;
            t = s_schedQueue.front();
            s_schedQueue.pop();
        }
        if (t.fn) {
            t.fn(t.arg);
            std::lock_guard<std::mutex> lock(s_schedMutex);
            s_schedResults[t.id] = nullptr;
            s_schedCv.notify_all();
        }
    }
}

int Scheduler_Initialize(uint32_t /*workerCount*/, uint32_t /*numaEnabled*/) {
    if (s_schedRunning.exchange(true)) return 0;
    s_schedWorker = std::thread(schedWorkerLoop);
    return 0;
}

void Scheduler_Shutdown(void) {
    s_schedRunning.store(false);
    s_schedCv.notify_all();
    if (s_schedWorker.joinable()) s_schedWorker.join();
}

int64_t Scheduler_SubmitTask(void* fn, void* arg,
                            uint32_t /*priority*/, uint32_t /*depCount*/,
                            const int64_t* /*depIds*/) {
    if (!fn) return -1;
    int64_t id = s_nextTaskId++;
    std::lock_guard<std::mutex> lock(s_schedMutex);
    s_schedQueue.push({id, (TaskFn)fn, arg});
    s_schedCv.notify_one();
    return id;
}

void* Scheduler_WaitForTask(int64_t taskId, uint32_t timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::unique_lock<std::mutex> lock(s_schedMutex);
    bool done = s_schedCv.wait_until(lock, deadline, [&]() {
        return s_schedResults.count(taskId) != 0;
    });
    if (done) {
        auto it = s_schedResults.find(taskId);
        void* r = (it != s_schedResults.end()) ? it->second : nullptr;
        s_schedResults.erase(taskId);
        return r;
    }
    return nullptr;
}

// ----- ConflictDetector: minimal mutex-based resource lock -----
static std::mutex s_conflictMutex;
static std::map<uint64_t, int64_t> s_resourceOwner;  // resourceId -> taskId (-1 = unlocked)
static uint32_t s_maxResources = 0;

int ConflictDetector_Initialize(uint32_t maxResources, uint32_t /*checkIntervalMs*/) {
    s_maxResources = maxResources > 0 ? maxResources : 256;
    return 0;
}

int ConflictDetector_RegisterResource(uint64_t resourceId) {
    std::lock_guard<std::mutex> lock(s_conflictMutex);
    if (s_resourceOwner.size() >= s_maxResources && s_resourceOwner.count(resourceId) == 0)
        return -1;
    s_resourceOwner[resourceId] = -1;
    return 0;
}

int ConflictDetector_LockResource(uint64_t resourceId, int64_t taskId, uint32_t timeoutMs) {
    (void)timeoutMs;
    std::lock_guard<std::mutex> lock(s_conflictMutex);
    auto it = s_resourceOwner.find(resourceId);
    if (it == s_resourceOwner.end()) return -1;
    if (it->second >= 0 && it->second != taskId) return 1;  // deadlock / contested
    it->second = taskId;
    return 0;
}

int ConflictDetector_UnlockResource(uint64_t resourceId) {
    std::lock_guard<std::mutex> lock(s_conflictMutex);
    auto it = s_resourceOwner.find(resourceId);
    if (it == s_resourceOwner.end()) return -1;
    it->second = -1;
    return 0;
}

int Heartbeat_Initialize(uint16_t /*listenPort*/, uint32_t /*sendIntervalMs*/) {
    return 0;
}

int Heartbeat_AddNode(uint32_t /*nodeId*/, const char* /*ip*/, uint16_t /*port*/) {
    return 0;
}

void Heartbeat_Shutdown(void) {
}

int GPU_SubmitDMATransfer(void* src, void* dst, uint64_t size, void* /*slotPtr*/) {
    if (!src || !dst || size == 0) return -1;
    memcpy(dst, src, (size_t)size);
    return 0;
}

int GPU_WaitForDMA(void* /*slotPtr*/, uint64_t /*timeoutNs*/) {
    return 1;  // CPU fallback: transfer already done in SubmitDMATransfer
}

void Tensor_QuantizedMatMul(const void* /*A*/, const void* /*B*/, float* C,
                           uint32_t M, uint32_t N, uint32_t /*K*/,
                           uint32_t /*quantType*/) {
    for (uint32_t i = 0; i < M * N; ++i) C[i] = 0.f;  // minimal fallback: zero output
}

uint64_t GetHighResTick(void) {
#ifdef _WIN32
    LARGE_INTEGER t;
    if (QueryPerformanceCounter(&t))
        return (uint64_t)t.QuadPart;
#endif
    return 0;
}

uint64_t TicksToMicroseconds(uint64_t ticks) {
#ifdef _WIN32
    LARGE_INTEGER freq;
    if (QueryPerformanceFrequency(&freq) && freq.QuadPart > 0)
        return (ticks * 1000000ULL) / (uint64_t)freq.QuadPart;
#endif
    return ticks;  /* fallback: assume ticks are microseconds */
}

uint64_t TicksToMilliseconds(uint64_t ticks) {
#ifdef _WIN32
    LARGE_INTEGER freq;
    if (QueryPerformanceFrequency(&freq) && freq.QuadPart > 0)
        return (ticks * 1000ULL) / (uint64_t)freq.QuadPart;
#endif
    return ticks / 1000;
}

static uint32_t s_crc32Table[256];
static int s_crc32TableInit = 0;
static void initCrc32Table() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        s_crc32Table[i] = c;
    }
    s_crc32TableInit = 1;
}

uint32_t CalculateCRC32(const void* data, uint64_t len) {
    if (!data) return 0;
    if (s_crc32TableInit == 0) initCrc32Table();
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t* p = (const uint8_t*)data;
    for (uint64_t i = 0; i < len; ++i)
        crc = s_crc32Table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

void* AllocateDMABuffer(uint64_t size) {
#ifdef _WIN32
    if (size == 0 || size > 0x7FFFFFFF) return nullptr;
    return VirtualAlloc(nullptr, (SIZE_T)size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    (void)size;
    return nullptr;
#endif
}

#ifdef __cplusplus
}
#endif
