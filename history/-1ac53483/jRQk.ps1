#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Feature Synchronization Analyzer for RawrXD IDE
    
.DESCRIPTION
    Analyzes feature differences between CLI and GUI implementations
    and ensures both have identical capabilities including 800b loading.
    
.EXAMPLE
    .\FeatureSyncAnalyzer.ps1 -AnalyzeAll
    .\FeatureSyncAnalyzer.ps1 -SyncFeatures
    .\FeatureSyncAnalyzer.ps1 -GenerateReport
#>

param(
    [switch]$AnalyzeAll,
    [switch]$SyncFeatures,
    [switch]$GenerateReport,
    [switch]$ShowDifferences
)

# ============================================================================
# CONFIGURATION
# ============================================================================

$script:ScriptsRoot = Split-Path $PSScriptRoot -Parent
$script:CLIScripts = Join-Path $script:ScriptsRoot "scripts"
$script:GUIScripts = Join-Path $script:ScriptsRoot "src\win32app"
$script:FeatureRegistry = @{
    "800b_Loading" = @{
        CLI_Implemented = $false
        GUI_Implemented = $false
        CLI_Path = $null
        GUI_Path = $null
        Description = "800-byte optimized loading mechanism"
    }
    "Plugin_Craft_Room" = @{
        CLI_Implemented = $false
        GUI_Implemented = $false
        CLI_Path = $null
        GUI_Path = $null
        Description = "On-the-fly custom extension creator"
    }
    "Extension_Manager" = @{
        CLI_Implemented = $false
        GUI_Implemented = $false
        CLI_Path = $null
        GUI_Path = $null
        Description = "Unified extension/plugin management"
    }
    "Module_Lifecycle" = @{
        CLI_Implemented = $false
        GUI_Implemented = $false
        CLI_Path = $null
        GUI_Path = $null
        Description = "Module install/uninstall/enable/disable"
    }
    "Pattern_Engine" = @{
        CLI_Implemented = $false
        GUI_Implemented = $false
        CLI_Path = $null
        GUI_Path = $null
        Description = "Pattern recognition and processing"
    }
    "Performance_Framework" = @{
        CLI_Implemented = $false
        GUI_Implemented = $false
        CLI_Path = $null
        GUI_Path = $null
        Description = "Performance monitoring and optimization"
    }
    "Security_Framework" = @{
        CLI_Implemented = $false
        GUI_Implemented = $false
        CLI_Path = $null
        GUI_Path = $null
        Description = "Security and capability sandboxing"
    }
    "TODO_AutoResolver" = @{
        CLI_Implemented = $false
        GUI_Implemented = $false
        CLI_Path = $null
        GUI_Path = $null
        Description = "Automatic TODO resolution system"
    }
    "Language_Registry" = @{
        CLI_Implemented = $false
        GUI_Implemented = $false
        CLI_Path = $null
        GUI_Path = $null
        Description = "Language model and support registry"
    }
    "Swarm_Control" = @{
        CLI_Implemented = $false
        GUI_Implemented = $false
        CLI_Path = $null
        GUI_Path = $null
        Description = "Swarm intelligence and model coordination"
    }
}

$script:FeatureDifferences = @()

# ============================================================================
# FEATURE DETECTION
# ============================================================================

function Test-FeatureInCLI {
    param([string]$FeatureName)
    
    $featureFiles = @{
        "800b_Loading" = @("RawrXD-DirectOptimized.ps1", "RawrXD-IDE-Bridge.ps1")
        "Plugin_Craft_Room" = @("plugin_craft_room.psm1")
        "Extension_Manager" = @("ExtensionManager.psm1")
        "Module_Lifecycle" = @("ModuleLifecycleManager.psm1")
        "Pattern_Engine" = @("PatternTemplates.psm1", "Benchmark-PatternEngine*.ps1")
        "Performance_Framework" = @("PerformanceFramework.psm1")
        "Security_Framework" = @("SecurityFramework.psm1")
        "TODO_AutoResolver" = @("TODOAutoResolver*.psm1", "Resolve-TODO*.ps1")
        "Language_Registry" = @("language_model_registry.psm1", "language_support.psm1")
        "Swarm_Control" = @("swarm_control_center.ps1", "swarm_*.ps1")
    }
    
    if ($featureFiles.ContainsKey($FeatureName)) {
        foreach ($pattern in $featureFiles[$FeatureName]) {
            $files = Get-ChildItem -Path $script:CLIScripts -Filter $pattern -ErrorAction SilentlyContinue
            if ($files) {
                return $files[0].FullName
            }
        }
    }
    return $null
}

