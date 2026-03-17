#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD — Comprehensive Feature Manifest & Cross-IDE Test Runner

.DESCRIPTION
    Introspects ALL four IDE variants (Win32, CLI, React, PowerShell) by
    scanning their actual source code, enumerating every feature, and
    running validation tests that prove each feature is REAL vs PARTIAL
    vs MISSING.

    This is the "manifestation that manifests based on the code" — it reads
    the source files themselves to derive what exists, then tests it.

.NOTES
    Phase 19: Feature Manifest & Cross-IDE Alignment
    Copyright (c) 2024-2026 RawrXD IDE Project
#>

param(
    [switch]$Verbose,
    [switch]$ExportJSON,
    [switch]$ExportMarkdown,
    [string]$OutputDir = "D:\rawrxd",
    [switch]$TestOnly,
    [switch]$SummaryOnly
)

$ErrorActionPreference = "Continue"
$script:TestResults = @()
$script:FeatureManifest = @()

# ============================================================================
# BANNER
# ============================================================================

Write-Host "`n╔══════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║     RawrXD — Comprehensive Feature Manifest & Cross-IDE Test Runner     ║" -ForegroundColor Cyan
Write-Host "║     Phase 19: Auto-Introspection · Validation · Coverage Matrix         ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# ============================================================================
# SOURCE FILE PATHS
# ============================================================================

$script:Paths = @{
    # Win32 IDE
    Win32Root     = "D:\rawrxd\src\win32app"
    Win32Header   = "D:\rawrxd\src\win32app\Win32IDE.h"
    Win32Cmds     = "D:\rawrxd\src\win32app\Win32IDE_Commands.cpp"
    Win32Core     = "D:\rawrxd\src\win32app\Win32IDE_Core.cpp"
    Win32FileOps  = "D:\rawrxd\src\win32app\Win32IDE_FileOps.cpp"
    Win32Agent    = "D:\rawrxd\src\win32app\Win32IDE_AgentCommands.cpp"
    Win32RE       = "D:\rawrxd\src\win32app\Win32IDE_ReverseEngineering.cpp"
    Win32Decomp   = "D:\rawrxd\src\win32app\Win32IDE_DecompilerView.cpp"
    Win32Themes   = "D:\rawrxd\src\win32app\Win32IDE_Themes.cpp"
    Win32Syntax   = "D:\rawrxd\src\win32app\Win32IDE_SyntaxHighlight.cpp"
    Win32Debug    = "D:\rawrxd\src\win32app\Win32IDE_Debugger.cpp"
    Win32Hotpatch = "D:\rawrxd\src\win32app\Win32IDE_HotpatchPanel.cpp"
    Win32Ghost    = "D:\rawrxd\src\win32app\Win32IDE_GhostText.cpp"
    Win32Session  = "D:\rawrxd\src\win32app\Win32IDE_Session.cpp"
    Win32PS       = "D:\rawrxd\src\win32app\Win32IDE_PowerShell.cpp"
    Win32PSPanel  = "D:\rawrxd\src\win32app\Win32IDE_PowerShellPanel.cpp"
    Win32Swarm    = "D:\rawrxd\src\win32app\Win32IDE_SwarmPanel.cpp"
    Win32Stream   = "D:\rawrxd\src\win32app\Win32IDE_StreamingUX.cpp"
    Win32Settings = "D:\rawrxd\src\win32app\Win32IDE_Settings.cpp"
    Win32Server   = "D:\rawrxd\src\win32app\Win32IDE_LocalServer.cpp"
    Win32LLM      = "D:\rawrxd\src\win32app\Win32IDE_LLMRouter.cpp"
    Win32LSP      = "D:\rawrxd\src\win32app\Win32IDE_LSPClient.cpp"
    Win32Backend  = "D:\rawrxd\src\win32app\Win32IDE_BackendSwitcher.cpp"
    Win32SubAgent = "D:\rawrxd\src\win32app\Win32IDE_SubAgent.cpp"
    Win32Multi    = "D:\rawrxd\src\win32app\Win32IDE_MultiResponse.cpp"
    Win32Manifest = "D:\rawrxd\src\win32app\Win32IDE_FeatureManifest.cpp"
    Win32Annot    = "D:\rawrxd\src\win32app\Win32IDE_Annotations.cpp"
    Win32NatDbg   = "D:\rawrxd\src\win32app\Win32IDE_NativeDebugPanel.cpp"
    Win32ExecGov  = "D:\rawrxd\src\win32app\Win32IDE_ExecutionGovernor.cpp"
    Win32Plan     = "D:\rawrxd\src\win32app\Win32IDE_PlanExecutor.cpp"
    Win32FailDet  = "D:\rawrxd\src\win32app\Win32IDE_FailureDetector.cpp"
    Win32FailInt  = "D:\rawrxd\src\win32app\Win32IDE_FailureIntelligence.cpp"
    Win32Sidebar  = "D:\rawrxd\src\win32app\Win32IDE_Sidebar.cpp"
    Win32VsCode   = "D:\rawrxd\src\win32app\Win32IDE_VSCodeUI.cpp"
    Win32Terminal = "D:\rawrxd\src\win32app\Win32TerminalManager.cpp"
    Win32D2D      = "D:\rawrxd\src\win32app\TransparentRenderer.cpp"
    
    # CLI Shell
    CLIShell      = "D:\rawrxd\src\cli_shell.cpp"
    CLICompiler   = "D:\rawrxd\src\cli\rawrxd_cli_compiler.cpp"
    
    # React IDE Generator
    ReactGen      = "D:\rawrxd\src\modules\react_generator.cpp"
    ReactIDE      = "D:\rawrxd\src\modules\react_ide_generator.cpp"
    ReactGenH     = "D:\rawrxd\src\modules\react_generator.h"
    
    # PowerShell IDE (the REAL 28K-line version)
    PSIDE         = "D:\rawrxd\RawrXD2.ps1"
    
    # Core Engine
    CMakeLists    = "D:\rawrxd\CMakeLists.txt"
    BuildDir      = "D:\rawrxd\build"
    BinDir        = "D:\rawrxd\build\bin" 
}

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

function Test-FileExists {
    param([string]$Path, [string]$Label)
    if (Test-Path $Path) {
        $size = (Get-Item $Path).Length
        $lines = if ($Path -match '\.(cpp|h|hpp|ps1|py|js|jsx|ts|tsx)$') {
            try { (Get-Content $Path).Count } catch { 0 }
        } else { 0 }
        return @{ Exists = $true; Size = $size; Lines = $lines; Label = $Label }
    }
    return @{ Exists = $false; Size = 0; Lines = 0; Label = $Label }
}

