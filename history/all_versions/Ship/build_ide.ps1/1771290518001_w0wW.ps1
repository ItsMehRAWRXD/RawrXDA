# ============================================================================
# RawrXD IDE - Full Production Build Script
# Compiles ALL 31 DLLs + Foundation Integration + IDE EXE
# Pure Win32/C++ - Zero Qt Dependencies
# ============================================================================

$ErrorActionPreference = "Continue"
$startTime = Get-Date

# ============================================================================
# TOOLCHAIN DETECTION
# ============================================================================
$vsBase = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
$msvcVer = (Get-ChildItem "$vsBase\VC\Tools\MSVC" -Directory | Sort-Object Name -Descending | Select-Object -First 1).Name
$clExe   = "$vsBase\VC\Tools\MSVC\$msvcVer\bin\Hostx64\x64\cl.exe"
$linkExe = "$vsBase\VC\Tools\MSVC\$msvcVer\bin\Hostx64\x64\link.exe"
$libExe  = "$vsBase\VC\Tools\MSVC\$msvcVer\bin\Hostx64\x64\lib.exe"
$vcInc   = "$vsBase\VC\Tools\MSVC\$msvcVer\include"
$vcLib   = "$vsBase\VC\Tools\MSVC\$msvcVer\lib\x64"

$sdkVer  = "10.0.22621.0"  # Use complete SDK (26100 missing ucrt + specstrings_strict.h)
$sdkBase = "C:\Program Files (x86)\Windows Kits\10"
$sdkInc  = "$sdkBase\Include\$sdkVer"
$sdkLib  = "$sdkBase\Lib\$sdkVer"

if (!(Test-Path $clExe)) { Write-Host "ERROR: cl.exe not found at $clExe" -ForegroundColor Red; exit 1 }

# Environment
$env:PATH = "$vsBase\VC\Tools\MSVC\$msvcVer\bin\Hostx64\x64;$env:PATH"
$env:INCLUDE = "$vcInc;$sdkInc\ucrt;$sdkInc\um;$sdkInc\shared;$sdkInc\winrt"
$env:LIB = "$vcLib;$sdkLib\ucrt\x64;$sdkLib\um\x64"

