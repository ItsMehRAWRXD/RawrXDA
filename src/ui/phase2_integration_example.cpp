// Phase 2 Polish Features - Integration Example
// Shows how to use all new widgets in the Win32/HTML-based IDE.
// All Qt dependencies replaced with Win32 API + STL.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <thread>

namespace RawrXD {

// ============================================================================
// Lightweight callback types (replacing Qt signals/slots)
// ============================================================================

using CodeChangeCallback = std::function<void(const std::string& file,
                                               const std::string& original,
                                               const std::string& proposed)>;
using AcceptCallback     = std::function<void(const std::string& filePath,
                                               const std::string& content)>;
using RejectCallback     = std::function<void(const std::string& filePath)>;
using BackendChangedCb   = std::function<void(int backendIndex)>;
using TelemetryDecisionCb = std::function<void(bool enabled)>;

// ============================================================================
// INTEGRATION EXAMPLE 1: Diff Preview
// ============================================================================
// Instead of QDockWidget, use an HWND panel child of the main window.
// The diff content is rendered via the HTML/Monaco layer using postMessage.

struct DiffChange {
    std::string filePath;
    std::string originalContent;
    std::string proposedContent;
    std::string changeDescription;
};

class DiffPreviewPanel {
public:
    DiffPreviewPanel() = default;
    ~DiffPreviewPanel() = default;

    void showDiff(const DiffChange& change) {
        m_currentChange = change;
        m_visible = true;
        fprintf(stderr, "[DiffPreview] Showing diff for: %s\n", change.filePath.c_str());
        // In production: post change to Monaco editor via HTML bridge
    }

    void hide() { m_visible = false; }
    bool isVisible() const { return m_visible; }

    AcceptCallback onAccept = nullptr;
    RejectCallback onReject = nullptr;

    void acceptCurrent() {
        if (onAccept && !m_currentChange.filePath.empty()) {
            onAccept(m_currentChange.filePath, m_currentChange.proposedContent);
        }
    }

    void rejectCurrent() {
        if (onReject && !m_currentChange.filePath.empty()) {
            onReject(m_currentChange.filePath);
        }
    }

private:
    DiffChange m_currentChange;
    bool m_visible = false;
};

void integrateDiffPreview(DiffPreviewPanel& panel) {
    // Set accept handler: write proposed content to file
    panel.onAccept = [](const std::string& filePath, const std::string& content) {
        std::ofstream ofs(filePath, std::ios::binary | std::ios::trunc);
        if (ofs.is_open()) {
            ofs.write(content.data(), content.size());
            ofs.close();
            fprintf(stderr, "[DiffPreview] OK: Changes applied to %s\n", filePath.c_str());
        } else {
            fprintf(stderr, "[DiffPreview] ERROR: Failed to write to %s\n", filePath.c_str());
        }
    };

    panel.onReject = [](const std::string& filePath) {
        fprintf(stderr, "[DiffPreview] Changes rejected for %s\n", filePath.c_str());
    };
}

// ============================================================================
// INTEGRATION EXAMPLE 2: Streaming Token Progress Bar
// ============================================================================
// Instead of adding a QProgressBar to QStatusBar, use a Win32 progress
// control in the status bar area, or report via console / HTML status line.

class StreamingTokenProgress {
public:
    void startGeneration(int estimatedTokens) {
        m_estimatedTokens = estimatedTokens;
        m_generatedTokens = 0;
        m_startTime = std::chrono::steady_clock::now();
        fprintf(stderr, "[TokenProgress] Generation started (est. %d tokens)\n", estimatedTokens);
    }

    void onTokenGenerated() {
        m_generatedTokens++;
    }

    void completeGeneration() {
        auto elapsed = std::chrono::steady_clock::now() - m_startTime;
        double seconds = std::chrono::duration<double>(elapsed).count();
        double tokPerSec = (seconds > 0.0) ? (m_generatedTokens / seconds) : 0.0;
        fprintf(stderr, "[TokenProgress] Generation complete: %d tokens @ %.1f tok/s\n",
                m_generatedTokens, tokPerSec);

        if (onGenerationCompleted) {
            onGenerationCompleted(m_generatedTokens, tokPerSec);
        }
    }

    std::function<void(int totalTokens, double tokensPerSecond)> onGenerationCompleted = nullptr;

private:
    int m_estimatedTokens = 0;
    int m_generatedTokens = 0;
    std::chrono::steady_clock::time_point m_startTime;
};

void integrateTokenProgress(StreamingTokenProgress& progress) {
    // Optional: Log performance metrics via callback
    progress.onGenerationCompleted = [](int totalTokens, double tokensPerSecond) {
        fprintf(stderr, "[Metrics] Generation metrics: %d tokens @ %.2f tok/s\n",
                totalTokens, tokensPerSecond);
    };
}

// ============================================================================
// INTEGRATION EXAMPLE 3: GPU Backend Selector
// ============================================================================
// Instead of a QComboBox in QToolBar, use a Win32 ComboBox or an HTML
// dropdown rendered in the IDE toolbar area.

enum class ComputeBackend { CPU = 0, Vulkan = 1, CUDA = 2, Auto = 3 };

class GPUBackendSelector {
public:
    GPUBackendSelector() : m_current(ComputeBackend::Auto) {}

    void setBackend(ComputeBackend backend) {
        m_current = backend;
        const char* names[] = { "cpu", "vulkan", "cuda", "auto" };
        int idx = static_cast<int>(backend);
        fprintf(stderr, "[BackendSelector] Switched to: %s\n", names[idx]);

        if (onBackendChanged) {
            onBackendChanged(idx);
        }
    }

    ComputeBackend currentBackend() const { return m_current; }

