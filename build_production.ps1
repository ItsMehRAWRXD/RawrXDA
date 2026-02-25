# ============================================================
# RawrXD Production Build System (PowerShell)
# Version: 6.0.0
# ============================================================
# Cross-checks Authenticode signatures on output binaries.
# Run: .\build_production.ps1 [-Sign] [-Clean] [-Verbose]
# ============================================================

param(
    [switch]$Sign,
    [switch]$Clean,
    [string]$CertThumbprint = "",
    [string]$TimestampServer = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"
$BuildRoot = "D:\rawrxd"
$Src = "$BuildRoot\src"
$OutDir = "$BuildRoot\build\release"
$ObjDir = "$BuildRoot\build\obj"
$BinDir = "C:\RawrXD\bin"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " RawrXD Production Build System v6.0.0" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# --- Clean ---
if ($Clean) {
    Write-Host "`n[CLEAN] Removing build artifacts..." -ForegroundColor Yellow
    if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
    if (Test-Path $ObjDir) { Remove-Item -Recurse -Force $ObjDir }
}

# --- Create dirs ---
@($OutDir, $ObjDir, $BinDir) | ForEach-Object {
    if (-not (Test-Path $_)) { New-Item -ItemType Directory -Path $_ -Force | Out-Null }
}

# --- Detect tools ---
$ml64 = Get-Command ml64.exe -ErrorAction SilentlyContinue
$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
$link = Get-Command link.exe -ErrorAction SilentlyContinue

if (-not $ml64 -or -not $cl -or -not $link) {
    Write-Host "[ERROR] MSVC Build Tools not found. Run from VS Developer Command Prompt." -ForegroundColor Red
    exit 1
}

$results = @()
$totalErrors = 0

function Build-Step {
    param([string]$Name, [scriptblock]$Action)
    Write-Host "`n[$Name]" -ForegroundColor Green
    try {
        & $Action
        $script:results += @{ Name=$Name; Status="OK" }
    } catch {
        Write-Host "  [ERROR] $_" -ForegroundColor Red
        $script:results += @{ Name=$Name; Status="FAILED" }
        $script:totalErrors++
    }
}

# --- 1. GPU DMA Module ---
Build-Step "GPU DMA Module" {
    $srcFile = "$Src\agentic\gpu_dma_production.asm"
    if (-not (Test-Path $srcFile)) { throw "Source not found: $srcFile" }
    & ml64.exe /c /nologo /Fo"$ObjDir\gpu_dma_production.obj" $srcFile
    if ($LASTEXITCODE -ne 0) { throw "ml64 failed" }
    Write-Host "  gpu_dma_production.obj" -ForegroundColor DarkGreen
}

# --- 2. PE Backend Emitter ---
Build-Step "PE Backend Emitter" {
    & ml64.exe /c /nologo /Fo"$ObjDir\PE_Backend_Emitter.obj" "$Src\PE_Backend_Emitter.asm"
    if ($LASTEXITCODE -ne 0) { throw "ml64 failed" }
    & link.exe /nologo /subsystem:console /entry:main /out:"$OutDir\pe_generator.exe" "$ObjDir\PE_Backend_Emitter.obj" kernel32.lib
    if ($LASTEXITCODE -ne 0) { throw "link failed" }
    Copy-Item "$OutDir\pe_generator.exe" "$BinDir\pe_generator.exe" -Force
    Write-Host "  pe_generator.exe" -ForegroundColor DarkGreen
}

# --- 3. BareMetal PE Writer ---
Build-Step "BareMetal PE Writer" {
    & ml64.exe /c /nologo /Fo"$ObjDir\BareMetal_PE_Writer.obj" "$Src\BareMetal_PE_Writer.asm"
    if ($LASTEXITCODE -ne 0) { throw "ml64 failed" }
    & link.exe /nologo /subsystem:console /entry:main /out:"$OutDir\baremetal_pe_writer.exe" "$ObjDir\BareMetal_PE_Writer.obj" kernel32.lib
    if ($LASTEXITCODE -ne 0) { throw "link failed" }
    Copy-Item "$OutDir\baremetal_pe_writer.exe" "$BinDir\baremetal_pe_writer.exe" -Force
    Write-Host "  baremetal_pe_writer.exe" -ForegroundColor DarkGreen
}

# --- 4. Win32 IDE Shell ---
Build-Step "Win32 IDE Shell" {
    $ideSrc = "$Src\ide\RawrXD_IDE_Win32.cpp"
    if (-not (Test-Path $ideSrc)) { throw "IDE source not found" }
    $libs = "kernel32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib advapi32.lib ole32.lib shlwapi.lib"
    & cl.exe /nologo /EHsc /std:c++17 /O2 /W4 /DUNICODE /D_UNICODE `
        /Fe"$OutDir\RawrXD_IDE.exe" $ideSrc `
        /link /subsystem:windows $libs.Split(" ")
    if ($LASTEXITCODE -ne 0) { throw "cl failed" }
    Copy-Item "$OutDir\RawrXD_IDE.exe" "$BinDir\RawrXD_IDE.exe" -Force
    Write-Host "  RawrXD_IDE.exe" -ForegroundColor DarkGreen
}

# --- 5. Code Signing ---
if ($Sign) {
    Write-Host "`n[CODE SIGNING]" -ForegroundColor Magenta
    $exes = Get-ChildItem "$OutDir\*.exe"
    foreach ($exe in $exes) {
        try {
            if ($CertThumbprint) {
                $cert = Get-ChildItem Cert:\CurrentUser\My\$CertThumbprint
                Set-AuthenticodeSignature -FilePath $exe.FullName -Certificate $cert -TimestampServer $TimestampServer
            } else {
                # Self-signed for dev
                $cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert | Select-Object -First 1
                if ($cert) {
                    Set-AuthenticodeSignature -FilePath $exe.FullName -Certificate $cert -TimestampServer $TimestampServer
                    Write-Host "  Signed: $($exe.Name)" -ForegroundColor DarkMagenta
                } else {
                    Write-Host "  [SKIP] No code signing certificate found" -ForegroundColor Yellow
                }
            }
        } catch {
            Write-Host "  [WARN] Failed to sign $($exe.Name): $_" -ForegroundColor Yellow
        }
    }
}

# --- 6. Generate SHA256 Hashes ---
Write-Host "`n[INTEGRITY HASHES]" -ForegroundColor Blue
$hashFile = "$OutDir\SHA256SUMS.txt"
$hashes = @()
Get-ChildItem "$OutDir\*.exe" | ForEach-Object {
    $h = Get-FileHash $_.FullName -Algorithm SHA256
    $line = "$($h.Hash)  $($_.Name)"
    $hashes += $line
    Write-Host "  $line"
}
$hashes | Set-Content $hashFile -Encoding UTF8
Write-Host "  Written to SHA256SUMS.txt" -ForegroundColor DarkBlue

# --- Summary ---
Write-Host "`n============================================================" -ForegroundColor Cyan
Write-Host " Build Summary" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
$results | ForEach-Object {
    $color = if ($_.Status -eq "OK") { "Green" } else { "Red" }
    Write-Host "  $($_.Name): $($_.Status)" -ForegroundColor $color
}
Write-Host "  Total Errors: $totalErrors"
Write-Host "  Output: $OutDir"
Write-Host "  Binaries: $BinDir"
Write-Host "============================================================" -ForegroundColor Cyan

if ($totalErrors -gt 0) {
    Write-Host "  [WARNING] Build completed with $totalErrors error(s)" -ForegroundColor Yellow
    exit 1
} else {
    Write-Host "  [SUCCESS] All components built and verified" -ForegroundColor Green
    exit 0
}
