// ============================================================================
// Execution Pipeline - Test-Before-Apply with Safety Gating
// Integrates ExecMode, GhostOverlay, PatchEngine, and CTest
// ============================================================================

#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <future>
#include <vector>
#include <atomic>
#include "PatchEngine.h"

namespace RawrXD {
namespace UI {
    enum class ExecMode : int;
    class ExecModeToolbar;
}
namespace Agentic {

enum class VerificationResult {
    Pending,
    Passed,
    Failed,
    Cancelled,
    Timeout
};

struct VerificationConfig {
    std::wstring ctestCommand = L"ctest -C Debug --output-on-failure";
    std::wstring buildDir = L"build-ninja";
    uint32_t timeoutSeconds = 300;
    bool selectiveTesting = false;
    std::vector<std::wstring> testLabels;
    bool parallelTests = true;
    uint32_t parallelJobs = 0;
};

struct ExecTask {
    std::wstring command;
    std::wstring context;
    std::wstring diffText;
    std::wstring description;
    
    std::function<void()> onPreview;
    std::function<void()> onApply;
    std::function<void(const std::wstring&)> onFail;
    std::function<void(VerificationResult)> onComplete;
};

struct VerificationStatus {
    VerificationResult result = VerificationResult::Pending;
    std::wstring output;
    std::wstring errorOutput;
    int exitCode = 0;
    double durationSeconds = 0.0;
    int testsRun = 0;
    int testsPassed = 0;
    int testsFailed = 0;
};

class ExecPipeline {
public:
    ExecPipeline();
    ~ExecPipeline();

    void Initialize(const VerificationConfig& config);
    void Execute(const ExecTask& task);
    
    void ExecuteShadow(const ExecTask& task);
    void ExecuteNormal(const ExecTask& task);
    void ExecuteUnsafe(const ExecTask& task);
    void ExecuteKernel(const ExecTask& task);
    
    VerificationStatus RunVerification(const std::wstring& diffText);
    
    bool ApplyPatch(const std::wstring& diffText, std::wstring& error);
    bool RollbackLastPatch();
    
    bool IsRunning() const { return m_running; }
    VerificationStatus GetLastStatus() const { return m_lastStatus; }
    
    void SetStatusCallback(std::function<void(const std::wstring&)> cb) { m_statusCallback = cb; }
    void SetProgressCallback(std::function<void(double)> cb) { m_progressCallback = cb; }

private:
    void UpdateStatus(const std::wstring& status);
    void UpdateProgress(double progress);
    VerificationStatus RunCTest(const std::wstring& command);
    std::wstring BuildSelectiveCommand(const PatchSet& patch);
    
    VerificationConfig m_config;
    VerificationStatus m_lastStatus;
    std::atomic<bool> m_running{false};
    std::vector<std::wstring> m_patchedFiles;
    
    std::function<void(const std::wstring&)> m_statusCallback;
    std::function<void(double)> m_progressCallback;
};

ExecPipeline* GetExecPipeline();
void SetExecPipeline(ExecPipeline* pipeline);

// Legacy API (kept for compatibility)
enum class ExecResult {
    Pending,
    Passed,
    Failed
};

struct LegacyExecTask {
    std::wstring patchText;
    std::function<void()> onApply;
    std::function<void()> onFail;
    std::function<void(const std::wstring&)> onStatus;
};

void RunTestBeforeApply(const LegacyExecTask& task);
void ApplyPatchDirect(const LegacyExecTask& task);
ExecResult RunLocalTests();
ExecResult RunSelectiveTests(const std::vector<std::wstring>& modifiedFiles);

#define WM_AGENT_STATUS_UPDATE (WM_APP + 0x201)
#define WM_AGENT_APPLY_PATCH   (WM_APP + 0x202)

} // namespace Agentic
} // namespace RawrXD
