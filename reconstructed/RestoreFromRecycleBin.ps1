# PowerShell script to restore files from Recycle Bin for git changes

param(
    [string]$GitRepo = ".",
    [switch]$WhatIf = $false
)

# Function to get Recycle Bin items
function Get-RecycleBinItems {
    $recycleBinPath = "$env:SystemDrive\`$Recycle.Bin"
    $userSid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $userRecycleBin = Join-Path $recycleBinPath $userSid
    
    if (Test-Path $userRecycleBin) {
        $items = Get-ChildItem $userRecycleBin -File | Where-Object { $_.Name -like '$I*' }
        return $items
    }
    return @()
}

# Function to restore file from Recycle Bin
function Restore-RecycleBinFile {
    param(
        [string]$OriginalPath,
        [string]$RecycleBinItemPath
    )
    
    try {
        $directory = Split-Path $OriginalPath -Parent
        if (!(Test-Path $directory)) {
            New-Item -ItemType Directory -Path $directory -Force | Out-Null
        }
        
        # Get the corresponding data file ($R instead of $I)
        $dataFile = $RecycleBinItemPath -replace '^\$I', '$R'
        
        if (Test-Path $dataFile) {
            if ($WhatIf) {
                Write-Host "Would restore: $dataFile -> $OriginalPath" -ForegroundColor Yellow
            } else {
                Copy-Item $dataFile $OriginalPath -Force
                Write-Host "Restored: $OriginalPath" -ForegroundColor Green
            }
            return $true
        }
    }
    catch {
        Write-Warning "Failed to restore $OriginalPath : $($_.Exception.Message)"
    }
    return $false
}

# Function to parse original path from metadata file
function Get-OriginalPath {
    param([string]$MetadataFile)
    
    try {
        $bytes = [System.IO.File]::ReadAllBytes($MetadataFile)
        if ($bytes.Length -gt 24) {
            # Skip header and read Unicode string
            $pathBytes = $bytes[24..($bytes.Length-1)]
            $originalPath = [System.Text.Encoding]::Unicode.GetString($pathBytes).TrimEnd("`0")
            return $originalPath
        }
    }
    catch {
        Write-Warning "Could not parse metadata from $MetadataFile"
    }
    return $null
}

# Main execution
Write-Host "Scanning for git changes and Recycle Bin items..." -ForegroundColor Cyan

# Get git changed files
Push-Location $GitRepo
try {
    $gitChanges = @()
    
    # Get staged changes
    $staged = git diff --cached --name-only 2>$null
    if ($staged) { $gitChanges += $staged }
    
    # Get unstaged changes
    $unstaged = git diff --name-only 2>$null
    if ($unstaged) { $gitChanges += $unstaged }
    
    # Get untracked files
    $untracked = git ls-files --others --exclude-standard 2>$null
    if ($untracked) { $gitChanges += $untracked }
    
    $gitChanges = $gitChanges | Sort-Object -Unique
}
catch {
    Write-Warning "Error getting git status: $($_.Exception.Message)"
    $gitChanges = @()
}
finally {
    Pop-Location
}

# Get Recycle Bin items
$recycleBinItems = Get-RecycleBinItems

Write-Host "Found $($gitChanges.Count) git changes and $($recycleBinItems.Count) Recycle Bin items" -ForegroundColor Cyan

$restoredCount = 0

foreach ($item in $recycleBinItems) {
    $originalPath = Get-OriginalPath $item.FullName
    
    if ($originalPath) {
        # Check if this file matches any git changes
        $relativePath = $originalPath -replace '^[A-Z]:\\', '' -replace '\\', '/'
        $matchingGitFile = $gitChanges | Where-Object { $_ -like "*$relativePath*" -or $relativePath -like "*$_*" }
        
        if ($matchingGitFile) {
            Write-Host "`nFound match:" -ForegroundColor Magenta
            Write-Host "  Git file: $matchingGitFile" -ForegroundColor White
            Write-Host "  Recycle Bin: $originalPath" -ForegroundColor White
            
            if (Restore-RecycleBinFile -OriginalPath $originalPath -RecycleBinItemPath $item.FullName) {
                $restoredCount++
            }
        }
    }
}

Write-Host "`nOperation completed. Restored $restoredCount files." -ForegroundColor Cyan

if ($WhatIf) {
    Write-Host "This was a preview. Remove -WhatIf to actually restore files." -ForegroundColor Yellow
}
