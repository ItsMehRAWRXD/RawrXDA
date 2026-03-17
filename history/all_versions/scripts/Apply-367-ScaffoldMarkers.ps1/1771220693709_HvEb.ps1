#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Apply all 367 scaffold markers to their corresponding source files.
.DESCRIPTION
    Parses SCAFFOLD_MARKERS.md and adds // SCAFFOLD_NNN comments to the appropriate
    files based on the File/Area column. Markers are inserted at logical points
    (class declarations, function definitions, namespace blocks) without modifying
    existing logic.
.PARAMETER DryRun
    Show what would be applied without actually modifying files.
.PARAMETER Force
    Overwrite existing markers with updated references.
#>
param(
    [switch]$DryRun,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot
$markersFile = Join-Path $repoRoot "SCAFFOLD_MARKERS.md"
$srcRoot = Join-Path $repoRoot "src"

if (-not (Test-Path $markersFile)) {
    Write-Error "SCAFFOLD_MARKERS.md not found at $markersFile"
    exit 1
}

Write-Host "=== RawrXD 367 Scaffold Marker Application ===" -ForegroundColor Cyan
Write-Host "Parsing marker registry: $markersFile" -ForegroundColor Gray

# Parse the markers table
$markers = @()
$content = Get-Content $markersFile -Raw
$lines = $content -split "`n"

foreach ($line in $lines) {
    if ($line -match '^\|\s*SCAFFOLD_(\d{3})\s*\|\s*([^|]+)\s*\|\s*([^|]+)\s*\|\s*([^|]+)\s*\|\s*([^|]+)\s*\|') {
        $markerNum = $matches[1]
        $category = $matches[2].Trim()
        $description = $matches[3].Trim()
        $fileArea = $matches[4].Trim()
        $status = $matches[5].Trim()
        
        $markers += [PSCustomObject]@{
            ID = "SCAFFOLD_$markerNum"
            Number = [int]$markerNum
            Category = $category
            Description = $description
            FileArea = $fileArea
            Status = $status
        }
    }
}

Write-Host "Found $($markers.Count) markers" -ForegroundColor Green

if ($markers.Count -ne 367) {
    Write-Warning "Expected 367 markers but found $($markers.Count)"
}

# File area to actual file mapping patterns
$filePatterns = @{
    'Win32IDE' = @('Win32IDE.cpp', 'Win32IDE.h')
    'Win32IDE_VSCodeUI' = @('Win32IDE_VSCodeUI.cpp', 'Win32IDE_VSCodeUI.h')
    'Win32IDE_Core' = @('Win32IDE.cpp', 'Win32IDE.h')
    'Win32IDE_AgentCommands' = @('Win32IDE_AgentCommands.cpp', 'Win32IDE_AgentCommands.h')
    'Win32IDE_PlanExecutor' = @('Win32IDE_PlanExecutor.cpp', 'Win32IDE_PlanExecutor.h')
    'Win32IDE_AgenticBridge' = @('Win32IDE_AgenticBridge.cpp', 'Win32IDE_AgenticBridge.h')
    'Win32IDE_Autonomy' = @('Win32IDE_Autonomy.cpp', 'Win32IDE_Autonomy.h')
    'Win32IDE_Tier2Cosmetics' = @('Win32IDE_Tier2Cosmetics.cpp', 'Win32IDE_Tier2Cosmetics.h')
    'Win32IDE_Themes' = @('Win32IDE_Themes.cpp', 'Win32IDE_Themes.h')
    'Win32IDE_SyntaxHighlight' = @('Win32IDE_SyntaxHighlight.cpp', 'Win32IDE_SyntaxHighlight.h')
    'Win32IDE_Breadcrumbs' = @('Win32IDE_Breadcrumbs.cpp', 'Win32IDE_Breadcrumbs.h')
    'Win32IDE_VoiceChat' = @('Win32IDE_VoiceChat.cpp', 'Win32IDE_VoiceChat.h')
    'Win32IDE_VoiceAutomation' = @('Win32IDE_VoiceAutomation.cpp', 'Win32IDE_VoiceAutomation.h')
    'Win32IDE_TelemetryDashboard' = @('Win32IDE_TelemetryDashboard.cpp', 'Win32IDE_TelemetryDashboard.h')
    'Win32IDE_TestExplorerTree' = @('Win32IDE_TestExplorerTree.cpp', 'Win32IDE_TestExplorerTree.h')
    'Win32IDE_FileOps' = @('Win32IDE_FileOps.cpp', 'Win32IDE_FileOps.h')
    'Win32IDE_FailureDetector' = @('Win32IDE_FailureDetector.cpp', 'Win32IDE_FailureDetector.h')
    'Win32IDE_FailureIntelligence' = @('Win32IDE_FailureIntelligence.cpp', 'Win32IDE_FailureIntelligence.h')
    'Win32IDE_CursorParity' = @('Win32IDE_CursorParity.cpp', 'Win32IDE_CursorParity.h')
    'Win32IDE_AgentHistory' = @('Win32IDE_AgentHistory.cpp', 'Win32IDE_AgentHistory.h')
    'Win32IDE_WebView2' = @('Win32IDE_WebView2.cpp', 'Win32IDE_WebView2.h')
    'Win32IDE_GUILayoutHotpatch' = @('Win32IDE_GUILayoutHotpatch.cpp', 'Win32IDE_GUILayoutHotpatch.h')
    'Win32IDE_GameEnginePanel' = @('Win32IDE_GameEnginePanel.cpp', 'Win32IDE_GameEnginePanel.h')
    'Win32IDE_CopilotGapPanel' = @('Win32IDE_CopilotGapPanel.cpp', 'Win32IDE_CopilotGapPanel.h')
    'LSPClient.hpp' = @('LSPClient.hpp', 'LSPClient.cpp')
    'MCPServer.hpp' = @('MCPServer.hpp', 'MCPServer.cpp')
    'ExtensionLoader' = @('ExtensionLoader.cpp', 'ExtensionLoader.h')
    'win32_plugin_loader' = @('win32_plugin_loader.cpp', 'win32_plugin_loader.h')
    'BoundedAgentLoop' = @('BoundedAgentLoop.cpp', 'BoundedAgentLoop.h')
    'AgentOrchestrator' = @('AgentOrchestrator.cpp', 'AgentOrchestrator.h')
    'OrchestratorBridge' = @('OrchestratorBridge.cpp', 'OrchestratorBridge.h')
    'agentic_composer_ux' = @('agentic_composer_ux.cpp', 'agentic_composer_ux.h')
    'OllamaProvider' = @('OllamaProvider.cpp', 'OllamaProvider.h')
    'AgentOllamaClient' = @('AgentOllamaClient.cpp', 'AgentOllamaClient.h')
    'agentic_executor' = @('agentic_executor.cpp', 'agentic_executor.h')
    'agentic_failure_detector' = @('agentic_failure_detector.cpp', 'agentic_failure_detector.h')
    'agentic_puppeteer' = @('agentic_puppeteer.cpp', 'agentic_puppeteer.h')
    'agentic_hotpatch_orchestrator' = @('agentic_hotpatch_orchestrator.cpp', 'agentic_hotpatch_orchestrator.h')
    'DeterministicReplayEngine' = @('DeterministicReplayEngine.cpp', 'DeterministicReplayEngine.h')
    'agentic_task_graph' = @('agentic_task_graph.cpp', 'agentic_task_graph.h')
    'autonomous_workflow_engine' = @('autonomous_workflow_engine.cpp', 'autonomous_workflow_engine.h')
    'swarm_decision_bridge' = @('swarm_decision_bridge.cpp', 'swarm_decision_bridge.h')
    'agentic_decision_tree' = @('agentic_decision_tree.cpp', 'agentic_decision_tree.h')
    'ToolExecutionEngine' = @('ToolExecutionEngine.cpp', 'ToolExecutionEngine.h')
    'ToolImplementations' = @('ToolImplementations.cpp', 'ToolImplementations.h')
    'gguf_loader' = @('gguf_loader.cpp', 'gguf_loader.h')
    'streaming_gguf_loader' = @('streaming_gguf_loader.cpp', 'streaming_gguf_loader.h')
    'model_source_resolver' = @('model_source_resolver.cpp', 'model_source_resolver.h')
    'RawrXD_InferenceEngine' = @('RawrXD_InferenceEngine.cpp', 'RawrXD_InferenceEngine.h')
    'vulkan_compute' = @('vulkan_compute.cpp', 'vulkan_compute.h')
    'inference_engine' = @('inference_engine.cpp', 'inference_engine.h')
    'bpe_tokenizer' = @('bpe_tokenizer.cpp', 'bpe_tokenizer.h')
    'sentencepiece_tokenizer' = @('sentencepiece_tokenizer.cpp', 'sentencepiece_tokenizer.h')
    'vocabulary_loader' = @('vocabulary_loader.cpp', 'vocabulary_loader.h')
    'inference' = @('inference.cpp', 'inference.h')
    'multi_gpu_manager' = @('multi_gpu_manager.cpp', 'multi_gpu_manager.h')
    'cuda_inference_engine' = @('cuda_inference_engine.cpp', 'cuda_inference_engine.h')
    'RawrXD_ModelLoader' = @('RawrXD_ModelLoader.cpp', 'RawrXD_ModelLoader.h')
    'GGUFRunner' = @('GGUFRunner.cpp', 'GGUFRunner.h')
    'chat_panel_integration' = @('chat_panel_integration.cpp', 'chat_panel_integration.h')
    'FIMPromptBuilder' = @('FIMPromptBuilder.cpp', 'FIMPromptBuilder.h')
    'ultra_fast_inference' = @('ultra_fast_inference.cpp', 'ultra_fast_inference.h')
    'inference_state_machine' = @('inference_state_machine.cpp', 'inference_state_machine.h')
    'vision_embedding_cache' = @('vision_embedding_cache.cpp', 'vision_embedding_cache.h')
    'vision_kv_isolation' = @('vision_kv_isolation.cpp', 'vision_kv_isolation.h')
    'speculative_decoder' = @('speculative_decoder.cpp', 'speculative_decoder.h')
    'kv_cache_optimizer' = @('kv_cache_optimizer.cpp', 'kv_cache_optimizer.h')
    'engine_manager' = @('engine_manager.cpp', 'engine_manager.h')
    'codex_ultimate' = @('codex_ultimate.cpp', 'codex_ultimate.h')
    'crucible_engine' = @('crucible_engine.cpp', 'crucible_engine.h')
    'Ship' = @('Ship/**/*.cpp', 'Ship/**/*.h', 'Ship/**/*.asm')
    'HeadlessIDE' = @('HeadlessIDE.cpp', 'HeadlessIDE.h')
    'compression' = @('compression.cpp', 'compression.h')
    'bench_deflate' = @('bench_deflate.cpp')
    'asm' = @('**/*.asm')
    'custom_zlib.asm' = @('custom_zlib.asm')
    'inference_core.asm' = @('inference_core.asm')
    'feature_dispatch_bridge.asm' = @('feature_dispatch_bridge.asm')
    'pyre_compute' = @('pyre_compute.cpp', 'pyre_compute.h')
    'agent_tool_quantize' = @('agent_tool_quantize.cpp', 'agent_tool_quantize.h')
    'ModelLoader' = @('ModelLoader.cpp', 'ModelLoader.h')
    'RawrXD-ModelLoader' = @('RawrXD-ModelLoader/**/*')
    'LSPClient' = @('LSPClient.cpp', 'LSPClient.hpp')
    'LSPServer' = @('LSPServer.cpp', 'LSPServer.h')
    'diagnostic_consumer' = @('diagnostic_consumer.cpp', 'diagnostic_consumer.h')
    'lsp_hotpatch_bridge' = @('lsp_hotpatch_bridge.cpp', 'lsp_hotpatch_bridge.h')
    'IocpFileWatcher' = @('IocpFileWatcher.cpp', 'IocpFileWatcher.h')
    'rawrxd_cli' = @('rawrxd_cli.cpp', 'rawrxd_cli.h', 'RawrXD_CLI.cpp', 'RawrXD_CLI.h')
}

function Find-SourceFile {
    param([string]$FileArea)
    
    # Try exact match first
    if ($filePatterns.ContainsKey($FileArea)) {
        $patterns = $filePatterns[$FileArea]
        foreach ($pattern in $patterns) {
            $path = Join-Path $srcRoot $pattern
            if (Test-Path $path -PathType Leaf) {
                return $path
            }
            # Try wildcard search
            $found = Get-ChildItem -Path $srcRoot -Recurse -Filter (Split-Path -Leaf $pattern) -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($found) {
                return $found.FullName
            }
        }
    }
    
    # Fallback: search by simple name match
    $simpleName = $FileArea -replace '_', '' -replace 'RawrXD', '' -replace 'Win32IDE', ''
    $found = Get-ChildItem -Path $srcRoot -Recurse -Filter "*$FileArea*" -ErrorAction SilentlyContinue | 
             Where-Object { $_.Extension -match '\.(cpp|h|hpp|asm)$' } |
             Select-Object -First 1
    
    if ($found) {
        return $found.FullName
    }
    
    return $null
}

function Add-MarkerToFile {
    param(
        [string]$FilePath,
        [PSCustomObject]$Marker,
        [bool]$IsDryRun
    )
    
    if (-not (Test-Path $FilePath)) {
        Write-Warning "File not found: $FilePath (for $($Marker.ID))"
        return $false
    }
    
    $content = Get-Content $FilePath -Raw
    
    # Check if marker already exists
    if ($content -match "//\s*$($Marker.ID)") {
        if (-not $Force) {
            Write-Verbose "$($Marker.ID) already present in $FilePath"
            return $false
        }
    }
    
    # Determine insertion point based on file type and content
    $markerComment = "// $($Marker.ID): $($Marker.Description)"
    
    # For header files, insert after include guards or at top
    if ($FilePath -match '\.(h|hpp)$') {
        if ($content -match '(?m)^#ifndef\s+\w+\s*$\s*^#define\s+\w+\s*$') {
            $insertPos = $matches[0].Length + $matches.Index
            $before = $content.Substring(0, $insertPos)
            $after = $content.Substring($insertPos)
            $newContent = $before + "`n`n$markerComment`n" + $after
        } else {
            $newContent = "$markerComment`n`n" + $content
        }
    }
    # For .cpp files, insert after includes or at top
    elseif ($FilePath -match '\.cpp$') {
        if ($content -match '(?m)^#include\s+.+$') {
            $lastIncludeEnd = $matches.Index + $matches[0].Length
            $before = $content.Substring(0, $lastIncludeEnd)
            $after = $content.Substring($lastIncludeEnd)
            $newContent = $before + "`n`n$markerComment`n" + $after
        } else {
            $newContent = "$markerComment`n`n" + $content
        }
    }
    # For .asm files
    elseif ($FilePath -match '\.asm$') {
        $markerComment = "; $($Marker.ID): $($Marker.Description)"
        $newContent = "$markerComment`n`n" + $content
    }
    else {
        $newContent = "$markerComment`n`n" + $content
    }
    
    if ($IsDryRun) {
        Write-Host "  [DRY-RUN] Would add $($Marker.ID) to $FilePath" -ForegroundColor Yellow
        return $true
    }
    
    Set-Content -Path $FilePath -Value $newContent -NoNewline
    Write-Host "  ✓ Added $($Marker.ID) to $FilePath" -ForegroundColor Green
    return $true
}

# Group markers by file area
$markersByFile = $markers | Group-Object -Property FileArea

Write-Host "`n=== Applying markers to source files ===" -ForegroundColor Cyan
$applied = 0
$notFound = 0
$skipped = 0

foreach ($group in $markersByFile) {
    $fileArea = $group.Name
    $groupMarkers = $group.Group
    
    Write-Host "`nProcessing $($groupMarkers.Count) marker(s) for [$fileArea]..." -ForegroundColor Gray
    
    $targetFile = Find-SourceFile -FileArea $fileArea
    
    if (-not $targetFile) {
        Write-Warning "No source file found for [$fileArea]"
        $notFound += $groupMarkers.Count
        continue
    }
    
    Write-Host "  Target: $targetFile" -ForegroundColor DarkGray
    
    foreach ($marker in $groupMarkers) {
        if (Add-MarkerToFile -FilePath $targetFile -Marker $marker -IsDryRun:$DryRun) {
            $applied++
        } else {
            $skipped++
        }
    }
}

Write-Host "`n=== Summary ===" -ForegroundColor Cyan
Write-Host "Total markers: $($markers.Count)" -ForegroundColor White
Write-Host "Applied: $applied" -ForegroundColor Green
Write-Host "Skipped (already present): $skipped" -ForegroundColor Yellow
Write-Host "Not found: $notFound" -ForegroundColor Red

if ($DryRun) {
    Write-Host "`n[DRY-RUN MODE] No files were modified." -ForegroundColor Yellow
    Write-Host "Run with -Force to overwrite existing markers, or without -DryRun to apply." -ForegroundColor Yellow
}

exit 0
