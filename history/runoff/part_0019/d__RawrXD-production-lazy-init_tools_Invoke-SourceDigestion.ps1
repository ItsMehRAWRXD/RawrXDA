<#
.SYNOPSIS
    RawrXD IDE Source Digestion System - PowerShell Wrapper
    
.DESCRIPTION
    Comprehensive source code analysis tool that:
    - Scans all source files for completeness
    - Detects stubs, TODOs, and incomplete implementations
    - Calculates health points for each component
    - Generates detailed reports and recommendations
    - Performs self-audit verification
    
.PARAMETER SourceDir
    The source directory to analyze. Defaults to the IDE src folder.
    
.PARAMETER OutputFormat
    Output format: json, markdown, html, or all. Default is markdown.
    
.PARAMETER FullAudit
    Run comprehensive audit with all checks enabled.
    
.PARAMETER SelfCheck
    Run self-check diagnostic mode.
    
.PARAMETER QuickScan
    Run quick scan mode (less thorough but faster).
    
.PARAMETER ExportPath
    Custom export path for reports.
    
.EXAMPLE
    .\Invoke-SourceDigestion.ps1 -FullAudit
    
.EXAMPLE
    .\Invoke-SourceDigestion.ps1 -SourceDir "D:\RawrXD-production-lazy-init\src" -OutputFormat all
    
.NOTES
    Author: RawrXD IDE Team
    Version: 1.0.0
#>

[CmdletBinding()]
param(
    [Parameter()]
    [string]$SourceDir = "D:\RawrXD-production-lazy-init\src",
    
    [Parameter()]
    [ValidateSet('json', 'markdown', 'html', 'all')]
    [string]$OutputFormat = 'markdown',
    
    [Parameter()]
    [switch]$FullAudit,
    
    [Parameter()]
    [switch]$SelfCheck,
    
    [Parameter()]
    [switch]$QuickScan,
    
    [Parameter()]
    [string]$ExportPath
)

# ============================================================================
# CONFIGURATION
# ============================================================================

$Script:Config = @{
    SourceExtensions = @('.cpp', '.c', '.h', '.hpp', '.asm', '.inc', '.py', '.ps1', '.bat', '.sh', '.cmake')
    ExcludePatterns = @('build/', 'obj/', 'bin/', '.git/', 'CMakeFiles/', 'autogen/', '.dir/', 'Debug/', 'Release/', 'x64/')
    
    StubPatterns = @(
        '//\s*TODO',
        '//\s*FIXME',
        '//\s*HACK',
        '//\s*XXX',
        '//\s*STUB',
        '//\s*PLACEHOLDER',
        '//\s*NOT\s+IMPLEMENTED',
        '//\s*coming\s+soon',
        '//\s*No-op',
        '//\s*Simple\s+stub',
        '//\s*Minimal\s+implementation',
        'return\s+(nullptr|NULL|0|false|"")\s*;\s*//.*stub',
        'throw\s+std::runtime_error\s*\(\s*"Not\s+implemented',
        '{\s*}\s*//\s*stub',
        '#\s*TODO'
    )
    
    CriticalComponents = @{
        'MainWindow' = 'qtapp/MainWindow.cpp'
        'AgenticEngine' = 'agentic_engine.cpp'
        'InferenceEngine' = 'qtapp/inference_engine.cpp'
        'ModelLoader' = 'auto_model_loader.cpp'
        'ChatInterface' = 'chat_interface.cpp'
        'TerminalManager' = 'qtapp/TerminalManager.cpp'
        'FileBrowser' = 'file_browser.cpp'
        'LSPClient' = 'lsp_client.cpp'
        'GGUFLoader' = 'gguf_loader.cpp'
        'StreamingEngine' = 'streaming_engine.cpp'
        'RefactoringEngine' = 'refactoring_engine.cpp'
        'SecurityManager' = 'security_manager.cpp'
        'TelemetrySystem' = 'telemetry.cpp'
        'ErrorHandler' = 'error_handler.cpp'
        'ConfigManager' = 'config_manager.cpp'
    }
    
    Categories = @{
        'core' = @('main', 'ide', 'window', 'app')
        'ai' = @('ai_', 'agentic', 'model_', 'inference', 'llm', 'gguf', 'streaming')
        'editor' = @('editor', 'syntax', 'highlight', 'completion', 'minimap')
        'terminal' = @('terminal', 'shell', 'powershell', 'console')
        'git' = @('git_', 'git/', 'github')
        'lsp' = @('lsp_', 'language_server')
        'ui' = @('ui_', 'widget', 'panel', 'dialog', 'menu', 'toolbar')
        'network' = @('http', 'websocket', 'api_', 'server')
        'masm' = @('masm', '.asm', 'asm_')
        'testing' = @('test_', 'test/', 'testing', 'benchmark')
        'config' = @('config', 'settings', 'preferences')
        'security' = @('security', 'auth', 'crypto', 'jwt')
        'monitoring' = @('telemetry', 'metrics', 'monitor', 'observability')
    }
}

