#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "../headers/asm_bindings.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace {

using Clock = std::chrono::steady_clock;

struct DmaTransferState {
    bool complete = false;
};

struct ConflictResource {
    uint64_t resourceId = 0;
    uint64_t resourceSize = 0;
    std::timed_mutex lock;
};

struct SchedulerTaskState {
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
};

std::atomic<uint64_t> g_nextDmaTransferId{1};
std::atomic<uint64_t> g_nextConflictHandle{1};
std::atomic<uint64_t> g_nextTaskId{1};

std::mutex g_dmaMutex;
std::unordered_map<uint64_t, DmaTransferState> g_dmaTransfers;

std::mutex g_conflictMutex;
std::unordered_map<uint64_t, std::shared_ptr<ConflictResource>> g_conflictResources;

std::mutex g_heartbeatMutex;
std::unordered_map<uint32_t, std::string> g_heartbeatNodes;
std::atomic<uint32_t> g_heartbeatIntervalMs{1000};
std::atomic<bool> g_heartbeatInitialized{false};

std::mutex g_schedulerMutex;
std::unordered_map<uint64_t, std::shared_ptr<SchedulerTaskState>> g_schedulerTasks;
std::atomic<bool> g_schedulerInitialized{false};
std::atomic<uint32_t> g_schedulerMaxWorkers{4};

uint32_t crc32Update(uint32_t crc, const uint8_t* data, uint64_t length) {
    static uint32_t table[256];
    static std::atomic<bool> tableInit{false};
    if (!tableInit.load(std::memory_order_acquire)) {
        uint32_t tmp[256];
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1U) ? (0xEDB88320U ^ (c >> 1U)) : (c >> 1U);
            }
            tmp[i] = c;
        }
        for (int i = 0; i < 256; ++i) {
            table[i] = tmp[i];
        }
        tableInit.store(true, std::memory_order_release);
    }

    uint32_t out = ~crc;
    for (uint64_t i = 0; i < length; ++i) {
        out = table[(out ^ data[i]) & 0xFFU] ^ (out >> 8U);
    }
    return ~out;
}

}  // namespace

extern "C" {

void* AllocateDMABuffer(uint64_t size_bytes) {
    if (size_bytes == 0) {
        return nullptr;
    }
#ifdef _WIN32
    return VirtualAlloc(nullptr, static_cast<SIZE_T>(size_bytes), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    constexpr size_t kAlign = 64;
    const size_t size = static_cast<size_t>(size_bytes);
    const size_t aligned = (size + (kAlign - 1U)) & ~(kAlign - 1U);
    return std::aligned_alloc(kAlign, aligned);
#endif
}

uint64_t GPU_SubmitDMATransfer(const void* src_buffer, void* dst_buffer, uint64_t size_bytes) {
    if (src_buffer == nullptr || dst_buffer == nullptr || size_bytes == 0) {
        return 0;
    }

    std::memcpy(dst_buffer, src_buffer, static_cast<size_t>(size_bytes));

    const uint64_t transferId = g_nextDmaTransferId.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_dmaMutex);
    g_dmaTransfers[transferId] = DmaTransferState{true};
    return transferId;
}

int GPU_WaitForDMA(uint64_t transfer_id, uint32_t timeout_ms) {
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms == 0 ? 60000U : timeout_ms);
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(g_dmaMutex);
            auto it = g_dmaTransfers.find(transfer_id);
            if (it != g_dmaTransfers.end() && it->second.complete) {
                g_dmaTransfers.erase(it);
                return 1;
            }
        }
        if (Clock::now() >= deadline) {
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

uint32_t CalculateCRC32(const void* data, uint64_t length, uint32_t initial_crc) {
    if (data == nullptr || length == 0) {
        return initial_crc;
    }
    return crc32Update(initial_crc, static_cast<const uint8_t*>(data), length);
}

int ConflictDetector_Initialize(void) {
    std::lock_guard<std::mutex> lock(g_conflictMutex);
    g_conflictResources.clear();
    g_nextConflictHandle.store(1, std::memory_order_relaxed);
    return 1;
}

uint64_t ConflictDetector_RegisterResource(uint64_t resource_id, uint64_t resource_size) {
    const uint64_t handle = g_nextConflictHandle.fetch_add(1, std::memory_order_relaxed);
    auto resource = std::make_shared<ConflictResource>();
    resource->resourceId = resource_id;
    resource->resourceSize = resource_size;

    std::lock_guard<std::mutex> lock(g_conflictMutex);
    g_conflictResources[handle] = std::move(resource);
    return handle;
}

int ConflictDetector_LockResource(uint64_t resource_handle, uint32_t lock_timeout_us) {
    std::shared_ptr<ConflictResource> resource;
    {
        std::lock_guard<std::mutex> lock(g_conflictMutex);
        auto it = g_conflictResources.find(resource_handle);
        if (it == g_conflictResources.end()) {
            return 0;
        }
        resource = it->second;
    }
    if (lock_timeout_us == 0) {
        resource->lock.lock();
        return 1;
    }
    return resource->lock.try_lock_for(std::chrono::microseconds(lock_timeout_us)) ? 1 : 0;
}

int ConflictDetector_UnlockResource(uint64_t resource_handle) {
    std::shared_ptr<ConflictResource> resource;
    {
        std::lock_guard<std::mutex> lock(g_conflictMutex);
        auto it = g_conflictResources.find(resource_handle);
        if (it == g_conflictResources.end()) {
            return 0;
        }
        resource = it->second;
    }
    resource->lock.unlock();
    return 1;
}

uint64_t GetHighResTick(void) {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
}

uint64_t TicksToMicroseconds(uint64_t ticks) {
    return ticks / 1000ULL;
}

uint64_t TicksToMilliseconds(uint64_t ticks) {
    return ticks / 1000000ULL;
}

int Heartbeat_Initialize(uint32_t interval_ms) {
    g_heartbeatIntervalMs.store(interval_ms == 0 ? 1000U : interval_ms, std::memory_order_relaxed);
    g_heartbeatInitialized.store(true, std::memory_order_release);
    return 1;
}

int Heartbeat_AddNode(uint32_t node_id, const char* node_name) {
    if (!g_heartbeatInitialized.load(std::memory_order_acquire)) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_heartbeatMutex);
    g_heartbeatNodes[node_id] = node_name ? node_name : "";
    return 1;
}

