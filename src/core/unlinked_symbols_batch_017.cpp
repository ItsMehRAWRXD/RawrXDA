// unlinked_symbols_batch_017.cpp
// RawrXD_Gold: eight extern "C" symbols — Titan SIMD entry points (when NativeModelBridge.asm
// is excluded from the Gold MASM set), fused sampler helpers, and NLShell validation.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C"
{

    void Titan_SiLU_AVX512(float* pData, int n)
    {
        if (!pData || n <= 0)
        {
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            float x = pData[i];
            pData[i] = x / (1.0f + std::exp(-x));
        }
    }

    void Titan_RMSNorm_AVX512(const float* pIn, float* pOut, const float* pWeight, int n)
    {
        if (!pIn || !pOut || !pWeight || n <= 0)
        {
            return;
        }
        float ss = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float v = pIn[i];
            ss += v * v;
        }
        const float rms = std::sqrt(ss / static_cast<float>(n) + 1e-5f);
        const float inv = 1.0f / rms;
        for (int i = 0; i < n; ++i)
        {
            pOut[i] = pIn[i] * inv * pWeight[i];
        }
    }

    void Sampler_ApplyTemperature_AVX512(float* pLogits, int n, float invTemp)
    {
        if (!pLogits || n <= 0)
        {
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            pLogits[i] *= invTemp;
        }
    }

    float Sampler_FindMax_AVX512(const float* pLogits, int n)
    {
        if (!pLogits || n <= 0)
        {
            return 0.0f;
        }
        float m = pLogits[0];
        for (int i = 1; i < n; ++i)
        {
            m = std::max(m, pLogits[i]);
        }
        return m;
    }

    float Sampler_ExpSum_AVX512(const float* pLogits, int n, float maxVal)
    {
        if (!pLogits || n <= 0)
        {
            return 0.0f;
        }
        float s = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            s += std::exp(pLogits[i] - maxVal);
        }
        return s;
    }

    void Sampler_SoftMax_TopK_Fused(float* pLogits, std::uint32_t* pIndices, int n, int K)
    {
        if (!pLogits || !pIndices || n <= 0 || K <= 0)
        {
            return;
        }
        const int kk = (K < n) ? K : n;
        struct IdxVal
        {
            int idx;
            float v;
        };
        std::vector<IdxVal> scratch(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i)
        {
            scratch[static_cast<size_t>(i)].idx = i;
            scratch[static_cast<size_t>(i)].v = pLogits[i];
        }
        std::partial_sort(scratch.begin(), scratch.begin() + kk, scratch.end(),
                          [](const IdxVal& a, const IdxVal& b) { return a.v > b.v; });
        float m = scratch[0].v;
        for (int i = 1; i < kk; ++i)
        {
            m = std::max(m, scratch[static_cast<size_t>(i)].v);
        }
        float sum = 0.0f;
        for (int i = 0; i < kk; ++i)
        {
            pLogits[i] = std::exp(scratch[static_cast<size_t>(i)].v - m);
            sum += pLogits[i];
            pIndices[i] = static_cast<std::uint32_t>(scratch[static_cast<size_t>(i)].idx);
        }
        const float inv = (sum > 0.0f) ? (1.0f / sum) : 0.0f;
        for (int i = 0; i < kk; ++i)
        {
            pLogits[i] *= inv;
        }
        for (int i = kk; i < n; ++i)
        {
            pLogits[i] = 0.0f;
        }
    }

    uint64_t NLShell_ValidateCommand(const char* cmd, uint64_t len)
    {
        if (!cmd || len == 0)
        {
            return 0;
        }
        std::string s(cmd, static_cast<size_t>(len));
        std::string lo;
        lo.reserve(s.size());
        for (unsigned char c : s)
        {
            lo.push_back(static_cast<char>(std::tolower(c)));
        }
        static const char* const kDangerous[] = {
            "rm -rf",     "del /f",          "del /s",         "format ",     "mkfs",        "dd if=",
            "fdisk",      "diskpart",        "> /dev/",        "rmdir /s",    "reg delete",  "reg add",
            "net user",   "net localgroup",  "netsh",          "sc delete",   "sc create",   "cipher /w",
            "sfc /",      "bcdedit",         "bootrec",        "shutdown /r", "shutdown /s", "taskkill /f",
            ":(){ :|: }", "powershell -enc", "powershell -e ", "cmd /c del",  "cmd /c rd",   nullptr};
        for (int i = 0; kDangerous[i]; ++i)
        {
            if (lo.find(kDangerous[i]) != std::string::npos)
            {
                return 0;
            }
        }
        return 1;
    }

}  // extern "C"