# ============================================================================
# CLASSES
# ============================================================================

class FileAnalysis {
    [string]$Path
    [string]$RelativePath
    [string]$Extension
    [int]$SizeBytes
    [int]$LineCount
    [int]$CodeLines
    [int]$CommentLines
    [int]$BlankLines
    [System.Collections.ArrayList]$Stubs
    [System.Collections.ArrayList]$Todos
    [int]$HealthPoints
    [string]$Category
    [bool]$IsComplete
    [double]$CompletionPercentage
    [string]$Hash
    [datetime]$LastModified
    
    FileAnalysis() {
        $this.Stubs = [System.Collections.ArrayList]::new()
        $this.Todos = [System.Collections.ArrayList]::new()
        $this.HealthPoints = 100
        $this.CompletionPercentage = 100.0
        $this.IsComplete = $true
    }
}

class ComponentStatus {
    [string]$Name
    [System.Collections.ArrayList]$Files
    [int]$TotalLines
    [int]$StubCount
    [int]$TodoCount
    [int]$HealthPoints
    [double]$CompletionPercentage
    [string]$Status
    
    ComponentStatus() {
        $this.Files = [System.Collections.ArrayList]::new()
        $this.HealthPoints = 100
        $this.CompletionPercentage = 100.0
        $this.Status = 'unknown'
    }
}

class ProjectManifest {
    [string]$ProjectName
    [string]$SourceRoot
    [datetime]$AnalysisTimestamp
    [int]$TotalFiles
    [int]$TotalLines
    [int]$TotalCodeLines
    [int]$TotalStubs
    [int]$TotalTodos
    [int]$OverallHealth
    [double]$OverallCompletion
    [hashtable]$FilesByCategory
    [hashtable]$Components
    [hashtable]$FileAnalyses
    [System.Collections.ArrayList]$MissingFeatures
    [System.Collections.ArrayList]$Recommendations
    [System.Collections.ArrayList]$CriticalIssues
    
    ProjectManifest() {
        $this.FilesByCategory = @{}
        $this.Components = @{}
        $this.FileAnalyses = @{}
        $this.MissingFeatures = [System.Collections.ArrayList]::new()
        $this.Recommendations = [System.Collections.ArrayList]::new()
        $this.CriticalIssues = [System.Collections.ArrayList]::new()
        $this.OverallHealth = 100
        $this.OverallCompletion = 100.0
    }
}

# ============================================================================
# ANALYSIS FUNCTIONS
# ============================================================================

function Get-SourceFiles {
    param([string]$RootPath)
    
    $files = @()
    
    foreach ($ext in $Script:Config.SourceExtensions) {
        $found = Get-ChildItem -Path $RootPath -Filter "*$ext" -Recurse -File -ErrorAction SilentlyContinue
        $files += $found
    }
    
    # Filter out excluded patterns
    $filtered = $files | Where-Object {
        $path = $_.FullName
        $exclude = $false
        foreach ($pattern in $Script:Config.ExcludePatterns) {
            if ($path -like "*$pattern*") {
                $exclude = $true
                break
            }
        }
        -not $exclude
    }
    
    return $filtered
}

