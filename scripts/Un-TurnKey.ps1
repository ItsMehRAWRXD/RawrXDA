#Requires -Version 5.1
<#
.SYNOPSIS
    Un-TurnKey Cleanup Script - De-provisioning and Cleanup for RawrXD
    
.DESCRIPTION
    Performs comprehensive cleanup of RawrXD deployment artifacts:
    1. Build artifacts removal
    2. Log file cleanup
    3. Temporary file removal
    4. Registry cleanup (if applicable)
    5. Service cleanup (if applicable)
    6. Backup creation before cleanup
    
    This is the inverse of the Turnkey deployment - ensures clean state.
    
.PARAMETER WhatIf
    Preview what would be deleted without actually deleting
    
.PARAMETER KeepBuild
    Preserve build directories
    
.PARAMETER KeepLogs
    Preserve log files
    
.PARAMETER KeepBackups
    Preserve backup files
    
.PARAMETER Force
    Skip confirmation prompts
    
.PARAMETER BackupFirst
    Create backup before cleanup
    
.PARAMETER BackupPath
    Path for backup (default: timestamped folder in temp)
    
.EXAMPLE
    .\Un-TurnKey.ps1 -WhatIf
    
.EXAMPLE
    .\Un-TurnKey.ps1 -BackupFirst -Force
    
.EXAMPLE
    .\Un-TurnKey.ps1 -KeepBuild -KeepLogs
#>

param(
    [switch]$WhatIf,
    [switch]$ValidationProbe,
    [switch]$KeepBuild,
    [switch]$KeepLogs,
    [switch]$KeepBackups,
    [switch]$Force,
    [switch]$BackupFirst,
    [string]$BackupPath = ""
)

$ErrorActionPreference = "Stop"

# ============================================================================
# CONFIGURATION
# ============================================================================

$script:Config = @{
    RepoRoot = $null
    BackupTimestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    ItemsToClean = @()
    ItemsCleaned = 0
    ItemsFailed = 0
    BytesFreed = 0
}

# ============================================================================
# INITIALIZATION
# ============================================================================

function Initialize-UnTurnKey {
    $script:Config.RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    
    if (-not $Force -and -not $WhatIf) {
        Write-Host ""
        Write-Host "╔══════════════════════════════════════════════════════════════════╗" -ForegroundColor Yellow
        Write-Host "║  RawrXD Un-TurnKey Cleanup                                       ║" -ForegroundColor Yellow
        Write-Host "║  Repository: $($script:Config.RepoRoot)" -ForegroundColor Yellow
        Write-Host "╚══════════════════════════════════════════════════════════════════╝" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "This will clean up build artifacts, logs, and temporary files." -ForegroundColor Yellow
        Write-Host ""
        
        $confirm = Read-Host "Type 'CLEANUP' to proceed or anything else to cancel"
        if ($confirm -ne "CLEANUP") {
            Write-Host "Cleanup cancelled." -ForegroundColor Cyan
            exit 0
        }
    }
    
    # Set default backup path
    if ([string]::IsNullOrWhiteSpace($BackupPath)) {
        $script:Config.BackupPath = Join-Path $env:TEMP "rawrxd_unturnkey_backup_$($script:Config.BackupTimestamp)"
    }
    else {
        $script:Config.BackupPath = $BackupPath
    }
}

# ============================================================================
# BACKUP FUNCTIONS
# ============================================================================