int Heartbeat_Shutdown(void) {
    std::lock_guard<std::mutex> lock(g_heartbeatMutex);
    g_heartbeatNodes.clear();
    g_heartbeatInitialized.store(false, std::memory_order_release);
    return 1;
}

int Scheduler_Initialize(uint32_t max_workers) {
    g_schedulerMaxWorkers.store(max_workers == 0 ? 1U : max_workers, std::memory_order_relaxed);
    g_schedulerInitialized.store(true, std::memory_order_release);
    return 1;
}

uint64_t Scheduler_SubmitTask(void (*task_func)(void*), void* task_context, uint8_t /*priority*/) {
    if (!g_schedulerInitialized.load(std::memory_order_acquire) || task_func == nullptr) {
        return 0;
    }

    const uint64_t taskId = g_nextTaskId.fetch_add(1, std::memory_order_relaxed);
    auto state = std::make_shared<SchedulerTaskState>();
    {
        std::lock_guard<std::mutex> lock(g_schedulerMutex);
        g_schedulerTasks[taskId] = state;
    }

    std::thread([state, task_func, task_context]() {
        task_func(task_context);
        {
            std::lock_guard<std::mutex> lock(state->mtx);
            state->done = true;
        }
        state->cv.notify_all();
    }).detach();

    return taskId;
}

int Scheduler_WaitForTask(uint64_t task_id, uint32_t timeout_ms, void* result_buffer, uint32_t result_size) {
    std::shared_ptr<SchedulerTaskState> state;
    {
        std::lock_guard<std::mutex> lock(g_schedulerMutex);
        auto it = g_schedulerTasks.find(task_id);
        if (it == g_schedulerTasks.end()) {
            return 0;
        }
        state = it->second;
    }

    std::unique_lock<std::mutex> lock(state->mtx);
    if (timeout_ms == 0) {
        state->cv.wait(lock, [&]() { return state->done; });
    } else if (!state->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&]() { return state->done; })) {
        return 0;
    }

    if (result_buffer != nullptr && result_size >= sizeof(uint32_t)) {
        uint32_t ok = 1;
        std::memcpy(result_buffer, &ok, sizeof(ok));
    }

    {
        std::lock_guard<std::mutex> mapLock(g_schedulerMutex);
        g_schedulerTasks.erase(task_id);
    }
    return 1;
}

int Scheduler_Shutdown(void) {
    std::lock_guard<std::mutex> lock(g_schedulerMutex);
    g_schedulerTasks.clear();
    g_schedulerInitialized.store(false, std::memory_order_release);
    return 1;
}

int Tensor_QuantizedMatMul(const float* a, const float* b, float* result, uint32_t m, uint32_t k, uint32_t n, uint8_t /*quant_bits*/) {
    if (a == nullptr || b == nullptr || result == nullptr || m == 0 || k == 0 || n == 0) {
        return 0;
    }
    for (uint32_t row = 0; row < m; ++row) {
        for (uint32_t col = 0; col < n; ++col) {
            float acc = 0.0f;
            for (uint32_t i = 0; i < k; ++i) {
                acc += a[row * k + i] * b[i * n + col];
            }
            result[row * n + col] = acc;
        }
    }
    return 1;
}

int INFINITY_Shutdown(void) {
    return 1;
}

int NtQuerySystemInformation(uint32_t /*info_class*/, void* /*info_buffer*/, uint32_t /*info_length*/, uint32_t* return_length) {
    if (return_length != nullptr) {
        *return_length = 0;
    }
#ifdef _WIN32
    return static_cast<int>(0xC0000002);  // STATUS_NOT_IMPLEMENTED fallback in runtime bridge
#else
    return -1;
#endif
}

int RtlGetVersion(void* /*version_info*/, uint32_t /*version_info_size*/) {
#ifdef _WIN32
    return static_cast<int>(0xC0000002);  // STATUS_NOT_IMPLEMENTED fallback in runtime bridge
#else
    return -1;
#endif
}

}  // extern "C"