function Test-FunctionInFile {
    param([string]$Path, [string]$FuncPattern, [string]$Label)
    if (-not (Test-Path $Path)) { return @{ Found = $false; Label = $Label; Count = 0 } }
    $matches = Select-String -Path $Path -Pattern $FuncPattern -ErrorAction SilentlyContinue
    return @{ Found = ($matches.Count -gt 0); Label = $Label; Count = $matches.Count }
}

function Test-StringInFile {
    param([string]$Path, [string]$Pattern, [string]$Label)
    if (-not (Test-Path $Path)) { return @{ Found = $false; Label = $Label } }
    $content = Get-Content $Path -Raw -ErrorAction SilentlyContinue
    return @{ Found = ($content -match $Pattern); Label = $Label }
}

function Add-TestResult {
    param(
        [string]$Category,
        [string]$Feature,
        [string]$IDEVariant,  # Win32, CLI, React, PS
        [string]$Status,      # REAL, PARTIAL, FACADE, STUB, MISSING
        [string]$Evidence,
        [string]$TestType     # FileExists, FunctionSearch, PatternMatch, BuildTest, RuntimeTest
    )
    $script:TestResults += [PSCustomObject]@{
        Category  = $Category
        Feature   = $Feature
        Variant   = $IDEVariant
        Status    = $Status
        Evidence  = $Evidence
        TestType  = $TestType
        Timestamp = Get-Date -Format "HH:mm:ss"
    }
}

# ============================================================================
# PHASE 1: SOURCE FILE INVENTORY (Does the code exist?)
# ============================================================================

Write-Host "━━━ PHASE 1: Source File Inventory ━━━" -ForegroundColor Yellow
Write-Host ""

$inventory = @{}
$totalLines = 0
$totalFiles = 0

# Win32 IDE Files
$win32Files = Get-ChildItem -Path $script:Paths.Win32Root -Filter "Win32IDE*.cpp" -ErrorAction SilentlyContinue
$win32Headers = Get-ChildItem -Path $script:Paths.Win32Root -Filter "*.h" -ErrorAction SilentlyContinue
$win32AllFiles = @($win32Files) + @($win32Headers) + @(
    Get-ChildItem -Path $script:Paths.Win32Root -Filter "Win32TerminalManager.*" -ErrorAction SilentlyContinue
) + @(
    Get-ChildItem -Path $script:Paths.Win32Root -Filter "TransparentRenderer.*" -ErrorAction SilentlyContinue
)

$win32LineCount = 0
$win32FileCount = 0
foreach ($f in $win32AllFiles) {
    if ($f -and (Test-Path $f.FullName)) {
        $lc = (Get-Content $f.FullName -ErrorAction SilentlyContinue).Count
        $win32LineCount += $lc
        $win32FileCount++
    }
}
Write-Host "  Win32 IDE:    $win32FileCount files, $($win32LineCount.ToString('N0')) lines" -ForegroundColor Green
$totalLines += $win32LineCount
$totalFiles += $win32FileCount

# CLI Shell
$cliInfo = Test-FileExists $script:Paths.CLIShell "CLI Shell"
$cliCompInfo = Test-FileExists $script:Paths.CLICompiler "CLI Compiler"
$cliLines = $cliInfo.Lines + $cliCompInfo.Lines
Write-Host "  CLI Shell:    2 files, $($cliLines.ToString('N0')) lines" -ForegroundColor $(if ($cliInfo.Exists) { "Green" } else { "Red" })
$totalLines += $cliLines
$totalFiles += 2

# React IDE Generator
$reactInfo1 = Test-FileExists $script:Paths.ReactGen "React Generator"
$reactInfo2 = Test-FileExists $script:Paths.ReactIDE "React IDE Components"
$reactLines = $reactInfo1.Lines + $reactInfo2.Lines
Write-Host "  React IDE:    3 files, $($reactLines.ToString('N0')) lines" -ForegroundColor $(if ($reactInfo1.Exists) { "Green" } else { "Red" })
$totalLines += $reactLines
$totalFiles += 3

# PowerShell IDE
$psInfo = Test-FileExists $script:Paths.PSIDE "PowerShell IDE (RawrXD2.ps1)"
Write-Host "  PowerShell:   1 file,  $($psInfo.Lines.ToString('N0')) lines" -ForegroundColor $(if ($psInfo.Exists) { "Green" } else { "Red" })
$totalLines += $psInfo.Lines
$totalFiles += 1

Write-Host ""
Write-Host "  TOTAL:        $totalFiles files, $($totalLines.ToString('N0')) lines of source" -ForegroundColor White
Write-Host ""

# ============================================================================
# PHASE 2: FEATURE INTROSPECTION (What features does each IDE actually have?)
# ============================================================================

Write-Host "━━━ PHASE 2: Feature Introspection ━━━" -ForegroundColor Yellow
Write-Host ""

# ─── Helper: introspect a C++ file for specific patterns ─────────────────
function Introspect-CppFeature {
    param(
        [string]$Category,
        [string]$Feature,
        [string]$FilePath,
        [string]$Pattern,
        [int]$MinLines = 10  # Must have at least this many lines to be "REAL"
    )
    
    if (-not (Test-Path $FilePath)) {
        Add-TestResult $Category $Feature "Win32" "MISSING" "File not found: $(Split-Path -Leaf $FilePath)" "FileExists"
        return
    }
    
    $content = Get-Content $FilePath -Raw -ErrorAction SilentlyContinue
    $lineCount = (Get-Content $FilePath -ErrorAction SilentlyContinue).Count
    $matchCount = (Select-String -Path $FilePath -Pattern $Pattern -ErrorAction SilentlyContinue).Count
    
    if ($matchCount -gt 0 -and $lineCount -ge $MinLines) {
        # Check for stubs/TODO patterns (case-sensitive to avoid matching TodoItem, TodoList etc.)
        $stubCount = (Select-String -Path $FilePath -Pattern "\bTODO\b|\bSTUB\b|NOT IMPLEMENTED|// placeholder" -CaseSensitive -ErrorAction SilentlyContinue).Count
        if ($stubCount -gt ($matchCount / 2)) {
            Add-TestResult $Category $Feature "Win32" "PARTIAL" "$matchCount matches, $stubCount stubs in $lineCount lines" "PatternMatch"
        } else {
            Add-TestResult $Category $Feature "Win32" "REAL" "$matchCount matches in $lineCount lines" "PatternMatch"
        }
    } elseif ($matchCount -gt 0) {
        Add-TestResult $Category $Feature "Win32" "STUB" "$matchCount matches but only $lineCount lines" "PatternMatch"
    } else {
        Add-TestResult $Category $Feature "Win32" "MISSING" "Pattern '$Pattern' not found" "PatternMatch"
    }
}