function Test-FeatureInGUI {
    param([string]$FeatureName)
    
    $featureFiles = @{
        "800b_Loading" = @("Win32IDE.h", "Win32IDE.cpp")
        "Plugin_Craft_Room" = @("*plugin*", "*craft*")
        "Extension_Manager" = @("*extension*", "*manager*")
        "Module_Lifecycle" = @("*module*", "*lifecycle*")
        "Pattern_Engine" = @("*pattern*", "*engine*")
        "Performance_Framework" = @("*performance*", "*framework*")
        "Security_Framework" = @("*security*", "*framework*")
        "TODO_AutoResolver" = @("*todo*", "*resolver*")
        "Language_Registry" = @("*language*", "*registry*")
        "Swarm_Control" = @("*swarm*", "*control*")
    }
    
    if ($featureFiles.ContainsKey($FeatureName)) {
        foreach ($pattern in $featureFiles[$FeatureName]) {
            $files = Get-ChildItem -Path $script:GUIScripts -Filter $pattern -Recurse -ErrorAction SilentlyContinue
            if ($files) {
                return $files[0].FullName
            }
        }
    }
    return $null
}

function Get-FeatureStatus {
    param([string]$FeatureName)
    
    $cliPath = Test-FeatureInCLI -FeatureName $FeatureName
    $guiPath = Test-FeatureInGUI -FeatureName $FeatureName
    
    $status = [PSCustomObject]@{
        Feature = $FeatureName
        CLI_Implemented = ($null -ne $cliPath)
        GUI_Implemented = ($null -ne $guiPath)
        CLI_Path = $cliPath
        GUI_Path = $guiPath
        Description = $script:FeatureRegistry[$FeatureName].Description
        Status = "Unknown"
    }
    
    if ($status.CLI_Implemented -and $status.GUI_Implemented) {
        $status.Status = "Synced"
    } elseif ($status.CLI_Implemented -and -not $status.GUI_Implemented) {
        $status.Status = "CLI-Only"
    } elseif (-not $status.CLI_Implemented -and $status.GUI_Implemented) {
        $status.Status = "GUI-Only"
    } else {
        $status.Status = "Missing"
    }
    
    return $status
}

# ============================================================================
# FEATURE ANALYSIS
# ============================================================================

function Get-AllFeatureStatus {
    Write-Host "🔍 Analyzing Feature Implementation Status..." -ForegroundColor Cyan
    Write-Host "=" * 80
    
    $results = @()
    foreach ($featureName in $script:FeatureRegistry.Keys) {
        $status = Get-FeatureStatus -FeatureName $featureName
        $results += $status
        
        Write-Host "Feature: $featureName" -ForegroundColor Yellow
        Write-Host "  Description: $($status.Description)" -ForegroundColor Gray
        Write-Host "  CLI: $(if($status.CLI_Implemented){'✅'}else{'❌'})" -ForegroundColor $(if($status.CLI_Implemented){'Green'}else{'Red'})
        Write-Host "  GUI: $(if($status.GUI_Implemented){'✅'}else{'❌'})" -ForegroundColor $(if($status.GUI_Implemented){'Green'}else{'Red'})
        Write-Host "  Status: $($status.Status)" -ForegroundColor $(switch($status.Status){
            "Synced" { "Green" }
            "CLI-Only" { "Yellow" }
            "GUI-Only" { "Yellow" }
            "Missing" { "Red" }
        })
        Write-Host ""
    }
    
    return $results
}

function Get-FeatureDifferences {
    param($FeatureStatus)
    
    $differences = @()
    
    foreach ($status in $FeatureStatus) {
        if ($status.Status -ne "Synced") {
            $diff = [PSCustomObject]@{
                Feature = $status.Feature
                Status = $status.Status
                CLI_Path = $status.CLI_Path
                GUI_Path = $status.GUI_Path
                Description = $status.Description
                Action = ""
            }
            
            switch ($status.Status) {
                "CLI-Only" {
                    $diff.Action = "Port to GUI"
                }
                "GUI-Only" {
                    $diff.Action = "Port to CLI"
                }
                "Missing" {
                    $diff.Action = "Implement in both"
                }
            }
            
            $differences += $diff
        }
    }
    
    return $differences
}

# ============================================================================
# SYNCHRONIZATION
# ============================================================================

function Sync-Feature {
    param(
        [string]$FeatureName,
        [string]$SourcePath,
        [string]$TargetType  # "CLI" or "GUI"
    )
    
    Write-Host "🔄 Syncing $FeatureName to $TargetType..." -ForegroundColor Cyan
    
    if ($TargetType -eq "CLI") {
        # Copy to CLI scripts directory
        $targetDir = $script:CLIScripts
        $targetFile = Join-Path $targetDir (Split-Path $SourcePath -Leaf)
        Copy-Item -Path $SourcePath -Destination $targetFile -Force
        
        # Ensure 800b loading capability
        Add-800bLoadingCapability -FilePath $targetFile
        
        Write-Host "  ✅ Copied to CLI: $targetFile" -ForegroundColor Green
    } else {
        # Copy to GUI source directory
        $targetDir = $script:GUIScripts
        $targetFile = Join-Path $targetDir (Split-Path $SourcePath -Leaf)
        Copy-Item -Path $SourcePath -Destination $targetFile -Force
        
        Write-Host "  ✅ Copied to GUI: $targetFile" -ForegroundColor Green
    }
}

