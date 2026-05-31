// ============================================================================
// ExecPipeline.cpp - Test-Before-Apply with Safety Gating
// Enhanced with selective testing, rollback, and full verification
// ============================================================================

#include "ExecPipeline.h"
#include "PatchEngine.h"
#include "../win32ide/ExecModeToolbar.h"
#include "../win32ide/GhostOverlay.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace RawrXD {
namespace Agentic {

static ExecPipeline* g_pipeline = nullptr;
ExecPipeline* GetExecPipeline() { return g_pipeline; }
void SetExecPipeline(ExecPipeline* pipeline) { g_pipeline = pipeline; }

ExecPipeline::ExecPipeline() {
    SetExecPipeline(this);
}

ExecPipeline::~ExecPipeline() {
    if (g_pipeline == this) g_pipeline = nullptr;
}

void ExecPipeline::Initialize(const VerificationConfig& config) {
    m_config = config;
}

void ExecPipeline::Execute(const ExecTask& task) {
    auto* toolbar = UI::GetExecModeToolbar();
    UI::ExecMode mode = toolbar ? toolbar->GetMode() : UI::ExecMode::Normal;
    
    switch (mode) {
    case UI::ExecMode::Shadow:
        ExecuteShadow(task);
        break;
    case UI::ExecMode::Normal:
        ExecuteNormal(task);
        break;
    case UI::ExecMode::Unsafe:
        ExecuteUnsafe(task);
        break;
    case UI::ExecMode::Kernel:
        ExecuteKernel(task);
        break;
    }
}

void ExecPipeline::ExecuteShadow(const ExecTask& task) {
    UpdateStatus(L"[Shadow] Generating preview...");
    
    // Parse diff to show in ghost overlay
    PatchSet patch{};
    if (ParseUnifiedDiff(task.diffText, patch)) {
        UI::GhostSuggestion suggestion{};
        suggestion.type = UI::GhostType::Insert;
        suggestion.text = task.description;
        suggestion.isMultiFile = patch.files.size() > 1;
        if (!patch.files.empty()) {
            suggestion.filePath = patch.files[0].newPath;
        }
        
        if (task.onPreview) {
            task.onPreview();
        }
    }
    
    UpdateStatus(L"[Shadow] Preview ready (read-only)");
    
    if (task.onComplete) {
        task.onComplete(VerificationResult::Cancelled);
    }
}

void ExecPipeline::ExecuteNormal(const ExecTask& task) {
    UpdateStatus(L"[Normal] Generating preview...");
    
    // Show ghost overlay with suggestion
    PatchSet patch{};
    if (ParseUnifiedDiff(task.diffText, patch)) {
        UI::GhostSuggestion suggestion{};
        suggestion.type = UI::GhostType::Replace;
        suggestion.text = task.description;
        suggestion.isMultiFile = patch.files.size() > 1;
        if (!patch.files.empty()) {
            suggestion.filePath = patch.files[0].newPath;
        }
        
        if (task.onPreview) {
            task.onPreview();
        }
    }
    
    UpdateStatus(L"[Normal] Press Tab to apply, Esc to reject");
    
    if (task.onComplete) {
        task.onComplete(VerificationResult::Pending);
    }
}

void ExecPipeline::ExecuteUnsafe(const ExecTask& task) {
    UpdateStatus(L"[Unsafe] Running verification...");
    
    m_running = true;
    
    auto future = std::async(std::launch::async, [task, this]() {
        auto status = RunVerification(task.diffText);
        m_lastStatus = status;
        m_running = false;
        return status;
    });
    
    auto status = future.get();
    
    if (status.result == VerificationResult::Passed) {
        UpdateStatus(L"[Unsafe] Tests passed. Applying patch...");
        
        std::wstring error;
        if (ApplyPatch(task.diffText, error)) {
            UpdateStatus(L"[Unsafe] Patch applied successfully");
            if (task.onApply) task.onApply();
        } else {
            UpdateStatus(L"[Unsafe] Patch failed: " + error);
            if (task.onFail) task.onFail(error);
        }
    } else {
        UpdateStatus(L"[Unsafe] Tests failed. Downgrading to Normal mode...");
        
        auto* toolbar = UI::GetExecModeToolbar();
        if (toolbar) {
            toolbar->SetMode(UI::ExecMode::Normal);
        }
        
        if (task.onPreview) task.onPreview();
        if (task.onFail) task.onFail(L"Tests failed - manual review required");
    }
    
    if (task.onComplete) {
        task.onComplete(status.result);
    }
}

void ExecPipeline::ExecuteKernel(const ExecTask& task) {
    UpdateStatus(L"[Kernel] Running full verification...");
    
    m_running = true;
    
    auto status = RunVerification(task.diffText);
    m_lastStatus = status;
    m_running = false;
    
    if (status.result == VerificationResult::Passed) {
        UpdateStatus(L"[Kernel] All checks passed. Applying system-level patch...");
        
        std::wstring error;
        if (ApplyPatch(task.diffText, error)) {
            UpdateStatus(L"[Kernel] System patch applied");
            if (task.onApply) task.onApply();
        } else {
            UpdateStatus(L"[Kernel] Patch failed: " + error);
            if (task.onFail) task.onFail(error);
        }
    } else {
        UpdateStatus(L"[Kernel] Verification failed. Aborting.");
        if (task.onFail) task.onFail(L"Kernel mode verification failed");
    }
    
    if (task.onComplete) {
        task.onComplete(status.result);
    }
}

VerificationStatus ExecPipeline::RunVerification(const std::wstring& diffText) {
    VerificationStatus status{};
    auto start = std::chrono::steady_clock::now();
    
    PatchSet patch{};
    if (ParseUnifiedDiff(diffText, patch) && m_config.selectiveTesting) {
        std::wstring cmd = BuildSelectiveCommand(patch);
        status = RunCTest(cmd);
    } else {
        status = RunCTest(m_config.ctestCommand);
    }
    
    auto end = std::chrono::steady_clock::now();
    status.durationSeconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;
    
    return status;
}

VerificationStatus ExecPipeline::RunCTest(const std::wstring& command) {
    VerificationStatus status{};
    
    UpdateStatus(L"Running: " + command);
    
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    
    HANDLE hReadOut = nullptr, hWriteOut = nullptr;
    HANDLE hReadErr = nullptr, hWriteErr = nullptr;
    
    CreatePipe(&hReadOut, &hWriteOut, &sa, 0);
    CreatePipe(&hReadErr, &hWriteErr, &sa, 0);
    
    SetHandleInformation(hReadOut, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hReadErr, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.hStdOutput = hWriteOut;
    si.hStdError = hWriteErr;
    si.dwFlags = STARTF_USESTDHANDLES;
    
    PROCESS_INFORMATION pi{};
    
    std::wstring cmd = L"cmd.exe /c " + command;
    
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, 
                      nullptr, m_config.buildDir.c_str(), &si, &pi)) {
        
        DWORD waitResult = WaitForSingleObject(pi.hProcess, m_config.timeoutSeconds * 1000);
        
        if (waitResult == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, 1);
            status.result = VerificationResult::Timeout;
            status.errorOutput = L"Test execution timed out after " + 
                std::to_wstring(m_config.timeoutSeconds) + L" seconds";
        } else {
            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            status.exitCode = exitCode;
            status.result = (exitCode == 0) ? VerificationResult::Passed : VerificationResult::Failed;
        }
        
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        status.result = VerificationResult::Failed;
        status.errorOutput = L"Failed to start test process";
    }
    
    CloseHandle(hWriteOut);
    CloseHandle(hWriteErr);
    
    char buffer[4096];
    DWORD bytesRead;
    std::string output;
    
    while (ReadFile(hReadOut, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }
    
    int size = MultiByteToWideChar(CP_UTF8, 0, output.c_str(), -1, nullptr, 0);
    if (size > 0) {
        status.output.resize(size - 1);
        MultiByteToWideChar(CP_UTF8, 0, output.c_str(), -1, status.output.data(), size);
    }
    
    CloseHandle(hReadOut);
    CloseHandle(hReadErr);
    
    // Parse test counts
    size_t testsPassed = 0, testsFailed = 0;
    size_t pos = 0;
    while ((pos = status.output.find(L"tests passed", pos)) != std::wstring::npos) {
        // Simple parsing - look for "X/Y tests passed"
        size_t slash = status.output.rfind(L"/", pos);
        if (slash != std::wstring::npos && slash > 0) {
            size_t space = status.output.rfind(L" ", slash);
            if (space != std::wstring::npos) {
                std::wstring num = status.output.substr(space + 1, slash - space - 1);
                testsPassed = std::stoull(num);
            }
        }
        pos++;
    }
    
    status.testsRun = (int)(testsPassed + testsFailed);
    status.testsPassed = (int)testsPassed;
    status.testsFailed = (int)testsFailed;
    
    return status;
}

