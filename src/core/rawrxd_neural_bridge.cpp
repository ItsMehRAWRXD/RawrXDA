// rawrxd_neural_bridge.cpp
// BCI / neural interface stub bridge — 13 extern "C" symbols for RawrXD-Win32IDE
// No hardware dependency; provides well-defined no-op implementations.

#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace
{
    std::mutex              g_neuralMtx;
    uint32_t                g_channelCount  = 0;
    uint32_t                g_sampleRate    = 0;
    std::atomic<uint64_t>   g_acquireCount  { 0 };
    std::array<float, 64>   g_baseline      {};
    bool                    g_calibrated    = false;
    bool                    g_initialized   = false;
} // namespace

extern "C"
{

int asm_neural_init(uint32_t channelCount, uint32_t sampleRate)
{
    std::lock_guard<std::mutex> lock(g_neuralMtx);
    g_channelCount  = channelCount;
    g_sampleRate    = sampleRate;
    g_initialized   = true;
    return 1;
}

int asm_neural_acquire_eeg(void* bufOut, uint32_t maxSamples)
{
    if (bufOut == nullptr || maxSamples == 0)
    {
        return -1;
    }
    std::memset(bufOut, 0, static_cast<size_t>(maxSamples) * sizeof(float));
    g_acquireCount.fetch_add(1, std::memory_order_relaxed);
    return static_cast<int>(maxSamples);
}

int asm_neural_fft_decompose(const float* input, uint32_t count, float* freqOut)
{
    if (input == nullptr || freqOut == nullptr || count == 0)
    {
        return -1;
    }
    for (uint32_t i = 0; i < count; ++i)
    {
        freqOut[i] = std::fabsf(input[i]);
    }
    return static_cast<int>(count);
}

int asm_neural_extract_csp(const float* eeg, uint32_t channels, uint32_t samples, float* cspOut)
{
    if (eeg == nullptr || cspOut == nullptr || channels == 0 || samples == 0)
    {
        return -1;
    }
    const uint32_t features = samples;
    const uint32_t totalFloats = channels * features;
    std::memcpy(cspOut, eeg, static_cast<size_t>(totalFloats) * sizeof(float));
    return 1;
}

int asm_neural_classify_intent(const float* features, uint32_t featureCount, uint32_t* intentOut)
{
    if (features == nullptr || intentOut == nullptr || featureCount == 0)
    {
        return -1;
    }
    float sum = 0.0f;
    for (uint32_t i = 0; i < featureCount; ++i)
    {
        sum += features[i];
    }
    *intentOut = static_cast<uint32_t>(sum) & 0xFu;
    return 1;
}

int asm_neural_detect_event(const float* eeg, uint32_t samples, uint32_t* eventTypeOut)
{
    if (eeg == nullptr || eventTypeOut == nullptr || samples == 0)
    {
        return -1;
    }
    float maxAbs = 0.0f;
    for (uint32_t i = 0; i < samples; ++i)
    {
        const float a = std::fabsf(eeg[i]);
        if (a > maxAbs)
        {
            maxAbs = a;
        }
    }
    *eventTypeOut = (maxAbs > 100.0f) ? 1u : 0u;
    return 1;
}

int asm_neural_encode_command(uint32_t intent, void* cmdBuf, uint32_t* cmdLen)
{
    if (cmdBuf == nullptr || cmdLen == nullptr)
    {
        return -1;
    }
    std::memcpy(cmdBuf, &intent, sizeof(uint32_t));
    *cmdLen = 4u;
    return 1;
}

int asm_neural_gen_phosphene(uint32_t /*x*/, uint32_t /*y*/, uint32_t /*intensity*/)
{
    // No display hardware — stub
    return 1;
}

int asm_neural_haptic_pulse(uint32_t /*channel*/, uint32_t /*durationMs*/, uint32_t /*intensity*/)
{
    // No haptic hardware — stub
    return 1;
}

int asm_neural_calibrate(const float* baseline, uint32_t count)
{
    if (baseline == nullptr || count == 0)
    {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_neuralMtx);
    const uint32_t copyCount = (count < 64u) ? count : 64u;
    std::memcpy(g_baseline.data(), baseline, static_cast<size_t>(copyCount) * sizeof(float));
    g_calibrated = true;
    return 1;
}

int asm_neural_adapt(const float* newData, uint32_t count, float learningRate)
{
    if (newData == nullptr || count == 0 || learningRate <= 0.0f)
    {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_neuralMtx);
    const uint32_t updateCount = (count < 64u) ? count : 64u;
    for (uint32_t i = 0; i < updateCount; ++i)
    {
        g_baseline[i] += learningRate * (newData[i] - g_baseline[i]);
    }
    return 1;
}

void asm_neural_get_stats(void* statsOut)
{
    if (statsOut == nullptr)
    {
        return;
    }
    // Layout: [uint32 channelCount][uint32 sampleRate][uint64 acquireCount][uint32 calibrated]
    uint8_t* p = static_cast<uint8_t*>(statsOut);
    uint32_t ch  = 0u;
    uint32_t sr  = 0u;
    uint32_t cal = 0u;
    {
        std::lock_guard<std::mutex> lock(g_neuralMtx);
        ch  = g_channelCount;
        sr  = g_sampleRate;
        cal = g_calibrated ? 1u : 0u;
    }
    const uint64_t acq = g_acquireCount.load(std::memory_order_relaxed);
    std::memcpy(p,      &ch,  sizeof(uint32_t));  p += sizeof(uint32_t);
    std::memcpy(p,      &sr,  sizeof(uint32_t));  p += sizeof(uint32_t);
    std::memcpy(p,      &acq, sizeof(uint64_t));  p += sizeof(uint64_t);
    std::memcpy(p,      &cal, sizeof(uint32_t));
}

void asm_neural_shutdown(void)
{
    std::lock_guard<std::mutex> lock(g_neuralMtx);
    g_channelCount  = 0;
    g_sampleRate    = 0;
    g_acquireCount.store(0, std::memory_order_relaxed);
    g_baseline.fill(0.0f);
    g_calibrated    = false;
    g_initialized   = false;
}

} // extern "C"