function Add-800bLoadingCapability {
    param([string]$FilePath)
    
    $content = Get-Content -Path $FilePath -Raw
    
    # Check if 800b loading is already implemented
    if ($content -notmatch "800b.*loading|optimized.*loading|fast.*load") {
        Write-Host "  ⚡ Adding 800b loading capability..." -ForegroundColor Yellow
        
        $loadingCode = @"

# ============================================================================
# 800B OPTIMIZED LOADING
# ============================================================================

`$script:FastLoadBuffer = New-Object byte[] 800
`$script:LoadOptimization = @{
    BufferSize = 800
    UseAsync = `$true
    ParallelLoad = `$true
    CacheResults = `$true
}

function Invoke-FastLoad {
    param([string]`$Path)
    
    `$sw = [System.Diagnostics.Stopwatch]::StartNew()
    `$result = Get-Content -Path `$Path -Raw -AsByteStream -ReadCount 800
    `$sw.Stop()
    
    Write-Host "⚡ FastLoad: `$($sw.ElapsedMilliseconds)ms" -ForegroundColor Green
    return `$result
}

function Enable-OptimizedLoading {
    `$env:RAWRXD_OPTIMIZED_LOADING = "1"
    `$env:RAWRXD_LOAD_BUFFER_SIZE = "800"
}

Enable-OptimizedLoading
"@
        
        # Add loading capability at the beginning of the file
        $lines = $content -split "`n"
        $insertIndex = 0
        
        # Find the first function or main code block
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match "^function|^param\(|^`$script:" -and $lines[$i] -notmatch "^#|^<#") {
                $insertIndex = $i
                break
            }
        }
        
        # Insert loading code
        $newContent = $lines[0..($insertIndex-1)] + $loadingCode + $lines[$insertIndex..($lines.Count-1)]
        Set-Content -Path $FilePath -Value ($newContent -join "`n") -NoNewline
    }
}

function Sync-AllFeatures {
    Write-Host "🔄 Synchronizing all features..." -ForegroundColor Cyan
    Write-Host "=" * 80
    
    $featureStatus = Get-AllFeatureStatus
    $differences = Get-FeatureDifferences -FeatureStatus $featureStatus
    
    foreach ($diff in $differences) {
        Write-Host "Feature: $($diff.Feature)" -ForegroundColor Yellow
        Write-Host "  Action: $($diff.Action)" -ForegroundColor Cyan
        
        switch ($diff.Status) {
            "CLI-Only" {
                if ($diff.CLI_Path) {
                    Sync-Feature -FeatureName $diff.Feature -SourcePath $diff.CLI_Path -TargetType "GUI"
                }
            }
            "GUI-Only" {
                if ($diff.GUI_Path) {
                    Sync-Feature -FeatureName $diff.Feature -SourcePath $diff.GUI_Path -TargetType "CLI"
                }
            }
            "Missing" {
                Write-Host "  ⚠️  Feature missing in both CLI and GUI" -ForegroundColor Red
                Write-Host "  💡 Recommendation: Implement using plugin_craft_room.psm1" -ForegroundColor Gray
            }
        }
        Write-Host ""
    }
    
    Write-Host "✅ Feature synchronization complete!" -ForegroundColor Green
}

# ============================================================================
# REPORTING
# ============================================================================

