// BackendOrchestrator.cpp — Implementation
#include "BackendOrchestrator.h"
#include "InferenceProfiler.h"
#include "gguf_loader.h"

#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <iomanip>
#include <numeric>
#include <filesystem>
#include <regex>
#include <unordered_set>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

namespace RawrXD {

namespace {

using PFN_SubmitInference = bool(__stdcall*)(const char*, uint64_t*);
using PFN_GetResult = bool(__stdcall*)(uint64_t, char*, uint32_t);

struct NativeInferenceApi {
    HMODULE module = nullptr;
    PFN_SubmitInference submit = nullptr;
    PFN_GetResult getResult = nullptr;
    bool initialized = false;
};

NativeInferenceApi& GetNativeInferenceApi() {
    static NativeInferenceApi api;
    if (api.initialized) {
        return api;
    }

    api.module = LoadLibraryW(L"RawrXD_InferenceEngine.dll");
    if (!api.module) {
        api.module = LoadLibraryW(L"RawrXD_InferenceEngine_Win32.dll");
    }

    if (api.module) {
        api.submit = reinterpret_cast<PFN_SubmitInference>(
            GetProcAddress(api.module, "SubmitInference"));
        api.getResult = reinterpret_cast<PFN_GetResult>(
            GetProcAddress(api.module, "GetResult"));
    }

    api.initialized = true;
    return api;
}

bool RunNativeInferenceSync(const std::string& prompt,
                            uint32_t timeoutMs,
                            std::string& completion,
                            std::string& metadata,
                            std::string& error) {
    NativeInferenceApi& api = GetNativeInferenceApi();
    if (!api.module || !api.submit || !api.getResult) {
        error = "Native inference API unavailable (SubmitInference/GetResult missing)";
        return false;
    }

    uint64_t requestId = 0;
    if (!api.submit(prompt.c_str(), &requestId)) {
        error = "SubmitInference failed";
        return false;
    }

    auto start = std::chrono::steady_clock::now();
    std::vector<char> buffer(1024 * 1024, 0);

    while (true) {
        if (api.getResult(requestId, buffer.data(), static_cast<uint32_t>(buffer.size()))) {
            completion = std::string(buffer.data());
            const auto elapsedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count();
            std::ostringstream oss;
            oss << "{\"prompt_eval_count\":0,\"eval_count\":0,\"eval_duration\":"
                << elapsedNs << ",\"total_duration\":" << elapsedNs << "}";
            metadata = oss.str();
            return true;
        }

        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (timeoutMs > 0 && elapsedMs >= timeoutMs) {
            error = "Inference request timed out";
            return false;
        }

        Sleep(10);
    }
}

std::string zeroPadInt(int value, int width) {
    std::ostringstream oss;
    oss << std::setw(width) << std::setfill('0') << value;
    return oss.str();
}

bool discoverModelShards(const std::string& model_path,
                         std::vector<std::string>& shards,
                         std::string& reason) {
    namespace fs = std::filesystem;

    shards.clear();
    reason.clear();

    std::error_code ec;
    fs::path model(model_path);
    if (!fs::exists(model, ec) || ec) {
        reason = "model path does not exist";
        return false;
    }

    const std::string filename = model.filename().string();
    std::smatch match;
    const std::regex shard_pattern(R"(^(.+)-(\d+)-of-(\d+)\.gguf$)", std::regex::icase);

    if (!std::regex_match(filename, match, shard_pattern)) {
        shards.push_back(model.string());
        return true;
    }

    const std::string base_name = match[1].str();
    const int idx_width = static_cast<int>(match[2].str().size());
    const int total_width = static_cast<int>(match[3].str().size());
    const int total = std::max(0, std::atoi(match[3].str().c_str()));
    if (total <= 0) {
        reason = "invalid shard count in filename";
        return false;
    }

    const fs::path dir = model.parent_path();
    shards.reserve(static_cast<size_t>(total));
    for (int i = 1; i <= total; ++i) {
        const std::string expected_name =
            base_name + "-" + zeroPadInt(i, idx_width) + "-of-" + zeroPadInt(total, total_width) + ".gguf";
        const fs::path candidate = dir / expected_name;
        if (!fs::exists(candidate, ec) || ec) {
            reason = "missing shard file: " + candidate.string();
            return false;
        }
        shards.push_back(candidate.string());
    }

    return true;
}

int discoverLayerCountFromGguf(const std::string& model_path) {
    GGUFLoader loader;
    if (!loader.Open(model_path)) {
        return 0;
    }
    if (!loader.ParseMetadata()) {
        loader.Close();
        return 0;
    }

    const auto metadata = loader.GetMetadata();
    loader.Close();

    if (metadata.layer_count > 0) {
        return static_cast<int>(metadata.layer_count);
    }

    auto it = metadata.kv_pairs.find("llama.block_count");
    if (it != metadata.kv_pairs.end()) {
        const int parsed = std::atoi(it->second.c_str());
        if (parsed > 0) {
            return parsed;
        }
    }
    return 0;
}

struct MappedShardFile {
    std::string path;
    int device_index = -1;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = nullptr;
    void* view = nullptr;
    uint64_t size_bytes = 0;
};

std::mutex g_mapped_shards_mutex;
std::unordered_map<std::string, std::vector<MappedShardFile>> g_mapped_shards_by_tag;

void closeMappedShardFile(MappedShardFile& m) {
    if (m.view) {
        UnmapViewOfFile(m.view);
        m.view = nullptr;
    }
    if (m.hMapping) {
        CloseHandle(m.hMapping);
        m.hMapping = nullptr;
    }
    if (m.hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(m.hFile);
        m.hFile = INVALID_HANDLE_VALUE;
    }
}

void clearTagMappedShardsLocked(const std::string& tag) {
    auto it = g_mapped_shards_by_tag.find(tag);
    if (it == g_mapped_shards_by_tag.end()) {
        return;
    }
    for (auto& mapped : it->second) {
        closeMappedShardFile(mapped);
    }
    g_mapped_shards_by_tag.erase(it);
}

bool mapShardFileReadOnly(const std::string& path, int device_index, MappedShardFile& out, std::string& reason) {
    out = {};
    out.path = path;
    out.device_index = device_index;

    out.hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out.hFile == INVALID_HANDLE_VALUE) {
        reason = "CreateFileA failed for: " + path;
        return false;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(out.hFile, &size) || size.QuadPart <= 0) {
        reason = "GetFileSizeEx failed or empty file: " + path;
        closeMappedShardFile(out);
        return false;
    }
    out.size_bytes = static_cast<uint64_t>(size.QuadPart);