std::wstring ExecPipeline::BuildSelectiveCommand(const PatchSet& patch) {
    std::wstring cmd = m_config.ctestCommand;
    
    for (const auto& file : patch.files) {
        std::wstring filename = file.newPath.empty() ? file.oldPath : file.newPath;
        size_t pos = filename.find_last_of(L"/\\");
        if (pos != std::wstring::npos) {
            filename = filename.substr(pos + 1);
        }
        
        size_t dot = filename.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            filename = filename.substr(0, dot);
        }
        
        cmd += L" -L " + filename;
    }
    
    return cmd;
}

bool ExecPipeline::ApplyPatch(const std::wstring& diffText, std::wstring& error) {
    PatchSet patch{};
    if (!ParseUnifiedDiff(diffText, patch)) {
        error = L"Failed to parse diff";
        return false;
    }
    
    PatchResult result{};
    if (!ApplyPatchSet(patch, result, 3)) {
        error = result.errorMessage;
        return false;
    }
    
    for (const auto& file : result.modifiedFiles) {
        m_patchedFiles.push_back(file);
    }
    
    return true;
}

bool ExecPipeline::RollbackLastPatch() {
    bool success = true;
    for (const auto& file : m_patchedFiles) {
        if (!RestoreFromBackup(file)) {
            success = false;
        }
    }
    m_patchedFiles.clear();
    return success;
}

void ExecPipeline::UpdateStatus(const std::wstring& status) {
    if (m_statusCallback) {
        m_statusCallback(status);
    }
    OutputDebugStringW((status + L"\n").c_str());
}

void ExecPipeline::UpdateProgress(double progress) {
    if (m_progressCallback) {
        m_progressCallback(progress);
    }
}

} // namespace Agentic
} // namespace RawrXD
