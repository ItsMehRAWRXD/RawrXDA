#define TITAN_ABI_EXPORTS

#include "titan/titan_abi.h"
#include "titan/titan_engine.h"
#include "titan/rx_kernel.hpp"

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace {

using EnginePtr = std::unique_ptr<titan::TitanEngine>;
using RxKernelPtr = std::unique_ptr<titan::ExtensionKernel>;

std::mutex g_titan_mutex;
std::unordered_map<uint64_t, EnginePtr> g_engines;
std::unordered_map<uint64_t, RxKernelPtr> g_rx_kernels;
std::atomic<uint64_t> g_next_handle{1};

static titan::TitanEngine* get_engine_unlocked(uint64_t handle) {
    auto it = g_engines.find(handle);
    if (it == g_engines.end()) {
        return nullptr;
    }
    return it->second.get();
}

static titan::ExtensionKernel* get_rx_kernel_unlocked(uint64_t handle) {
    auto it = g_rx_kernels.find(handle);
    if (it == g_rx_kernels.end()) {
        return nullptr;
    }
    return it->second.get();
}

} // namespace

extern "C" uint64_t Titan_CreateEngine(const char* model_path, const char* shm_name) {
    if (!model_path || !shm_name) {
        return 0;
    }

    EnginePtr engine = std::make_unique<titan::TitanEngine>(model_path, shm_name);
    if (!engine->is_ready()) {
        return 0;
    }

    const uint64_t handle = g_next_handle.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    g_engines.emplace(handle, std::move(engine));
    return handle;
}

extern "C" uint32_t Titan_DestroyEngine(uint64_t handle) {
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    auto it = g_engines.find(handle);
    if (it == g_engines.end()) {
        return TITAN_STATUS_NOT_FOUND;
    }
    g_engines.erase(it);
    return TITAN_STATUS_OK;
}