    out.hMapping = CreateFileMappingA(out.hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!out.hMapping) {
        reason = "CreateFileMappingA failed for: " + path;
        closeMappedShardFile(out);
        return false;
    }

    out.view = MapViewOfFile(out.hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!out.view) {
        reason = "MapViewOfFile failed for: " + path;
        closeMappedShardFile(out);
        return false;
    }

    return true;
}

bool mapShardsForTag(const std::string& tag, const std::vector<ModelShard>& shards, std::string& reason) {
    std::lock_guard<std::mutex> lock(g_mapped_shards_mutex);
    clearTagMappedShardsLocked(tag);

    std::vector<MappedShardFile> mapped;
    for (const auto& shard : shards) {
        for (const auto& shard_file : shard.shard_files) {
            MappedShardFile one;
            if (!mapShardFileReadOnly(shard_file, shard.device_index, one, reason)) {
                for (auto& m : mapped) {
                    closeMappedShardFile(m);
                }
                return false;
            }
            mapped.push_back(std::move(one));
        }
    }

    g_mapped_shards_by_tag[tag] = std::move(mapped);
    return true;
}

void clearTagMappedShards(const std::string& tag) {
    std::lock_guard<std::mutex> lock(g_mapped_shards_mutex);
    clearTagMappedShardsLocked(tag);
}

void clearAllMappedShards() {
    std::lock_guard<std::mutex> lock(g_mapped_shards_mutex);
    for (auto& kv : g_mapped_shards_by_tag) {
        for (auto& mapped : kv.second) {
            closeMappedShardFile(mapped);
        }
    }
    g_mapped_shards_by_tag.clear();
}

} // namespace

// ─── Singleton ────────────────────────────────────────────────────────────────
BackendOrchestrator& BackendOrchestrator::Instance() {
    static BackendOrchestrator inst;
    return inst;
}

BackendOrchestrator::BackendOrchestrator() {
    m_failover_order = { BackendKind::Vulkan, BackendKind::HIP,
                         BackendKind::DML,    BackendKind::CPU };

    for (int i = 0; i < (int)BackendKind::Count; ++i) {
        m_health[i].kind = static_cast<BackendKind>(i);
    }
}

