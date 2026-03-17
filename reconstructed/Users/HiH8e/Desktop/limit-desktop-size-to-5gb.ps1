<#
limit-desktop-size-to-5gb.ps1

Purpose:
  - Keep your active Desktop folder (which may be OneDrive\Desktop) under a configurable size limit by moving items to a local folder (default on D:).
  - Safe by default: supports -DryRun and backs up moved items by moving them to the target folder.

Usage examples:
  # Dry run (shows what would be moved)
  pwsh -ExecutionPolicy Bypass -File "C:\Users\HiH8e\Desktop\limit-desktop-size-to-5gb.ps1" -DryRun

  # Actually perform moves with confirmation prompts
  pwsh -ExecutionPolicy Bypass -File "C:\Users\HiH8e\Desktop\limit-desktop-size-to-5gb.ps1"

  # Perform moves non-interactively (auto confirm)
  pwsh -ExecutionPolicy Bypass -File "C:\Users\HiH8e\Desktop\limit-desktop-size-to-5gb.ps1" -AutoConfirm

Parameters:
  -SizeLimitGB: target maximum Desktop size in GB (default 5)
  -TargetRoot: destination root for moved items (default D:\LocalDesktop)
  -DryRun: shows planned moves without performing them
  -AutoConfirm: skip prompts and perform moves

Notes:
  - The script moves top-level items (files and directories) from Desktop to the target folder until the Desktop total size is <= SizeLimitGB.
  - It prefers moving largest items first to reach the target quickly.
  - If D: does not exist or is not writable, change -TargetRoot to another local path.
#>

param(
    [int]$SizeLimitGB = 5,
    [string]$TargetRoot = 'D:\LocalDesktop',
    [switch]$DryRun,
    [switch]$AutoConfirm
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-FolderSizeBytes([string]$path){
    if(-not (Test-Path $path)) { return 0 }
    $items = Get-ChildItem -LiteralPath $path -Recurse -Force -ErrorAction SilentlyContinue | Where-Object { -not $_.PSIsContainer }
    if($items.Count -eq 0){ return 0 }
    return ($items | Measure-Object -Property Length -Sum).Sum
}

function Confirm-OrAuto([string]$msg){
    if($AutoConfirm){ return $true }
    $r = Read-Host "$msg [y/N]"
    return $r -match '^[Yy]'
}

$desktop = [Environment]::GetFolderPath('Desktop')
Write-Host "Active Desktop path: $desktop"
Write-Host "Target root for moved items: $TargetRoot"
Write-Host "Size limit: $SizeLimitGB GB"

if(-not (Test-Path $desktop)){
    Write-Error "Desktop path not found: $desktop"; exit 1
}

if(-not (Test-Path $TargetRoot)){
    if($DryRun){
        Write-Host "(DryRun) Would create target folder: $TargetRoot"
    } else {
        if(Confirm-OrAuto("Target folder $TargetRoot does not exist. Create it?")){
            New-Item -Path $TargetRoot -ItemType Directory -Force | Out-Null
            Write-Host "Created $TargetRoot"
        } else {
            Write-Error "Target folder required. Exiting."; exit 1
        }
    }
}

$limitBytes = [int64]($SizeLimitGB * 1GB)
$currentBytes = Get-FolderSizeBytes $desktop
Write-Host "Current Desktop size: $([math]::Round($currentBytes/1GB,2)) GB"

if($currentBytes -le $limitBytes){
    Write-Host "Desktop is within the size limit. No action required."; exit 0
}

# Gather top-level items on Desktop (files and directories) with sizes
$topItems = Get-ChildItem -LiteralPath $desktop -Force -ErrorAction SilentlyContinue | Where-Object { $_.Name -notin @('desktop.ini','$RECYCLE.BIN') }

$entryList = @()
foreach($it in $topItems){
    if($it.PSIsContainer){
        $size = Get-FolderSizeBytes $it.FullName
    } else {
        $size = $it.Length
    }
    $entryList += [PSCustomObject]@{ Path = $it.FullName; Name = $it.Name; IsDir = $it.PSIsContainer; SizeBytes = $size; LastWrite = $it.LastWriteTime }
}

# Sort by Size descending (move largest first). You can change to LastWrite ascending if you prefer oldest first.
$entryList = $entryList | Sort-Object -Property SizeBytes -Descending

$toMove = @()
$bytesToFree = $currentBytes - $limitBytes
Write-Host "Need to free: $([math]::Round($bytesToFree/1GB,2)) GB"

$acc = 0
foreach($e in $entryList){
    if($acc -ge $bytesToFree){ break }
    # skip zero-size entries
    if($e.SizeBytes -le 0){ continue }
    $toMove += $e
    $acc += $e.SizeBytes
}

Write-Host "Planned moves: $($toMove.Count) item(s) totalling $([math]::Round($acc/1GB,2)) GB"

foreach($m in $toMove){
    $src = $m.Path
    $dest = Join-Path $TargetRoot $m.Name
    if(Test-Path $dest){
        # avoid name collision
        $base = [IO.Path]::GetFileNameWithoutExtension($m.Name)
        $ext = [IO.Path]::GetExtension($m.Name)
        $n = 1
        do { $dest = Join-Path $TargetRoot ("{0}-{1}{2}" -f $base,$n,$ext); $n++ } while(Test-Path $dest)
    }
    if($DryRun){
        Write-Host "[DryRun] Would move: $src -> $dest (Size: $([math]::Round($m.SizeBytes/1GB,2)) GB)"
    } else {
        if(Confirm-OrAuto("Move $src -> $dest ?")){
            try{
                Move-Item -LiteralPath $src -Destination $dest -Force
                Write-Host "Moved: $src -> $dest"
            }catch{
                Write-Warning "Failed to move $src : $_"
            }
        } else {
            Write-Host "Skipped: $src"
        }
    }
}

if(-not $DryRun){
    $newBytes = Get-FolderSizeBytes $desktop
    Write-Host "New Desktop size: $([math]::Round($newBytes/1GB,2)) GB"
    if($newBytes -gt $limitBytes){
        Write-Warning "Desktop still exceeds limit. Consider increasing target folder or moving more files manually."
    } else {
        Write-Host "Desktop is now within the limit."
    }
}

Write-Host "Done. If you want this to run on a schedule, we can create a Scheduled Task to run weekly or at logon."
