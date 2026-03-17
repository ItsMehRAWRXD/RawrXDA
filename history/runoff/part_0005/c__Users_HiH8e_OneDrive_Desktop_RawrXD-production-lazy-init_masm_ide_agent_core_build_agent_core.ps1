# ============================================================================
# build_agent_core.ps1 - Build all autonomous agent MASM modules
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
Write-Host "Building RawrXD Autonomous Agent Core" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Source files
$CoreFiles = @(
    "IDE_CRIT.ASM",
    "IDE_JSONLOG.ASM"
)

$MainFiles = @(
    "IDE_01_MASTER.ASM",
    "IDE_13_CACHE.ASM",
    "IDE_15_AUTH.ASM",
    "IDE_17_PLUGIN.ASM",
    "IDE_18_COLLAB.ASM",
    "IDE_19_DEBUG.ASM",
    "IDE_20_TELEMETRY.ASM"
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
Write-Host "`n[3/3] Linking RawrXD_Master.dll..." -ForegroundColor Yellow
$ObjFiles = Get-ChildItem -Path $OutDir -Filter *.obj | ForEach-Object { $_.FullName }

$LinkArgs = @(
    "/DLL",
    "/DEF:RawrXD_Master.def",
    "/OUT:$BinDir\RawrXD_Master.dll",
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
Write-Host "BUILD SUCCESSFUL!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host "`nOutput: $BinDir\RawrXD_Master.dll" -ForegroundColor White
Write-Host "Logs:   C:\ProgramData\RawrXD\logs\ide_runtime.jsonl`n" -ForegroundColor White

# Display exports
Write-Host "Key autonomous functions exported:" -ForegroundColor Cyan
Write-Host "  - AgentPlan_Create / Resolve" -ForegroundColor Gray
Write-Host "  - AgentLoop_SingleStep / RunUntilDone" -ForegroundColor Gray
Write-Host "  - AgentMemory_Store / Recall" -ForegroundColor Gray
Write-Host "  - AgentTool_Dispatch / ResultToMemory" -ForegroundColor Gray
Write-Host "  - AgentSelfReflect / Crit_SelfHeal" -ForegroundColor Gray
Write-Host "  - AgentPolicy_CheckSafety / Enforce" -ForegroundColor Gray
Write-Host "  - AgentComm_SendA2A / RecvA2A" -ForegroundColor Gray
Write-Host "  - AgentTelemetry_Step`n" -ForegroundColor Gray

Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Run smoke test: .\test_agent.exe" -ForegroundColor White
Write-Host "  2. View logs:      Get-Content C:\ProgramData\RawrXD\logs\ide_runtime.jsonl -Tail 20" -ForegroundColor White
Write-Host "  3. Integrate:      Copy bin\RawrXD_Master.dll to your IDE folder`n" -ForegroundColor White
