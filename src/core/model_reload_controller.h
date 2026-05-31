// ============================================================================
// model_reload_controller.h — Live Model Reload with In-Flight Draining
// ============================================================================
// Zero-downtime model hot-swap:
//   1. Signal in-flight requests to drain (configurable timeout)
//   2. Serialize outstanding KV caches to staging area
//   3. Atomically swap model pointer (CAS spin on m_activeModel)
//   4. Resume suspended requests on new model if KV-compatible
//   5. Support tier downgrades (70B → 7B) with graceful fallback
//
// Integration model:
//   The controller wraps an opaque model pointer (void*) and provides
//   acquire/release semantics.  Callers call acquireModel() before each
//   inference step and releaseModel() after.  The reload controller uses
//   a generation counter + CAS to atomically swap the model pointer
//   without tearing any in-flight user.
//
// Threading:
//   - acquireModel / releaseModel: lock-free (atomic increment)
//   - reload: blocks until in-flight count drops to 0 or timeout
//   - KV serialization runs on the reload thread
// ============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rawrxd {

// ---------------------------------------------------------------------------
// ReloadStatus — lifecycle of a reload operation
// ---------------------------------------------------------------------------
enum class ReloadStatus : uint8_t {
    Idle          = 0,
    Draining      = 1,   // waiting for in-flight requests to finish
    Serializing   = 2,   // serializing KV caches
    Loading       = 3,   // loading new model weights
    Swapping      = 4,   // atomic model pointer swap
    Resuming      = 5,   // resuming suspended requests
    Complete      = 6,
    Failed        = 7,
};

// ---------------------------------------------------------------------------
// ReloadResult — outcome of a reload attempt
// ---------------------------------------------------------------------------
struct ReloadResult {
    ReloadStatus finalStatus;
    uint32_t     drainedRequests;
    uint32_t     resumedRequests;
    uint32_t     droppedRequests;   // requests that couldn't resume (KV-incompatible)
    double       totalMs;
    double       drainMs;
    double       loadMs;
    double       swapMs;
    std::string  errorMessage;

    static ReloadResult ok(double ms) {
        ReloadResult r{};
        r.finalStatus = ReloadStatus::Complete;
        r.totalMs = ms;
        return r;
    }
    static ReloadResult fail(const char* msg) {
        ReloadResult r{};
        r.finalStatus = ReloadStatus::Failed;
        r.errorMessage = msg;
        return r;
    }
};

// ---------------------------------------------------------------------------
// ModelReloadConfig
// ---------------------------------------------------------------------------
struct ModelReloadConfig {
    uint32_t drainTimeoutMs       = 5000;     // max wait for in-flight drain
    uint32_t kvSerializeTimeoutMs = 3000;     // max time to serialize KV
    bool     allowPartialDrain    = false;     // if true, cancel remaining after timeout
    bool     enableKVMigration    = true;      // try to resume KV on new model
    bool     enableAsyncPreload   = true;      // preload new model before drain
};

// ---------------------------------------------------------------------------
// Model loader callback — called by the controller to load a new model.
// Returns the new model pointer (void*), or nullptr on failure.
// ---------------------------------------------------------------------------
using ModelLoaderFn = std::function<void*(const std::string& modelPath)>;

// Model unloader callback — called to free the old model.
using ModelUnloaderFn = std::function<void(void* model)>;

// KV serializer — saves in-flight KV state.  Returns serialized bytes.
using KVSerializerFn = std::function<std::vector<uint8_t>(void* model)>;

// KV deserializer — restores KV state into the new model.
using KVDeserializerFn = std::function<bool(void* model, const std::vector<uint8_t>& data)>;

// ---------------------------------------------------------------------------
// ModelReloadController
// ---------------------------------------------------------------------------
class ModelReloadController {
public:
    explicit ModelReloadController(const ModelReloadConfig& cfg = {});
    ~ModelReloadController();

    // Set the initial model pointer.
    void setModel(void* model);

    // Register callbacks
    void setModelLoader(ModelLoaderFn fn)       { m_loaderFn = std::move(fn); }
    void setModelUnloader(ModelUnloaderFn fn)    { m_unloaderFn = std::move(fn); }
    void setKVSerializer(KVSerializerFn fn)      { m_kvSerFn = std::move(fn); }
    void setKVDeserializer(KVDeserializerFn fn)  { m_kvDesFn = std::move(fn); }

    // Acquire/release model for inference (lock-free fast path).
    // Callers MUST pair these.  acquireModel returns nullptr if a reload
    // is in the swapping phase (caller should retry or fail gracefully).
    void* acquireModel();
    void  releaseModel();

    // Current in-flight count (for monitoring).
    uint32_t inFlightCount() const {
        return m_inFlight.load(std::memory_order_acquire);
    }

    // Trigger a reload.  Blocks until complete or failed.
    ReloadResult reload(const std::string& newModelPath);

    // Trigger an async reload on a background thread.
    void reloadAsync(const std::string& newModelPath,
                     std::function<void(const ReloadResult&)> callback);

    // Current status
    ReloadStatus status() const {
        return m_status.load(std::memory_order_acquire);
    }

    // Current model generation (monotonically increasing).
    uint64_t generation() const {
        return m_generation.load(std::memory_order_acquire);
    }

private:
    bool drainInFlight(uint32_t timeoutMs);

    ModelReloadConfig       m_cfg;

    // The active model pointer — swapped atomically via CAS
    std::atomic<void*>      m_activeModel{nullptr};
    std::atomic<uint64_t>   m_generation{0};
    std::atomic<ReloadStatus> m_status{ReloadStatus::Idle};

    // In-flight reference count
    std::atomic<uint32_t>   m_inFlight{0};
    std::mutex              m_drainMu;
    std::condition_variable m_drainCv;

    // Reload serialization (only one reload at a time)
    std::mutex              m_reloadMu;

    // Callbacks
    ModelLoaderFn           m_loaderFn;
    ModelUnloaderFn         m_unloaderFn;
    KVSerializerFn          m_kvSerFn;
    KVDeserializerFn        m_kvDesFn;

    // Async reload thread
    std::thread             m_reloadThread;
};

} // namespace rawrxd
