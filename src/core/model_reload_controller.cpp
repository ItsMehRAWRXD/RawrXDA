// ============================================================================
// model_reload_controller.cpp — Live Model Reload with In-Flight Draining
// ============================================================================
#include "model_reload_controller.h"
#include "../config/IDEConfig.h"
#include <algorithm>

namespace {
double reloadStatusValue(rawrxd::ReloadStatus status)
{
    return static_cast<double>(static_cast<uint8_t>(status));
}
}

namespace rawrxd {

// ============================================================================
// Construction / destruction
// ============================================================================

ModelReloadController::ModelReloadController(const ModelReloadConfig& cfg)
    : m_cfg(cfg)
{
}

ModelReloadController::~ModelReloadController()
{
    if (m_reloadThread.joinable())
        m_reloadThread.join();
}

void ModelReloadController::setModel(void* model)
{
    m_activeModel.store(model, std::memory_order_release);
    m_generation.fetch_add(1, std::memory_order_release);
    m_status.store(ReloadStatus::Idle, std::memory_order_release);
    METRICS.gauge("runtime.reload.generation", static_cast<double>(m_generation.load(std::memory_order_acquire)));
    METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Idle));
}

// ============================================================================
// acquireModel / releaseModel — lock-free fast path
// ============================================================================

void* ModelReloadController::acquireModel()
{
    // If a swap is in progress, return nullptr so the caller can back off.
    ReloadStatus st = m_status.load(std::memory_order_acquire);
    if (st == ReloadStatus::Swapping)
        return nullptr;

    m_inFlight.fetch_add(1, std::memory_order_acq_rel);

    // Double-check after incrementing: if a swap started between the check
    // and the increment, we must release and fail.
    st = m_status.load(std::memory_order_acquire);
    if (st == ReloadStatus::Swapping) {
        m_inFlight.fetch_sub(1, std::memory_order_acq_rel);
        m_drainCv.notify_all();
        return nullptr;
    }

    METRICS.gauge("runtime.reload.inflight", static_cast<double>(m_inFlight.load(std::memory_order_acquire)));

    return m_activeModel.load(std::memory_order_acquire);
}

void ModelReloadController::releaseModel()
{
    uint32_t prev = m_inFlight.fetch_sub(1, std::memory_order_acq_rel);
    METRICS.gauge("runtime.reload.inflight",
                  static_cast<double>(m_inFlight.load(std::memory_order_acquire)));
    if (prev <= 1) {
        // Was the last in-flight — notify draining thread
        m_drainCv.notify_all();
    }
}

// ============================================================================
// drainInFlight — wait for all in-flight requests to finish
// ============================================================================

bool ModelReloadController::drainInFlight(uint32_t timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now()
                  + std::chrono::milliseconds(timeoutMs);

    std::unique_lock<std::mutex> lk(m_drainMu);
    return m_drainCv.wait_until(lk, deadline, [&] {
        return m_inFlight.load(std::memory_order_acquire) == 0;
    });
}

// ============================================================================
// reload — synchronous model reload with draining
// ============================================================================