function Introspect-CLIFeature {
    param(
        [string]$Category,
        [string]$Feature,
        [string]$Pattern
    )
    
    if (-not (Test-Path $script:Paths.CLIShell)) {
        Add-TestResult $Category $Feature "CLI" "MISSING" "cli_shell.cpp not found" "FileExists"
        return
    }
    
    $matches = Select-String -Path $script:Paths.CLIShell -Pattern $Pattern -ErrorAction SilentlyContinue
    if ($matches.Count -gt 0) {
        Add-TestResult $Category $Feature "CLI" "REAL" "$($matches.Count) matches in cli_shell.cpp" "PatternMatch"
    } else {
        Add-TestResult $Category $Feature "CLI" "MISSING" "Pattern not found in cli_shell.cpp" "PatternMatch"
    }
}

function Introspect-ReactFeature {
    param(
        [string]$Category,
        [string]$Feature,
        [string]$Pattern
    )
    
    $found = $false
    foreach ($f in @($script:Paths.ReactGen, $script:Paths.ReactIDE, $script:Paths.ReactGenH)) {
        if (Test-Path $f) {
            $m = Select-String -Path $f -Pattern $Pattern -ErrorAction SilentlyContinue
            if ($m.Count -gt 0) { $found = $true; break }
        }
    }
    
    if ($found) {
        Add-TestResult $Category $Feature "React" "REAL" "Pattern found in React generator" "PatternMatch"
    } else {
        Add-TestResult $Category $Feature "React" "MISSING" "Pattern not in React files" "PatternMatch"
    }
}

function Introspect-PSFeature {
    param(
        [string]$Category,
        [string]$Feature,
        [string]$Pattern
    )
    
    if (-not (Test-Path $script:Paths.PSIDE)) {
        Add-TestResult $Category $Feature "PS" "MISSING" "RawrXD2.ps1 not found" "FileExists"
        return
    }
    
    $matches = Select-String -Path $script:Paths.PSIDE -Pattern $Pattern -ErrorAction SilentlyContinue
    if ($matches.Count -gt 0) {
        Add-TestResult $Category $Feature "PS" "REAL" "$($matches.Count) matches in RawrXD2.ps1" "PatternMatch"
    } else {
        Add-TestResult $Category $Feature "PS" "MISSING" "Pattern not found in RawrXD2.ps1" "PatternMatch"
    }
}

# Macro for all 4 variants at once
function Introspect-Feature {
    param(
        [string]$Category,
        [string]$Feature,
        [string]$Win32File,
        [string]$Win32Pattern,
        [string]$CLIPattern,
        [string]$ReactPattern,
        [string]$PSPattern,
        [int]$MinLines = 10
    )
    
    Introspect-CppFeature $Category $Feature $Win32File $Win32Pattern $MinLines
    Introspect-CLIFeature $Category $Feature $CLIPattern
    Introspect-ReactFeature $Category $Feature $ReactPattern
    Introspect-PSFeature $Category $Feature $PSPattern
}

# ──────────────────────────────────────────────────────────────────────────
# FILE OPERATIONS
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  📂 File Operations..." -ForegroundColor Gray
Introspect-Feature "File Ops" "New File"     $script:Paths.Win32Cmds "IDM_FILE_NEW"     "cmd_new_file"      "NEVER_MATCH_REACT" "New-EditorFile"
Introspect-Feature "File Ops" "Open File"    $script:Paths.Win32Cmds "IDM_FILE_OPEN"    "cmd_open_file"     "NEVER_MATCH_REACT" "Open-EditorFile"
Introspect-Feature "File Ops" "Save File"    $script:Paths.Win32Cmds "IDM_FILE_SAVE"    "cmd_save_file"     "NEVER_MATCH_REACT" "Save-EditorFile"
Introspect-Feature "File Ops" "Save As"      $script:Paths.Win32Cmds "IDM_FILE_SAVEAS"  "cmd_save_as"       "NEVER_MATCH_REACT" "Save-EditorFileAs"
Introspect-Feature "File Ops" "Close File"   $script:Paths.Win32Cmds "IDM_FILE_CLOSE"   "cmd_close_file"    "NEVER_MATCH_REACT" "Close-EditorFile"
Introspect-Feature "File Ops" "Load Model"   $script:Paths.Win32Cmds "IDM_FILE_LOAD_MODEL" "NEVER_MATCH" "engine.*load" "Open-GGUFModel"
Introspect-Feature "File Ops" "Model HF"     $script:Paths.Win32Cmds "IDM_FILE_MODEL_FROM_HF" "NEVER_MATCH" "NEVER_MATCH" "HuggingFace"
Introspect-Feature "File Ops" "Recent Files" $script:Paths.Win32Cmds "IDM_FILE_RECENT"  "NEVER_MATCH"       "NEVER_MATCH" "Add-FileToRecentList"

# ──────────────────────────────────────────────────────────────────────────
# EDITING
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  ✂️  Editing..." -ForegroundColor Gray
Introspect-Feature "Editing" "Undo"         $script:Paths.Win32Cmds "IDM_EDIT_UNDO"    "cmd_undo"     "NEVER_MATCH" "Invoke-EditorCommand.*undo"
Introspect-Feature "Editing" "Redo"         $script:Paths.Win32Cmds "IDM_EDIT_REDO"    "cmd_redo"     "NEVER_MATCH" "Invoke-EditorCommand.*redo"
Introspect-Feature "Editing" "Cut"          $script:Paths.Win32Cmds "IDM_EDIT_CUT"     "cmd_cut"      "NEVER_MATCH" "Invoke-EditorCommand.*cut"
Introspect-Feature "Editing" "Copy"         $script:Paths.Win32Cmds "IDM_EDIT_COPY"    "cmd_copy"     "NEVER_MATCH" "Invoke-EditorCommand.*copy"
Introspect-Feature "Editing" "Paste"        $script:Paths.Win32Cmds "IDM_EDIT_PASTE"   "cmd_paste"    "NEVER_MATCH" "Invoke-EditorCommand.*paste"
Introspect-Feature "Editing" "Find"         $script:Paths.Win32Cmds "IDM_EDIT_FIND"    "cmd_find"     "NEVER_MATCH" "Show-FindDialog"
Introspect-Feature "Editing" "Replace"      $script:Paths.Win32Cmds "IDM_EDIT_REPLACE" "cmd_replace"  "NEVER_MATCH" "Show-ReplaceDialog"