$shipDir = "D:\rawrxd\Ship"
$buildDir = "$shipDir\build"
if (!(Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir -Force | Out-Null }

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " RawrXD IDE - Full Production Build" -ForegroundColor Cyan
Write-Host " MSVC $msvcVer | SDK $sdkVer" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan

$commonFlags = "/O2 /DNDEBUG /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /EHsc /std:c++17 /W1 /nologo"
$commonLibs  = "kernel32.lib user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib ole32.lib advapi32.lib shlwapi.lib wininet.lib"

$totalSuccess = 0
$totalFail = 0
$failedFiles = @()

# ============================================================================
# FUNCTION: Compile a DLL from .cpp or .c source
# ============================================================================
function Build-DLL {
    param([string]$Source, [string]$DllName, [string]$ExtraLibs = "")
    
    $srcPath = "$shipDir\$Source"
    $dllPath = "$shipDir\$DllName"
    $objPath = "$buildDir\$([System.IO.Path]::GetFileNameWithoutExtension($Source)).obj"
    $expPath = "$shipDir\$([System.IO.Path]::ChangeExtension($DllName, '.exp'))"
    $libPath = "$shipDir\$([System.IO.Path]::ChangeExtension($DllName, '.lib'))"
    
    if (!(Test-Path $srcPath)) {
        Write-Host "  SKIP $Source (not found)" -ForegroundColor DarkGray
        return $false
    }
    
    $ext = [System.IO.Path]::GetExtension($Source).ToLower()
    $langFlag = if ($ext -eq ".c") { "/TC" } else { "/TP" }
    
    $cmd = "& '$clExe' $commonFlags $langFlag /LD `"$srcPath`" /Fe`"$dllPath`" /Fo`"$objPath`" /link $commonLibs $ExtraLibs /DLL /NOLOGO 2>&1"
    $output = Invoke-Expression $cmd
    
    if ($LASTEXITCODE -eq 0 -and (Test-Path $dllPath)) {
        Write-Host "  OK  $DllName" -ForegroundColor Green
        return $true
    } else {
        $errLines = ($output | Select-String "error" | Select-Object -First 2 | ForEach-Object { $_.Line.Trim() })
        Write-Host "  FAIL $DllName" -ForegroundColor Red
        foreach ($e in $errLines) { Write-Host "       $e" -ForegroundColor DarkRed }
        return $false
    }
}

# ============================================================================
# PHASE 1: Core DLLs (no inter-dependencies)
# ============================================================================
Write-Host ""
Write-Host "--- Phase 1: Core Components ---" -ForegroundColor Yellow

$phase1 = @(
    @("RawrXD_Core.cpp",                    "RawrXD_Core.dll"),
    @("RawrXD_ErrorHandler.cpp",            "RawrXD_ErrorHandler.dll"),
    @("RawrXD_MemoryManager.cpp",           "RawrXD_MemoryManager.dll"),
    @("RawrXD_Configuration.cpp",           "RawrXD_Configuration.dll"),
    @("RawrXD_SystemMonitor.cpp",           "RawrXD_SystemMonitor.dll", "psapi.lib"),
    @("RawrXD_TaskScheduler.cpp",           "RawrXD_TaskScheduler.dll"),
    @("RawrXD_ResourceManager_Win32.cpp",   "RawrXD_ResourceManager_Win32.dll"),
    @("RawrXD_SettingsManager_Win32.cpp",   "RawrXD_SettingsManager_Win32.dll"),
    @("RawrXD_Settings.c",                  "RawrXD_Settings.dll")
)

foreach ($item in $phase1) {
    $extra = if ($item.Count -gt 2) { $item[2] } else { "" }
    if (Build-DLL $item[0] $item[1] $extra) { $totalSuccess++ } else { $totalFail++; $failedFiles += $item[0] }
}

# ============================================================================
# PHASE 2: File/IO/Terminal DLLs
# ============================================================================
Write-Host ""
Write-Host "--- Phase 2: File & Terminal ---" -ForegroundColor Yellow

$phase2 = @(
    @("RawrXD_FileManager_Win32.cpp",       "RawrXD_FileManager_Win32.dll"),
    @("RawrXD_FileOperations.cpp",          "RawrXD_FileOperations.dll"),
    @("RawrXD_FileBrowser.c",               "RawrXD_FileBrowser.dll"),
    @("RawrXD_TerminalManager_Win32.cpp",   "RawrXD_TerminalManager_Win32.dll"),
    @("RawrXD_TerminalMgr.c",              "RawrXD_TerminalMgr.dll"),
    @("RawrXD_TextEditor_Win32.cpp",        "RawrXD_TextEditor_Win32.dll"),
    @("RawrXD_MainWindow_Win32.cpp",        "RawrXD_MainWindow_Win32.dll"),
    @("RawrXD_Search.c",                    "RawrXD_Search.dll"),
    @("RawrXD_SyntaxHL.c",                  "RawrXD_SyntaxHL.dll")
)

foreach ($item in $phase2) {
    $extra = if ($item.Count -gt 2) { $item[2] } else { "" }
    if (Build-DLL $item[0] $item[1] $extra) { $totalSuccess++ } else { $totalFail++; $failedFiles += $item[0] }
}

# ============================================================================
# PHASE 3: Inference & Model DLLs
# ============================================================================
Write-Host ""
Write-Host "--- Phase 3: Inference & Models ---" -ForegroundColor Yellow

$phase3 = @(
    @("RawrXD_InferenceEngine.c",           "RawrXD_InferenceEngine.dll"),
    @("RawrXD_InferenceEngine_Win32.cpp",   "RawrXD_InferenceEngine_Win32.dll"),
    @("RawrXD_ModelLoader.cpp",             "RawrXD_ModelLoader.dll"),
    @("RawrXD_ModelRouter.c",               "RawrXD_ModelRouter.dll"),
    @("RawrXD_AICompletion.c",              "RawrXD_AICompletion.dll"),
    @("RawrXD_LSPClient.c",                 "RawrXD_LSPClient.dll")
)

foreach ($item in $phase3) {
    $extra = if ($item.Count -gt 2) { $item[2] } else { "" }
    if (Build-DLL $item[0] $item[1] $extra) { $totalSuccess++ } else { $totalFail++; $failedFiles += $item[0] }
}

# ============================================================================
# PHASE 4: Agentic Framework DLLs
# ============================================================================
Write-Host ""
Write-Host "--- Phase 4: Agentic Framework ---" -ForegroundColor Yellow

$phase4 = @(
    @("RawrXD_AgenticEngine.cpp",           "RawrXD_AgenticEngine.dll"),
    @("RawrXD_AgenticController.cpp",       "RawrXD_AgenticController.dll"),
    @("RawrXD_AgentCoordinator.cpp",        "RawrXD_AgentCoordinator.dll"),
    @("RawrXD_AgentPool.cpp",               "RawrXD_AgentPool.dll"),
    @("RawrXD_AdvancedCodingAgent.cpp",     "RawrXD_AdvancedCodingAgent.dll"),
    @("RawrXD_Executor.cpp",                "RawrXD_Executor.dll"),
    @("RawrXD_CopilotBridge.cpp",           "RawrXD_CopilotBridge.dll"),
    @("RawrXD_PlanOrchestrator.c",          "RawrXD_PlanOrchestrator.dll")
)

foreach ($item in $phase4) {
    $extra = if ($item.Count -gt 2) { $item[2] } else { "" }
    if (Build-DLL $item[0] $item[1] $extra) { $totalSuccess++ } else { $totalFail++; $failedFiles += $item[0] }
}

# ============================================================================
# PHASE 5: Foundation Integration (Master Orchestrator)
# ============================================================================
Write-Host ""
Write-Host "--- Phase 5: Foundation ---" -ForegroundColor Yellow

if (Build-DLL "RawrXD_Foundation_Integration.cpp" "RawrXD_Foundation_Integration.dll" "psapi.lib dbghelp.lib") {
    $totalSuccess++
} else { $totalFail++; $failedFiles += "RawrXD_Foundation_Integration.cpp" }

# ============================================================================
# PHASE 6: IDE Executable (links against all DLL import libs)
# ============================================================================
Write-Host ""
Write-Host "--- Phase 6: IDE Executable ---" -ForegroundColor Yellow

$ideSrc = "$shipDir\RawrXD_Win32_IDE.cpp"
$ideExe = "$shipDir\RawrXD_Win32_IDE.exe"
$ideObj = "$buildDir\RawrXD_Win32_IDE.obj"

$ideLibs  = "$commonLibs psapi.lib dbghelp.lib"
$ideFlags = "$commonFlags /Fe`"$ideExe`" /Fo`"$ideObj`""

$ideCmd = "& '$clExe' $ideFlags `"$ideSrc`" /link $ideLibs /SUBSYSTEM:WINDOWS /NOLOGO 2>&1"
$ideOutput = Invoke-Expression $ideCmd

if ($LASTEXITCODE -eq 0 -and (Test-Path $ideExe)) {
    $totalSuccess++
    Write-Host "  OK  RawrXD_Win32_IDE.exe" -ForegroundColor Green
} else {
    $totalFail++
    $failedFiles += "RawrXD_Win32_IDE.cpp"
    $errLines = ($ideOutput | Select-String "error" | Select-Object -First 5 | ForEach-Object { $_.Line.Trim() })
    Write-Host "  FAIL RawrXD_Win32_IDE.exe" -ForegroundColor Red
    foreach ($e in $errLines) { Write-Host "       $e" -ForegroundColor DarkRed }
}

# ============================================================================
# SUMMARY
# ============================================================================
$elapsed = (Get-Date) - $startTime
Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host " BUILD COMPLETE" -ForegroundColor Cyan
Write-Host " Success: $totalSuccess | Failed: $totalFail" -ForegroundColor $(if ($totalFail -eq 0) { "Green" } else { "Yellow" })
Write-Host " Time: $($elapsed.TotalSeconds.ToString('F1'))s" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan

if ($totalFail -gt 0) {
    Write-Host ""
    Write-Host "Failed files:" -ForegroundColor Red
    foreach ($f in $failedFiles) { Write-Host "  - $f" -ForegroundColor Red }
}

# List built artifacts
Write-Host ""
Write-Host "Artifacts:" -ForegroundColor Gray
$dlls = (Get-ChildItem "$shipDir\RawrXD_*.dll" | Measure-Object).Count
$exes = (Get-ChildItem "$shipDir\RawrXD_*.exe" | Measure-Object).Count
Write-Host "  $dlls DLLs | $exes EXEs" -ForegroundColor Gray