extern "C" uint32_t Titan_SubmitContext(uint64_t handle, const uint32_t* tokens, uint32_t token_count) {
    if (!tokens || token_count == 0u) {
        return TITAN_STATUS_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    titan::TitanEngine* engine = get_engine_unlocked(handle);
    if (!engine) {
        return TITAN_STATUS_NOT_FOUND;
    }
    return engine->submit_context(tokens, token_count) ? TITAN_STATUS_OK : TITAN_STATUS_ENGINE_NOT_READY;
}

extern "C" uint32_t Titan_RunOnce(uint64_t handle) {
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    titan::TitanEngine* engine = get_engine_unlocked(handle);
    if (!engine) {
        return 0;
    }
    return engine->run_once(nullptr);
}

extern "C" uint32_t Titan_ReadResults(uint64_t handle, uint32_t* out_tokens, uint32_t max_tokens) {
    if (!out_tokens || max_tokens == 0u) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    titan::TitanEngine* engine = get_engine_unlocked(handle);
    if (!engine) {
        return 0;
    }
    return engine->read_results(out_tokens, max_tokens);
}

extern "C" uint32_t Titan_GetSharedView(uint64_t handle, void** out_shm_ptr) {
    if (!out_shm_ptr) {
        return TITAN_STATUS_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    titan::TitanEngine* engine = get_engine_unlocked(handle);
    if (!engine) {
        return TITAN_STATUS_NOT_FOUND;
    }
    *out_shm_ptr = static_cast<void*>(engine->shared_header());
    return (*out_shm_ptr != nullptr) ? TITAN_STATUS_OK : TITAN_STATUS_ENGINE_NOT_READY;
}

extern "C" uint32_t Titan_StopEngine(uint64_t handle) {
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    titan::TitanEngine* engine = get_engine_unlocked(handle);
    if (!engine) {
        return TITAN_STATUS_NOT_FOUND;
    }
    engine->stop();
    return TITAN_STATUS_OK;
}

extern "C" uint64_t Titan_CreateRxKernel(const char* map_name) {
    if (!map_name) {
        return 0;
    }

    RxKernelPtr kernel = std::make_unique<titan::ExtensionKernel>(map_name);
    if (!kernel->is_ready()) {
        return 0;
    }

    const uint64_t handle = g_next_handle.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    g_rx_kernels.emplace(handle, std::move(kernel));
    return handle;
}

extern "C" uint32_t Titan_DestroyRxKernel(uint64_t handle) {
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    auto it = g_rx_kernels.find(handle);
    if (it == g_rx_kernels.end()) {
        return TITAN_STATUS_NOT_FOUND;
    }
    g_rx_kernels.erase(it);
    return TITAN_STATUS_OK;
}

extern "C" uint32_t Titan_RxSubmitContext(uint64_t handle, const char* context_buf) {
    if (!context_buf) {
        return TITAN_STATUS_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    titan::ExtensionKernel* kernel = get_rx_kernel_unlocked(handle);
    if (!kernel) {
        return TITAN_STATUS_NOT_FOUND;
    }
    return kernel->submit_context(context_buf) ? TITAN_STATUS_OK : TITAN_STATUS_ENGINE_NOT_READY;
}

extern "C" uint32_t Titan_RxStepOnce(uint64_t handle) {
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    titan::ExtensionKernel* kernel = get_rx_kernel_unlocked(handle);
    if (!kernel) {
        return 0;
    }
    return kernel->step_once();
}

extern "C" uint32_t Titan_RxGetChannel(uint64_t handle, void** out_channel_ptr) {
    if (!out_channel_ptr) {
        return TITAN_STATUS_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    titan::ExtensionKernel* kernel = get_rx_kernel_unlocked(handle);
    if (!kernel) {
        return TITAN_STATUS_NOT_FOUND;
    }
    *out_channel_ptr = static_cast<void*>(kernel->channel());
    return (*out_channel_ptr != nullptr) ? TITAN_STATUS_OK : TITAN_STATUS_ENGINE_NOT_READY;
}

extern "C" uint32_t Titan_RxStopKernel(uint64_t handle) {
    std::lock_guard<std::mutex> lock(g_titan_mutex);
    titan::ExtensionKernel* kernel = get_rx_kernel_unlocked(handle);
    if (!kernel) {
        return TITAN_STATUS_NOT_FOUND;
    }
    kernel->stop();
    return TITAN_STATUS_OK;
}

extern "C" uint32_t Titan_RxReadDraftBlock(uint64_t handle, uint32_t* out_tokens8, float* out_conf8) {
    if (!out_tokens8 || !out_conf8) {
        return TITAN_STATUS_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_titan_mutex);
    titan::ExtensionKernel* kernel = get_rx_kernel_unlocked(handle);
    if (!kernel) {
        return TITAN_STATUS_NOT_FOUND;
    }

    const titan::RxChannel* channel = kernel->channel();
    if (!channel) {
        return TITAN_STATUS_ENGINE_NOT_READY;
    }

    std::memcpy(out_tokens8, channel->draft_tokens, sizeof(uint32_t) * 8);
    std::memcpy(out_conf8, channel->confidence, sizeof(float) * 8);
    return TITAN_STATUS_OK;
}

extern "C" uint32_t Titan_RxSubmit(uint64_t handle, const char* context_buf) {
    return Titan_RxSubmitContext(handle, context_buf);
}

extern "C" uint32_t Titan_RxStep(uint64_t handle) {
    return Titan_RxStepOnce(handle);
}

extern "C" uint32_t Titan_GetRxSharedChannel(uint64_t handle, void** out_channel_ptr) {
    return Titan_RxGetChannel(handle, out_channel_ptr);
}

extern "C" uint32_t Titan_ExecuteCommand(const TitanAbiCommand* cmd, TitanAbiResponse* out_resp) {
    if (!cmd || !out_resp) {
        return TITAN_STATUS_INVALID_ARGUMENT;
    }

    out_resp->status = TITAN_STATUS_OK;
    out_resp->value = 0;
    out_resp->handle = 0;
    out_resp->ptr = 0;

    switch (cmd->opcode) {
        case TITAN_OP_CREATE_ENGINE: {
            const char* model_path = reinterpret_cast<const char*>(cmd->ptr0);
            const char* shm_name = reinterpret_cast<const char*>(cmd->ptr1);
            const uint64_t handle = Titan_CreateEngine(model_path, shm_name);
            if (handle == 0) {
                out_resp->status = TITAN_STATUS_ENGINE_NOT_READY;
                return out_resp->status;
            }
            out_resp->handle = handle;
            return TITAN_STATUS_OK;
        }
        case TITAN_OP_DESTROY_ENGINE: {
            const uint32_t status = Titan_DestroyEngine(cmd->handle);
            out_resp->status = status;
            return status;
        }
        case TITAN_OP_SUBMIT_CONTEXT: {
            const uint32_t* tokens = reinterpret_cast<const uint32_t*>(cmd->ptr0);
            const uint32_t status = Titan_SubmitContext(cmd->handle, tokens, cmd->count);
            out_resp->status = status;
            return status;
        }
        case TITAN_OP_RUN_ONCE: {
            out_resp->value = Titan_RunOnce(cmd->handle);
            return TITAN_STATUS_OK;
        }
        case TITAN_OP_READ_RESULTS: {
            uint32_t* out_tokens = reinterpret_cast<uint32_t*>(cmd->ptr0);
            out_resp->value = Titan_ReadResults(cmd->handle, out_tokens, cmd->count);
            return TITAN_STATUS_OK;
        }
        case TITAN_OP_GET_SHM_VIEW: {
            void* shm_ptr = nullptr;
            const uint32_t status = Titan_GetSharedView(cmd->handle, &shm_ptr);
            out_resp->status = status;
            out_resp->ptr = reinterpret_cast<uint64_t>(shm_ptr);
            return status;
        }
        case TITAN_OP_STOP_ENGINE: {
            const uint32_t status = Titan_StopEngine(cmd->handle);
            out_resp->status = status;
            return status;
        }
        case TITAN_OP_CREATE_RX_KERNEL: {
            const char* map_name = reinterpret_cast<const char*>(cmd->ptr0);
            const uint64_t handle = Titan_CreateRxKernel(map_name);
            if (handle == 0) {
                out_resp->status = TITAN_STATUS_ENGINE_NOT_READY;
                return out_resp->status;
            }
            out_resp->handle = handle;
            return TITAN_STATUS_OK;
        }
        case TITAN_OP_DESTROY_RX_KERNEL: {
            const uint32_t status = Titan_DestroyRxKernel(cmd->handle);
            out_resp->status = status;
            return status;
        }
        case TITAN_OP_RX_SUBMIT_CONTEXT: {
            const char* context = reinterpret_cast<const char*>(cmd->ptr0);
            const uint32_t status = Titan_RxSubmitContext(cmd->handle, context);
            out_resp->status = status;
            return status;
        }
        case TITAN_OP_RX_STEP_ONCE: {
            out_resp->value = Titan_RxStepOnce(cmd->handle);
            return TITAN_STATUS_OK;
        }
        case TITAN_OP_RX_GET_CHANNEL: {
            void* channel_ptr = nullptr;
            const uint32_t status = Titan_RxGetChannel(cmd->handle, &channel_ptr);
            out_resp->status = status;
            out_resp->ptr = reinterpret_cast<uint64_t>(channel_ptr);
            return status;
        }
        case TITAN_OP_RX_STOP_KERNEL: {
            const uint32_t status = Titan_RxStopKernel(cmd->handle);
            out_resp->status = status;
            return status;
        }
        case TITAN_OP_RX_READ_DRAFT_BLOCK: {
            uint32_t* out_tokens = reinterpret_cast<uint32_t*>(cmd->ptr0);
            float* out_conf = reinterpret_cast<float*>(cmd->ptr1);
            const uint32_t status = Titan_RxReadDraftBlock(cmd->handle, out_tokens, out_conf);
            out_resp->status = status;
            return status;
        }
        default:
            out_resp->status = TITAN_STATUS_INVALID_ARGUMENT;
            return TITAN_STATUS_INVALID_ARGUMENT;
    }
}