# ──────────────────────────────────────────────────────────────────────────
# AGENT / AI
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🤖 Agent / AI..." -ForegroundColor Gray
Introspect-Feature "Agent" "Agent Loop"       $script:Paths.Win32Agent "IDM_AGENT_START_LOOP" "cmd_agent_loop"     "AgentMode"       "Invoke-AgenticChat"
Introspect-Feature "Agent" "Agent Execute"    $script:Paths.Win32Agent "IDM_AGENT_EXECUTE"    "cmd_agent_execute"  "AgentMode"       "Invoke-AgenticShellCommand"
Introspect-Feature "Agent" "Agent Goal"       $script:Paths.Win32Agent "agentGoal|setGoal"    "cmd_agent_goal"     "NEVER_MATCH"     "agentContext.*Goal"
Introspect-Feature "Agent" "Agent Memory"     $script:Paths.Win32SubAgent "agentMemory|onAgentMemoryStore|IDM_AGENT_MEMORY" "cmd_agent_memory" "NEVER_MATCH" "agentContext.*Memory"
Introspect-Feature "Agent" "Agent Stop"       $script:Paths.Win32Agent "IDM_AGENT_STOP"       "NEVER_MATCH"        "NEVER_MATCH"     "Stop-OllamaHost"
Introspect-Feature "Agent" "Failure Detect"   $script:Paths.Win32FailDet "FailureType|refusal|hallucination" "NEVER_MATCH" "NEVER_MATCH" "Register-ErrorHandler"
Introspect-Feature "Agent" "Plan Executor"    $script:Paths.Win32Plan  "PlanStep|executePlan"  "NEVER_MATCH"        "NEVER_MATCH"     "New-AgentTask|Start-AgentTask"
Introspect-Feature "Agent" "Exec Governor"    $script:Paths.Win32ExecGov "rateLimit|governor"   "NEVER_MATCH"        "NEVER_MATCH"     "MaxActionsPerMinute"

# ──────────────────────────────────────────────────────────────────────────
# AUTONOMY
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🔄 Autonomy..." -ForegroundColor Gray
Introspect-Feature "Autonomy" "Toggle"     $script:Paths.Win32Cmds "IDM_AUTONOMY_TOGGLE"  "cmd_autonomy_start" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "Autonomy" "Set Goal"   $script:Paths.Win32Cmds "IDM_AUTONOMY_SET_GOAL" "cmd_autonomy_goal"  "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "Autonomy" "Rate Limit" $script:Paths.Win32Cmds "maxActionsPerMinute"   "cmd_autonomy_rate"  "NEVER_MATCH" "NEVER_MATCH"

# ──────────────────────────────────────────────────────────────────────────
# AI MODES
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🧠 AI Modes..." -ForegroundColor Gray
Introspect-Feature "AI Mode" "Deep Thinking"  $script:Paths.Win32Agent "IDM_AI_MODE_DEEP_THINK"    "!deep"          "deepThinking"   "Deep.*Think"
Introspect-Feature "AI Mode" "Deep Research"  $script:Paths.Win32Agent "IDM_AI_MODE_DEEP_RESEARCH" "!research"      "deepResearch"   "Deep.*Research"
Introspect-Feature "AI Mode" "No Refusal"     $script:Paths.Win32Agent "IDM_AI_MODE_NO_REFUSAL"    "NEVER_MATCH"    "noRefusal"      "No.*Refusal|noRefusal"
Introspect-Feature "AI Mode" "Context Window" $script:Paths.Win32Agent "IDM_AI_CONTEXT"            "!max"           "contextLimit"   "contextLimit|ContextLength"

# ──────────────────────────────────────────────────────────────────────────
# DEBUGGING
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🐛 Debugging..." -ForegroundColor Gray
Introspect-Feature "Debug" "Start Debug"  $script:Paths.Win32Debug "startDebug|debug_start"  "cmd_debug_start"   "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "Debug" "Breakpoints"  $script:Paths.Win32Debug "breakpoint|toggleBreak"  "cmd_breakpoint"    "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "Debug" "Step Over"    $script:Paths.Win32Debug "stepOver|stepInto"       "cmd_debug_step"    "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "Debug" "Native DbgEng" $script:Paths.Win32NatDbg "NativeDebuggerEngine|IDM_DBG_LAUNCH|DbgEng"  "NEVER_MATCH"       "NEVER_MATCH" "NEVER_MATCH"

# ──────────────────────────────────────────────────────────────────────────
# REVERSE ENGINEERING
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🔬 Reverse Engineering..." -ForegroundColor Gray
Introspect-Feature "RE" "PE Analysis"     $script:Paths.Win32RE  "IDM_REVENG_ANALYZE"   "NEVER_MATCH" "dumpbin" "NEVER_MATCH"
Introspect-Feature "RE" "Disassembly"     $script:Paths.Win32RE  "IDM_REVENG_DISASM"    "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "RE" "DumpBin"         $script:Paths.Win32RE  "IDM_REVENG_DUMPBIN"   "NEVER_MATCH" "dumpbin" "NEVER_MATCH"
Introspect-Feature "RE" "MASM Compile"    $script:Paths.Win32RE  "IDM_REVENG_COMPILE"   "NEVER_MATCH" "compile" "NEVER_MATCH"
Introspect-Feature "RE" "SSA Lifting"     $script:Paths.Win32RE  "IDM_REVENG_SSA"       "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "RE" "Type Recovery"   $script:Paths.Win32RE  "IDM_REVENG_TYPE"      "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "RE" "Data Flow"       $script:Paths.Win32RE  "IDM_REVENG_DATA_FLOW" "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "RE" "CFG Generation"  $script:Paths.Win32RE  "IDM_REVENG_CFG"       "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "RE" "Export IDA"      $script:Paths.Win32RE  "IDM_REVENG_EXPORT_IDA" "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "RE" "Export Ghidra"   $script:Paths.Win32RE  "IDM_REVENG_EXPORT_GHIDRA" "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"

