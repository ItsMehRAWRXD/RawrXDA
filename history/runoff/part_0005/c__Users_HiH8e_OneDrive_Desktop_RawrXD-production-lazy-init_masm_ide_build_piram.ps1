Param(
    [string]$MasmRoot = "C:\masm32",
    [string]$OutDir = "build",
    [switch]$NoImportLibs,
    [switch]$Test
)

$ErrorActionPreference = 'Stop'

$ml = Join-Path $MasmRoot 'bin\ml.exe'
$link = Join-Path $MasmRoot 'bin\link.exe'
$libPath = Join-Path $MasmRoot 'lib'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$include = Join-Path $scriptDir 'include'
$src = Join-Path $scriptDir 'src'
$out = Join-Path $scriptDir $OutDir

if (-not (Test-Path $ml)) { throw "ml.exe not found at $ml" }
if (-not (Test-Path $link)) { throw "link.exe not found at $link" }
if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out | Out-Null }

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║          π-RAM Ultra-Minimal Compression Builder            ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

$modules = @('piram_ultra', 'piram_compress')
$libraries = @()

if ($NoImportLibs) {
    Write-Host "`n🔧 MASM-ONLY MODE (No Import Libs)" -ForegroundColor Yellow
    $modules = @('dynapi_x86') + $modules
    $mlDefines = @('/DPURE_MASM_NO_IMPORTLIBS')
} else {
    Write-Host "`n🔧 STANDARD MODE (With Import Libs)" -ForegroundColor Yellow
    $libraries = @('kernel32.lib')
    $mlDefines = @()
}

Write-Host "`nModules: $($modules -join ', ')" -ForegroundColor Gray

$objs = @()
foreach ($m in $modules) {
    $asm = Join-Path $src "$m.asm"
    $obj = Join-Path $out "$m.obj"
    
    if (-not (Test-Path $asm)) { 
        Write-Host "⚠ Skipping $m (source not found)" -ForegroundColor Yellow
        continue
    }
    
    Write-Host "  Assembling $m.asm..." -ForegroundColor White
    & $ml /c /coff /Cp /nologo @mlDefines /I $include /Fo $obj $asm
    if ($LASTEXITCODE -ne 0) { throw "Assembly failed: $m.asm" }
    
    Write-Host "    ✓ $m.obj" -ForegroundColor Green
    $objs += $obj
}

if ($Test) {
    Write-Host "`n🧪 Building Test Harness..." -ForegroundColor Cyan
    
    $testAsm = Join-Path $src 'piram_test.asm'
    if (-not (Test-Path $testAsm)) {
        # Create minimal test harness
        @'
.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

PiRam_Compress PROTO
PiRam_GetCompressionRatio PROTO
PiRam_EnableHalving PROTO

ExitProcess PROTO :DWORD

.data
    testData db 256 dup(?)
    
.code
start:
    ; Enable halving
    invoke PiRam_EnableHalving, TRUE
    
    ; Compress test data
    mov edx, 256
    call PiRam_Compress
    
    ; Get ratio
    call PiRam_GetCompressionRatio
    
    ; Exit with ratio as exit code
    invoke ExitProcess, eax
end start
'@ | Out-File -FilePath $testAsm -Encoding ASCII
    }
    
    $testObj = Join-Path $out 'piram_test.obj'
    Write-Host "  Assembling piram_test.asm..." -ForegroundColor White
    & $ml /c /coff /Cp /nologo @mlDefines /I $include /Fo $testObj $testAsm
    if ($LASTEXITCODE -ne 0) { throw "Test assembly failed" }
    
    $objs += $testObj
    
    $testExe = Join-Path $out 'piram_test.exe'
    Write-Host "  Linking piram_test.exe..." -ForegroundColor White
    
    if ($libraries.Count -gt 0) {
        & $link /SUBSYSTEM:CONSOLE /LIBPATH:$libPath "/OUT:$testExe" $objs $libraries
    } else {
        & $link /SUBSYSTEM:CONSOLE "/OUT:$testExe" $objs
    }
    
    if ($LASTEXITCODE -ne 0) { throw "Link failed" }
    
    Write-Host "`n✅ Test executable built: $testExe" -ForegroundColor Green
    
    # Run test
    Write-Host "`n🏃 Running test..." -ForegroundColor Cyan
    & $testExe
    $exitCode = $LASTEXITCODE
    Write-Host "  Compression ratio: $exitCode%" -ForegroundColor $(if ($exitCode -ge 200) { 'Green' } else { 'Yellow' })
}

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║                 π-RAM BUILD COMPLETE ✅                      ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Green

Write-Host "`n📦 Artifacts:" -ForegroundColor Cyan
Get-ChildItem $out -Filter piram_*.obj | ForEach-Object {
    $sizeKB = [math]::Round($_.Length / 1KB, 2)
    Write-Host "  ✓ $($_.Name) ($sizeKB KB)" -ForegroundColor Green
}

Write-Host "`n💡 Next Steps:" -ForegroundColor Cyan
Write-Host "  • Link with GGUF loader: .\build_pure_masm.ps1 -Modules dynapi_x86,piram_ultra,piram_compress,gguf_loader_working -NoImportLibs" -ForegroundColor Gray
Write-Host "  • Run tests: .\build_piram.ps1 -Test" -ForegroundColor Gray
Write-Host "  • See PIRAM_BUILD_GUIDE.md for usage examples" -ForegroundColor Gray
