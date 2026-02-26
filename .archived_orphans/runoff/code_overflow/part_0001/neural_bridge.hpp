// ============================================================================
// neural_bridge.hpp — Phase I: The Neural Bridge (Biological Integration)
// ============================================================================
//
// C++ bridge to the RawrXD_NeuralBridge.asm kernel.
// Direct cortex interface: EEG intent recognition, brainwave-to-code
// translation, neural lace stimulation stubs, ocular nerve HUD display.
//
// Think "refactor" → it is done before your finger twitches.
//
// Pattern: singleton + PatchResult error model + RAWR_HAS_MASM guards
// ============================================================================
#pragma once

#include "model_memory_hotpatch.hpp"    // PatchResult
#include <cstdint>
#include <mutex>

namespace rawrxd {

// ============================================================================
//  Enums
// ============================================================================

/// EEG frequency bands
enum class BrainwaveBand : uint32_t {
    Delta = 0,      // 0.5-4 Hz — deep sleep, unconscious processing
    Theta = 1,      // 4-8 Hz — drowsiness, meditation, creativity
    Alpha = 2,      // 8-13 Hz — relaxation, calm focus
    Beta  = 3,      // 13-30 Hz — active thinking, concentration
    Gamma = 4       // 30-100 Hz — higher cognitive functions, insight
};

/// Neural intent classification (coding actions)
enum class NeuralIntent : uint32_t {
    None           = 0,
    Refactor       = 1,
    Compile        = 2,
    DebugStart     = 3,
    DebugStep      = 4,
    DebugContinue  = 5,
    NavigateDef    = 6,
    NavigateRef    = 7,
    Search         = 8,
    Undo           = 9,
    Redo           = 10,
    Accept         = 11,
    Reject         = 12,
    ScrollUp       = 13,
    ScrollDown     = 14,
    Select         = 15,
    Copy           = 16,
    Paste          = 17,
    Save           = 18,
    Optimize       = 19,
    Explain        = 20,
    Test           = 21,
    Deploy         = 22,
    Hotpatch       = 23
};

/// Cortical events detected from brainwave analysis
enum class CorticalEvent : uint32_t {
    None        = 0,
    Focus       = 1,    // Deep focus (alpha suppression)
    Attention   = 2,    // High attention (beta surge)
    Intention   = 3,    // Motor intention detected
    Frustration = 4,    // Frustration (theta/beta ratio)
    Relaxation  = 5,    // Relaxation (alpha increase)
    Eureka      = 6,    // "Aha!" moment (gamma burst)
    Fatigue     = 7     // Mental fatigue (theta increase)
};

/// Phosphene pattern types (visual HUD through optic nerve)
enum class PhosphenePattern : uint32_t {
    Text        = 0,
    Icon        = 1,
    ProgressBar = 2,
    Highlight   = 3
};

/// Haptic feedback pattern types
enum class HapticPattern : uint32_t {
    Confirm      = 0,
    Error        = 1,
    Warning      = 2,
    Notification = 3
};

/// BCI calibration type
enum class CalibrationMode : uint32_t {
    Resting = 0,
    Focus   = 1,
    Intent  = 2
};

// ============================================================================
//  Structures
// ============================================================================

static constexpr uint32_t EEG_CHANNELS       = 8;
static constexpr uint32_t EEG_BUFFER_SAMPLES  = 1024;
static constexpr uint32_t BAND_COUNT          = 5;
static constexpr uint32_t INTENT_FEATURE_DIM  = 40;    // 5 bands * 8 channels
static constexpr uint32_t PHOSPHENE_GRID_W    = 32;
static constexpr uint32_t PHOSPHENE_GRID_H    = 32;
static constexpr uint32_t PHOSPHENE_GRID_SIZE = PHOSPHENE_GRID_W * PHOSPHENE_GRID_H;

/// Band power values for a single EEG channel
struct BandPower {
    float delta;
    float theta;
    float alpha;
    float beta;
    float gamma;
};

/// EEG sample frame (all channels, one time point)
struct EEGFrame {
    float channels[EEG_CHANNELS];
};

/// Neural command (output of intent encoding)
struct NeuralCommand {
    uint32_t intentId;
    uint32_t confidence;    // float bits
    uint64_t timestamp;     // RDTSC timestamp
};

/// Neural bridge statistics
struct NeuralStats {
    uint64_t samplesAcquired;
    uint64_t fftsDone;
    uint64_t intentsClassified;
    uint64_t eventsDetected;
    uint64_t commandsEncoded;
    uint64_t phosphenesGenerated;
    uint64_t hapticsGenerated;
    uint64_t calibrations;
    uint64_t adaptations;
    uint64_t accuracyBasisPoints;   // 0-10000 (0.00-100.00%)
    uint64_t _reserved[6];
};

/// EEG device configuration
struct EEGDeviceConfig {
    uint32_t    channelCount;
    uint32_t    sampleRate;
    uint32_t    resolution;     // ADC bits
    uint32_t    gainMultiplier;
    const char* deviceName;
};

/// Classification result with confidence
struct IntentResult {
    NeuralIntent intent;
    float        confidence;
    CorticalEvent concurrentEvent;
};

/// Calibration session data
struct CalibrationData {
    float   baselineAlpha;
    float   baselineBeta;
    float   baselineTheta;
    float   baselineGamma;
    float   focusThreshold;
    float   attentionThreshold;
    float   frustrationThreshold;
    float   fatigueThreshold;
    float   eurekaThreshold;
    uint32_t trialsCompleted;
};

// ============================================================================
//  ASM Kernel Externs (No-CRT, conditional on RAWR_HAS_MASM)
// ============================================================================
#ifdef RAWR_HAS_MASM
extern "C" {
    int      asm_neural_init();
    int      asm_neural_acquire_eeg(const void* data, int samplesPerChannel);
    int      asm_neural_fft_decompose(int numSamples);
    int      asm_neural_extract_csp(const float* spatialFilter, float* output);
    int      asm_neural_classify_intent(const float* features);
    int      asm_neural_detect_event();
    int      asm_neural_encode_command(int intentId, uint32_t confidence, void* outCmd);
    int      asm_neural_gen_phosphene(int patternType, int param, void* outGrid);
    int      asm_neural_haptic_pulse(int patternType, int duration, float* outWaveform);
    int      asm_neural_calibrate(int calibType, int numTrials);
    int      asm_neural_adapt(int correctIntent, int predictedIntent);
    void*    asm_neural_get_stats();
    int      asm_neural_shutdown();
}
#endif

// ============================================================================
//  NeuralBridge — Singleton C++ Bridge
// ============================================================================
class NeuralBridge {
public:
    static NeuralBridge& instance();