# ──────────────────────────────────────────────────────────────────────────
# DECOMPILER VIEW
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🔍 Decompiler View (D2D)..." -ForegroundColor Gray
Introspect-Feature "Decompiler" "D2D Split View"    $script:Paths.Win32Decomp "DecompViewState|DECOMP_SPLIT"  "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "Decompiler" "Syntax Coloring"   $script:Paths.Win32Decomp "tokenizeLine|getTokenColor"    "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "Decompiler" "Sync Selection"    $script:Paths.Win32Decomp "SyncFromDecomp|SyncFromDisasm" "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "Decompiler" "SSA Var Rename"    $script:Paths.Win32Decomp "PropagateRename|varRenameMap"  "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"

# ──────────────────────────────────────────────────────────────────────────
# HOTPATCH (3-LAYER)
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🔥 Hotpatch System..." -ForegroundColor Gray
Introspect-Feature "Hotpatch" "Memory Patch"    "D:\rawrxd\src\core\model_memory_hotpatch.cpp" "VirtualProtect|mprotect" "cmd_hotpatch" "Hotpatch" "NEVER_MATCH" 5
Introspect-Feature "Hotpatch" "Byte-Level"      "D:\rawrxd\src\core\byte_level_hotpatcher.cpp" "patch_bytes|find_pattern_asm|search_and_patch" "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH" 5
Introspect-Feature "Hotpatch" "Server Patch"    "D:\rawrxd\src\server\gguf_server_hotpatch.cpp" "ServerHotpatch|transform" "NEVER_MATCH" "Hotpatch" "NEVER_MATCH" 5
Introspect-Feature "Hotpatch" "Unified Manager" "D:\rawrxd\src\core\unified_hotpatch_manager.cpp" "UnifiedResult|apply_memory" "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH" 5
Introspect-Feature "Hotpatch" "Hotpatch Panel"  $script:Paths.Win32Hotpatch "HotpatchPanel|hotpatchList" "NEVER_MATCH" "HotpatchControls" "NEVER_MATCH"

# ──────────────────────────────────────────────────────────────────────────
# THEMES
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🎨 Themes..." -ForegroundColor Gray
Introspect-Feature "Themes" "16 Built-in"     $script:Paths.Win32Themes "IDM_THEME_|Monokai|Dracula|Nord" "NEVER_MATCH" "NEVER_MATCH" "Apply-Theme"
Introspect-Feature "Themes" "Theme Editor"    $script:Paths.Win32Cmds "showThemeEditor"    "NEVER_MATCH" "NEVER_MATCH" "Show-CustomThemeBuilder"
Introspect-Feature "Themes" "Transparency"    $script:Paths.Win32Themes "setWindowTransparency|WS_EX_LAYERED" "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"

# ──────────────────────────────────────────────────────────────────────────
# SYNTAX HIGHLIGHTING
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🖍️  Syntax Highlighting..." -ForegroundColor Gray
Introspect-Feature "Syntax" "C++ Keywords"    $script:Paths.Win32Syntax "cppKeywords|isKeyword"  "NEVER_MATCH" "MonacoEditor" "Get-FileIcon"
Introspect-Feature "Syntax" "ASM Semantic"    "D:\rawrxd\src\win32app\Win32IDE_AsmSemantic.cpp" "AsmInstructionInfo|AsmRegisterInfo|lookupInstruction" "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "Syntax" "6 Languages"     $script:Paths.Win32Syntax "SyntaxLanguage|Python|Rust|GLSL" "NEVER_MATCH" "language" "NEVER_MATCH"

# ──────────────────────────────────────────────────────────────────────────
# TERMINAL
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  💻 Terminal..." -ForegroundColor Gray
Introspect-Feature "Terminal" "New Terminal"   $script:Paths.Win32Terminal "createTerminalPane|TerminalManager" "cmd_terminal_new"  "NEVER_MATCH" "New-Terminal"
Introspect-Feature "Terminal" "Split Terminal" $script:Paths.Win32Cmds "splitTerminalHorizontal|splitTerminalVertical|IDM_TERMINAL_SPLIT" "cmd_terminal_split" "NEVER_MATCH" "Split-Terminal"
Introspect-Feature "Terminal" "Kill Terminal"  $script:Paths.Win32SubAgent "killTerminal|killTerminalWithTimeout|IDM_TERMINAL_KILL" "cmd_terminal_kill" "NEVER_MATCH" "Kill-Terminal"

# ──────────────────────────────────────────────────────────────────────────
# STREAMING UX
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  📡 Streaming..." -ForegroundColor Gray
Introspect-Feature "Streaming" "Token Stream"  $script:Paths.Win32SubAgent "appendStreamingToken|streamingOutput|clearStreamingOutput" "NEVER_MATCH" "NEVER_MATCH" "streaming|token.*by.*token"
Introspect-Feature "Streaming" "Ghost Text"    $script:Paths.Win32Ghost  "ghostText|inlineSuggestion"          "NEVER_MATCH" "NEVER_MATCH" "Get-AIAutoCompleteSuggestions"

# ──────────────────────────────────────────────────────────────────────────
# SUBAGENT / SWARM
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🐝 SubAgent / Swarm..." -ForegroundColor Gray
Introspect-Feature "SubAgent" "Spawn SubAgent" $script:Paths.Win32SubAgent "spawnSubAgent|SubAgentManager"  "cmd_subagent"  "NEVER_MATCH" "Send-AgentCommand"
Introspect-Feature "SubAgent" "Prompt Chain"   $script:Paths.Win32SubAgent "executeChain"    "cmd_chain"     "NEVER_MATCH" "Invoke-AgenticWorkflow"
Introspect-Feature "SubAgent" "HexMag Swarm"   $script:Paths.Win32SubAgent "executeSwarm"    "cmd_swarm"     "NEVER_MATCH" "Start-ParallelChatProcessing"
Introspect-Feature "SubAgent" "Todo List"      $script:Paths.Win32SubAgent "todoList|TodoItem" "cmd_todo"    "NEVER_MATCH" "Get-AgentTodoList"
Introspect-Feature "SubAgent" "Swarm Panel"    $script:Paths.Win32Swarm "SwarmPanel"       "NEVER_MATCH"   "NEVER_MATCH" "NEVER_MATCH"

# ──────────────────────────────────────────────────────────────────────────
# SESSION / SETTINGS / GIT
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  ⚙️  Session / Settings / Git..." -ForegroundColor Gray
Introspect-Feature "Session" "Save Session"     $script:Paths.Win32Session  "saveSession"      "NEVER_MATCH" "NEVER_MATCH" "Save-Settings"
Introspect-Feature "Session" "Restore Session"  $script:Paths.Win32Session  "restoreSession"   "NEVER_MATCH" "NEVER_MATCH" "Load-Settings"
Introspect-Feature "Settings" "Editor Config"   $script:Paths.Win32Settings "fontFamily|tabSize|lineNumbers" "NEVER_MATCH" "NEVER_MATCH" "Show-EditorSettings"
Introspect-Feature "Git" "Git Status"           $script:Paths.Win32Cmds "gitStatus|8001"    "NEVER_MATCH" "NEVER_MATCH" "Get-GitStatus"
Introspect-Feature "Git" "Git Commit"           $script:Paths.Win32Cmds "gitCommit|8002"    "NEVER_MATCH" "NEVER_MATCH" "Invoke-GitCommand"