ReloadResult ModelReloadController::reload(const std::string& newModelPath)
{
    std::lock_guard<std::mutex> reloadLk(m_reloadMu);
    METRICS.increment("runtime.reload.requests_total");

    auto t0 = std::chrono::steady_clock::now();
    ReloadResult result{};

    // Phase 0: Pre-load new model while current one is still serving
    void* newModel = nullptr;
    double loadMs = 0;
    if (m_cfg.enableAsyncPreload && m_loaderFn) {
        m_status.store(ReloadStatus::Loading, std::memory_order_release);
        METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Loading));
        auto tLoad0 = std::chrono::steady_clock::now();
        newModel = m_loaderFn(newModelPath);
        auto tLoad1 = std::chrono::steady_clock::now();
        loadMs = std::chrono::duration<double, std::milli>(tLoad1 - tLoad0).count();
        METRICS.recordDuration("runtime.reload.load_ms", loadMs);

        if (!newModel) {
            m_status.store(ReloadStatus::Idle, std::memory_order_release);
            METRICS.increment("runtime.reload.failures");
            METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Failed));
            return ReloadResult::fail("Model loader returned nullptr");
        }
    }

    // Phase 1: Drain in-flight requests
    m_status.store(ReloadStatus::Draining, std::memory_order_release);
    METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Draining));
    auto tDrain0 = std::chrono::steady_clock::now();

    bool drained = drainInFlight(m_cfg.drainTimeoutMs);
    auto tDrain1 = std::chrono::steady_clock::now();
    double drainMs = std::chrono::duration<double, std::milli>(tDrain1 - tDrain0).count();

    result.drainedRequests = 0; // all that were in-flight are now done
    result.drainMs = drainMs;
    METRICS.recordDuration("runtime.reload.drain_ms", drainMs);
    METRICS.gauge("runtime.reload.inflight", static_cast<double>(m_inFlight.load(std::memory_order_acquire)));

    if (!drained && !m_cfg.allowPartialDrain) {
        // Abort reload — couldn't drain in time
        if (newModel && m_unloaderFn)
            m_unloaderFn(newModel);
        m_status.store(ReloadStatus::Idle, std::memory_order_release);
        METRICS.increment("runtime.reload.failures");
        METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Failed));
        return ReloadResult::fail("Drain timeout: in-flight requests did not complete");
    }

    // Phase 2: Serialize KV caches (if enabled)
    std::vector<uint8_t> kvData;
    if (m_cfg.enableKVMigration && m_kvSerFn) {
        m_status.store(ReloadStatus::Serializing, std::memory_order_release);
        METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Serializing));
        void* oldModel = m_activeModel.load(std::memory_order_acquire);
        kvData = m_kvSerFn(oldModel);
        METRICS.gauge("runtime.reload.kv_bytes", static_cast<double>(kvData.size()));
    }

    // Phase 3: Load model (if not pre-loaded)
    if (!newModel && m_loaderFn) {
        m_status.store(ReloadStatus::Loading, std::memory_order_release);
        METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Loading));
        auto tLoad0 = std::chrono::steady_clock::now();
        newModel = m_loaderFn(newModelPath);
        auto tLoad1 = std::chrono::steady_clock::now();
        loadMs = std::chrono::duration<double, std::milli>(tLoad1 - tLoad0).count();
        METRICS.recordDuration("runtime.reload.load_ms", loadMs);

        if (!newModel) {
            m_status.store(ReloadStatus::Idle, std::memory_order_release);
            METRICS.increment("runtime.reload.failures");
            METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Failed));
            return ReloadResult::fail("Model loader returned nullptr (deferred load)");
        }
    }

    // Phase 4: Atomic model pointer swap via CAS
    m_status.store(ReloadStatus::Swapping, std::memory_order_release);
    METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Swapping));
    auto tSwap0 = std::chrono::steady_clock::now();

    void* oldModel = m_activeModel.load(std::memory_order_acquire);
    while (!m_activeModel.compare_exchange_weak(
               oldModel, newModel,
               std::memory_order_acq_rel,
               std::memory_order_acquire))
    {
        // CAS failed — another thread changed it (shouldn't happen with
        // the reload mutex, but defensive).
    }

    m_generation.fetch_add(1, std::memory_order_release);
    auto tSwap1 = std::chrono::steady_clock::now();
    double swapMs = std::chrono::duration<double, std::milli>(tSwap1 - tSwap0).count();
    METRICS.recordDuration("runtime.reload.swap_ms", swapMs);
    METRICS.gauge("runtime.reload.generation", static_cast<double>(m_generation.load(std::memory_order_acquire)));

    // Phase 5: Restore KV state on new model
    m_status.store(ReloadStatus::Resuming, std::memory_order_release);
    METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Resuming));
    if (m_cfg.enableKVMigration && m_kvDesFn && !kvData.empty()) {
        bool kvOk = m_kvDesFn(newModel, kvData);
        if (kvOk) {
            result.resumedRequests = 1; // KV migrated
            METRICS.increment("runtime.reload.kv_resume_success");
        } else {
            result.droppedRequests = 1; // KV-incompatible
            METRICS.increment("runtime.reload.kv_resume_dropped");
        }
    }

    // Phase 6: Free old model
    if (oldModel && m_unloaderFn) {
        m_unloaderFn(oldModel);
    }

    // Done
    m_status.store(ReloadStatus::Complete, std::memory_order_release);
    auto t1 = std::chrono::steady_clock::now();

    result.finalStatus = ReloadStatus::Complete;
    result.loadMs  = loadMs;
    result.drainMs = drainMs;
    result.swapMs  = swapMs;
    result.totalMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    METRICS.increment("runtime.reload.success_total");
    METRICS.recordDuration("runtime.reload.total_ms", result.totalMs);
    METRICS.gauge("runtime.reload.last_resumed", static_cast<double>(result.resumedRequests));
    METRICS.gauge("runtime.reload.last_dropped", static_cast<double>(result.droppedRequests));

    // Settle back to idle
    m_status.store(ReloadStatus::Idle, std::memory_order_release);
    METRICS.gauge("runtime.reload.status", reloadStatusValue(ReloadStatus::Idle));

    return result;
}

// ============================================================================
// reloadAsync — non-blocking variant
// ============================================================================

void ModelReloadController::reloadAsync(
    const std::string& newModelPath,
    std::function<void(const ReloadResult&)> callback)
{
    // Detach previous async thread if any
    if (m_reloadThread.joinable())
        m_reloadThread.join();

    m_reloadThread = std::thread([this, path = newModelPath, cb = std::move(callback)]() {
        ReloadResult r = reload(path);
        if (cb) cb(r);
    });
}

} // namespace rawrxd