function Invoke-PreCleanupBackup {
    if (-not $BackupFirst) { return }
    
    Write-Host "`n[BACKUP] Creating pre-cleanup backup..." -ForegroundColor Cyan
    
    try {
        New-Item -ItemType Directory -Path $script:Config.BackupPath -Force | Out-Null
        
        $backupItems = @(
            @{ Source = "src"; Dest = "src_backup" },
            @{ Source = "scripts"; Dest = "scripts_backup" },
            @{ Source = "include"; Dest = "include_backup" },
            @{ Source = "memories"; Dest = "memories_backup" }
        )
        
        foreach ($item in $backupItems) {
            $sourcePath = Join-Path $script:Config.RepoRoot $item.Source
            $destPath = Join-Path $script:Config.BackupPath $item.Dest
            
            if (Test-Path $sourcePath) {
                if ($WhatIf) {
                    Write-Host "  [WhatIf] Would backup: $($item.Source) -> $($item.Dest)" -ForegroundColor Gray
                }
                else {
                    Copy-Item -Path $sourcePath -Destination $destPath -Recurse -Force -ErrorAction SilentlyContinue
                    Write-Host "  [BACKUP] $($item.Source)" -ForegroundColor Green
                }
            }
        }
        
        # Create backup manifest
        $manifest = @{
            timestamp = (Get-Date).ToString("o")
            computer = $env:COMPUTERNAME
            user = $env:USERNAME
            backupPath = $script:Config.BackupPath
            items = $backupItems | ForEach-Object { $_.Source }
        }
        
        $manifestPath = Join-Path $script:Config.BackupPath "backup_manifest.json"
        $manifest | ConvertTo-Json | Out-File -FilePath $manifestPath -Encoding utf8
        
        Write-Host "  [BACKUP] Complete: $($script:Config.BackupPath)" -ForegroundColor Green
    }
    catch {
        Write-Error "Backup failed: $_"
        exit 1
    }
}

# ============================================================================
# CLEANUP FUNCTIONS
# ============================================================================

