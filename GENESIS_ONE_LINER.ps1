# RawrXD Genesis One-Liner - Bootstrap from repo (no VS in PATH required)
# Usage: .\GENESIS_ONE_LINER.ps1   or   irm "https://your-cdn/Genesis.ps1" | iex
$root = if ($PSScriptRoot) { $PSScriptRoot } else { "D:\rawrxd" }
$asm = Join-Path $root "genesis\RawrXD_Genesis.asm"
if (-not (Test-Path $asm)) { Write-Host "Genesis.asm not found at $asm"; exit 1 }
$vspath = "C:\Program Files\Microsoft Visual Studio\2022"
$ml64 = Get-ChildItem "$vspath\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
$link = Get-ChildItem "$vspath\*\VC\Tools\MSVC\*\bin\Hostx64\x64\link.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $ml64) { Write-Host "ML64 not found under $vspath"; exit 1 }
& $ml64.FullName /c /Fo"$env:TEMP\g.obj" /W3 "$asm"
if ($LASTEXITCODE -ne 0) { exit 1 }
& $link.FullName /OUT:"$env:TEMP\genesis.exe" "$env:TEMP\g.obj" /SUBSYSTEM:CONSOLE /ENTRY:GenesisMain kernel32.lib
if ($LASTEXITCODE -ne 0) { exit 1 }
Push-Location (Split-Path $asm)
& "$env:TEMP\genesis.exe"
Pop-Location
Write-Host "Done: genesis.exe built and run; stub written to genesis\RawrXD_Genesis.exe if no args"
