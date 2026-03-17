Write-Host "[RawrXD] Packaging Release..." -ForegroundColor Cyan

$DistDir = "$PSScriptRoot\..\dist\RawrXD_Enterprise_v3.0"
$BuildDir = "$PSScriptRoot\..\build\bin\Release"
$KernelDir = "$PSScriptRoot\..\build"

# Clean Setup
if (Test-Path $DistDir) { Remove-Item -Recurse -Force $DistDir }
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

# 1. Copy Main IDE & Dependencies
Write-Host " -> Copying Agentic IDE..."
Copy-Item "$BuildDir\RawrXD-AgenticIDE.exe" -Destination $DistDir
Copy-Item "$BuildDir\*.dll" -Destination $DistDir
if (Test-Path "$BuildDir\platforms") { Copy-Item -Recurse "$BuildDir\platforms" -Destination $DistDir }
if (Test-Path "$BuildDir\multimedia") { Copy-Item -Recurse "$BuildDir\multimedia" -Destination $DistDir }

# 2. Copy Pocket Lab Kernel & Manifest
Write-Host " -> Copying Pocket Lab Kernel..."
if (Test-Path "$KernelDir\pocket_lab.exe") {
    Copy-Item "$KernelDir\pocket_lab.exe" -Destination $DistDir
} else {
    Write-Warning "pocket_lab.exe not found in build dir!"
}

# 3. Copy Documentation
Write-Host " -> Copying Documentation..."
Copy-Item "$PSScriptRoot\..\RELEASENOTES.md" -Destination $DistDir
Copy-Item "$PSScriptRoot\..\audit\interface_manifest.json" -Destination $DistDir

# 4. Create Launcher Script
$Launcher = @"
@echo off
echo [RawrXD] Booting Sovereign Enterprise Environment...
start "" "pocket_lab.exe"
start "" "RawrXD-AgenticIDE.exe"
"@
Set-Content -Path "$DistDir\Launch_Sovereign_IDE.bat" -Value $Launcher

# 5. Zip it
Write-Host " -> Zipping Archive..."
$ZipFile = "$PSScriptRoot\..\dist\RawrXD_Enterprise_v3.0.zip"
if (Test-Path $ZipFile) { Remove-Item -Force $ZipFile }
Compress-Archive -Path "$DistDir\*" -DestinationPath $ZipFile

Write-Host "SUCCESS: Package created at $ZipFile" -ForegroundColor Green
Get-Item $ZipFile | Select-Object Name, Length, LastWriteTime
