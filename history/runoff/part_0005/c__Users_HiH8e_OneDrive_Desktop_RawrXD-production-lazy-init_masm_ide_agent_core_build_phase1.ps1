# ============================================================================
# build_phase1.ps1 - Build only core agent loop (Phase 1)
# ============================================================================

$ErrorActionPreference = "Stop"

$MASM32 = "C:\masm32"
$ML = "$MASM32\bin\ml.exe"
$LINK = "$MASM32\bin\link.exe"

$OutDir = ".\obj"
$BinDir = ".\bin"

# Create output directories
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path $BinDir | Out-Null
New-Item -ItemType Directory -Force -Path "C:\ProgramData\RawrXD\logs" | Out-Null

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Building RawrXD Agent Core (Phase 1)" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Source files
$CoreFiles = @(
    "IDE_CRIT.ASM",
    "IDE_JSONLOG.ASM"
)

$MainFiles = @(
    "IDE_01_MASTER.ASM",
    "IDE_03_DEBUG.ASM",
    "IDE_04_EXT.ASM"
)

# Assemble core infrastructure first
Write-Host "[1/3] Assembling core infrastructure..." -ForegroundColor Yellow
foreach ($file in $CoreFiles) {
    Write-Host "  - Assembling $file" -ForegroundColor Gray
    & $ML /c /coff /Cp /nologo /Fo"$OutDir\$($file -replace '\.ASM$', '.obj')" $file
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Failed to assemble $file" -ForegroundColor Red
        exit 1
    }
}

# Assemble main modules
Write-Host "`n[2/3] Assembling autonomous agent modules..." -ForegroundColor Yellow
foreach ($file in $MainFiles) {
    Write-Host "  - Assembling $file" -ForegroundColor Gray
    & $ML /c /coff /Cp /nologo /Fo"$OutDir\$($file -replace '\.ASM$', '.obj')" $file
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Failed to assemble $file" -ForegroundColor Red
        exit 1
    }
}

# Link into DLL
Write-Host "`n[3/3] Linking RawrXD_AgentPhase1.dll..." -ForegroundColor Yellow
$ObjFiles = Get-ChildItem -Path $OutDir -Filter *.obj | ForEach-Object { $_.FullName }

$LinkArgs = @(
    "/DLL",
    "/DEF:RawrXD_Phase1.def",
    "/OUT:$BinDir\RawrXD_AgentPhase1.dll",
    "/SUBSYSTEM:WINDOWS",
    "/NOLOGO",
    "/OPT:REF",
    "/RELEASE"
) + $ObjFiles + @(
    "kernel32.lib",
    "user32.lib",
    "advapi32.lib",
    "/LIBPATH:$MASM32\lib"
)

& $LINK $LinkArgs
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to link DLL" -ForegroundColor Red
    exit 1
}

Write-Host "`n========================================" -ForegroundColor Green
Write-Host "PHASE 1 BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host "`nOutput: $BinDir\RawrXD_AgentPhase1.dll" -ForegroundColor White
Write-Host "Logs:   C:\ProgramData\RawrXD\logs\ide_runtime.jsonl`n" -ForegroundColor White

Write-Host "Core autonomous functions available:" -ForegroundColor Cyan
Write-Host "  ✓ IDEMaster_Initialize" -ForegroundColor Green
Write-Host "  ✓ IDEMaster_LoadModel" -ForegroundColor Green
Write-Host "  ✓ IDEMaster_HotSwapModel" -ForegroundColor Green
Write-Host "  ✓ IDEMaster_ExecuteAgenticTask" -ForegroundColor Green
Write-Host "  ✓ AgentPlan_Create" -ForegroundColor Green
Write-Host "  ✓ AgentPlan_Resolve" -ForegroundColor Green
Write-Host "  ✓ AgentLoop_SingleStep" -ForegroundColor Green
Write-Host "  ✓ AgentLoop_RunUntilDone`n" -ForegroundColor Green
