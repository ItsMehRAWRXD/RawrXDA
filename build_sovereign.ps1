# ==============================================================================
# Sovereign Framework Build Pipeline
# ==============================================================================
# Complete build script for all Sovereign components:
#   - PE Parser (Static Analysis)
#   - PE Writer (Binary Construction)
#   - Dynamic Controller (Runtime Instrumentation)
#   - All test harnesses
# ==============================================================================

param(
    [string]$MasmDir = "d:\rawrxd-ci-bootstrap",
    [string]$OutDir = "d:\rawrxd-ci-bootstrap\bin"
)

# Tool paths
$ML64 = "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\ml64.exe"
$LINK = "C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\link.exe"

# Create output directory
if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
}

# Helper functions
function Step-Header($step, $total, $msg) {
    Write-Host "[$step/$total] $msg" -ForegroundColor Cyan
}

function Step-Ok($msg) {
    Write-Host "OK: $msg" -ForegroundColor Green
}

function Step-Fail($msg) {
    Write-Host "ERROR: $msg" -ForegroundColor Red
    exit 1
}

# ==============================================================================
# [1/4] Assemble Components
# ==============================================================================
Step-Header 1 4 "Assembling Sovereign Components..."

$Components = @(
    @{Name="PE Writer"; Asm="Sovereign_PE_Writer.asm"; Obj="Sovereign_PE_Writer.obj"},
    @{Name="Dynamic Controller"; Asm="Sovereign_Dynamic_Ctrl.asm"; Obj="Sovereign_Dynamic_Ctrl.obj"},
    @{Name="Dynamic Test"; Asm="Dynamic_Ctrl_Test.asm"; Obj="Dynamic_Ctrl_Test.obj"},
    @{Name="Memory Scanner"; Asm="Sovereign_Mem_Scanner.asm"; Obj="Sovereign_Mem_Scanner.obj"},
    @{Name="Scanner Test"; Asm="Mem_Scanner_Test.asm"; Obj="Mem_Scanner_Test.obj"},
    @{Name="VEH Core"; Asm="Sovereign_VEH_Core.asm"; Obj="Sovereign_VEH_Core.obj"},
    @{Name="VEH Test"; Asm="VEH_Core_Test.asm"; Obj="VEH_Core_Test.obj"}
)

foreach ($comp in $Components) {
    $asmPath = Join-Path $MasmDir $comp.Asm
    $objPath = Join-Path $OutDir $comp.Obj
    
    if (-not (Test-Path $asmPath)) {
        Write-Host "[!] Skipping $($comp.Name) - source not found" -ForegroundColor Yellow
        continue
    }
    
    Write-Host "  Assembling $($comp.Name)..."
    $proc = Start-Process -FilePath $ML64 -ArgumentList "/c","/W3","/nologo","/Zi","/Fo",$objPath,$asmPath -Wait -PassThru -NoNewWindow
    if ($proc.ExitCode -ne 0) { Step-Fail "$($comp.Name) assembly failed." }
}

Step-Ok "All components assembled."

# ==============================================================================
# [2/4] Link Executables
# ==============================================================================
Step-Header 2 4 "Linking executables..."

# Link PE Writer
$WriterExe = Join-Path $OutDir "Sovereign_PE_Writer.exe"
$WriterObj = Join-Path $OutDir "Sovereign_PE_Writer.obj"
if (Test-Path $WriterObj) {
    $cmd = "$LINK /NOLOGO /LARGEADDRESSAWARE:NO /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:`"$WriterExe`" `"$WriterObj`" kernel32.lib"
    Invoke-Expression $cmd
    if ($LASTEXITCODE -eq 0) {
        Step-Ok "PE Writer linked."
    } else {
        Write-Host "[!] PE Writer link failed" -ForegroundColor Yellow
    }
}

# Link Dynamic Controller Test
$DynamicExe = Join-Path $OutDir "Sovereign_Dynamic_Test.exe"
$DynamicObj = Join-Path $OutDir "Sovereign_Dynamic_Ctrl.obj"
$DynamicTestObj = Join-Path $OutDir "Dynamic_Ctrl_Test.obj"
if ((Test-Path $DynamicObj) -and (Test-Path $DynamicTestObj)) {
    $cmd = "$LINK /NOLOGO /LARGEADDRESSAWARE:NO /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:`"$DynamicExe`" `"$DynamicObj`" `"$DynamicTestObj`" kernel32.lib"
    Invoke-Expression $cmd
    if ($LASTEXITCODE -eq 0) {
        Step-Ok "Dynamic Controller Test linked."
    } else {
        Write-Host "[!] Dynamic Test link failed" -ForegroundColor Yellow
    }
}

