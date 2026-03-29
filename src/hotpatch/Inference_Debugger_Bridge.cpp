// Inference_Debugger_Bridge.cpp

#include "Inference_Debugger_Bridge.hpp"
#include "../RawrXD_Inference_Wrapper.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace rawrxd::inference_debug
{
namespace
{

std::mutex g_ringMutex;
DebugFrame g_ring[kRingCap]{};
std::atomic<std::uint32_t> g_writeSeq{0};
DebugFrame g_latest{};
std::atomic<bool> g_hasLatest{false};

void pickTopK(const llama_token_data_array* arr, std::int32_t* outId, float* outLogit)
{
    for (int i = 0; i < kTopK; ++i)
    {
        outId[i] = -1;
        outLogit[i] = -1.0e30f;
    }
    if (!arr || !arr->data || arr->size == 0)
        return;

    const std::size_t n = std::min(arr->size, std::size_t(16384));
    for (std::size_t i = 0; i < n; ++i)
    {
        const float L = arr->data[i].logit;
        const std::int32_t id = arr->data[i].id;
        for (int k = 0; k < kTopK; ++k)
        {
            if (L > outLogit[k])
            {
                for (int j = kTopK - 1; j > k; --j)
                {
                    outLogit[j] = outLogit[j - 1];
                    outId[j] = outId[j - 1];
                }
                outLogit[k] = L;
                outId[k] = id;
                break;
            }
        }
    }
}

}  // namespace

void pushCandidateSnapshot(std::int32_t chosenId, const llama_token_data_array* candidates, bool chosenUnauthorized)
{
    DebugFrame f{};
    LARGE_INTEGER li{};
    QueryPerformanceCounter(&li);
    f.qpcTick = static_cast<std::uint64_t>(li.QuadPart);
    f.chosenId = chosenId;
    if (chosenUnauthorized)
        f.flags |= 1u;
    pickTopK(candidates, f.topId, f.topLogit);

    {
        std::lock_guard<std::mutex> lock(g_ringMutex);
        const std::uint32_t idx = g_writeSeq.fetch_add(1, std::memory_order_relaxed) % kRingCap;
        g_ring[idx] = f;
        g_latest = f;
    }
    g_hasLatest.store(true, std::memory_order_release);
}

bool copyLatestFrame(DebugFrame& out)
{
    if (!g_hasLatest.load(std::memory_order_acquire))
        return false;
    std::lock_guard<std::mutex> lock(g_ringMutex);
    out = g_latest;
    return true;
}

}  // namespace rawrxd::inference_debug