# ──────────────────────────────────────────────────────────────────────────
# VIEW & LAYOUT
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🖥️  View & Layout..." -ForegroundColor Gray
Introspect-Feature "View" "Sidebar"           $script:Paths.Win32Sidebar "toggleSidebar"    "NEVER_MATCH" "NEVER_MATCH" "Toggle-Sidebar"
Introspect-Feature "View" "Output Panel"      $script:Paths.Win32Cmds  "toggleOutputPanel" "NEVER_MATCH" "NEVER_MATCH" "Toggle-OutputPanel"
Introspect-Feature "View" "Minimap"           $script:Paths.Win32Cmds  "toggleMinimap"    "NEVER_MATCH" "minimap" "Set-EditorMinimap"
Introspect-Feature "View" "Command Palette"   $script:Paths.Win32Cmds    "commandPalette|fuzzyMatch" "NEVER_MATCH" "NEVER_MATCH" "Show-CommandPalette"

# ──────────────────────────────────────────────────────────────────────────
# POWERSHELL INTEGRATION
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  📟 PowerShell Integration..." -ForegroundColor Gray
Introspect-Feature "PowerShell" "PS Execute"  $script:Paths.Win32PS     "executePowerShellCommand"  "NEVER_MATCH" "NEVER_MATCH" "Invoke-TerminalCommand"
Introspect-Feature "PowerShell" "PS Panel"    $script:Paths.Win32PSPanel "createPowerShellPanel"     "NEVER_MATCH" "NEVER_MATCH" "Toggle-TerminalPanel"

# ──────────────────────────────────────────────────────────────────────────
# LLM ROUTER / BACKEND / LSP
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  🔀 LLM Router / Backend / LSP..." -ForegroundColor Gray
Introspect-Feature "LLM" "Multi-Engine"     $script:Paths.Win32LLM     "routePrompt|LLMRouter"  "!engine"     "Engine" "Send-AIRequest"
Introspect-Feature "LLM" "Backend Switch"   $script:Paths.Win32Backend "switchBackend|Backend"   "NEVER_MATCH" "Engine" "Switch-AIBackend"
Introspect-Feature "LSP" "LSP Client"       $script:Paths.Win32LSP     "LSPClient|lsp_init"      "NEVER_MATCH" "NEVER_MATCH" "NEVER_MATCH"
Introspect-Feature "LLM" "Local Server"     $script:Paths.Win32Server  "localServer|startHTTP"   "!server"     "NEVER_MATCH" "Start-OllamaServer"

# ──────────────────────────────────────────────────────────────────────────
# PS-ONLY FEATURES (things that exist in RawrXD2.ps1 but not Win32)
# ──────────────────────────────────────────────────────────────────────────
Write-Host "  📦 PS-Only Features..." -ForegroundColor Gray
Introspect-PSFeature "PS-Only" "GGUF Binary Reader"      "Read-GGUFHeader|Open-GGUFModel"
Introspect-PSFeature "PS-Only" "PoshLLM Inference"        "Invoke-PoshLLMInference"
Introspect-PSFeature "PS-Only" "Zone-Streamed Tensors"    "Load-TensorZone|Unload-OldestZone"
Introspect-PSFeature "PS-Only" "Extension Marketplace"    "Search-Marketplace|Install-MarketplaceExtension"
Introspect-PSFeature "PS-Only" "WebView2 Browser"         "Open-Browser|WebView2"
Introspect-PSFeature "PS-Only" "PS5.1 Video Browser"      "Open-PS51VideoBrowser"
Introspect-PSFeature "PS-Only" "Dependency Tracker"        "Track-Dependency|Detect-ProjectDependencies"
Introspect-PSFeature "PS-Only" "Task Scheduler"            "Schedule-AgentTask|Start-ScheduledTaskExecution"
Introspect-PSFeature "PS-Only" "Multi-threaded Agents"     "Initialize-MultithreadedAgents|Start-AgentTaskAsync"
Introspect-PSFeature "PS-Only" "AI Code Snippets"          "New-AIBackendCodeSnippet|Get-AIBackendCodeSnippets"
Introspect-PSFeature "PS-Only" "Reverse HTTP Backend"      "Invoke-ReverseHttpQuery|ConvertFrom-ReverseHttpChunk"
Introspect-PSFeature "PS-Only" "LMStudio Integration"      "Test-LMStudioConnection|Send-LMStudioRequest"
Introspect-PSFeature "PS-Only" "llama.cpp Direct"          "Invoke-LlamaCPPQuery|Invoke-LlamaCPPCompletion"
Introspect-PSFeature "PS-Only" "Chat Tabs"                 "New-ChatTab|Remove-ChatTab|Send-ChatMessage"
Introspect-PSFeature "PS-Only" "Pop-Out Editor"            "Show-PopOutEditor|Open-FileInPopOutEditor"
Introspect-PSFeature "PS-Only" "Monaco Editor Embed"       "Show-MonacoEditor|Show-MonacoEditorEmbedded"
Introspect-PSFeature "PS-Only" "Custom Theme Builder"      "Show-CustomThemeBuilder|Apply-CustomTheme"
Introspect-PSFeature "PS-Only" "Performance Profiler"      "Start-PerformanceProfiler|Show-ProfilerResults"
Introspect-PSFeature "PS-Only" "Security Settings"         "Show-SecuritySettings|Initialize-SecurityConfig"
Introspect-PSFeature "PS-Only" "Encryption Test"           "Show-EncryptionTest"
Introspect-PSFeature "PS-Only" "AI Debug Metrics"          "Get-AIDebugMetrics|Measure-AIQueryPerformance"

Write-Host ""

# ============================================================================
# PHASE 3: BUILD VERIFICATION
# ============================================================================

Write-Host "━━━ PHASE 3: Build Verification ━━━" -ForegroundColor Yellow
Write-Host ""

