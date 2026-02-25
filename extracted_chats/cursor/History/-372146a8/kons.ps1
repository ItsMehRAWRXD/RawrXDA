# Force-unlock stuck files / git lock. Run in PowerShell (Admin) if needed.
# Use when terminal or IDE is stuck and a file or .git/index.lock is locked.
param(
    [string]$RepoPath = "D:\rawrxd",
    [string]$UnlockFile = ""
)

# 1. Kill likely lock holders (no prompts)
Stop-Process -Name "RawrXD*" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "cheesecloth*" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "cl" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "msbuild" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "cmake" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "ninja" -Force -ErrorAction SilentlyContinue
$null = cmd /c "taskkill /F /IM cl.exe 2>nul"
$null = cmd /c "taskkill /F /IM msbuild.exe 2>nul"

# 2. Remove git index lock
$lockPath = Join-Path $RepoPath ".git\index.lock"
if (Test-Path $lockPath) {
    Remove-Item -Force $lockPath -ErrorAction SilentlyContinue
    if (Test-Path $lockPath) { Write-Host "Could not remove index.lock (busy). Close Git/IDE and retry." } else { Write-Host "Removed .git\index.lock" }
}

# 3. Optional: clear read-only on a specific file
if ($UnlockFile -ne "") {
    $p = if ([System.IO.Path]::IsPathRooted($UnlockFile)) { $UnlockFile } else { Join-Path $RepoPath $UnlockFile }
    if (Test-Path $p) {
        attrib -R $p 2>$null
        Write-Host "Cleared read-only: $p"
    }
}

Write-Host "Unlock done. Save file if needed; lock usually releases in 2-3s after killing build processes."
