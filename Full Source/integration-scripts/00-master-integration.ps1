# RawrXD Complete Integration Master Script
# Integrates all features from RawrXD-production-lazy-init into rawrxd
# Run this to fully sync and integrate all production features

param(
    [string]$SourcePath = "D:\RawrXD-production-lazy-init",
    [string]$TargetPath = "D:\rawrxd",
    [switch]$DryRun = $false,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Continue"
$startTime = Get-Date

# Colors for output
$colors = @{
    Success = "Green"
    Warning = "Yellow"
    Error = "Red"
    Info = "Cyan"
    Progress = "Magenta"
}

function Write-Status {
    param([string]$Message, [string]$Type = "Info")
    $timestamp = Get-Date -Format "HH:mm:ss"
    Write-Host "[$timestamp] " -NoNewline -ForegroundColor Gray
    Write-Host $Message -ForegroundColor $colors[$Type]
}

function Copy-FileIfMissing {
    param([string]$Source, [string]$Target, [string]$Description)
    
    if (-not (Test-Path $Source)) {
        Write-Status "SKIP: $Description - source not found" -Type Warning
        return $false
    }
    
    if (Test-Path $Target) {
        if ($Verbose) {
            Write-Status "EXISTS: $Description" -Type Warning
        }
        return $false
    }
    
    try {
        $targetDir = Split-Path $Target -Parent
        if (-not (Test-Path $targetDir)) {
            New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        }
        
        if (-not $DryRun) {
            Copy-Item -Path $Source -Destination $Target -Force
        }
        
        Write-Status "COPY: $Description" -Type Success
        return $true
    }
    catch {
        Write-Status "ERROR copying $Description - $_" -Type Error
        return $false
    }
}

function Sync-Directory {
    param([string]$SourceDir, [string]$TargetDir, [string]$Pattern, [string]$Description)
    
    $count = 0
    $skipped = 0
    
    if (-not (Test-Path $SourceDir)) {
        Write-Status "SKIP: $Description - directory not found" -Type Warning
        return @{Copied=0; Skipped=0}
    }
    
    Write-Status "Syncing $Description..." -Type Progress
    
    $items = Get-ChildItem -Path $SourceDir -Filter $Pattern -ErrorAction SilentlyContinue
    
    foreach ($item in $items) {
        $relativePath = $item.FullName.Substring($SourceDir.Length + 1)
        $targetPath = Join-Path $TargetDir $relativePath
        
        if (Copy-FileIfMissing -Source $item.FullName -Target $targetPath -Description $relativePath) {
            $count++
        }
        else {
            $skipped++
        }
    }
    
    Write-Status "  → Copied: $count, Skipped: $skipped" -Type Info
    return @{Copied=$count; Skipped=$skipped}
}

# Main Integration Steps
Write-Status "╔══════════════════════════════════════════════════════════╗" -Type Progress
Write-Status "║ RawrXD COMPLETE INTEGRATION SCRIPT                     ║" -Type Progress
Write-Status "║ Source: $SourcePath" -Type Progress
Write-Status "║ Target: $TargetPath" -Type Progress
Write-Status "║ DryRun: $DryRun" -Type Progress
Write-Status "╚══════════════════════════════════════════════════════════╝" -Type Progress

$totalCopied = 0
$totalSkipped = 0

# Step 1: Copy Core QtApp Files (AI/ML)
Write-Status "`n[STEP 1/8] Copying QtApp AI/ML Components..." -Type Info
$result = Sync-Directory "$SourcePath\src\qtapp" "$TargetPath\src\qtapp" "ai_*.cpp" "AI Components (CPP)"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

$result = Sync-Directory "$SourcePath\src\qtapp" "$TargetPath\src\qtapp" "ai_*.h" "AI Components (Headers)"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

# Step 2: Copy UI/Dashboard Components
Write-Status "`n[STEP 2/8] Copying UI & Dashboard Components..." -Type Info
$uiPatterns = @("*dashboard*.cpp", "*panel*.cpp", "*metrics*.cpp", "theme*.cpp", "code_*.cpp", "*minimap*.cpp")
foreach ($pattern in $uiPatterns) {
    $result = Sync-Directory "$SourcePath\src\qtapp" "$TargetPath\src\qtapp" $pattern "UI Pattern: $pattern"
    $totalCopied += $result.Copied
    $totalSkipped += $result.Skipped
}

# Step 3: Copy Analysis & Intelligence Components
Write-Status "`n[STEP 3/8] Copying Analysis & Intelligence..." -Type Info
$result = Sync-Directory "$SourcePath\src\qtapp" "$TargetPath\src\qtapp" "*intelligenc*.cpp" "Intelligence (CPP)"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

$result = Sync-Directory "$SourcePath\src\qtapp" "$TargetPath\src\qtapp" "*intelligenc*.h" "Intelligence (Headers)"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

# Step 4: Copy Enterprise & Monitoring
Write-Status "`n[STEP 4/8] Copying Enterprise & Monitoring..." -Type Info
$result = Sync-Directory "$SourcePath\src\qtapp" "$TargetPath\src\qtapp" "*enterprise*.cpp" "Enterprise (CPP)"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

$result = Sync-Directory "$SourcePath\src\qtapp" "$TargetPath\src\qtapp" "*latency*.cpp" "Latency Monitoring"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

# Step 5: Copy Agentic Components
Write-Status "`n[STEP 5/8] Copying Agentic System Components..." -Type Info
$result = Sync-Directory "$SourcePath\src\agentic" "$TargetPath\src\agentic" "*.cpp" "Agentic (CPP)"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

$result = Sync-Directory "$SourcePath\src\agentic" "$TargetPath\src\agentic" "*.h" "Agentic (Headers)"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

# Step 6: Copy Model & Inference Components
Write-Status "`n[STEP 6/8] Copying Model & Inference Components..." -Type Info
$result = Sync-Directory "$SourcePath\src" "$TargetPath\src" "*streaming*.cpp" "Streaming (CPP)"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

$result = Sync-Directory "$SourcePath\src" "$TargetPath\src" "*loader*.cpp" "Loaders (CPP)"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

# Step 7: Copy MASM Components
Write-Status "`n[STEP 7/8] Copying MASM & Optimization Components..." -Type Info
$result = Sync-Directory "$SourcePath\src" "$TargetPath\src" "*.asm" "MASM Assembly"
$totalCopied += $result.Copied
$totalSkipped += $result.Skipped

# Step 8: Copy Documentation
Write-Status "`n[STEP 8/8] Copying Documentation..." -Type Info
if (Test-Path "$SourcePath\docs") {
    $result = Sync-Directory "$SourcePath\docs" "$TargetPath\docs" "*" "Documentation"
    $totalCopied += $result.Copied
    $totalSkipped += $result.Skipped
}

$copyNotFoundInTarget = Get-ChildItem -Path "$SourcePath" -Filter "*.md" -ErrorAction SilentlyContinue | Where-Object { -not (Test-Path "$TargetPath\$($_.Name)") }
foreach ($doc in $copyNotFoundInTarget) {
    if (Copy-FileIfMissing -Source $doc.FullName -Target "$TargetPath\$($doc.Name)" -Description "Doc: $($doc.Name)") {
        $totalCopied++
    }
}

# Summary
$duration = (Get-Date) - $startTime
Write-Status "`n╔══════════════════════════════════════════════════════════╗" -Type Progress
Write-Status "║ INTEGRATION COMPLETE                                   ║" -Type Progress
Write-Status "╚══════════════════════════════════════════════════════════╝" -Type Progress
Write-Status "Total Files Copied: $totalCopied" -Type Success
Write-Status "Total Files Skipped (exist): $totalSkipped" -Type Info
Write-Status "Duration: $($duration.Minutes)m $($duration.Seconds)s" -Type Info
Write-Status "`nNext Steps:" -Type Info
Write-Status "1. Run: .\integration-scripts\01-update-cmakelists.ps1" -Type Info
Write-Status "2. Run: .\integration-scripts\02-fix-includes.ps1" -Type Info
Write-Status "3. Run: .\integration-scripts\03-validate-integration.ps1" -Type Info
Write-Status "4. Build: cmake --build build" -Type Info