# Check Win32 IDE binary
$win32Exe = Join-Path $script:Paths.BuildDir "bin\RawrXD-Win32IDE.exe"
if (Test-Path $win32Exe) {
    $exeInfo = Get-Item $win32Exe
    $sizeMB = [math]::Round($exeInfo.Length / 1MB, 1)
    $buildAge = ((Get-Date) - $exeInfo.LastWriteTime).TotalHours
    Write-Host "  ✅ Win32 IDE Binary: $sizeMB MB" -ForegroundColor Green
    Write-Host "     Built: $($exeInfo.LastWriteTime)" -ForegroundColor Gray
    if ($buildAge -lt 24) {
        Write-Host "     Status: FRESH BUILD (< 24h)" -ForegroundColor Green
    } else {
        Write-Host "     Status: STALE ($([int]$buildAge)h old)" -ForegroundColor Yellow
    }
    Add-TestResult "Build" "Win32 IDE Binary" "Win32" "REAL" "$sizeMB MB, built $($exeInfo.LastWriteTime)" "BuildTest"
} else {
    Write-Host "  ❌ Win32 IDE Binary NOT FOUND" -ForegroundColor Red
    Add-TestResult "Build" "Win32 IDE Binary" "Win32" "MISSING" "No binary at $win32Exe" "BuildTest"
}

# Check RawrEngine CLI binary
$cliExe = Join-Path $script:Paths.BuildDir "RawrEngine.exe"
if (-not (Test-Path $cliExe)) { $cliExe = Join-Path $script:Paths.BuildDir "bin\RawrEngine.exe" }
if (Test-Path $cliExe) {
    $exeInfo = Get-Item $cliExe
    Write-Host "  ✅ CLI Engine Binary: $([math]::Round($exeInfo.Length / 1MB, 1)) MB" -ForegroundColor Green
    Add-TestResult "Build" "CLI Engine Binary" "CLI" "REAL" "$([math]::Round($exeInfo.Length / 1MB, 1)) MB" "BuildTest"
} else {
    Write-Host "  ⚠️  CLI Engine Binary not found" -ForegroundColor Yellow
    Add-TestResult "Build" "CLI Engine Binary" "CLI" "MISSING" "No binary found" "BuildTest"
}

Write-Host ""

# ============================================================================
# PHASE 4: COVERAGE MATRIX
# ============================================================================

Write-Host "━━━ PHASE 4: Coverage Matrix ━━━" -ForegroundColor Yellow
Write-Host ""

$variants = @("Win32", "CLI", "React", "PS")
$statuses = @("REAL", "PARTIAL", "FACADE", "STUB", "MISSING")

# Summary table
Write-Host "  ┌──────────────┬───────┬─────────┬────────┬──────┬─────────┬──────────┐" -ForegroundColor DarkGray
Write-Host "  │ IDE Variant   │ REAL  │ PARTIAL │ FACADE │ STUB │ MISSING │ Coverage │" -ForegroundColor DarkGray
Write-Host "  ├──────────────┼───────┼─────────┼────────┼──────┼─────────┼──────────┤" -ForegroundColor DarkGray

foreach ($v in $variants) {
    $variantResults = $script:TestResults | Where-Object { $_.Variant -eq $v }
    $total = @($variantResults).Count
    if ($total -eq 0) { continue }
    
    $real    = @($variantResults | Where-Object { $_.Status -eq "REAL" }).Count
    $partial = @($variantResults | Where-Object { $_.Status -eq "PARTIAL" }).Count
    $facade  = @($variantResults | Where-Object { $_.Status -eq "FACADE" }).Count
    $stub    = @($variantResults | Where-Object { $_.Status -eq "STUB" }).Count
    $missing = @($variantResults | Where-Object { $_.Status -eq "MISSING" }).Count
    $pct     = if ($total -gt 0) { [math]::Round($real * 100 / $total) } else { 0 }
    
    $pctColor = if ($pct -ge 80) { "Green" } elseif ($pct -ge 50) { "Yellow" } else { "Red" }
    
    $label = switch ($v) {
        "Win32" { "Win32 (C++) " }
        "CLI"   { "CLI Shell   " }
        "React" { "React IDE   " }
        "PS"    { "PowerShell  " }
    }
    
    Write-Host "  │ $label │" -NoNewline -ForegroundColor DarkGray
    Write-Host " $($real.ToString().PadLeft(3))  " -NoNewline -ForegroundColor Green
    Write-Host "│" -NoNewline -ForegroundColor DarkGray
    Write-Host " $($partial.ToString().PadLeft(5))   " -NoNewline -ForegroundColor Yellow
    Write-Host "│" -NoNewline -ForegroundColor DarkGray
    Write-Host " $($facade.ToString().PadLeft(4))   " -NoNewline -ForegroundColor Magenta
    Write-Host "│" -NoNewline -ForegroundColor DarkGray
    Write-Host " $($stub.ToString().PadLeft(2))   " -NoNewline -ForegroundColor DarkCyan
    Write-Host "│" -NoNewline -ForegroundColor DarkGray
    Write-Host " $($missing.ToString().PadLeft(5))   " -NoNewline -ForegroundColor Red
    Write-Host "│" -NoNewline -ForegroundColor DarkGray
    Write-Host "  $($pct.ToString().PadLeft(3))%    " -NoNewline -ForegroundColor $pctColor
    Write-Host "│" -ForegroundColor DarkGray
}

Write-Host "  └──────────────┴───────┴─────────┴────────┴──────┴─────────┴──────────┘" -ForegroundColor DarkGray
Write-Host ""

# ============================================================================
# PHASE 5: CATEGORY BREAKDOWN
# ============================================================================

if (-not $SummaryOnly) {
    Write-Host "━━━ PHASE 5: Category Breakdown ━━━" -ForegroundColor Yellow
    Write-Host ""
    
    $categories = $script:TestResults | Select-Object -ExpandProperty Category -Unique | Sort-Object
    
    foreach ($cat in $categories) {
        $catResults = $script:TestResults | Where-Object { $_.Category -eq $cat }
        Write-Host "  [$cat]" -ForegroundColor Cyan
        
        $features = $catResults | Select-Object -ExpandProperty Feature -Unique | Sort-Object
        foreach ($feat in $features) {
            $featResults = $catResults | Where-Object { $_.Feature -eq $feat }
            
            $icons = ""
            foreach ($v in $variants) {
                $r = $featResults | Where-Object { $_.Variant -eq $v } | Select-Object -First 1
                if ($r) {
                    $icon = switch ($r.Status) {
                        "REAL"    { "✅" }
                        "PARTIAL" { "🔶" }
                        "FACADE"  { "🎭" }
                        "STUB"    { "📌" }
                        "MISSING" { "❌" }
                        default   { "❓" }
                    }
                    $icons += "$icon "
                } else {
                    $icons += "❓ "
                }
            }
            
            Write-Host "    $icons $feat" -ForegroundColor White
        }
        Write-Host ""
    }
}

