<#
.SYNOPSIS
  Ensures each standalone Desktop file lives inside its own project folder.
.DESCRIPTION
  Iterates over files on the specified Desktop path, creates a folder named after
  each file's base name, and moves the file into that folder.
  Directories are skipped to avoid nesting existing projects.
.PARAMETER SourcePath
  Path to the Desktop to organize. Defaults to the current user's desktop.
.PARAMETER DryRun
  Shows planned actions without moving anything.
.PARAMETER Exclude
  Names to skip during the sweep (e.g., shortcuts you want to keep untouched).
.EXAMPLE
  ./organize-desktop.ps1 -DryRun
  Preview what would happen without touching files.
.EXAMPLE
  ./organize-desktop.ps1 -SourcePath "C:\Users\HiH8e\Desktop" -Exclude "DoNotTouch.txt"
#>
param(
    [string]$SourcePath = [Environment]::GetFolderPath('Desktop'),
    [switch]$DryRun,
    [string[]]$Exclude = @()
)

function Get-SafeFolderName {
    param([string]$Name)
    $safe = $Name -replace '[\\/:*?"<>|]', ''
    if ([string]::IsNullOrWhiteSpace($safe)) { $safe = "Project" }
    return $safe.Trim()
}

if (-not (Test-Path $SourcePath)) {
    Write-Error "Source path '$SourcePath' does not exist."
    exit 1
}

$items = Get-ChildItem -Path $SourcePath -Force | Where-Object {
    # skip hidden/system, the script file itself, and current directory context
    -not $_.Attributes.ToString().Contains('Hidden') -and
    -not $_.Attributes.ToString().Contains('System') -and
    $_.Name -ne 'organize-desktop.ps1'
}

foreach ($item in $items) {
    if ($Exclude -contains $item.Name) {
        Write-Host "Skipping excluded item: $($item.Name)"
        continue
    }

    if ($item.PSIsContainer) {
        Write-Host "Skipping directory (already a project folder): $($item.Name)"
        continue
    }

    $folderName = Get-SafeFolderName -Name $item.BaseName
    $targetFolder = Join-Path $SourcePath $folderName

    if (-not (Test-Path $targetFolder)) {
        if ($DryRun) {
            Write-Host "Would create folder: $folderName"
        }
        else {
            New-Item -Path $targetFolder -ItemType Directory | Out-Null
            Write-Host "Created folder: $folderName"
        }
    }

    $destination = Join-Path $targetFolder $item.Name

    if ($DryRun) {
        Write-Host "Would move $($item.Name) -> $folderName"
        continue
    }

    try {
        Move-Item -Path $item.FullName -Destination $targetFolder -Force
        Write-Host "Moved $($item.Name) -> $folderName"
    }
    catch {
        Write-Warning "Failed to move $($item.Name): $_"
    }
}