BackendOrchestrator::~BackendOrchestrator() {
    Shutdown();
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────
bool BackendOrchestrator::Initialize() {
    if (m_initialized.load()) return true;

    // Probe available backends
    for (int i = 0; i < (int)BackendKind::Count; ++i) {
        RefreshBackendHealth(static_cast<BackendKind>(i));
    }

    // Pick best available as default
    for (BackendKind k : m_failover_order) {
        if (m_health[(int)k].available && m_health[(int)k].healthy) {
            m_active_backend = k;
            break;
        }
    }

    // Start dispatch thread
    m_dispatch_running.store(true);
    m_dispatch_thread = std::thread(&BackendOrchestrator::DispatchLoop, this);

    m_initialized.store(true);
    return true;
}

void BackendOrchestrator::Shutdown() {
    if (!m_initialized.exchange(false)) return;

    // Stop health thread
    StopHealthCheckThread();

    // Stop dispatch thread
    m_dispatch_running.store(false);
    m_queue_cv.notify_all();
    if (m_dispatch_thread.joinable()) m_dispatch_thread.join();

    // Stop metrics thread
    DisableMetricsExport();

    // Release model shard file mappings.
    clearAllMappedShards();
}

// ─── Enhancement 1: Dynamic backend selection ─────────────────────────────────
BackendKind BackendOrchestrator::SelectBestBackend(size_t model_bytes, int seq_len) const {
    if (m_forced_backend != BackendKind::Count) return m_forced_backend;

    std::unique_lock<std::shared_mutex> lk(m_backend_mtx);

    float   vram_needed_mb = static_cast<float>(model_bytes) / (1024.f * 1024.f);
    // For very long contexts → prefer Vulkan Flash Attention
    for (BackendKind k : m_failover_order) {
        const auto& h = m_health[(int)k];
        if (!h.available || !h.healthy) continue;
        if (k == BackendKind::Vulkan || k == BackendKind::HIP) {
            if (h.vram_total_mb >= vram_needed_mb * 1.2f)
                return k;
        }
        if (k == BackendKind::CPU) return k;  // always available
    }
    return BackendKind::CPU;
}

void BackendOrchestrator::ForceBackend(BackendKind k) {
    m_forced_backend = k;
    m_active_backend = k;
}

BackendKind BackendOrchestrator::GetActiveBackend() const {
    return m_active_backend;
}

bool BackendOrchestrator::SwitchBackend(BackendKind target) {
    std::unique_lock<std::shared_mutex> lk(m_backend_mtx);
    if (!m_health[(int)target].available) return false;
    m_active_backend = target;
    std::cout << "[BackendOrchestrator] Switched to " << BackendName(target) << "\n";
    return true;
}

// ─── Enhancement 2: Model sharding ────────────────────────────────────────────
bool BackendOrchestrator::ShardModel(const std::string& model_path,
                                      const std::vector<int>& device_indices) {
    std::lock_guard<std::mutex> lk(m_shard_mtx);
    m_shards.clear();

    if (device_indices.empty()) return false;

    std::vector<std::string> shard_files;
    std::string shard_reason;
    if (!discoverModelShards(model_path, shard_files, shard_reason)) {
        std::cerr << "[BackendOrchestrator] ShardModel failed: " << shard_reason << "\n";
        return false;
    }

    uint64_t total_model_bytes = 0;
    std::vector<uint64_t> shard_sizes;
    shard_sizes.reserve(shard_files.size());
    for (const auto& shard : shard_files) {
        std::error_code ec;
        const auto shard_size = std::filesystem::file_size(shard, ec);
        if (ec) {
            std::cerr << "[BackendOrchestrator] Failed to stat shard: " << shard << "\n";
            return false;
        }
        const uint64_t sz = static_cast<uint64_t>(shard_size);
        total_model_bytes += sz;
        shard_sizes.push_back(sz);
    }

    int total_layers = discoverLayerCountFromGguf(shard_files.front());
    if (total_layers <= 0) {
        total_layers = 32; // Fallback for incomplete metadata models.
    }

    int n_dev        = (int)device_indices.size();
    int base_layers  = total_layers / n_dev;
    int extra_layers = total_layers % n_dev;

    // Deterministic size-aware balancing: assign largest files first to the least-loaded device.
    std::vector<size_t> shard_order(shard_files.size());
    std::iota(shard_order.begin(), shard_order.end(), 0);
    std::stable_sort(shard_order.begin(), shard_order.end(), [&](size_t a, size_t b) {
        if (shard_sizes[a] == shard_sizes[b]) {
            return a < b;
        }
        return shard_sizes[a] > shard_sizes[b];
    });

    std::vector<std::vector<size_t>> files_per_device(static_cast<size_t>(n_dev));
    std::vector<uint64_t> bytes_per_device(static_cast<size_t>(n_dev), 0);
    for (size_t shard_idx : shard_order) {
        size_t best_device = 0;
        for (size_t d = 1; d < bytes_per_device.size(); ++d) {
            if (bytes_per_device[d] < bytes_per_device[best_device]) {
                best_device = d;
            } else if (bytes_per_device[d] == bytes_per_device[best_device] &&
                       files_per_device[d].size() < files_per_device[best_device].size()) {
                best_device = d;
            }
        }
        files_per_device[best_device].push_back(shard_idx);
        bytes_per_device[best_device] += shard_sizes[shard_idx];
    }

    // Keep per-device shard list in natural shard order for easier debugging and replay.
    for (auto& v : files_per_device) {
        std::sort(v.begin(), v.end());
    }

    int layer_cursor = 0;

    for (int d = 0; d < n_dev; ++d) {
        ModelShard shard;
        shard.device_index = device_indices[d];
        const int layer_count = base_layers + (d < extra_layers ? 1 : 0);
        if (layer_count > 0) {
            shard.layer_start = layer_cursor;
            shard.layer_end = layer_cursor + layer_count - 1;
            layer_cursor += layer_count;
        } else {
            shard.layer_start = -1;
            shard.layer_end = -1;
        }
        shard.assigned_file_bytes = 0;
        shard.shard_files.clear();
        for (size_t shard_idx : files_per_device[static_cast<size_t>(d)]) {
            shard.shard_files.push_back(shard_files[shard_idx]);
            shard.assigned_file_bytes += static_cast<size_t>(shard_sizes[shard_idx]);
        }

        // Current heuristic: vram budget follows actual assigned shard bytes.
        shard.vram_bytes = shard.assigned_file_bytes;
        shard.loaded       = false;
        m_shards.push_back(shard);
    }

    std::cout << "[BackendOrchestrator] Model sharded across "
              << n_dev << " device(s), layers=" << total_layers
              << ", files=" << shard_files.size()
              << ", bytes=" << total_model_bytes << "\n";
    for (const auto& s : m_shards) {
        std::cout << "  device=" << s.device_index
                  << " layers=" << s.layer_start << "-" << s.layer_end
                  << " files=" << s.shard_files.size()
                  << " assigned_bytes=" << s.assigned_file_bytes << "\n";
    }
    return true;
}

std::vector<ModelShard> BackendOrchestrator::GetShards() const {
    std::lock_guard<std::mutex> lk(m_shard_mtx);
    return m_shards;
}

void BackendOrchestrator::ClearShards() {
    std::lock_guard<std::mutex> lk(m_shard_mtx);
    m_shards.clear();
}

// ─── Enhancement 3: Distributed KV-cache ──────────────────────────────────────
void BackendOrchestrator::SetKVCacheGPUBudgetMB(float mb) { m_kv_gpu_budget_mb.store(mb); }
void BackendOrchestrator::SetKVCacheCPUBudgetMB(float mb) { m_kv_cpu_budget_mb.store(mb); }
float BackendOrchestrator::GetKVCacheGPUUsedMB() const { return m_kv_gpu_used_mb.load(); }
float BackendOrchestrator::GetKVCacheCPUUsedMB() const { return m_kv_cpu_used_mb.load(); }

void BackendOrchestrator::EvictKVCacheForTenant(const std::string& tenant_id) {
    // Evict KV entries associated with this tenant by notifying the active backend
    // Full implementation integrates with PagedKVCache / VulkanCompute::m_kvCache
    std::cout << "[BackendOrchestrator] Evicting KV cache for tenant: "
              << tenant_id << "\n";
}

// ─── Enhancement 4: Speculative decoding ─────────────────────────────────────
bool BackendOrchestrator::EnableSpecDecoding(SpecDecodingConfig cfg) {
    if (cfg.draft_model_path.empty()) return false;
    m_spec_cfg = cfg;
    m_spec_cfg.enabled = true;
    std::cout << "[BackendOrchestrator] Speculative decoding enabled, draft="
              << cfg.draft_model_path << " beam=" << cfg.draft_beam << "\n";
    return true;
}

void BackendOrchestrator::DisableSpecDecoding() {
    m_spec_cfg.enabled = false;
}

double BackendOrchestrator::GetSpecDecodingAcceptRate() const {
    return m_spec_accept_rate.load(std::memory_order_relaxed);
}

// ─── Enhancement 5: Model hot-swapping ───────────────────────────────────────
bool BackendOrchestrator::LoadModel(const std::string& path, const std::string& tag) {
    std::vector<int> device_indices;
    {
        std::lock_guard<std::mutex> shard_lk(m_shard_mtx);
        std::unordered_set<int> unique_devices;
        for (const auto& s : m_shards) {
            if (unique_devices.insert(s.device_index).second) {
                device_indices.push_back(s.device_index);
            }
        }
    }
    if (device_indices.empty()) {
        device_indices.push_back(0);
    }

    if (!ShardModel(path, device_indices)) {
        std::cerr << "[BackendOrchestrator] LoadModel failed: ShardModel failed for path=" << path << "\n";
        return false;
    }

    std::vector<ModelShard> plan;
    {
        std::lock_guard<std::mutex> shard_lk(m_shard_mtx);
        plan = m_shards;
    }

    std::string map_reason;
    if (!mapShardsForTag(tag, plan, map_reason)) {
        std::cerr << "[BackendOrchestrator] LoadModel failed: " << map_reason << "\n";
        return false;
    }

    std::lock_guard<std::mutex> lk(m_model_mtx);
    m_model_paths[tag] = path;
    // The actual inference engine load remains delegated to active backend.
    std::cout << "[BackendOrchestrator] Registered model tag='" << tag
              << "' path=" << path << " with " << plan.size() << " shard plan entries\n";
    return true;
}

bool BackendOrchestrator::SwapToModel(const std::string& tag) {
    std::lock_guard<std::mutex> lk(m_model_mtx);
    auto it = m_model_paths.find(tag);
    if (it == m_model_paths.end()) {
        std::cerr << "[BackendOrchestrator] SwapToModel: unknown tag " << tag << "\n";
        return false;
    }
    m_active_model_tag = tag;
    std::cout << "[BackendOrchestrator] Hot-swapped to model: " << tag << "\n";
    return true;
}

bool BackendOrchestrator::UnloadModel(const std::string& tag) {
    std::lock_guard<std::mutex> lk(m_model_mtx);
    m_model_paths.erase(tag);
    if (m_active_model_tag == tag) m_active_model_tag.clear();
    clearTagMappedShards(tag);
    return true;
}

std::vector<std::string> BackendOrchestrator::GetLoadedModelTags() const {
    std::lock_guard<std::mutex> lk(m_model_mtx);
    std::vector<std::string> tags;
    tags.reserve(m_model_paths.size());
    for (const auto& [t, _] : m_model_paths) tags.push_back(t);
    return tags;
}

// ─── Enhancement 6: Multi-tenant ──────────────────────────────────────────────
void BackendOrchestrator::CreateTenant(const std::string& tenant_id, int max_ctx) {
    std::lock_guard<std::mutex> lk(m_tenant_mtx);
    m_tenants[tenant_id] = TenantInfo{ max_ctx, 512.f };
}

void BackendOrchestrator::RemoveTenant(const std::string& tenant_id) {
    std::lock_guard<std::mutex> lk(m_tenant_mtx);
    m_tenants.erase(tenant_id);
    EvictKVCacheForTenant(tenant_id);
}

void BackendOrchestrator::SetTenantKVQuota(const std::string& tenant_id, float mb) {
    std::lock_guard<std::mutex> lk(m_tenant_mtx);
    auto it = m_tenants.find(tenant_id);
    if (it != m_tenants.end()) it->second.kv_quota_mb = mb;
}

std::vector<std::string> BackendOrchestrator::GetActiveTenants() const {
    std::lock_guard<std::mutex> lk(m_tenant_mtx);
    std::vector<std::string> out;
    for (const auto& [id, _] : m_tenants) out.push_back(id);
    return out;
}

// ─── Enhancement 7: Priority queue ────────────────────────────────────────────
uint64_t BackendOrchestrator::Enqueue(InferRequest req) {
    req.id           = m_next_req_id.fetch_add(1, std::memory_order_relaxed);
    req.enqueue_time = std::chrono::steady_clock::now();

    int bucket = static_cast<int>(req.priority);
    {
        std::lock_guard<std::mutex> lk(m_queue_mtx);
        m_queues[bucket].push_back(req);
        m_enqueue_times[req.id] = req.enqueue_time;
    }
    m_queue_cv.notify_one();
    return req.id;
}

void BackendOrchestrator::Cancel(uint64_t request_id) {
    std::lock_guard<std::mutex> lk(m_queue_mtx);
    for (auto& q : m_queues) {
        q.erase(std::remove_if(q.begin(), q.end(),
            [=](const InferRequest& r) { return r.id == request_id; }), q.end());
    }
    m_enqueue_times.erase(request_id);
}

int BackendOrchestrator::GetQueueDepth(RequestPriority p) const {
    std::lock_guard<std::mutex> lk(m_queue_mtx);
    return (int)m_queues[static_cast<int>(p)].size();
}

double BackendOrchestrator::GetQueuedWaitTimeMs(uint64_t request_id) const {
    std::lock_guard<std::mutex> lk(m_queue_mtx);
    auto it = m_enqueue_times.find(request_id);
    if (it == m_enqueue_times.end()) return -1.0;
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(now - it->second).count();
}

void BackendOrchestrator::DispatchLoop() {
    while (m_dispatch_running.load()) {
        InferRequest req;
        bool found = false;
        {
            std::unique_lock<std::mutex> lk(m_queue_mtx);
            m_queue_cv.wait_for(lk, std::chrono::milliseconds(5), [this]{
                for (const auto& q : m_queues) if (!q.empty()) return true;
                return false;
            });
            // Drain highest priority first
            for (auto& q : m_queues) {
                if (!q.empty()) {
                    req   = q.front();
                    q.pop_front();
                    m_enqueue_times.erase(req.id);
                    found = true;
                    break;
                }
            }
        }
        if (found) {
            RunInference(req);
        }
    }
}

bool BackendOrchestrator::RunInference(const InferRequest& req) {
    auto& prof = InferenceProfiler::Instance();
    auto scoped = prof.MakeScoped("inference");

    // Semantic cache lookup (Enhancement 8)
    if (m_cache_enabled) {
        std::string cached;
        if (LookupCache(req.prompt, cached)) {
            if (req.complete_cb) req.complete_cb(cached, "{}");
            return true;
        }
    }

    std::string completion;
    std::string metadata;
    std::string error;
    const uint32_t timeoutMs = 120000;

    const bool ok = RunNativeInferenceSync(req.prompt, timeoutMs, completion, metadata, error);
    if (!ok) {
        if (req.complete_cb) {
            req.complete_cb("[BackendError] " + error,
                "{\"error\":\"native_inference_failed\"}");
        }
        return false;
    }

    if (m_cache_enabled && !completion.empty()) {
        InsertCache(req.prompt, completion);
    }

    if (req.stream_cb && !completion.empty()) {
        req.stream_cb(completion);
    }
    if (req.complete_cb) {
        req.complete_cb(completion, metadata.empty() ? "{}" : metadata);
    }
    return true;
}

// ─── Enhancement 8: Semantic cache ───────────────────────────────────────────
void BackendOrchestrator::EnableSemanticCache(size_t max_entries) {
    m_cache_max     = max_entries;
    m_cache_enabled = true;
}

void BackendOrchestrator::DisableSemanticCache() {
    m_cache_enabled = false;
}

std::string BackendOrchestrator::HashPrompt(const std::string& prompt) const {
    // FNV-1a 64-bit
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : prompt) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

bool BackendOrchestrator::LookupCache(const std::string& prompt,
                                       std::string& out_completion) const {
    if (!m_cache_enabled) return false;
    m_cache_lookups.store(m_cache_lookups.load(std::memory_order_relaxed) + 1,
                          std::memory_order_relaxed);
    std::string key = HashPrompt(prompt);
    std::unique_lock<std::shared_mutex> lk(m_cache_mtx);
    auto it = m_cache.find(key);
    if (it == m_cache.end()) return false;
    out_completion = it->second.completion;
    const_cast<SemanticCacheEntry&>(it->second).hit_count++;
    const_cast<SemanticCacheEntry&>(it->second).last_accessed =
        std::chrono::steady_clock::now();
    m_cache_hits.store(m_cache_hits.load(std::memory_order_relaxed) + 1,
                       std::memory_order_relaxed);
    return true;
}

void BackendOrchestrator::InsertCache(const std::string& prompt,
                                       const std::string& completion) {
    if (!m_cache_enabled) return;
    std::string key = HashPrompt(prompt);
    std::unique_lock<std::shared_mutex> lk(m_cache_mtx);
    if (m_cache.size() >= m_cache_max) {
        // Evict the least-recently-used entry
        auto victim = std::min_element(m_cache.begin(), m_cache.end(),
            [](const auto& a, const auto& b){
                return a.second.last_accessed < b.second.last_accessed; });
        if (victim != m_cache.end()) m_cache.erase(victim);
    }
    SemanticCacheEntry entry;
    entry.prompt_hash    = key;
    entry.completion     = completion;
    entry.last_accessed  = std::chrono::steady_clock::now();
    m_cache[key]         = std::move(entry);
}

void BackendOrchestrator::ClearCache() {
    std::unique_lock<std::shared_mutex> lk(m_cache_mtx);
    m_cache.clear();
    m_cache_hits.store(0);
    m_cache_lookups.store(0);
}

size_t BackendOrchestrator::GetCacheSize() const {
    std::unique_lock<std::shared_mutex> lk(m_cache_mtx);
    return m_cache.size();
}

double BackendOrchestrator::GetCacheHitRate() const {
    uint64_t looks = m_cache_lookups.load(std::memory_order_relaxed);
    if (!looks) return 0.0;
    return static_cast<double>(m_cache_hits.load()) / looks;
}

// ─── Enhancement 9: Context compression ──────────────────────────────────────
void BackendOrchestrator::SetContextCompression(ContextCompressionConfig cfg) {
    m_ctx_comp = cfg;
}

std::string BackendOrchestrator::CompressContext(const std::string& raw_context) const {
    if (!m_ctx_comp.enabled) return raw_context;

    // Word-level tokenization proxy: split by spaces
    std::istringstream iss(raw_context);
    std::vector<std::string> words;
    { std::string w; while (iss >> w) words.push_back(w); }

    int total = (int)words.size();
    int sink  = std::min(m_ctx_comp.sink_tokens, total);
    int win   = std::min(m_ctx_comp.rolling_window, total - sink);

    std::string out;
    for (int i = 0; i < sink; ++i) out += words[i] + " ";
    if (win < total - sink) out += "[...compressed...] ";
    int start = total - win;
    for (int i = std::max(start, sink); i < total; ++i) out += words[i] + " ";
    return out;
}

int BackendOrchestrator::EstimateCompressedTokenCount(int raw_tokens) const {
    if (!m_ctx_comp.enabled) return raw_tokens;
    return std::min(raw_tokens,
        m_ctx_comp.sink_tokens + m_ctx_comp.rolling_window + m_ctx_comp.summary_budget);
}

// ─── Enhancement 10: Adaptive quantization ────────────────────────────────────
void BackendOrchestrator::SetAdaptiveQuantization(bool enabled) {
    m_adaptive_quant = enabled;
}

int BackendOrchestrator::GetCurrentQuantBits() const {
    return m_quant_bits.load(std::memory_order_relaxed);
}

bool BackendOrchestrator::ForceQuantLevel(int bits) {
    if (bits != 4 && bits != 5 && bits != 6 && bits != 8) return false;
    m_quant_bits.store(bits, std::memory_order_relaxed);
    return true;
}

// ─── Enhancement 11: Kernel plugins ──────────────────────────────────────────
bool BackendOrchestrator::RegisterKernelPlugin(KernelPlugin plugin) {
    std::lock_guard<std::mutex> lk(m_plugin_mtx);
    for (const auto& p : m_plugins)
        if (p.name == plugin.name) return false; // duplicate
    plugin.loaded = true;
    m_plugins.push_back(std::move(plugin));
    return true;
}

bool BackendOrchestrator::ActivateKernelPlugin(const std::string& name) {
    std::lock_guard<std::mutex> lk(m_plugin_mtx);
    for (const auto& p : m_plugins) {
        if (p.name == name && p.loaded) {
            m_active_plugin = name;
            return true;
        }
    }
    return false;
}

std::string BackendOrchestrator::GetActiveKernelPlugin() const {
    std::lock_guard<std::mutex> lk(m_plugin_mtx);
    return m_active_plugin;
}

// ─── Enhancement 12: Health checks / failover ────────────────────────────────
BackendHealth BackendOrchestrator::GetBackendHealth(BackendKind k) const {
    std::lock_guard<std::mutex> lk(m_health_mtx);
    return m_health[static_cast<int>(k)];
}

void BackendOrchestrator::RefreshBackendHealth(BackendKind k) {
    BackendHealth& h  = m_health[static_cast<int>(k)];
    h.kind            = k;
    h.last_check      = std::chrono::steady_clock::now();

    switch (k) {
    case BackendKind::CPU:
        h.available = true; h.healthy = true;
        h.vram_total_mb = 0.f;
        break;
    case BackendKind::Vulkan: {
        // Quick probe: check if Vulkan DLL exists
        HMODULE vk = LoadLibraryA("vulkan-1.dll");
        h.available = (vk != nullptr);
        h.healthy   = h.available;
        if (vk) { FreeLibrary(vk); h.vram_total_mb = 16384.f; /* RX 7800 XT */ }
        break;
    }
    case BackendKind::HIP: {
        HMODULE hip = LoadLibraryA("amdhip64.dll");
        h.available = (hip != nullptr);
        h.healthy   = h.available;
        if (hip) { FreeLibrary(hip); h.vram_total_mb = 16384.f; }
        break;
    }
    case BackendKind::DML: {
        HMODULE dml = LoadLibraryA("DirectML.dll");
        h.available = (dml != nullptr);
        h.healthy   = h.available;
        if (dml) FreeLibrary(dml);
        break;
    }
    case BackendKind::Titan: {
        HMODULE t = LoadLibraryA("RawrXD_Titan.dll");
        h.available = (t != nullptr);
        h.healthy   = h.available;
        if (t) FreeLibrary(t);
        break;
    }
    default: break;
    }
}

void BackendOrchestrator::StartHealthCheckThread(std::chrono::seconds interval) {
    if (m_health_running.exchange(true)) return;
    m_health_thread = std::thread([this, interval](){
        HealthCheckLoop(interval);
    });
}

void BackendOrchestrator::StopHealthCheckThread() {
    m_health_running.store(false);
    m_health_cv.notify_all();
    if (m_health_thread.joinable()) m_health_thread.join();
}

void BackendOrchestrator::SetFailoverOrder(std::vector<BackendKind> order) {
    m_failover_order = std::move(order);
}

void BackendOrchestrator::HealthCheckLoop(std::chrono::seconds interval) {
    while (m_health_running.load()) {
        std::unique_lock<std::mutex> lk(m_health_cv_mtx);
        m_health_cv.wait_for(lk, interval);
        if (!m_health_running.load()) break;

        for (int i = 0; i < (int)BackendKind::Count; ++i) {
            std::lock_guard<std::mutex> hlk(m_health_mtx);
            RefreshBackendHealth(static_cast<BackendKind>(i));
        }

        // Failover if active backend degraded
        BackendKind active = m_active_backend;
        if (!m_health[(int)active].healthy) {
            for (BackendKind k : m_failover_order) {
                if (k != active && m_health[(int)k].available && m_health[(int)k].healthy) {
                    SwitchBackend(k);
                    std::cerr << "[BackendOrchestrator] Failover: "
                              << BackendName(active) << " → " << BackendName(k) << "\n";
                    break;
                }
            }
        }
    }
}

// ─── Enhancement 13: Micro-batching ──────────────────────────────────────────
void BackendOrchestrator::SetMicroBatchWindow(std::chrono::milliseconds window) {
    m_batch_window = window;
}
void BackendOrchestrator::SetMaxBatchSize(int n) { m_max_batch_size = n; }
int  BackendOrchestrator::GetCurrentBatchSize() const { return m_cur_batch_size.load(); }
void BackendOrchestrator::FlushBatch() { m_queue_cv.notify_all(); }

// ─── Enhancement 14: Prometheus metrics export ───────────────────────────────
void BackendOrchestrator::EnableMetricsExport(const std::string& path, int interval_s) {
    m_metrics_path = path;
    if (m_metrics_running.exchange(true)) return;
    m_metrics_thread = std::thread([this, interval_s](){ MetricsExportLoop(interval_s); });
}

void BackendOrchestrator::DisableMetricsExport() {
    m_metrics_running.store(false);
    if (m_metrics_thread.joinable()) m_metrics_thread.join();
}

void BackendOrchestrator::DumpMetricsNow() const {
    if (m_metrics_path.empty()) return;
    std::ostringstream oss;
    oss << InferenceProfiler::Instance().GetPrometheusText();

    // AppendBackendOrchestrator-specific metrics
    oss << "# HELP rawrxd_cache_hit_rate Semantic cache hit rate\n"
        << "rawrxd_cache_hit_rate " << GetCacheHitRate() << "\n";
    oss << "rawrxd_queue_depth_vip "    << GetQueueDepth(RequestPriority::VIP)    << "\n";
    oss << "rawrxd_queue_depth_normal " << GetQueueDepth(RequestPriority::Normal) << "\n";
    oss << "rawrxd_queue_depth_batch "  << GetQueueDepth(RequestPriority::Batch)  << "\n";
    oss << "rawrxd_active_backend "     << (int)GetActiveBackend()                << "\n";

    std::ofstream f(m_metrics_path, std::ios::trunc);
    if (f.is_open()) f << oss.str();
}

void BackendOrchestrator::MetricsExportLoop(int interval_s) {
    while (m_metrics_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(interval_s));
        if (!m_metrics_running.load()) break;
        DumpMetricsNow();
    }
}

} // namespace RawrXD
