# 🔍 SYSTEM-WIDE DISCOVERY SCRIPT
# This script will map ALL development projects across your entire system

Write-Host "🌐 STARTING COMPREHENSIVE SYSTEM DISCOVERY..." -ForegroundColor Cyan
Write-Host "Scanning: C:\ drive, D:\ drive, Desktop, and all development locations" -ForegroundColor Yellow

# Initialize results array
$DiscoveryResults = @()
$TotalProjects = 0
$TotalFiles = 0
$TotalSize_GB = 0

# Define comprehensive search locations
$SearchLocations = @(
    'C:\',
    'C:\dev',
    'C:\Development', 
    'C:\Projects',
    'C:\Code',
    'C:\Source',
    'D:\',
    'D:\BIGDADDYG-RECOVERY',
    'D:\13-Recovery-Files',
    "$env:USERPROFILE\dev",
    "$env:USERPROFILE\Development",
    "$env:USERPROFILE\Projects",
    "$env:USERPROFILE\source",
    "$env:USERPROFILE\OneDrive\Desktop",
    'C:\Users\HiH8e\OneDrive\Desktop'
)

# Comprehensive project type detection patterns
$ProjectPatterns = @{
    'Mirai-Bot' = @('mirai', 'loader', 'dlr', 'bot', '*.c', '*.cpp', '*.h', 'CMakeLists.txt')
    'FUD-Security' = @('fud', 'payload', 'crypt', 'security', '*.py', 'requirements.txt')
    'RawrZ-Platform' = @('rawrz', 'BRxC', 'dashboard', '*.html', 'platform')
    'Beast-AI' = @('beast', 'swarm', 'ai', 'ml', 'training', 'model')
    'BigDaddyG' = @('bigdaddyg', 'launcher', 'integration')
    'Python-Projects' = @('*.py', 'main.py', 'setup.py', 'requirements.txt', '__init__.py')
    'Web-Projects' = @('*.html', '*.css', '*.js', 'index.html', 'package.json', 'node_modules')
    'C/C++' = @('*.c', '*.cpp', '*.h', '*.hpp', 'CMakeLists.txt', 'Makefile', '*.vcxproj')
    'PowerShell' = @('*.ps1', '*.psm1', '*.psd1', '*.bat')
    'Documentation' = @('*.md', 'README*', 'CHANGELOG*', '*.txt')
    'Build-Tools' = @('build', 'make', 'cmake', 'msbuild', '*.sln', '*.proj')
    'Recovery-Data' = @('recovery', 'backup', 'archive', 'audit')
}

function Test-ProjectType {
    param($Path, $Patterns)
    
    $detectedTypes = @()
    $fileCount = 0
    
    try {
        foreach ($type in $Patterns.Keys) {
            $matchFound = $false
            
            foreach ($pattern in $Patterns[$type]) {
                # Check if folder name matches pattern
                if ($Path -like "*$pattern*") {
                    $detectedTypes += $type
                    $matchFound = $true
                    break
                }
                
                # Check if pattern is a file extension or specific file
                if ($pattern -like "*.*") {
                    $files = Get-ChildItem $Path -Filter $pattern -ErrorAction SilentlyContinue
                    if ($files) {
                        $detectedTypes += $type
                        $fileCount += $files.Count
                        $matchFound = $true
                        break
                    }
                }
            }
            
            if ($matchFound) { continue }
        }
    }
    catch {
        # Ignore errors, continue scanning
    }
    
    return @{
        Types = $detectedTypes
        FileCount = $fileCount
    }
}

Write-Host "`n📂 SCANNING SYSTEM LOCATIONS..." -ForegroundColor Cyan