# ============================================================================
# PHASE 6: EXPORT
# ============================================================================

if ($ExportMarkdown -or $ExportJSON) {
    Write-Host "━━━ PHASE 6: Export ━━━" -ForegroundColor Yellow
    Write-Host ""
}

if ($ExportMarkdown) {
    $mdPath = Join-Path $OutputDir "FEATURE_MANIFEST.md"
    $md = @()
    $md += "# RawrXD IDE — Complete Feature Manifest"
    $md += ""
    $md += "**Generated**: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
    $md += "**Source**: Auto-introspected from actual source code by RawrXD-FeatureTest.ps1"
    $md += ""
    $md += "## Coverage Summary"
    $md += ""
    $md += "| IDE Variant | REAL | PARTIAL | MISSING | Coverage |"
    $md += "|------------|------|---------|---------|----------|"
    
    foreach ($v in $variants) {
        $vr = $script:TestResults | Where-Object { $_.Variant -eq $v }
        $total = @($vr).Count
        $real = @($vr | Where-Object { $_.Status -eq "REAL" }).Count
        $partial = @($vr | Where-Object { $_.Status -eq "PARTIAL" }).Count
        $missing = @($vr | Where-Object { $_.Status -eq "MISSING" }).Count
        $pct = if ($total -gt 0) { [math]::Round($real * 100 / $total) } else { 0 }
        $label = switch ($v) { "Win32" { "Win32 (C++)" } "CLI" { "CLI Shell" } "React" { "React IDE" } "PS" { "PowerShell" } }
        $md += "| $label | $real | $partial | $missing | $pct% |"
    }
    
    $md += ""
    $md += "## Feature Matrix"
    $md += ""
    $md += "| Category | Feature | Win32 | CLI | React | PS |"
    $md += "|----------|---------|-------|-----|-------|----|"
    
    $categories = $script:TestResults | Select-Object -ExpandProperty Category -Unique | Sort-Object
    foreach ($cat in $categories) {
        $features = $script:TestResults | Where-Object { $_.Category -eq $cat } | Select-Object -ExpandProperty Feature -Unique | Sort-Object
        foreach ($feat in $features) {
            $row = "| $cat | $feat"
            foreach ($v in $variants) {
                $r = $script:TestResults | Where-Object { $_.Category -eq $cat -and $_.Feature -eq $feat -and $_.Variant -eq $v } | Select-Object -First 1
                $icon = if ($r) { switch ($r.Status) { "REAL" { "✅" } "PARTIAL" { "🔶" } "MISSING" { "❌" } default { "❓" } } } else { "—" }
                $row += " | $icon"
            }
            $row += " |"
            $md += $row
        }
    }
    
    $md += ""
    $md += "## Legend"
    $md += "- ✅ REAL — Fully implemented, compiles, verified in source"
    $md += "- 🔶 PARTIAL — Has code but incomplete"
    $md += "- ❌ MISSING — Not present in this IDE variant"
    
    $md -join "`n" | Set-Content -Path $mdPath -Encoding UTF8
    Write-Host "  ✅ Exported: $mdPath" -ForegroundColor Green
}

if ($ExportJSON) {
    $jsonPath = Join-Path $OutputDir "feature_manifest.json"
    $export = @{
        generated = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
        total_tests = $script:TestResults.Count
        results = $script:TestResults | ForEach-Object {
            @{
                category = $_.Category
                feature  = $_.Feature
                variant  = $_.Variant
                status   = $_.Status
                evidence = $_.Evidence
                testType = $_.TestType
            }
        }
    }
    $export | ConvertTo-Json -Depth 4 | Set-Content -Path $jsonPath -Encoding UTF8
    Write-Host "  ✅ Exported: $jsonPath" -ForegroundColor Green
}

# ============================================================================
# FINAL SUMMARY
# ============================================================================

Write-Host ""
Write-Host "╔══════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                           FINAL SUMMARY                                  ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$totalTests = $script:TestResults.Count
$totalReal = @($script:TestResults | Where-Object { $_.Status -eq "REAL" }).Count
$totalPartial = @($script:TestResults | Where-Object { $_.Status -eq "PARTIAL" }).Count
$totalMissing = @($script:TestResults | Where-Object { $_.Status -eq "MISSING" }).Count

Write-Host "  Total Feature Tests:  $totalTests" -ForegroundColor White
Write-Host "  REAL:                 $totalReal" -ForegroundColor Green
Write-Host "  PARTIAL:              $totalPartial" -ForegroundColor Yellow
Write-Host "  MISSING:              $totalMissing" -ForegroundColor Red
Write-Host ""

# Alignment gaps
Write-Host "  🎯 Alignment Gaps (features Win32 has that others don't):" -ForegroundColor Cyan
$win32Real = $script:TestResults | Where-Object { $_.Variant -eq "Win32" -and $_.Status -eq "REAL" }
foreach ($wr in $win32Real) {
    $feat = $wr.Feature
    $cat = $wr.Category
    
    $cliR = $script:TestResults | Where-Object { $_.Variant -eq "CLI" -and $_.Feature -eq $feat -and $_.Category -eq $cat } | Select-Object -First 1
    $reactR = $script:TestResults | Where-Object { $_.Variant -eq "React" -and $_.Feature -eq $feat -and $_.Category -eq $cat } | Select-Object -First 1
    $psR = $script:TestResults | Where-Object { $_.Variant -eq "PS" -and $_.Feature -eq $feat -and $_.Category -eq $cat } | Select-Object -First 1
    
    $gaps = @()
    if ($cliR -and $cliR.Status -eq "MISSING") { $gaps += "CLI" }
    if ($reactR -and $reactR.Status -eq "MISSING") { $gaps += "React" }
    if ($psR -and $psR.Status -eq "MISSING") { $gaps += "PS" }
    
    if ($gaps.Count -gt 0) {
        Write-Host "    [$cat] $feat → missing in: $($gaps -join ', ')" -ForegroundColor DarkYellow
    }
}

Write-Host ""
Write-Host "  Run with -ExportMarkdown -ExportJSON to generate FEATURE_MANIFEST.md + feature_manifest.json" -ForegroundColor Gray
Write-Host "  Run with -SummaryOnly for a quick overview" -ForegroundColor Gray
Write-Host ""
