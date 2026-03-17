# Move Active Content to Current Folder
# Moves all non-empty folders and files to a "Current" folder, excluding shortcuts and system folders

$desktopPath = "C:\Users\HiH8e\OneDrive\Desktop"
Set-Location $desktopPath

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  ORGANIZING TO 'Current' FOLDER" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Create Current folder
$currentFolder = Join-Path $desktopPath "Current"
if (-not (Test-Path $currentFolder)) {
    New-Item -ItemType Directory -Path $currentFolder -Force | Out-Null
    Write-Host "Created 'Current' folder" -ForegroundColor Green
}

Write-Host ""

# Exclusion list - folders/files to skip
$excludeNames = @(
    "Current",
    "Recycle Bin",
    '$Recycle.Bin',
    "CCleaner",
    "desktop.ini",
    "System Volume Information"
)

# Get all items on Desktop
$allItems = Get-ChildItem -Path $desktopPath -Force -ErrorAction SilentlyContinue

$movedCount = 0
$skippedCount = 0

foreach ($item in $allItems) {
    # Skip if in exclusion list
    if ($excludeNames -contains $item.Name) {
        Write-Host "⊗ Skipping: $($item.Name) (excluded)" -ForegroundColor Gray
        $skippedCount++
        continue
    }
    
    # Skip shortcuts (.lnk files)
    if ($item.Extension -eq '.lnk') {
        Write-Host "⊗ Skipping: $($item.Name) (shortcut)" -ForegroundColor Gray
        $skippedCount++
        continue
    }
    
    # Skip "Learn about this picture" file
    if ($item.Name -like "*Learn about this picture*") {
        Write-Host "⊗ Skipping: $($item.Name) (system file)" -ForegroundColor Gray
        $skippedCount++
        continue
    }
    
    # Check if folder is empty
    if ($item.PSIsContainer) {
        $contents = Get-ChildItem -Path $item.FullName -Recurse -Force -ErrorAction SilentlyContinue
        if ($contents.Count -eq 0) {
            Write-Host "⊗ Skipping: $($item.Name) (empty folder)" -ForegroundColor Yellow
            $skippedCount++
            continue
        }
    }
    
    # Move the item
    try {
        $destPath = Join-Path $currentFolder $item.Name
        
        # Handle name conflicts
        if (Test-Path $destPath) {
            Write-Host "⚠ Conflict: $($item.Name) already exists in Current folder" -ForegroundColor Yellow
            $skippedCount++
            continue
        }
        
        Move-Item -Path $item.FullName -Destination $currentFolder -Force -ErrorAction Stop
        Write-Host "✓ Moved: $($item.Name)" -ForegroundColor Green
        $movedCount++
    }
    catch {
        Write-Host "✗ Error moving $($item.Name): $_" -ForegroundColor Red
        $skippedCount++
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  SUMMARY" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Items moved: $movedCount" -ForegroundColor Green
Write-Host "Items skipped: $skippedCount" -ForegroundColor Yellow
Write-Host ""
Write-Host "Desktop is now organized!" -ForegroundColor Green
Write-Host "All active content is in the 'Current' folder." -ForegroundColor Cyan
