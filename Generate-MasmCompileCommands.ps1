param(
    [string]$SourceDir = "D:\Stash House\RawrXD-Main",
    [string]$OutputFile = "D:\Stash House\RawrXD-Main\build_qt_free\compile_commands.json"
)

$ErrorActionPreference = "Stop"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " RAWRXD MASM COMPILE COMMANDS GENERATOR" -ForegroundColor Cyan
Write-Host " Generating compile_commands.json for LSP/IntelliSense" -ForegroundColor Gray
Write-Host "============================================================" -ForegroundColor Cyan

Write-Host "Scanning for MASM files in $SourceDir..." -ForegroundColor Yellow

$OutDir = Split-Path $OutputFile -Parent
if (!(Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

# Find all .asm files, excluding build and temp directories
$asmFiles = Get-ChildItem -Path $SourceDir -Filter "*.asm" -Recurse | Where-Object { 
    $_.FullName -notmatch "\\build" -and 
    $_.FullName -notmatch "\\temp" -and
    $_.FullName -notmatch "\\.git"
}

$commands = @()

# Use the absolute path to ML64 for better LSP compatibility
$ml64Path = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"

# Common include paths for the RawrXD project
$includes = "/I`"$SourceDir\src`" /I`"$SourceDir\include`" /I`"$SourceDir\src\masm`" /I`"$SourceDir\src\asm`""

foreach ($file in $asmFiles) {
    $objName = [System.IO.Path]::GetFileNameWithoutExtension($file.Name) + ".obj"
    $objPath = Join-Path $OutDir $objName

    # Construct the exact compilation command that would be run
    $command = "`"$ml64Path`" /c /nologo /Zi /W3 $includes /Fo`"$objPath`" `"$($file.FullName)`""

    $commands += [ordered]@{
        directory = $file.DirectoryName
        command   = $command
        file      = $file.FullName
        output    = $objPath
    }
}

# Convert to JSON and save
$commands | ConvertTo-Json -Depth 4 | Set-Content -Path $OutputFile -Encoding UTF8

Write-Host "Successfully generated compile_commands.json with $($commands.Count) entries!" -ForegroundColor Green
Write-Host "Output saved to: $OutputFile" -ForegroundColor Green