    void refreshBackends() {
        fprintf(stderr, "[BackendSelector] Refreshing available backends...\n");
        // In production: query Vulkan/CUDA availability and update dropdown
    }

    BackendChangedCb onBackendChanged = nullptr;

private:
    ComputeBackend m_current;
};

void integrateBackendSelector(GPUBackendSelector& selector) {
    selector.onBackendChanged = [](int backendIndex) {
        const char* names[] = { "cpu", "vulkan", "cuda", "auto" };
        fprintf(stderr, "[IDE] Backend changed to: %s\n", names[backendIndex]);
        // In production: reload model with new backend via InferenceEngine
    };
}

// ============================================================================
// INTEGRATION EXAMPLE 4: Auto Model Download
// ============================================================================
// Instead of QDialog + QTimer::singleShot, use Win32 MessageBox or a
// custom HWND dialog. Download via WinHTTP / libcurl.

class AutoModelDownloader {
public:
    bool hasLocalModels() const {
        // Check standard model directories for .gguf files
        const char* modelDirs[] = { "models", "~/.rawrxd/models", nullptr };
        for (int i = 0; modelDirs[i]; i++) {
            std::error_code ec;
            if (std::filesystem::exists(modelDirs[i], ec) &&
                std::filesystem::is_directory(modelDirs[i], ec)) {
                for (const auto& entry : std::filesystem::directory_iterator(modelDirs[i], ec)) {
                    if (entry.path().extension() == ".gguf") {
                        return true;
                    }
                }
            }
        }
        return false;
    }
};

void integrateAutoModelDownload() {
    AutoModelDownloader downloader;

    if (!downloader.hasLocalModels()) {
        fprintf(stderr, "[ModelDownload] No local models detected\n");

#ifdef _WIN32
        int result = MessageBoxA(nullptr,
            "No GGUF models found.\nWould you like to download a recommended model?",
            "RawrXD - Model Setup",
            MB_YESNO | MB_ICONQUESTION);

        if (result == IDYES) {
            fprintf(stderr, "[ModelDownload] User accepted model download\n");
            // In production: launch WinHTTP download of recommended .gguf
        } else {
            fprintf(stderr, "[ModelDownload] User skipped model download\n");
        }
#else
        fprintf(stderr, "[ModelDownload] No models installed. Place .gguf files in models/ dir\n");
#endif
    }
}

// ============================================================================
// INTEGRATION EXAMPLE 5: Telemetry Opt-In
// ============================================================================
// Instead of a QDialog with signals, use a Win32 dialog or a startup
// prompt in the HTML layer with postMessage callback.

class TelemetryPreference {
public:
    bool hasDecision() const {
        // Check if preference file exists
        std::error_code ec;
        return std::filesystem::exists(preferencePath(), ec);
    }

    bool isEnabled() const {
        std::ifstream ifs(preferencePath());
        if (!ifs.is_open()) return false;
        std::string val;
        std::getline(ifs, val);
        return val == "enabled";
    }

    void save(bool enabled) {
        auto dir = std::filesystem::path(preferencePath()).parent_path();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        std::ofstream ofs(preferencePath(), std::ios::trunc);
        ofs << (enabled ? "enabled" : "disabled") << "\n";
        ofs.close();
        fprintf(stderr, "[Telemetry] Preference saved: %s\n", enabled ? "enabled" : "disabled");
    }

private:
    static std::string preferencePath() {
#ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            return std::string(appdata) + "\\RawrXD\\telemetry_preference.txt";
        }
        return "telemetry_preference.txt";
#else
        const char* home = std::getenv("HOME");
        if (home) {
            return std::string(home) + "/.rawrxd/telemetry_preference.txt";
        }
        return "telemetry_preference.txt";
#endif
    }
};

void integrateTelemetryOptIn(TelemetryPreference& pref) {
    if (!pref.hasDecision()) {
        fprintf(stderr, "[Telemetry] No telemetry preference found - prompting user\n");

#ifdef _WIN32
        int result = MessageBoxA(nullptr,
            "Help improve RawrXD IDE by sending anonymous usage data?\n\n"
            "This includes performance metrics and error reports.\n"
            "No personal data or model content is collected.\n\n"
            "You can change this later in Settings.",
            "RawrXD - Telemetry",
            MB_YESNO | MB_ICONINFORMATION);

        bool enabled = (result == IDYES);
        pref.save(enabled);
        fprintf(stderr, "[Telemetry] User chose: %s\n", enabled ? "ENABLED" : "DISABLED");
#else
        // Non-Windows: default to disabled, user can enable via config
        pref.save(false);
        fprintf(stderr, "[Telemetry] Defaulting to disabled (set in config to enable)\n");
#endif
    } else {
        bool enabled = pref.isEnabled();
        fprintf(stderr, "[Telemetry] Loaded saved preference: %s\n", enabled ? "enabled" : "disabled");
    }
}

// ============================================================================
// COMPLETE INTEGRATION - Call all integration functions
// ============================================================================

struct Phase2Features {
    DiffPreviewPanel diffPreview;
    StreamingTokenProgress tokenProgress;
    GPUBackendSelector backendSelector;
    TelemetryPreference telemetryPreference;
};

void initializePhase2Features(Phase2Features& features) {
    fprintf(stderr, "[Phase2] Initializing Phase 2 polish features...\n");

    integrateDiffPreview(features.diffPreview);
    integrateTokenProgress(features.tokenProgress);
    integrateBackendSelector(features.backendSelector);
    integrateAutoModelDownload();
    integrateTelemetryOptIn(features.telemetryPreference);

    fprintf(stderr, "[Phase2] OK: Phase 2 features initialized\n");
}

} // namespace RawrXD
