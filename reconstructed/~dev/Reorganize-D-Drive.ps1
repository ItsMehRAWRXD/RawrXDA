# D: DRIVE MASTER REORGANIZATION SCRIPT
# Version: 1.0
# Date: November 21, 2025
# Purpose: Organize entire D: drive into structured ~dev folder

param(
    [switch]$DryRun = $false,
    [switch]$SkipBackup = $false,
    [switch]$Verbose = $false
)

$ErrorActionPreference = "Continue"
$VerbosePreference = if ($Verbose) { "Continue" } else { "SilentlyContinue" }

# Colors
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Warning { Write-Host $args -ForegroundColor Yellow }
function Write-Error { Write-Host $args -ForegroundColor Red }

Write-Info "═══════════════════════════════════════════════════════════"
Write-Info "    D: DRIVE MASTER REORGANIZATION"
Write-Info "═══════════════════════════════════════════════════════════"
Write-Info ""

if ($DryRun) {
    Write-Warning "🔍 DRY RUN MODE - No files will be moved"
}

# Step 1: Create master structure
Write-Info "📁 Step 1: Creating master folder structure..."

$devRoot = "D:\~dev"
$categories = @(
    "01-Active-Projects",
    "02-IDEs-Development-Tools",
    "03-Web-Projects",
    "04-AI-ML-Projects",
    "05-Compilers-Toolchains",
    "06-Security-Research",
    "07-Extensions-Plugins",
    "08-Cloud-AWS-Projects",
    "09-Archives-Old-Projects",
    "10-Recovery-Files",
    "11-Models-Data",
    "12-Tools-Utilities",
    "13-Scripts-Automation",
    "14-Testing-Debug",
    "15-Documentation",
    "16-Temp-Working",
    "17-Executables-Builds",
    "18-Node-Modules-Deps",
    "19-Backups-Snapshots",
    "20-Misc-Uncategorized"
)

if (-not $DryRun) {
    foreach ($cat in $categories) {
        $path = Join-Path $devRoot $cat
        if (-not (Test-Path $path)) {
            New-Item -Path $path -ItemType Directory -Force | Out-Null
            Write-Success "  ✅ Created: ~dev\$cat"
        } else {
            Write-Info "  ℹ️  Exists: ~dev\$cat"
        }
    }
} else {
    foreach ($cat in $categories) {
        Write-Info "  [DRY RUN] Would create: ~dev\$cat"
    }
}

Write-Info ""

