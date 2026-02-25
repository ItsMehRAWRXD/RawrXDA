# Complete Drive Organization Script - C: and D: Drives
# This script will organize both C: and D: drives and create a comprehensive TOC

Write-Host "🚀 Starting Complete Drive Organization (C: and D:)..." -ForegroundColor Green

# Create the main organization structure on D: drive
$folders = @(
    "01-AI-Models",
    "02-IDE-Projects", 
    "03-Tools-Utilities",
    "04-Compilers",
    "05-Tests-Debug",
    "06-Documentation",
    "07-Scripts-PowerShell",
    "08-Web-Frontend",
    "09-Backend-Services",
    "10-Data-Configs",
    "11-Temp-Working",
    "12-Archives-Backups",
    "13-Recovery-Files",
    "14-Desktop-Files",
    "15-Downloads-Files"
)

# Create all folders on D: drive
foreach ($folder in $folders) {
    $path = "D:\$folder"
    if (!(Test-Path $path)) {
        New-Item -ItemType Directory -Path $path -Force
        Write-Host "✅ Created folder: $folder" -ForegroundColor Cyan
    }
}

# Function to categorize a file
function Get-FileCategory {
    param($fileName, $filePath)
    
    $fileName = $fileName.ToLower()
    $filePath = $filePath.ToLower()
    
    # Special handling for desktop and downloads
    if ($filePath -like "*desktop*") { return "14-Desktop-Files" }
    if ($filePath -like "*downloads*") { return "15-Downloads-Files" }
    if ($filePath -like "*recovery*" -or $filePath -like "*backup*") { return "13-Recovery-Files" }
    
    # File categorization rules
    $fileCategories = @{
        "01-AI-Models" = @("ollama", "model", "ai", "neural", "llm", "chat", "copilot", "agent", "bigdaddyg", "rawrz", "cursor", "kimi")
        "02-IDE-Projects" = @("ide", "editor", "vscode", "cursor", "mycopilot", "workspace", "project", "bigdaddyg-cursor-extension")
        "03-Tools-Utilities" = @("tool", "utility", "manager", "launcher", "setup", "install", "batch", "exe", "portable")
        "04-Compilers" = @("compiler", "build", "make", "nasm", "asm", "c", "cpp", "java", "python", "rust", "go", "msbuild")
        "05-Tests-Debug" = @("test", "debug", "benchmark", "verify", "check", "validate", "spec")
        "06-Documentation" = @("readme", "guide", "manual", "doc", "md", "txt", "summary", "report", "changelog")
        "07-Scripts-PowerShell" = @("ps1", "bat", "sh", "cmd", "script", "hook")
        "08-Web-Frontend" = @("html", "css", "js", "json", "web", "frontend", "ui", "react", "vue", "angular")
        "09-Backend-Services" = @("server", "api", "backend", "service", "proxy", "daemon", "lambda")
        "10-Data-Configs" = @("config", "json", "yaml", "xml", "ini", "cfg", "data", "log", "settings")
        "11-Temp-Working" = @("temp", "tmp", "working", "scratch", "draft", "untitled")
        "12-Archives-Backups" = @("backup", "archive", "zip", "rar", "7z", "tar", "gz", "bak")
    }
    
    foreach ($category in $fileCategories.GetEnumerator()) {
        foreach ($keyword in $category.Value) {
            if ($fileName -like "*$keyword*") {
                return $category.Key
            }
        }
    }
    
    # Default category based on extension
    $extension = [System.IO.Path]::GetExtension($fileName).ToLower()
    switch ($extension) {
        {$_ -in @(".ps1", ".bat", ".sh", ".cmd")} { return "07-Scripts-PowerShell" }
        {$_ -in @(".html", ".css", ".js", ".json")} { return "08-Web-Frontend" }
        {$_ -in @(".md", ".txt", ".doc", ".docx")} { return "06-Documentation" }
        {$_ -in @(".exe", ".msi", ".dll")} { return "03-Tools-Utilities" }
        {$_ -in @(".zip", ".rar", ".7z", ".tar", ".gz")} { return "12-Archives-Backups" }
        default { return "11-Temp-Working" }
    }
}

