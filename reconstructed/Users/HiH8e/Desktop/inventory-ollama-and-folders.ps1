<#
Inventory script: sizes for Home, New Folder, D:\LocalDesktop, common Ollama dirs, and ollama ls
Improvements added:
- Error handling
- File/dir counts and total size in GB
- DryRun mode (no modifications) and explicit AllowModify flag
- Clear, parseable output
#>

param(
    [switch]$DryRun = $true,
    [switch]$AllowModifyOutsideWorkspace = $false
)

Write-Host '=== START INVENTORY ===' -ForegroundColor Cyan

# Configuration (edit paths if your profile changed)
$desktopHome = 'C:\Users\HiH8e\OneDrive\Desktop\Home'
$desktopNew = 'C:\Users\HiH8e\OneDrive\Desktop\New Folder'
$targetRoot = 'D:\LocalDesktop'

function Get-PathStats([string]$path){
    try{
        if (-not (Test-Path -LiteralPath $path)){
            return [PSCustomObject]@{ Path = $path; Found = $false; FileCount = 0; DirCount = 0; SizeBytes = 0 }
        }

        $items = Get-ChildItem -LiteralPath $path -Recurse -Force -ErrorAction Stop
        $files = $items | Where-Object { -not $_.PSIsContainer }
        $dirs = $items | Where-Object { $_.PSIsContainer }
        $size = ($files | Measure-Object -Property Length -Sum -ErrorAction SilentlyContinue).Sum
        if ($null -eq $size) { $size = 0 }

        return [PSCustomObject]@{
            Path = $path
            Found = $true
            FileCount = ($files | Measure-Object).Count
            DirCount = ($dirs | Measure-Object).Count
            SizeBytes = [int64]$size
        }
    } catch {
        return [PSCustomObject]@{ Path = $path; Found = $false; FileCount = 0; DirCount = 0; SizeBytes = 0; Error = $_.Exception.Message }
    }
}

function Format-Stats($stat){
    if (-not $stat.Found){ return "$($stat.Path) -> (not found)" }
    $gb = [math]::Round($stat.SizeBytes / 1GB, 3)
    return "$($stat.Path) -> $gb GB ($($stat.FileCount) files, $($stat.DirCount) dirs)"
}

Write-Host '--- Desktop folders ---' -ForegroundColor Yellow
Get-PathStats $desktopHome | ForEach-Object { Write-Host (Format-Stats $_) }
Get-PathStats $desktopNew  | ForEach-Object { Write-Host (Format-Stats $_) }
Get-PathStats $targetRoot  | ForEach-Object { Write-Host (Format-Stats $_) }

Write-Host '--- Ollama directories ---' -ForegroundColor Yellow
$paths = @("$env:USERPROFILE\.ollama", "$env:LOCALAPPDATA\ollama", 'C:\ProgramData\ollama')
foreach($p in $paths){ Get-PathStats $p | ForEach-Object { Write-Host (Format-Stats $_) } }

Write-Host '--- Ollama ls (if available) ---' -ForegroundColor Yellow
try{
    $ollamaExe = Get-Command -Name ollama -ErrorAction SilentlyContinue
    if ($null -eq $ollamaExe){ Write-Host 'ollama not found in PATH' } else { & ollama ls }
} catch {
    Write-Host "ollama invocation failed: $($_.Exception.Message)"
}

Write-Host '=== END INVENTORY ===' -ForegroundColor Cyan