# Step 2: Create backup manifest
if (-not $SkipBackup -and -not $DryRun) {
    Write-Info "💾 Step 2: Creating backup manifest..."
    $manifestPath = "D:\~dev\PRE-ORGANIZATION-MANIFEST_$(Get-Date -Format 'yyyy-MM-dd_HH-mm-ss').txt"
    
    Get-ChildItem "D:\" -Directory | 
    Where-Object { $_.Name -notmatch '^~dev$|^Organized$|^Microsoft|^LocalDesktop$|^Screenshots$' } |
    Select-Object Name, FullName, LastWriteTime, @{n='Size_MB';e={(Get-ChildItem $_.FullName -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB}} |
    Export-Csv -Path $manifestPath -NoTypeInformation
    
    Write-Success "  ✅ Manifest saved: $manifestPath"
} else {
    Write-Warning "  ⏭️  Skipping backup manifest (DryRun or SkipBackup)"
}

Write-Info ""

# Step 3: Define move mappings
Write-Info "🗺️  Step 3: Preparing move mappings..."

$moveMappings = @{
    "01-Active-Projects" = @{
        "Patterns" = @("bigdaddyg-*", "BigDaddyG-*", "RawrZ*", "rawrZ", "RAWR", "RAwrZProject")
        "SubFolders" = @{
            "BigDaddyG-AI-Beast" = @("bigdaddyg-*", "BigDaddyG-*")
            "RawrZ-Platform" = @("RawrZ*", "rawrZ", "RAWR", "RAwrZProject")
        }
    }
    "02-IDEs-Development-Tools" = @{
        "Patterns" = @("MyCopilot-*", "Glassquill*", "DevMarketIDE", "ProjectIDEAI", "agentic-screen-share", 
                       "ai-copilot-*", "amazonq-ide", "aws-enhanced-ide", "IDE-*", "ide_*", "java-ide-electron",
                       "cursor-*", "mycopilot-*")
        "SubFolders" = @{
            "MyCopilot-Variants" = @("MyCopilot-*", "mycopilot-*")
            "Cursor-Extensions" = @("cursor-*")
        }
    }
    "03-Web-Projects" = @{
        "Patterns" = @("aws-cost-*", "HTML-Projects", "web-experiments", "portfolio-site", "08-Web-Frontend", 
                       "chatgpt-plus-bridge", "web")
    }
    "04-AI-ML-Projects" = @{
        "Patterns" = @("Neural-*", "OOP-AI-Model", "offline_ai", "ml-framework", "AI-Systems", "agentic_*", 
                       "ai-assistant-*", "ai-editor-*", "ai-web-*", "agentic_framework")
    }
    "05-Compilers-Toolchains" = @{
        "Patterns" = @("portable-toolchains", "portable_toolchains", "04-Compilers", "UniversalCompiler", 
                       "CompilerStudio", "PowerShell-Compilers", "nasm", "Compilers-*", "demo-compiler", 
                       "generated-compilers", "professional-nasm-ide")
    }
    "06-Security-Research" = @{
        "Patterns" = @("Security Research*")
    }
    "07-Extensions-Plugins" = @{
        "Patterns" = @("vscode-extensions", "RawrZ-Extensions", "UnifiedAI-Extension", "extension-fix", 
                       "bigdaddyg-extension", "cursor-circumvention-*")
    }
    "08-Cloud-AWS-Projects" = @{
        "Patterns" = @("aws-*", "Cloud-AWS", "turnkey_aws_saas")
    }
    "09-Archives-Old-Projects" = @{
        "Patterns" = @("Archived-*", "backup-before-cleanup", "12-Archives-Backups", "TEMP-REUPLOAD-*")
    }
    "10-Recovery-Files" = @{
        "Patterns" = @("13-Recovery-Files", "14-Desktop-Files", "15-Downloads-Files", "BIGDADDYG-RECOVERY")
    }
    "11-Models-Data" = @{
        "Patterns" = @("01-AI-Models", "OllamaModels", "ollama", "models", "Neural-Training-Data")
    }
    "12-Tools-Utilities" = @{
        "Patterns" = @("03-Tools-Utilities", "05-Utilities", "tools", "utils")
    }
    "13-Scripts-Automation" = @{
        "Patterns" = @("07-Scripts-PowerShell", "Scripts", "organize-logs")
    }
    "14-Testing-Debug" = @{
        "Patterns" = @("04-Testing", "05-Tests-Debug", "test-*", "TestResults", "tests", "*_test_*", "demo_project")
    }
    "15-Documentation" = @{
        "Patterns" = @("03-Documentation", "06-Documentation", "Documentation")
    }
    "16-Temp-Working" = @{
        "Patterns" = @("08-Temp", "11-Temp-Working", "TEMP-*", "temp", "New folder")
    }
    "17-Executables-Builds" = @{
        "Patterns" = @("07-Executables", "Executables", "exe_files", "compiled_projects", "build", "builds", "dist")
    }
    "18-Node-Modules-Deps" = @{
        "Patterns" = @("node_modules", "modules", "SDK")
    }
    "19-Backups-Snapshots" = @{
        "Patterns" = @("Backup", "backups", "backup-*")
    }
}

Write-Success "  ✅ Move mappings loaded: $($moveMappings.Count) categories"
Write-Info ""

# Step 4: Execute moves
Write-Info "🚚 Step 4: Moving folders..."
$movedCount = 0
$errorCount = 0
$skipCount = 0

$excludeList = @("~dev", "Organized", "Microsoft Visual Studio", "Microsoft VS Code", "LocalDesktop", "Screenshots")

foreach ($category in $moveMappings.Keys) {
    $categoryPath = Join-Path $devRoot $category
    $patterns = $moveMappings[$category]["Patterns"]
    
    Write-Info "  📂 Processing category: $category"
    
    foreach ($pattern in $patterns) {
        $items = Get-ChildItem "D:\" -Directory -Filter $pattern -ErrorAction SilentlyContinue |
                 Where-Object { $excludeList -notcontains $_.Name }
        
        foreach ($item in $items) {
            try {
                $targetPath = Join-Path $categoryPath $item.Name
                
                if ($DryRun) {
                    Write-Info "    [DRY RUN] Would move: $($item.Name) → $category\$($item.Name)"
                    $movedCount++
                } else {
                    # Check if subfolder mapping exists
                    $subFolders = $moveMappings[$category]["SubFolders"]
                    if ($subFolders) {
                        $matched = $false
                        foreach ($subFolder in $subFolders.Keys) {
                            $subPatterns = $subFolders[$subFolder]
                            foreach ($subPattern in $subPatterns) {
                                if ($item.Name -like $subPattern) {
                                    $targetPath = Join-Path (Join-Path $categoryPath $subFolder) $item.Name
                                    $parentDir = Split-Path $targetPath -Parent
                                    if (-not (Test-Path $parentDir)) {
                                        New-Item -Path $parentDir -ItemType Directory -Force | Out-Null
                                    }
                                    $matched = $true
                                    break
                                }
                            }
                            if ($matched) { break }
                        }
                    }
                    
                    if (Test-Path $targetPath) {
                        Write-Warning "    ⚠️  Skipped (exists): $($item.Name)"
                        $skipCount++
                    } else {
                        Move-Item -Path $item.FullName -Destination $targetPath -Force -ErrorAction Stop
                        Write-Success "    ✅ Moved: $($item.Name) → $category"
                        $movedCount++
                    }
                }
            } catch {
                Write-Error "    ❌ Failed: $($item.Name) - $($_.Exception.Message)"
                $errorCount++
            }
        }
    }
}

Write-Info ""

# Step 5: Move remaining unmatched folders
Write-Info "🔍 Step 5: Categorizing remaining folders..."

$remaining = Get-ChildItem "D:\" -Directory -ErrorAction SilentlyContinue |
             Where-Object { 
                 $excludeList -notcontains $_.Name -and 
                 $_.Name -ne "~dev" -and
                 $_.Name -notmatch '^\$|^System Volume Information'
             }

if ($remaining.Count -gt 0) {
    Write-Info "  Found $($remaining.Count) uncategorized folders"
    $miscPath = Join-Path $devRoot "20-Misc-Uncategorized"
    
    foreach ($item in $remaining) {
        try {
            $targetPath = Join-Path $miscPath $item.Name
            
            if ($DryRun) {
                Write-Info "    [DRY RUN] Would move: $($item.Name) → 20-Misc-Uncategorized"
            } else {
                if (Test-Path $targetPath) {
                    Write-Warning "    ⚠️  Skipped (exists): $($item.Name)"
                    $skipCount++
                } else {
                    Move-Item -Path $item.FullName -Destination $targetPath -Force -ErrorAction Stop
                    Write-Success "    ✅ Moved: $($item.Name) → 20-Misc-Uncategorized"
                    $movedCount++
                }
            }
        } catch {
            Write-Error "    ❌ Failed: $($item.Name) - $($_.Exception.Message)"
            $errorCount++
        }
    }
} else {
    Write-Success "  ✅ No uncategorized folders found!"
}

Write-Info ""

# Step 6: Generate final report
Write-Info "📊 Step 6: Generating summary report..."

$reportPath = "D:\~dev\ORGANIZATION-REPORT_$(Get-Date -Format 'yyyy-MM-dd_HH-mm-ss').txt"
$report = @"
═══════════════════════════════════════════════════════════
D: DRIVE REORGANIZATION REPORT
═══════════════════════════════════════════════════════════

Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
Mode: $(if ($DryRun) { "DRY RUN" } else { "LIVE EXECUTION" })

SUMMARY:
--------
Folders Moved: $movedCount
Folders Skipped: $skipCount
Errors: $errorCount

STRUCTURE CREATED:
------------------
$($categories | ForEach-Object { "  ✅ D:\~dev\$_" } | Out-String)

NEXT STEPS:
-----------
1. Review D:\~dev\ structure
2. Verify all projects in correct locations
3. Update any hardcoded paths in scripts
4. Test key projects still work
5. Delete empty folders from D:\ root (manually)

NOTES:
------
- Original manifest saved in D:\~dev\PRE-ORGANIZATION-MANIFEST_*.txt
- Excluded from move: Organized, Microsoft installations, LocalDesktop, Screenshots
- All moves preserved folder structure and contents

═══════════════════════════════════════════════════════════
"@

if (-not $DryRun) {
    $report | Out-File -FilePath $reportPath -Encoding UTF8
    Write-Success "  ✅ Report saved: $reportPath"
}

Write-Info ""
Write-Info "═══════════════════════════════════════════════════════════"
Write-Success "    REORGANIZATION COMPLETE!"
Write-Info "═══════════════════════════════════════════════════════════"
Write-Info ""
Write-Info "Summary:"
Write-Success "  ✅ Folders moved: $movedCount"
if ($skipCount -gt 0) { Write-Warning "  ⚠️  Folders skipped: $skipCount" }
if ($errorCount -gt 0) { Write-Error "  ❌ Errors: $errorCount" }
Write-Info ""

if ($DryRun) {
    Write-Warning "This was a DRY RUN - no files were actually moved"
    Write-Info "Run without -DryRun flag to execute the organization:"
    Write-Info "  .\Reorganize-D-Drive.ps1"
} else {
    Write-Success "All files have been organized into D:\~dev\"
    Write-Info ""
    Write-Info "Next steps:"
    Write-Info "  1. Review D:\~dev\ folder structure"
    Write-Info "  2. Check $reportPath for details"
    Write-Info "  3. Test your key projects still work"
    Write-Info "  4. Clean up empty folders in D:\ root"
}

Write-Info ""
