# Desktop Organization Script
# Organizes all files on Desktop into categorized folders

$desktopPath = "C:\Users\HiH8e\OneDrive\Desktop"
Set-Location $desktopPath

Write-Host "Starting Desktop Organization..." -ForegroundColor Cyan

# Create organizational folders
$folders = @(
    "IDE-Projects",
    "Documentation",
    "PowerShell-Scripts",
    "JavaScript-Tools",
    "HTML-Files",
    "Configuration-Files",
    "Source-Code",
    "Screenshots",
    "Text-Notes",
    "Models-AI",
    "Duplicates-Archive",
    "Node-Projects",
    "CPP-Projects",
    "Java-Projects",
    "Python-Scripts"
)

foreach ($folder in $folders) {
    $folderPath = Join-Path $desktopPath $folder
    if (-not (Test-Path $folderPath)) {
        New-Item -ItemType Directory -Path $folderPath -Force | Out-Null
        Write-Host "Created folder: $folder" -ForegroundColor Green
    }
}

Write-Host "`nOrganizing files..." -ForegroundColor Cyan

# Function to move files safely
function Move-FilesSafely {
    param(
        [string]$Pattern,
        [string]$Destination,
        [string]$Description
    )
    
    $files = Get-ChildItem -Path $desktopPath -Filter $Pattern -File -ErrorAction SilentlyContinue
    if ($files) {
        foreach ($file in $files) {
            try {
                Move-Item -Path $file.FullName -Destination (Join-Path $desktopPath $Destination) -Force
                Write-Host "  Moved: $($file.Name)" -ForegroundColor Gray
            } catch {
                Write-Host "  Error moving $($file.Name): $_" -ForegroundColor Red
            }
        }
        Write-Host "Moved $($files.Count) $Description" -ForegroundColor Green
    }
}

# Move Documentation (MD files)
Write-Host "`n--- Documentation Files ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.md" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "Documentation") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Move PowerShell Scripts
Write-Host "`n--- PowerShell Scripts ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.ps1" -File | Where-Object { $_.Name -ne "organize-desktop.ps1" } | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "PowerShell-Scripts") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Move JavaScript files
Write-Host "`n--- JavaScript Files ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.js" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "JavaScript-Tools") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Move HTML files
Write-Host "`n--- HTML Files ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.html" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "HTML-Files") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Move Python files
Write-Host "`n--- Python Files ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.py" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "Python-Scripts") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Move Java files
Write-Host "`n--- Java Files ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.java" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "Java-Projects") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Move C++ files
Write-Host "`n--- C++ Files ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.cpp" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "CPP-Projects") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}
Get-ChildItem -Path $desktopPath -Filter "*.hpp" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "CPP-Projects") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Move C# files
Write-Host "`n--- C# Files ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.cs" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "Source-Code") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Move configuration files
Write-Host "`n--- Configuration Files ---" -ForegroundColor Yellow
$configExtensions = @("*.json", "*.gradle", "*.iml", "*.css", "*.ini")
foreach ($ext in $configExtensions) {
    Get-ChildItem -Path $desktopPath -Filter $ext -File | ForEach-Object {
        Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "Configuration-Files") -Force
        Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
    }
}

# Move text files
Write-Host "`n--- Text Files ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.txt" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "Text-Notes") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Move screenshots
Write-Host "`n--- Screenshots ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.png" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "Screenshots") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Move shortcuts
Write-Host "`n--- Shortcuts ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "*.lnk" -File | ForEach-Object {
    # Keep shortcuts on desktop root
    Write-Host "  Keeping shortcut: $($_.Name)" -ForegroundColor Cyan
}

# Move Modelfiles
Write-Host "`n--- Model Files ---" -ForegroundColor Yellow
Get-ChildItem -Path $desktopPath -Filter "Modelfile*" -File | ForEach-Object {
    Move-Item -Path $_.FullName -Destination (Join-Path $desktopPath "Models-AI") -Force
    Write-Host "  Moved: $($_.Name)" -ForegroundColor Gray
}

# Organize folders
Write-Host "`n--- Organizing Folders ---" -ForegroundColor Yellow

# Move IDE-related folders
$ideFolders = @("BigDaddyG-IDE-cursor-find-most-complete-commit-across-branches-8387", "Phase4UltraKit", "agentic_ide", "hmm", "stuff")
foreach ($folder in $ideFolders) {
    $folderPath = Join-Path $desktopPath $folder
    if (Test-Path $folderPath) {
        Move-Item -Path $folderPath -Destination (Join-Path $desktopPath "IDE-Projects") -Force
        Write-Host "  Moved folder: $folder" -ForegroundColor Gray
    }
}

# Move C++ project folders
$cppFolders = @("cpp_ide", "cpp_ide_advanced")
foreach ($folder in $cppFolders) {
    $folderPath = Join-Path $desktopPath $folder
    if (Test-Path $folderPath) {
        Move-Item -Path $folderPath -Destination (Join-Path $desktopPath "CPP-Projects") -Force
        Write-Host "  Moved folder: $folder" -ForegroundColor Gray
    }
}

# Move node_modules to Node-Projects
$nodeFolders = @("node_modules", "node_modules - Copy", "node_modules - Copy (2)")
foreach ($folder in $nodeFolders) {
    $folderPath = Join-Path $desktopPath $folder
    if (Test-Path $folderPath) {
        Move-Item -Path $folderPath -Destination (Join-Path $desktopPath "Node-Projects") -Force
        Write-Host "  Moved folder: $folder" -ForegroundColor Gray
    }
}

# Move duplicate .idea folders
$ideaFolders = @(".idea - Copy", ".idea - Copy (2)")
foreach ($folder in $ideaFolders) {
    $folderPath = Join-Path $desktopPath $folder
    if (Test-Path $folderPath) {
        Move-Item -Path $folderPath -Destination (Join-Path $desktopPath "Duplicates-Archive") -Force
        Write-Host "  Moved folder: $folder" -ForegroundColor Gray
    }
}

# Move other folders
$otherFolders = @("agent", "scripts", "examples", "OllamaTools", "vsix", "src", "project")
foreach ($folder in $otherFolders) {
    $folderPath = Join-Path $desktopPath $folder
    if (Test-Path $folderPath) {
        $destPath = switch ($folder) {
            "agent" { "PowerShell-Scripts" }
            "scripts" { "JavaScript-Tools" }
            "examples" { "Documentation" }
            "OllamaTools" { "PowerShell-Scripts" }
            "vsix" { "IDE-Projects" }
            "src" { "Source-Code" }
            "project" { "IDE-Projects" }
            default { "IDE-Projects" }
        }
        Move-Item -Path $folderPath -Destination (Join-Path $desktopPath $destPath) -Force
        Write-Host "  Moved folder: $folder to $destPath" -ForegroundColor Gray
    }
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Desktop Organization Complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan

# Show summary
Write-Host "`nOrganization Summary:" -ForegroundColor Yellow
foreach ($folder in $folders) {
    $folderPath = Join-Path $desktopPath $folder
    $count = (Get-ChildItem -Path $folderPath -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
    Write-Host "  $folder : $count files" -ForegroundColor Cyan
}