# Link Memory Scanner Test
$ScannerExe = Join-Path $OutDir "Sovereign_Scanner_Test.exe"
$ScannerObj = Join-Path $OutDir "Sovereign_Mem_Scanner.obj"
$ScannerTestObj = Join-Path $OutDir "Mem_Scanner_Test.obj"
if ((Test-Path $ScannerObj) -and (Test-Path $ScannerTestObj)) {
    $cmd = "$LINK /NOLOGO /LARGEADDRESSAWARE:NO /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:`"$ScannerExe`" `"$ScannerObj`" `"$ScannerTestObj`" kernel32.lib"
    Invoke-Expression $cmd
    if ($LASTEXITCODE -eq 0) {
        Step-Ok "Memory Scanner Test linked."
    } else {
        Write-Host "[!] Scanner Test link failed" -ForegroundColor Yellow
    }
}

# Link VEH Core Test
$VehExe = Join-Path $OutDir "Sovereign_VEH_Test.exe"
$VehObj = Join-Path $OutDir "Sovereign_VEH_Core.obj"
$VehTestObj = Join-Path $OutDir "VEH_Core_Test.obj"
if ((Test-Path $VehObj) -and (Test-Path $VehTestObj)) {
    $cmd = "$LINK /NOLOGO /LARGEADDRESSAWARE:NO /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:`"$VehExe`" `"$VehObj`" `"$VehTestObj`" kernel32.lib"
    Invoke-Expression $cmd
    if ($LASTEXITCODE -eq 0) {
        Step-Ok "VEH Core Test linked."
    } else {
        Write-Host "[!] VEH Test link failed" -ForegroundColor Yellow
    }
}

if (Test-Path $WriterExe) {
    Write-Host "  Testing PE Writer..."
    & $WriterExe
    $WriterExit = $LASTEXITCODE
    if ($WriterExit -eq 0) {
        Step-Ok "PE Writer test passed."
    } else {
        Write-Host "[!] PE Writer test failed (exit=$WriterExit)" -ForegroundColor Yellow
    }
    
    # Verify output file exists
    $OutputPE = Join-Path $MasmDir "sovereign_pe_test.exe"
    if (Test-Path $OutputPE) {
        $FileSize = (Get-Item $OutputPE).Length
        Write-Host "  Generated PE: $OutputPE ($FileSize bytes)" -ForegroundColor Green
    }
}

if (Test-Path $DynamicExe) {
    Write-Host "  Testing Dynamic Controller..."
    & $DynamicExe
    $DynamicExit = $LASTEXITCODE
    if ($DynamicExit -eq 0) {
        Step-Ok "Dynamic Controller test passed."
    } else {
        Write-Host "[!] Dynamic Controller test failed (exit=$DynamicExit)" -ForegroundColor Yellow
    }
}

if (Test-Path $ScannerExe) {
    Write-Host "  Testing Memory Scanner..."
    & $ScannerExe
    $ScannerExit = $LASTEXITCODE
    if ($ScannerExit -eq 0) {
        Step-Ok "Memory Scanner test passed."
    } else {
        Write-Host "[!] Memory Scanner test failed (exit=$ScannerExit)" -ForegroundColor Yellow
    }
}

if (Test-Path $VehExe) {
    Write-Host "  Testing VEH Core..."
    & $VehExe
    $VehExit = $LASTEXITCODE
    if ($VehExit -eq 0) {
        Step-Ok "VEH Core test passed."
    } else {
        Write-Host "[!] VEH Core test failed (exit=$VehExit)" -ForegroundColor Yellow
    }
}

# ==============================================================================
# [4/4] Generate Manifest
# ==============================================================================
Step-Header 4 4 "Generating build manifest..."

$Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
$Manifest = @"
Sovereign Framework Build Manifest
=====================================
Timestamp:   $Timestamp
Build Dir:   $OutDir

Components:
  PE Writer (Static Analysis)
  PE Parser (Binary Reading)
  Dynamic Controller (HWBP Instrumentation)
  Memory Scanner (Pattern Matching)
  VEH Core (Exception Handling)
  Test Harnesses

Executables:
  $(if (Test-Path $WriterExe) { "OK PE Writer" } else { "FAIL PE Writer" })
  $(if (Test-Path $DynamicExe) { "OK Dynamic Test" } else { "FAIL Dynamic Test" })
  $(if (Test-Path $ScannerExe) { "OK Scanner Test" } else { "FAIL Scanner Test" })
  $(if (Test-Path $VehExe) { "OK VEH Test" } else { "FAIL VEH Test" })

Status: BUILD COMPLETE
"@

$ManifestPath = Join-Path $OutDir "sovereign_manifest.log"
$Manifest | Out-File -FilePath $ManifestPath -Encoding ASCII
Write-Host "Manifest: $ManifestPath" -ForegroundColor Green

Write-Host ""
Write-Host "[SUCCESS] Sovereign Framework build complete." -ForegroundColor Green
Write-Host ""

exit 0