function Remove-CleanupItem {
    param(
        [string]$Path,
        [string]$Description,
        [switch]$Recurse,
        [switch]$Force
    )
    
    $fullPath = Join-Path $script:Config.RepoRoot $Path
    
    if (-not (Test-Path $fullPath)) {
        return
    }
    
    $item = Get-Item $fullPath
    $size = 0
    
    if ($item -is [System.IO.DirectoryInfo]) {
        try {
            $size = (Get-ChildItem $fullPath -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
        }
        catch {
            $size = 0
        }
    }
    else {
        $size = $item.Length
    }
    
    if ($WhatIf) {
        Write-Host "  [WhatIf] Would delete: $Description ($([math]::Round($size / 1MB, 2)) MB)" -ForegroundColor Gray
    }
    else {
        try {
            Remove-Item -Path $fullPath -Recurse:$Recurse -Force:$Force -ErrorAction Stop
            $script:Config.ItemsCleaned++
            $script:Config.BytesFreed += $size
            Write-Host "  [CLEANED] $Description ($([math]::Round($size / 1MB, 2)) MB)" -ForegroundColor Green
        }
        catch {
            $script:Config.ItemsFailed++
            Write-Host "  [FAILED] $Description - $_" -ForegroundColor Red
        }
    }
}

function Invoke-BuildCleanup {
    if ($KeepBuild) {
        Write-Host "`n[SKIP] Build cleanup disabled via -KeepBuild" -ForegroundColor Yellow
        return
    }
    
    Write-Host "`n[PHASE] Build Artifact Cleanup..." -ForegroundColor Cyan

    if ($ValidationProbe) {
        $probeItems = @("build", "build-win32", "build-ninja", "build-ninja-ctx2", "build_smoke_auto", "out", "CMakeCache.txt", "CMakeFiles")
        foreach ($item in $probeItems) {
            Remove-CleanupItem -Path $item -Description "Build probe: $item" -Recurse -Force
        }

        $cmakeBuildDirs = Get-ChildItem -Path $script:Config.RepoRoot -Directory -Filter "cmake-build-*" -ErrorAction SilentlyContinue
        $repoPath = $script:Config.RepoRoot.Path
        foreach ($match in $cmakeBuildDirs) {
            Remove-CleanupItem -Path $match.FullName.Replace($repoPath + "\", "") -Description "Build probe: $($match.Name)" -Recurse -Force
        }
        return
    }
    
    $buildDirs = @("build", "build-win32", "build-ninja", "build-ninja-ctx2", "build_smoke_auto", "out", "cmake-build-*")
    
    foreach ($dir in $buildDirs) {
        $pattern = Join-Path $script:Config.RepoRoot $dir
        $matches = Get-ChildItem -Path $pattern -Directory -ErrorAction SilentlyContinue
        $repoPath = $script:Config.RepoRoot.Path
        foreach ($match in $matches) {
            Remove-CleanupItem -Path $match.FullName.Replace($repoPath + "\", "") -Description "Build: $($match.Name)" -Recurse -Force
        }
    }
    
    # Clean specific build artifacts
    $artifacts = @(
        @{ Path = "*.obj"; Desc = "Object files" },
        @{ Path = "*.pdb"; Desc = "Debug symbols" },
        @{ Path = "*.ilk"; Desc = "Incremental link files" },
        @{ Path = "*.exp"; Desc = "Export files" },
        @{ Path = "*.lib"; Desc = "Static libraries" },
        @{ Path = "CMakeCache.txt"; Desc = "CMake cache" },
        @{ Path = "CMakeFiles"; Desc = "CMake files" }
    )
    
    $repoPath = $script:Config.RepoRoot.Path
    foreach ($artifact in $artifacts) {
        $matches = Get-ChildItem -Path $script:Config.RepoRoot -Filter $artifact.Path -Recurse -ErrorAction SilentlyContinue
        foreach ($match in $matches) {
            Remove-CleanupItem -Path $match.FullName.Replace($repoPath + "\", "") -Description $artifact.Desc
        }
    }
}

function Invoke-LogCleanup {
    if ($KeepLogs) {
        Write-Host "`n[SKIP] Log cleanup disabled via -KeepLogs" -ForegroundColor Yellow
        return
    }
    
    Write-Host "`n[PHASE] Log File Cleanup..." -ForegroundColor Cyan

    if ($ValidationProbe) {
        $probeDirs = @("logs", "_logs")
        foreach ($dir in $probeDirs) {
            Remove-CleanupItem -Path $dir -Description "Log probe: $dir" -Recurse -Force
        }

        $repoPath = $script:Config.RepoRoot.Path
        $rootLogs = Get-ChildItem -Path $script:Config.RepoRoot -File -Filter "*.log" -ErrorAction SilentlyContinue
        foreach ($match in $rootLogs) {
            Remove-CleanupItem -Path $match.FullName.Replace($repoPath + "\", "") -Description "Log probe: $($match.Name)"
        }
        return
    }
    
    $logPatterns = @("*.log", "*.log.*", "logs\*", "_logs\*")
    
    $repoPath = $script:Config.RepoRoot.Path
    foreach ($pattern in $logPatterns) {
        $matches = Get-ChildItem -Path $script:Config.RepoRoot -Filter $pattern -Recurse -ErrorAction SilentlyContinue
        foreach ($match in $matches) {
            Remove-CleanupItem -Path $match.FullName.Replace($repoPath + "\", "") -Description "Log: $($match.Name)"
        }
    }
}

function Invoke-TempCleanup {
    Write-Host "`n[PHASE] Temporary File Cleanup..." -ForegroundColor Cyan

    if ($ValidationProbe) {
        $probeDirs = @("temp", "tmp", ".vs", ".vscode")
        foreach ($dir in $probeDirs) {
            Remove-CleanupItem -Path $dir -Description "Temp probe: $dir" -Recurse -Force
        }

        $repoPath = $script:Config.RepoRoot.Path
        $rootPatterns = @("*.tmp", "*.temp", "*~", "*.bak", "*.old")
        foreach ($pattern in $rootPatterns) {
            $matches = Get-ChildItem -Path $script:Config.RepoRoot -File -Filter $pattern -ErrorAction SilentlyContinue
            foreach ($match in $matches) {
                Remove-CleanupItem -Path $match.FullName.Replace($repoPath + "\", "") -Description "Temp probe: $($match.Name)"
            }
        }
        return
    }
    
    $tempPatterns = @(
        "*.tmp",
        "*.temp",
        "*~",
        "*.bak",
        "*.old",
        "temp\*",
        "tmp\*",
        ".vs\*",
        ".vscode\*"
    )
    
    $repoPath = $script:Config.RepoRoot.Path
    foreach ($pattern in $tempPatterns) {
        $matches = Get-ChildItem -Path $script:Config.RepoRoot -Filter $pattern -Recurse -ErrorAction SilentlyContinue | Where-Object { -not $_.PSIsContainer }
        foreach ($match in $matches) {
            Remove-CleanupItem -Path $match.FullName.Replace($repoPath + "\", "") -Description "Temp: $($match.Name)"
        }
    }
}

function Invoke-CacheCleanup {
    Write-Host "`n[PHASE] Cache Cleanup..." -ForegroundColor Cyan

    if ($ValidationProbe) {
        $probeDirs = @(".cache", "__pycache__", "node_modules", ".npm", ".gradle", ".m2")
        foreach ($dir in $probeDirs) {
            Remove-CleanupItem -Path $dir -Description "Cache probe: $dir" -Recurse -Force
        }
        return
    }
    
    $cacheDirs = @(
        ".cache",
        "__pycache__",
        "node_modules",
        ".npm",
        ".gradle",
        ".m2"
    )
    
    $repoPath = $script:Config.RepoRoot.Path
    foreach ($dir in $cacheDirs) {
        $matches = Get-ChildItem -Path $script:Config.RepoRoot -Filter $dir -Recurse -Directory -ErrorAction SilentlyContinue
        foreach ($match in $matches) {
            Remove-CleanupItem -Path $match.FullName.Replace($repoPath + "\", "") -Description "Cache: $($match.Name)" -Recurse -Force
        }
    }
}

function Invoke-ReportCleanup {
    Write-Host "`n[PHASE] Report/Output Cleanup..." -ForegroundColor Cyan

    if ($ValidationProbe) {
        Remove-CleanupItem -Path "reports" -Description "Report probe: reports" -Recurse -Force
        return
    }
    
    # Keep recent reports but clean old ones
    $reportDir = Join-Path $script:Config.RepoRoot "reports"
    if (Test-Path $reportDir) {
        $cutoffDate = (Get-Date).AddDays(-7)
        $repoPath = $script:Config.RepoRoot.Path
        $oldReports = Get-ChildItem -Path $reportDir -Filter "*.json" -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -lt $cutoffDate }
        
        foreach ($report in $oldReports) {
            Remove-CleanupItem -Path $report.FullName.Replace($repoPath + "\", "") -Description "Old report: $($report.Name)"
        }
    }
}

# ============================================================================
# SUMMARY
# ============================================================================

function Write-CleanupSummary {
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  UN-TURNKEY CLEANUP SUMMARY" -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Cyan
    
    if ($WhatIf) {
        Write-Host "  Mode: WhatIf (no changes made)" -ForegroundColor Yellow
    }
    else {
        Write-Host "  Items Cleaned: $($script:Config.ItemsCleaned)" -ForegroundColor Green
        Write-Host "  Items Failed: $($script:Config.ItemsFailed)" -ForegroundColor $(if ($script:Config.ItemsFailed -gt 0) { "Red" } else { "Green" })
        Write-Host "  Space Freed: $([math]::Round($script:Config.BytesFreed / 1MB, 2)) MB" -ForegroundColor Green
        
        if ($BackupFirst) {
            Write-Host "  Backup Location: $($script:Config.BackupPath)" -ForegroundColor Cyan
        }
    }
    
    Write-Host "========================================" -ForegroundColor Cyan
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

Initialize-UnTurnKey

if ($BackupFirst) {
    Invoke-PreCleanupBackup
}

Invoke-BuildCleanup
Invoke-LogCleanup
Invoke-TempCleanup
Invoke-CacheCleanup
Invoke-ReportCleanup

Write-CleanupSummary

exit [int]($script:Config.ItemsFailed -gt 0)