function Get-SyncReport {
    param($FeatureStatus, $Differences)
    
    $report = @"
╔══════════════════════════════════════════════════════════════════════════════╗
║                  RawrXD IDE Feature Synchronization Report                   ║
╚══════════════════════════════════════════════════════════════════════════════╝

📊 SUMMARY:
   Total Features Analyzed: $($FeatureStatus.Count)
   ✅ Fully Synced: $(($FeatureStatus | Where-Object { $_.Status -eq "Synced" }).Count)
   ⚠️  CLI-Only: $(($FeatureStatus | Where-Object { $_.Status -eq "CLI-Only" }).Count)
   ⚠️  GUI-Only: $(($FeatureStatus | Where-Object { $_.Status -eq "GUI-Only" }).Count)
   ❌ Missing: $(($FeatureStatus | Where-Object { $_.Status -eq "Missing" }).Count)

📋 DETAILED DIFFERENCES:
"@
    
    if ($Differences.Count -eq 0) {
        $report += "   ✅ All features are synchronized!`n"
    } else {
        foreach ($diff in $Differences) {
            $report += @"

   Feature: $($diff.Feature)
   Status: $($diff.Status)
   Action: $($diff.Action)
   Description: $($diff.Description)
"@
            if ($diff.CLI_Path) {
                $report += "   CLI Location: $($diff.CLI_Path)`n"
            }
            if ($diff.GUI_Path) {
                $report += "   GUI Location: $($diff.GUI_Path)`n"
            }
        }
    }
    
    $report += @"

🎯 RECOMMENDATIONS:
   1. Run .\FeatureSyncAnalyzer.ps1 -SyncFeatures to auto-sync differences
   2. For missing features, use plugin_craft_room.psm1 to create them
   3. Ensure all scripts have 800b loading capability for consistency
   4. Test both CLI and GUI after synchronization

⚡ 800B LOADING STATUS:
   All CLI scripts should include optimized loading for performance parity
   with GUI implementations.

═══════════════════════════════════════════════════════════════════════════════
"@
    
    return $report
}

function Export-SyncReport {
    param(
        [string]$OutputPath = "FeatureSyncReport.txt"
    )
    
    $featureStatus = Get-AllFeatureStatus
    $differences = Get-FeatureDifferences -FeatureStatus $featureStatus
    $report = Get-SyncReport -FeatureStatus $featureStatus -Differences $differences
    
    $report | Out-File -FilePath $OutputPath -Encoding UTF8
    Write-Host "📄 Report exported to: $OutputPath" -ForegroundColor Green
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

if ($AnalyzeAll) {
    $featureStatus = Get-AllFeatureStatus
    $differences = Get-FeatureDifferences -FeatureStatus $featureStatus
    
    Write-Host ""
    Write-Host "📊 FEATURE ANALYSIS COMPLETE" -ForegroundColor Green
    Write-Host "=" * 80
    Write-Host "Differences found: $($differences.Count)" -ForegroundColor $(if($differences.Count -eq 0){'Green'}else{'Yellow'})
}

if ($SyncFeatures) {
    Sync-AllFeatures
}

if ($GenerateReport) {
    Export-SyncReport
}

if ($ShowDifferences) {
    $featureStatus = Get-AllFeatureStatus
    $differences = Get-FeatureDifferences -FeatureStatus $featureStatus
    
    Write-Host ""
    Write-Host "🔍 FEATURE DIFFERENCES" -ForegroundColor Cyan
    Write-Host "=" * 80
    
    foreach ($diff in $differences) {
        Write-Host "Feature: $($diff.Feature)" -ForegroundColor Yellow
        Write-Host "  Status: $($diff.Status)" -ForegroundColor $(switch($diff.Status){
            "CLI-Only" { "Yellow" }
            "GUI-Only" { "Yellow" }
            "Missing" { "Red" }
        })
        Write-Host "  Action: $($diff.Action)" -ForegroundColor Cyan
        Write-Host ""
    }
}

# If no parameters specified, show help
if (-not $AnalyzeAll -and -not $SyncFeatures -and -not $GenerateReport -and -not $ShowDifferences) {
    Write-Host @"
╔══════════════════════════════════════════════════════════════════════════════╗
║                  RawrXD Feature Synchronization Analyzer                     ║
╚══════════════════════════════════════════════════════════════════════════════╝

USAGE:
    .\FeatureSyncAnalyzer.ps1 [-AnalyzeAll] [-SyncFeatures] [-GenerateReport] [-ShowDifferences]

PARAMETERS:
    -AnalyzeAll       Analyze all features and show implementation status
    -SyncFeatures     Automatically sync features between CLI and GUI
    -GenerateReport   Generate a detailed synchronization report
    -ShowDifferences  Show only the features that differ between CLI and GUI

EXAMPLES:
    .\FeatureSyncAnalyzer.ps1 -AnalyzeAll
    .\FeatureSyncAnalyzer.ps1 -SyncFeatures
    .\FeatureSyncAnalyzer.ps1 -GenerateReport -OutputPath "report.txt"
    .\FeatureSyncAnalyzer.ps1 -ShowDifferences

FEATURES ANALYZED:
    ✅ 800b_Loading          - Optimized loading mechanism
    ✅ Plugin_Craft_Room     - Custom extension creator
    ✅ Extension_Manager     - Extension management
    ✅ Module_Lifecycle      - Module lifecycle management
    ✅ Pattern_Engine        - Pattern recognition
    ✅ Performance_Framework - Performance monitoring
    ✅ Security_Framework    - Security sandboxing
    ✅ TODO_AutoResolver     - TODO resolution
    ✅ Language_Registry     - Language support
    ✅ Swarm_Control         - Swarm intelligence

═══════════════════════════════════════════════════════════════════════════════
"@
}