# Function to safely move files
function Move-FileSafely {
    param($sourcePath, $destinationPath)
    
    try {
        # Handle duplicate names
        $counter = 1
        $originalDestination = $destinationPath
        while (Test-Path $destinationPath) {
            $nameWithoutExt = [System.IO.Path]::GetFileNameWithoutExtension($originalDestination)
            $extension = [System.IO.Path]::GetExtension($originalDestination)
            $directory = [System.IO.Path]::GetDirectoryName($originalDestination)
            $destinationPath = Join-Path $directory "$($nameWithoutExt)_$counter$extension"
            $counter++
        }
        
        Move-Item $sourcePath $destinationPath -Force
        return $destinationPath
    }
    catch {
        throw "Failed to move file: $($_.Exception.Message)"
    }
}

# Scan both drives
$drives = @("C:\", "D:\")
$allMovedFiles = @()
$allErrors = @()

foreach ($drive in $drives) {
    Write-Host "`n🔍 Scanning $drive drive..." -ForegroundColor Yellow
    
    # Get files from various locations on C: drive
    $searchPaths = @()
    if ($drive -eq "C:\") {
        $searchPaths = @(
            "$env:USERPROFILE\Desktop",
            "$env:USERPROFILE\Downloads", 
            "$env:USERPROFILE\Documents",
            "C:\temp",
            "C:\tmp",
            "C:\Users\Public\Desktop",
            "C:\ProgramData\Microsoft\Windows\Start Menu\Programs"
        )
    } else {
        $searchPaths = @("D:\")
    }
    
    $filesToProcess = @()
    foreach ($searchPath in $searchPaths) {
        if (Test-Path $searchPath) {
            $files = Get-ChildItem $searchPath -File -Recurse -ErrorAction SilentlyContinue | 
                     Where-Object { $_.Name -notlike "organize_*" -and $_.Name -notlike "TABLE_OF_CONTENTS*" }
            $filesToProcess += $files
        }
    }
    
    Write-Host "📁 Found $($filesToProcess.Count) files on $drive" -ForegroundColor Cyan
    
    foreach ($file in $filesToProcess) {
        try {
            $category = Get-FileCategory $file.Name $file.FullName
            $destinationDir = "D:\$category"
            $destinationPath = Join-Path $destinationDir $file.Name
            
            $finalPath = Move-FileSafely $file.FullName $destinationPath
            
            $allMovedFiles += [PSCustomObject]@{
                OriginalName = $file.Name
                OriginalPath = $file.FullName
                Category = $category
                NewPath = $finalPath
                Size = $file.Length
                SourceDrive = $drive
            }
            Write-Host "📦 Moved: $($file.Name) → $category" -ForegroundColor Green
        }
        catch {
            $allErrors += "❌ Failed to move $($file.FullName): $($_.Exception.Message)"
            Write-Host "❌ Error moving $($file.Name): $($_.Exception.Message)" -ForegroundColor Red
        }
    }
}

# Create comprehensive Table of Contents
$tocPath = "D:\COMPLETE_TABLE_OF_CONTENTS.txt"
$toc = @"
╔══════════════════════════════════════════════════════════════════════════════╗
║                    COMPLETE DRIVE ORGANIZATION TABLE OF CONTENTS            ║
║                        Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')                    ║
║                           C: and D: Drives Cleaned Up                       ║
╚══════════════════════════════════════════════════════════════════════════════╝

📊 ORGANIZATION SUMMARY:
   • Total Files Organized: $($allMovedFiles.Count)
   • Categories Created: $($folders.Count)
   • Errors: $($allErrors.Count)
   • C: Drive Files: $(($allMovedFiles | Where-Object { $_.SourceDrive -eq 'C:\' }).Count)
   • D: Drive Files: $(($allMovedFiles | Where-Object { $_.SourceDrive -eq 'D:\' }).Count)

📁 FOLDER STRUCTURE ON D: DRIVE:
"@

foreach ($folder in $folders) {
    $fileCount = (Get-ChildItem "D:\$folder" -File -ErrorAction SilentlyContinue).Count
    $toc += "`n   $folder ($fileCount files)"
}

$toc += "`n`n📋 DETAILED FILE LISTING BY CATEGORY:`n"

# Group files by category
$groupedFiles = $allMovedFiles | Group-Object Category | Sort-Object Name

foreach ($group in $groupedFiles) {
    $toc += "`n╔══════════════════════════════════════════════════════════════════════════════╗`n"
    $toc += "║ $($group.Name.PadRight(78)) ║`n"
    $toc += "╚══════════════════════════════════════════════════════════════════════════════╝`n"
    
    foreach ($file in $group.Group | Sort-Object OriginalName) {
        $sizeKB = [math]::Round($file.Size / 1KB, 2)
        $driveIndicator = if ($file.SourceDrive -eq 'C:\') { '[C:]' } else { '[D:]' }
        $toc += "   • $($file.OriginalName.PadRight(45)) $driveIndicator ($sizeKB KB)`n"
    }
}

# Add source breakdown
$toc += "`n`n📊 FILES BY SOURCE LOCATION:`n"
$sourceBreakdown = $allMovedFiles | Group-Object SourceDrive | Sort-Object Name
foreach ($source in $sourceBreakdown) {
    $toc += "   $($source.Name) Drive: $($source.Count) files`n"
}

# Add errors if any
if ($allErrors.Count -gt 0) {
    $toc += "`n`n❌ ERRORS ENCOUNTERED:`n"
    foreach ($error in $allErrors) {
        $toc += "   $error`n"
    }
}

# Add quick access section
$toc += "`n`n🚀 QUICK ACCESS COMMANDS:`n"
$toc += "   • Open IDE Projects: cd D:\02-IDE-Projects`n"
$toc += "   • Open AI Models: cd D:\01-AI-Models`n"
$toc += "   • Open Desktop Files: cd D:\14-Desktop-Files`n"
$toc += "   • Open Downloads: cd D:\15-Downloads-Files`n"
$toc += "   • Open Recovery Files: cd D:\13-Recovery-Files`n"
$toc += "   • Open Scripts: cd D:\07-Scripts-PowerShell`n"

# Add HTML file quick access
$htmlFiles = $allMovedFiles | Where-Object { $_.OriginalName -like "*.html" }
if ($htmlFiles.Count -gt 0) {
    $toc += "`n`n🌐 HTML FILES (Ready to Open):`n"
    foreach ($htmlFile in $htmlFiles) {
        $toc += "   • $($htmlFile.OriginalName) → $($htmlFile.NewPath)`n"
    }
}

# Write TOC to file
$toc | Out-File -FilePath $tocPath -Encoding UTF8

Write-Host "`n✅ COMPLETE ORGANIZATION FINISHED!" -ForegroundColor Green
Write-Host "📄 Table of Contents created: $tocPath" -ForegroundColor Cyan
Write-Host "📊 Total files organized: $($allMovedFiles.Count)" -ForegroundColor Yellow
Write-Host "❌ Errors: $($allErrors.Count)" -ForegroundColor Red
Write-Host "🌐 HTML files ready to open: $(($allMovedFiles | Where-Object { $_.OriginalName -like '*.html' }).Count)" -ForegroundColor Magenta

# Open the TOC file
Start-Process notepad.exe $tocPath

Write-Host "`n🎉 Both C: and D: drives are now completely organized!" -ForegroundColor Magenta
Write-Host "💡 All your scattered files from the PC crash are now properly sorted!" -ForegroundColor Yellow