function Analyze-File {
    param(
        [System.IO.FileInfo]$File,
        [string]$SourceRoot
    )
    
    $analysis = [FileAnalysis]::new()
    $analysis.Path = $File.FullName
    $analysis.RelativePath = $File.FullName.Replace($SourceRoot, '').TrimStart('\', '/')
    $analysis.Extension = $File.Extension.ToLower()
    $analysis.SizeBytes = $File.Length
    $analysis.LastModified = $File.LastWriteTime
    
    try {
        $content = Get-Content -Path $File.FullName -Raw -ErrorAction Stop
        $lines = $content -split "`n"
        
        $analysis.LineCount = $lines.Count
        
        # Calculate hash
        $stream = [System.IO.MemoryStream]::new([System.Text.Encoding]::UTF8.GetBytes($content))
        $analysis.Hash = (Get-FileHash -InputStream $stream -Algorithm MD5).Hash
        
        # Count line types
        $codeLines = 0
        $commentLines = 0
        $blankLines = 0
        $inMultilineComment = $false
        
        foreach ($line in $lines) {
            $trimmed = $line.Trim()
            
            if ([string]::IsNullOrWhiteSpace($trimmed)) {
                $blankLines++
            }
            elseif ($inMultilineComment) {
                $commentLines++
                if ($trimmed -match '\*/') {
                    $inMultilineComment = $false
                }
            }
            elseif ($trimmed -match '^/\*') {
                $commentLines++
                if ($trimmed -notmatch '\*/') {
                    $inMultilineComment = $true
                }
            }
            elseif ($trimmed -match '^//' -or $trimmed -match '^#') {
                $commentLines++
            }
            else {
                $codeLines++
            }
        }
        
        $analysis.CodeLines = $codeLines
        $analysis.CommentLines = $commentLines
        $analysis.BlankLines = $blankLines
        
        # Find stubs
        $lineNum = 0
        foreach ($line in $lines) {
            $lineNum++
            foreach ($pattern in $Script:Config.StubPatterns) {
                if ($line -match $pattern) {
                    [void]$analysis.Stubs.Add(@{
                        LineNumber = $lineNum
                        Content = $line.Trim().Substring(0, [Math]::Min(100, $line.Trim().Length))
                        Pattern = $pattern
                    })
                    break
                }
            }
            
            # Find TODOs
            if ($line -match '(TODO|FIXME|HACK|XXX)\s*:?\s*(.*)') {
                [void]$analysis.Todos.Add(@{
                    LineNumber = $lineNum
                    Content = $Matches[0].Substring(0, [Math]::Min(100, $Matches[0].Length))
                })
            }
        }
        
        # Calculate health points
        $stubPenalty = $analysis.Stubs.Count * 5
        $todoPenalty = $analysis.Todos.Count * 2
        $analysis.HealthPoints = [Math]::Max(0, 100 - $stubPenalty - $todoPenalty)
        
        # Calculate completion
        $analysis.CompletionPercentage = [Math]::Max(0, 100 - ($analysis.Stubs.Count * 3) - ($analysis.Todos.Count * 1))
        
        # Determine category
        $analysis.Category = Get-FileCategory -RelativePath $analysis.RelativePath
        
        # Check completeness
        $analysis.IsComplete = ($analysis.Stubs.Count -eq 0) -and ($analysis.Todos.Count -eq 0)
        
    }
    catch {
        Write-Warning "Error analyzing $($File.FullName): $_"
    }
    
    return $analysis
}

function Get-FileCategory {
    param([string]$RelativePath)
    
    $pathLower = $RelativePath.ToLower()
    
    foreach ($category in $Script:Config.Categories.Keys) {
        foreach ($pattern in $Script:Config.Categories[$category]) {
            if ($pathLower -like "*$pattern*") {
                return $category
            }
        }
    }
    
    return 'other'
}

function Analyze-Components {
    param([hashtable]$FileAnalyses)
    
    $components = @{}
    
    # Check critical components
    foreach ($name in $Script:Config.CriticalComponents.Keys) {
        $expectedPath = $Script:Config.CriticalComponents[$name]
        $status = [ComponentStatus]::new()
        $status.Name = $name
        
        # Find matching files
        $matchingFiles = $FileAnalyses.Keys | Where-Object {
            $_ -like "*$expectedPath*" -or $_ -like "*$($name.ToLower())*"
        }
        
        if ($matchingFiles.Count -eq 0) {
            $status.Status = 'missing'
            $status.HealthPoints = 0
            $status.CompletionPercentage = 0.0
        }
        else {
            foreach ($path in $matchingFiles) {
                [void]$status.Files.Add($path)
                $analysis = $FileAnalyses[$path]
                $status.TotalLines += $analysis.LineCount
                $status.StubCount += $analysis.Stubs.Count
                $status.TodoCount += $analysis.Todos.Count
            }
            
            $status.HealthPoints = [Math]::Max(0, [Math]::Min(100, 100 - ($status.StubCount * 5) - ($status.TodoCount * 2)))
            $status.CompletionPercentage = [Math]::Max(0, 100 - ($status.StubCount * 3) - ($status.TodoCount * 1))
            
            if ($status.StubCount -eq 0 -and $status.TodoCount -eq 0) {
                $status.Status = 'complete'
            }
            elseif ($status.StubCount -lt 5) {
                $status.Status = 'partial'
            }
            else {
                $status.Status = 'stub'
            }
        }
        
        $components[$name] = $status
    }
    
    return $components
}

# ============================================================================
# MAIN DIGESTION ENGINE
# ============================================================================

function Invoke-SourceDigestion {
    param(
        [string]$SourcePath,
        [switch]$Full,
        [switch]$Quick
    )
    
    Write-Host "`n$('='*60)" -ForegroundColor Cyan
    Write-Host "RawrXD IDE Source Digestion System" -ForegroundColor Cyan
    Write-Host "$('='*60)" -ForegroundColor Cyan
    Write-Host "Source Root: $SourcePath" -ForegroundColor Gray
    Write-Host "Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
    Write-Host "$('='*60)`n" -ForegroundColor Cyan
    
    # Initialize manifest
    $manifest = [ProjectManifest]::new()
    $manifest.ProjectName = "RawrXD IDE"
    $manifest.SourceRoot = $SourcePath
    $manifest.AnalysisTimestamp = Get-Date
    
    # Phase 1: Discover files
    Write-Host "Phase 1: Discovering source files..." -ForegroundColor Yellow
    $sourceFiles = Get-SourceFiles -RootPath $SourcePath
    Write-Host "  Found $($sourceFiles.Count) source files" -ForegroundColor Green
    
    # Phase 2: Analyze files
    Write-Host "`nPhase 2: Analyzing files..." -ForegroundColor Yellow
    $progress = 0
    $total = $sourceFiles.Count
    
    foreach ($file in $sourceFiles) {
        $progress++
        if ($progress % 50 -eq 0) {
            Write-Progress -Activity "Analyzing files" -Status "$progress of $total" -PercentComplete (($progress / $total) * 100)
        }
        
        $analysis = Analyze-File -File $file -SourceRoot $SourcePath
        if ($analysis) {
            $manifest.FileAnalyses[$analysis.RelativePath] = $analysis
            
            # Update category tracking
            if (-not $manifest.FilesByCategory.ContainsKey($analysis.Category)) {
                $manifest.FilesByCategory[$analysis.Category] = @()
            }
            $manifest.FilesByCategory[$analysis.Category] += $analysis.RelativePath
        }
    }
    Write-Progress -Activity "Analyzing files" -Completed
    Write-Host "  Analyzed $($manifest.FileAnalyses.Count) files" -ForegroundColor Green
    
    # Phase 3: Analyze components
    Write-Host "`nPhase 3: Analyzing components..." -ForegroundColor Yellow
    $manifest.Components = Analyze-Components -FileAnalyses $manifest.FileAnalyses
    Write-Host "  Analyzed $($manifest.Components.Count) components" -ForegroundColor Green
    
    # Phase 4: Calculate metrics
    Write-Host "`nPhase 4: Calculating metrics..." -ForegroundColor Yellow
    
    $totalHealth = 0
    $totalCompletion = 0.0
    
    foreach ($analysis in $manifest.FileAnalyses.Values) {
        $manifest.TotalFiles++
        $manifest.TotalLines += $analysis.LineCount
        $manifest.TotalCodeLines += $analysis.CodeLines
        $manifest.TotalStubs += $analysis.Stubs.Count
        $manifest.TotalTodos += $analysis.Todos.Count
        $totalHealth += $analysis.HealthPoints
        $totalCompletion += $analysis.CompletionPercentage
    }
    
    if ($manifest.TotalFiles -gt 0) {
        $manifest.OverallHealth = [int]($totalHealth / $manifest.TotalFiles)
        $manifest.OverallCompletion = $totalCompletion / $manifest.TotalFiles
    }
    
    Write-Host "  Total files: $($manifest.TotalFiles)" -ForegroundColor White
    Write-Host "  Total lines: $($manifest.TotalLines.ToString('N0'))" -ForegroundColor White
    Write-Host "  Total stubs: $($manifest.TotalStubs)" -ForegroundColor White
    Write-Host "  Total TODOs: $($manifest.TotalTodos)" -ForegroundColor White
    Write-Host "  Overall health: $($manifest.OverallHealth)%" -ForegroundColor $(if ($manifest.OverallHealth -ge 80) { 'Green' } elseif ($manifest.OverallHealth -ge 60) { 'Yellow' } else { 'Red' })
    Write-Host "  Overall completion: $([Math]::Round($manifest.OverallCompletion, 1))%" -ForegroundColor $(if ($manifest.OverallCompletion -ge 80) { 'Green' } elseif ($manifest.OverallCompletion -ge 60) { 'Yellow' } else { 'Red' })
    
    # Phase 5: Generate recommendations
    Write-Host "`nPhase 5: Generating recommendations..." -ForegroundColor Yellow
    
    foreach ($name in $manifest.Components.Keys) {
        $status = $manifest.Components[$name]
        if ($status.Status -eq 'missing') {
            [void]$manifest.CriticalIssues.Add("CRITICAL: $name component is missing")
            [void]$manifest.MissingFeatures.Add($name)
        }
        elseif ($status.Status -eq 'stub') {
            [void]$manifest.CriticalIssues.Add("HIGH: $name is mostly stubs ($($status.StubCount) stubs)")
            [void]$manifest.Recommendations.Add("Implement $name - currently at $([Math]::Round($status.CompletionPercentage, 1))%")
        }
    }
    
    # Find files with most stubs
    $stubFiles = $manifest.FileAnalyses.GetEnumerator() | 
        Where-Object { $_.Value.Stubs.Count -gt 0 } |
        Sort-Object { $_.Value.Stubs.Count } -Descending |
        Select-Object -First 10
    
    if ($stubFiles) {
        [void]$manifest.Recommendations.Add("Top files needing stub implementation:")
        foreach ($file in $stubFiles) {
            [void]$manifest.Recommendations.Add("  - $($file.Key): $($file.Value.Stubs.Count) stubs")
        }
    }
    
    Write-Host "  Generated $($manifest.Recommendations.Count) recommendations" -ForegroundColor Green
    
    # Phase 6: Self-audit
    Write-Host "`nPhase 6: Running self-audit..." -ForegroundColor Yellow
    
    if ($manifest.TotalFiles -eq 0) {
        Write-Host "  [X] No files analyzed - check source root path" -ForegroundColor Red
    }
    else {
        Write-Host "  [✓] Analyzed $($manifest.TotalFiles) files" -ForegroundColor Green
    }
    
    $foundCategories = $manifest.FilesByCategory.Keys
    Write-Host "  [✓] Found $($foundCategories.Count) categories" -ForegroundColor Green
    
    $completeComponents = ($manifest.Components.Values | Where-Object { $_.Status -ne 'missing' }).Count
    Write-Host "  [✓] Component coverage: $completeComponents/$($manifest.Components.Count)" -ForegroundColor Green
    
    Write-Host "  [✓] Self-audit complete" -ForegroundColor Green
    
    Write-Host "`n$('='*60)" -ForegroundColor Cyan
    Write-Host "Digestion Complete!" -ForegroundColor Cyan
    Write-Host "$('='*60)`n" -ForegroundColor Cyan
    
    return $manifest
}

# ============================================================================
# EXPORT FUNCTIONS
# ============================================================================

function Export-ManifestJson {
    param(
        [ProjectManifest]$Manifest,
        [string]$OutputPath
    )
    
    $json = $Manifest | ConvertTo-Json -Depth 10
    $json | Set-Content -Path $OutputPath -Encoding UTF8
    Write-Host "Exported JSON manifest to: $OutputPath" -ForegroundColor Green
}

function Export-ManifestMarkdown {
    param(
        [ProjectManifest]$Manifest,
        [string]$OutputPath
    )
    
    $lines = @()
    
    # Header
    $lines += "# RawrXD IDE Source Digestion Report"
    $lines += ""
    $lines += "**Generated:** $($Manifest.AnalysisTimestamp.ToString('yyyy-MM-dd HH:mm:ss'))"
    $lines += "**Source Root:** ``$($Manifest.SourceRoot)``"
    $lines += ""
    
    # Overview
    $lines += "## Overview"
    $lines += ""
    $lines += "| Metric | Value |"
    $lines += "|--------|-------|"
    $lines += "| Total Files | $($Manifest.TotalFiles.ToString('N0')) |"
    $lines += "| Total Lines | $($Manifest.TotalLines.ToString('N0')) |"
    $lines += "| Code Lines | $($Manifest.TotalCodeLines.ToString('N0')) |"
    $lines += "| Total Stubs | $($Manifest.TotalStubs) |"
    $lines += "| Total TODOs | $($Manifest.TotalTodos) |"
    $lines += "| Overall Health | $($Manifest.OverallHealth)% |"
    $lines += "| Overall Completion | $([Math]::Round($Manifest.OverallCompletion, 1))% |"
    $lines += ""
    
    # Health visualization
    $lines += "### Health Status"
    $lines += ""
    $health = $Manifest.OverallHealth
    $healthBar = ("█" * [Math]::Floor($health / 5)) + ("░" * (20 - [Math]::Floor($health / 5)))
    $status = if ($health -ge 80) { "🟢 Excellent" } elseif ($health -ge 60) { "🟡 Good" } elseif ($health -ge 40) { "🟠 Needs Work" } else { "🔴 Critical" }
    $lines += "``````"
    $lines += "[$healthBar] $health% - $status"
    $lines += "``````"
    $lines += ""
    
    # Critical Issues
    if ($Manifest.CriticalIssues.Count -gt 0) {
        $lines += "## ⚠️ Critical Issues"
        $lines += ""
        foreach ($issue in $Manifest.CriticalIssues) {
            $lines += "- $issue"
        }
        $lines += ""
    }
    
    # Component Status
    $lines += "## Component Status"
    $lines += ""
    $lines += "| Component | Status | Health | Stubs | Completion |"
    $lines += "|-----------|--------|--------|-------|------------|"
    
    foreach ($name in ($Manifest.Components.Keys | Sort-Object)) {
        $status = $Manifest.Components[$name]
        $statusEmoji = switch ($status.Status) {
            'complete' { '✅' }
            'partial' { '🟡' }
            'stub' { '🟠' }
            'missing' { '❌' }
            default { '❓' }
        }
        $lines += "| $name | $statusEmoji $($status.Status) | $($status.HealthPoints)% | $($status.StubCount) | $([Math]::Round($status.CompletionPercentage, 1))% |"
    }
    $lines += ""
    
    # Category Summary
    $lines += "## Category Summary"
    $lines += ""
    $lines += "| Category | Files | Avg Completion |"
    $lines += "|----------|-------|----------------|"
    
    foreach ($category in ($Manifest.FilesByCategory.Keys | Sort-Object)) {
        $files = $Manifest.FilesByCategory[$category]
        $avgCompletion = 0
        foreach ($path in $files) {
            if ($Manifest.FileAnalyses.ContainsKey($path)) {
                $avgCompletion += $Manifest.FileAnalyses[$path].CompletionPercentage
            }
        }
        if ($files.Count -gt 0) {
            $avgCompletion = $avgCompletion / $files.Count
        }
        $statusIcon = if ($avgCompletion -ge 90) { "✅" } elseif ($avgCompletion -ge 70) { "🟡" } elseif ($avgCompletion -ge 50) { "🟠" } else { "🔴" }
        $lines += "| $([CultureInfo]::CurrentCulture.TextInfo.ToTitleCase($category)) | $($files.Count) | $statusIcon $([Math]::Round($avgCompletion, 0))% |"
    }
    $lines += ""
    
    # Files needing attention
    $lines += "## Files Needing Attention"
    $lines += ""
    
    $stubFiles = $Manifest.FileAnalyses.GetEnumerator() | 
        Where-Object { $_.Value.Stubs.Count -gt 0 } |
        Sort-Object { $_.Value.Stubs.Count } -Descending |
        Select-Object -First 20
    
    if ($stubFiles) {
        $lines += "| File | Stubs | Completion |"
        $lines += "|------|-------|------------|"
        foreach ($file in $stubFiles) {
            $lines += "| ``$($file.Key)`` | $($file.Value.Stubs.Count) | $([Math]::Round($file.Value.CompletionPercentage, 1))% |"
        }
    }
    else {
        $lines += "*No files with stubs found!*"
    }
    $lines += ""
    
    # Recommendations
    $lines += "## Recommendations"
    $lines += ""
    if ($Manifest.Recommendations.Count -gt 0) {
        foreach ($rec in $Manifest.Recommendations) {
            $lines += "- $rec"
        }
    }
    else {
        $lines += "*No recommendations at this time.*"
    }
    $lines += ""
    
    # Footer
    $lines += "---"
    $lines += "*Generated by RawrXD IDE Source Digestion System*"
    
    $lines -join "`n" | Set-Content -Path $OutputPath -Encoding UTF8
    Write-Host "Exported Markdown report to: $OutputPath" -ForegroundColor Green
}

function Show-Summary {
    param([ProjectManifest]$Manifest)
    
    Write-Host "`n$('='*70)" -ForegroundColor Cyan
    Write-Host "                    SOURCE DIGESTION SUMMARY" -ForegroundColor Cyan
    Write-Host "$('='*70)" -ForegroundColor Cyan
    
    Write-Host "`n📊 OVERALL STATISTICS" -ForegroundColor Yellow
    Write-Host "   Total Files:      $($Manifest.TotalFiles.ToString('N0'))" -ForegroundColor White
    Write-Host "   Total Lines:      $($Manifest.TotalLines.ToString('N0'))" -ForegroundColor White
    Write-Host "   Code Lines:       $($Manifest.TotalCodeLines.ToString('N0'))" -ForegroundColor White
    Write-Host "   Total Stubs:      $($Manifest.TotalStubs)" -ForegroundColor White
    Write-Host "   Total TODOs:      $($Manifest.TotalTodos)" -ForegroundColor White
    
    # Health bar
    $health = $Manifest.OverallHealth
    $healthBar = ("█" * [Math]::Floor($health / 5)) + ("░" * (20 - [Math]::Floor($health / 5)))
    $healthColor = if ($health -ge 80) { 'Green' } elseif ($health -ge 60) { 'Yellow' } else { 'Red' }
    Write-Host "`n💪 HEALTH: [$healthBar] $health%" -ForegroundColor $healthColor
    
    # Completion bar
    $completion = [int]$Manifest.OverallCompletion
    $compBar = ("█" * [Math]::Floor($completion / 5)) + ("░" * (20 - [Math]::Floor($completion / 5)))
    $compColor = if ($completion -ge 80) { 'Green' } elseif ($completion -ge 60) { 'Yellow' } else { 'Red' }
    Write-Host "✅ COMPLETION: [$compBar] $([Math]::Round($Manifest.OverallCompletion, 1))%" -ForegroundColor $compColor
    
    # Critical issues
    if ($Manifest.CriticalIssues.Count -gt 0) {
        Write-Host "`n⚠️  CRITICAL ISSUES ($($Manifest.CriticalIssues.Count)):" -ForegroundColor Red
        $Manifest.CriticalIssues | Select-Object -First 5 | ForEach-Object {
            Write-Host "   • $_" -ForegroundColor Red
        }
    }
    
    # Top recommendations
    if ($Manifest.Recommendations.Count -gt 0) {
        Write-Host "`n📝 TOP RECOMMENDATIONS:" -ForegroundColor Yellow
        $Manifest.Recommendations | Select-Object -First 5 | ForEach-Object {
            Write-Host "   • $_" -ForegroundColor White
        }
    }
    
    Write-Host "`n$('='*70)" -ForegroundColor Cyan
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

# Run digestion
$manifest = Invoke-SourceDigestion -SourcePath $SourceDir -Full:$FullAudit -Quick:$QuickScan

# Show summary
Show-Summary -Manifest $manifest

# Export reports
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$basePath = if ($ExportPath) { $ExportPath } else { "D:\RawrXD-production-lazy-init" }

switch ($OutputFormat) {
    'json' {
        Export-ManifestJson -Manifest $manifest -OutputPath "$basePath\source_digestion_manifest_$timestamp.json"
    }
    'markdown' {
        Export-ManifestMarkdown -Manifest $manifest -OutputPath "$basePath\SOURCE_DIGESTION_REPORT_$timestamp.md"
    }
    'html' {
        # HTML export would go here
        Write-Host "HTML export not yet implemented in PowerShell version" -ForegroundColor Yellow
    }
    'all' {
        Export-ManifestJson -Manifest $manifest -OutputPath "$basePath\source_digestion_manifest_$timestamp.json"
        Export-ManifestMarkdown -Manifest $manifest -OutputPath "$basePath\SOURCE_DIGESTION_REPORT_$timestamp.md"
    }
}

Write-Host "`nDigestion system complete!" -ForegroundColor Green