foreach ($location in $SearchLocations) {
    if (Test-Path $location) {
        Write-Host "🔍 Scanning: $location" -ForegroundColor Yellow
        
        try {
            # Get directories up to 3 levels deep to avoid infinite recursion
            $directories = @()
            
            # Scan root level first
            $rootDirs = Get-ChildItem $location -Directory -ErrorAction SilentlyContinue | Where-Object { 
                $_.Name -notlike "Windows*" -and 
                $_.Name -notlike "Program Files*" -and 
                $_.Name -notlike "System*" -and
                $_.Name -ne '$Recycle.Bin' -and
                $_.Name -ne 'hiberfil.sys'
            }
            $directories += $rootDirs
            
            # Scan one level deeper for major directories
            foreach ($rootDir in $rootDirs) {
                try {
                    $subDirs = Get-ChildItem $rootDir.FullName -Directory -ErrorAction SilentlyContinue | Select-Object -First 10
                    $directories += $subDirs
                }
                catch { continue }
            }
            
            foreach ($dir in $directories) {
                $projectInfo = Test-ProjectType -Path $dir.FullName -Patterns $ProjectPatterns
                
                if ($projectInfo.Types.Count -gt 0) {
                    try {
                        $size = (Get-ChildItem $dir.FullName -Recurse -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum
                        $size_GB = [math]::Round($size / 1GB, 3)
                        
                        $fileCount = (Get-ChildItem $dir.FullName -File -Recurse -ErrorAction SilentlyContinue | Measure-Object).Count
                        
                        $result = [PSCustomObject]@{
                            ProjectName = $dir.Name
                            FullPath = $dir.FullName
                            ProjectTypes = ($projectInfo.Types | Sort-Object -Unique) -join ', '
                            FileCount = $fileCount
                            Size_GB = $size_GB
                            LastModified = $dir.LastWriteTime
                            Priority = if ($projectInfo.Types -contains 'Mirai-Bot' -or $projectInfo.Types -contains 'FUD-Security') { "High" } else { "Medium" }
                            Action = "To Be Organized"
                        }
                        
                        $DiscoveryResults += $result
                        $TotalProjects++
                        $TotalFiles += $fileCount
                        $TotalSize_GB += $size_GB
                        
                        Write-Host "  ✅ Found: $($dir.Name) [$($projectInfo.Types -join ',')] ($fileCount files, $size_GB GB)" -ForegroundColor Green
                    }
                    catch {
                        Write-Host "  ⚠️ Access error: $($dir.FullName)" -ForegroundColor Yellow
                    }
                }
            }
        }
        catch {
            Write-Host "❌ Error scanning $location : $($_.Exception.Message)" -ForegroundColor Red
        }
    }
    else {
        Write-Host "⚠️ Location not found: $location" -ForegroundColor Yellow
    }
}

# Special scan for current workspace
Write-Host "`n🎯 DETAILED SCAN OF CURRENT WORKSPACE..." -ForegroundColor Cyan
$currentWorkspace = "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master"

if (Test-Path $currentWorkspace) {
    $workspaceFiles = Get-ChildItem $currentWorkspace -File | Group-Object Extension | Sort-Object Count -Descending
    
    Write-Host "📊 Current Workspace File Analysis:" -ForegroundColor Yellow
    foreach ($group in $workspaceFiles) {
        $ext = if ($group.Name) { $group.Name } else { "No Extension" }
        Write-Host "  $ext : $($group.Count) files" -ForegroundColor White
    }
    
    # Count documentation files
    $mdFiles = (Get-ChildItem $currentWorkspace -Filter "*.md" | Measure-Object).Count
    $psFiles = (Get-ChildItem $currentWorkspace -Filter "*.ps1" | Measure-Object).Count
    $pyFiles = (Get-ChildItem $currentWorkspace -Filter "*.py" | Measure-Object).Count
    $htmlFiles = (Get-ChildItem $currentWorkspace -Filter "*.html" | Measure-Object).Count
    
    Write-Host "`n📈 Key File Types in Workspace:" -ForegroundColor Cyan
    Write-Host "  📝 Markdown files: $mdFiles" -ForegroundColor White
    Write-Host "  🔧 PowerShell scripts: $psFiles" -ForegroundColor White  
    Write-Host "  🐍 Python files: $pyFiles" -ForegroundColor White
    Write-Host "  🌐 HTML files: $htmlFiles" -ForegroundColor White
}

# Generate summary report
Write-Host "`n📊 DISCOVERY SUMMARY REPORT" -ForegroundColor Green
Write-Host "================================" -ForegroundColor Green
Write-Host "Total Projects Found: $TotalProjects" -ForegroundColor Cyan
Write-Host "Total Files: $TotalFiles" -ForegroundColor Cyan
Write-Host "Total Size: $([math]::Round($TotalSize_GB, 2)) GB" -ForegroundColor Cyan

# Show top project types
$topTypes = $DiscoveryResults | ForEach-Object { $_.ProjectTypes -split ', ' } | Where-Object { $_ } | Group-Object | Sort-Object Count -Descending | Select-Object -First 10

Write-Host "`n🎯 TOP PROJECT TYPES FOUND:" -ForegroundColor Yellow
foreach ($type in $topTypes) {
    Write-Host "  $($type.Name): $($type.Count) projects" -ForegroundColor White
}

# Show high priority projects
$highPriorityProjects = $DiscoveryResults | Where-Object { $_.Priority -eq "High" }
if ($highPriorityProjects) {
    Write-Host "`n🔥 HIGH PRIORITY PROJECTS (Need immediate attention):" -ForegroundColor Red
    foreach ($project in $highPriorityProjects) {
        Write-Host "  📁 $($project.ProjectName) - $($project.ProjectTypes) ($($project.Size_GB) GB)" -ForegroundColor White
        Write-Host "     📍 $($project.FullPath)" -ForegroundColor Gray
    }
}

# Export results
$timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
$csvFile = "System-Discovery-Results_$timestamp.csv"
$jsonFile = "System-Discovery-Results_$timestamp.json"
$reportFile = "System-Discovery-Report_$timestamp.md"

$DiscoveryResults | Export-Csv $csvFile -NoTypeInformation
$DiscoveryResults | ConvertTo-Json | Out-File $jsonFile -Encoding UTF8

# Create markdown report
$reportContent = @"
# 🔍 System-Wide Discovery Report
**Generated**: $(Get-Date)
**Total Projects**: $TotalProjects
**Total Files**: $TotalFiles  
**Total Size**: $([math]::Round($TotalSize_GB, 2)) GB

## 📊 Project Summary
| Project Name | Type | Files | Size (GB) | Priority | Location |
|--------------|------|-------|-----------|----------|-----------|
"@

foreach ($result in $DiscoveryResults | Sort-Object Priority, Size_GB -Descending) {
    $reportContent += "`n| $($result.ProjectName) | $($result.ProjectTypes) | $($result.FileCount) | $($result.Size_GB) | $($result.Priority) | $($result.FullPath) |"
}

$reportContent += @"

## 🎯 Recommended Actions
1. **High Priority Projects** should be organized first
2. **Large Projects** (>1GB) need special attention
3. **Recent Projects** (modified within 30 days) are likely active

## 📁 Suggested Organization Structure
Based on discovered project types, recommended folder structure:
- **01-Active-Projects/** - Current development work
- **02-Security-Tools/** - FUD suite, security projects  
- **03-Bot-Frameworks/** - Mirai and bot-related projects
- **04-Web-Platforms/** - RawrZ, web dashboards
- **05-AI-ML-Projects/** - Beast Swarm, AI projects
- **06-Recovery-Archive/** - Recovery and backup data
- **07-Build-Tools/** - Build scripts and development tools
- **08-Documentation/** - All documentation and guides

*Report generated by System Discovery Script*
"@

$reportContent | Out-File $reportFile -Encoding UTF8

Write-Host "`n✅ DISCOVERY COMPLETE!" -ForegroundColor Green
Write-Host "📄 Results saved to:" -ForegroundColor Cyan
Write-Host "  📊 CSV: $csvFile" -ForegroundColor White
Write-Host "  📋 JSON: $jsonFile" -ForegroundColor White  
Write-Host "  📝 Report: $reportFile" -ForegroundColor White

Write-Host "`n🚀 NEXT STEPS:" -ForegroundColor Yellow
Write-Host "1. Review the discovery report" -ForegroundColor White
Write-Host "2. Decide on organization strategy" -ForegroundColor White
Write-Host "3. Execute migration plan" -ForegroundColor White

return $DiscoveryResults