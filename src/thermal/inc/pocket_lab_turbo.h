/**
 * @file pocket_lab_turbo.h
 * @brief C header for Pocket-Lab Turbo MASM DLL
 *
 * Pure MASM x64 kernel with TurboSparse + PowerInfer
 * Auto-scales: 70B (mobile) → 120B (workstation) → 800B (enterprise)
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Thermal snapshot from NVMe Oracle + inference state
 */
typedef struct ThermalSnapshot {
    double t0;              ///< Drive 0 temperature (°C)
    double t1;              ///< Drive 1 temperature (°C)
    double t2;              ///< Drive 2 temperature (°C)
    double t3;              ///< Drive 3 temperature (°C)
    double t4;              ///< Drive 4 temperature (°C)
    unsigned int tier;      ///< 0=70B, 1=120B, 2=800B
    unsigned int sparseSkipPct;  ///< TurboSparse skip percentage (0-100)
    unsigned int gpuSplit;  ///< PowerInfer GPU percentage (0-100)
} ThermalSnapshot;

/**
 * @brief Performance statistics
 */
typedef struct PocketLabStats {
    unsigned long long tokensProcessed;
    unsigned long long sparseSkipped;
    unsigned long long gpuProcessed;
    unsigned long long cpuProcessed;
    unsigned int tier;
    unsigned int reserved[7];
} PocketLabStats;

#ifdef POCKET_LAB_DLL_EXPORTS
    #define POCKET_LAB_API __declspec(dllexport)
#else
    #define POCKET_LAB_API __declspec(dllimport)
#endif

/**
 * @brief Initialize the Pocket-Lab kernel
 * @return 0 on success, -1 on failure
 *
 * Auto-detects RAM and configures tier:
 * - ≤8 GB  → 70B Q4 (mobile)
 * - ≤16 GB → 120B Q4 (workstation)
 * - >16 GB → 800B Q4 (enterprise)
 *
 * Connects to NVMe Oracle thermal MMF if available.
 */
POCKET_LAB_API int __stdcall PocketLabInit(void);

/**
 * @brief Get current thermal snapshot
 * @param out Pointer to ThermalSnapshot structure to fill
 *
 * Reads live temperatures from NVMe Oracle MMF.
 * Falls back to defaults (35°C) if MMF not available.
 */
POCKET_LAB_API void __stdcall PocketLabGetThermal(ThermalSnapshot* out);

/**
 * @brief Run one inference cycle
 * @param tokenCount Number of tokens to process
 * @return Number of tokens actually processed
 *
 * Applies TurboSparse skip and PowerInfer GPU/CPU split.
 */
POCKET_LAB_API unsigned long long __stdcall PocketLabRunCycle(unsigned long long tokenCount);

/**
 * @brief Get performance statistics
 * @param out Pointer to 64-byte buffer for stats
 */
POCKET_LAB_API void __stdcall PocketLabGetStats(PocketLabStats* out);

#ifdef __cplusplus
}
#endif