    /// Initialize neural bridge subsystem
    PatchResult initialize();

    /// Shutdown neural bridge
    PatchResult shutdown();

    /// Acquire EEG samples from device
    PatchResult acquireEEG(const float* data, uint32_t samplesPerChannel);

    /// Run FFT band decomposition on acquired data
    PatchResult decomposeFFT(uint32_t numSamples = 256);

    /// Extract Common Spatial Pattern features
    PatchResult extractCSP(const float* spatialFilter, float* features, uint32_t& featureDim);

    /// Classify intent from neural features
    IntentResult classifyIntent(const float* features = nullptr);

    /// Detect cortical event from band powers
    CorticalEvent detectEvent();

    /// Encode classified intent into command
    PatchResult encodeCommand(NeuralIntent intent, float confidence, NeuralCommand& outCmd);

    /// Generate phosphene grid pattern for visual HUD
    PatchResult generatePhosphene(PhosphenePattern pattern, uint32_t param, uint8_t* grid);

    /// Generate haptic feedback waveform
    PatchResult generateHaptic(HapticPattern pattern, uint32_t durationSamples, float* waveform, uint32_t& samplesWritten);

    /// Run BCI calibration session
    PatchResult calibrate(CalibrationMode mode, uint32_t trials);

    /// Online adaptation — update classifier with feedback
    PatchResult adapt(NeuralIntent correctLabel, NeuralIntent predictedLabel);

    /// Full pipeline: acquire → FFT → classify → encode
    IntentResult runPipeline(const float* eegData, uint32_t samplesPerChannel);

    /// Get neural bridge statistics
    NeuralStats getStats() const;

    /// Get band powers for a specific channel
    BandPower getBandPower(uint32_t channel) const;

    /// Check if the neural bridge is initialized
    bool isActive() const { return m_active; }

    /// Diagnostics dump
    void dumpDiagnostics() const;

private:
    NeuralBridge() = default;
    ~NeuralBridge() = default;
    NeuralBridge(const NeuralBridge&) = delete;
    NeuralBridge& operator=(const NeuralBridge&) = delete;

    mutable std::mutex  m_mutex;
    bool                m_active = false;

    // Software fallback state (when MASM not available)
    float               m_bandPowers[EEG_CHANNELS][BAND_COUNT] = {};
    float               m_features[INTENT_FEATURE_DIM] = {};
    NeuralStats         m_stats = {};
    CalibrationData     m_calibration = {};
};

} // namespace rawrxd
