param(
    [string]$MasmRoot = "C:\masm32",
    [string]$OutDir = "build",
    [string]$ExeName = "AgenticIDEWin.exe",
    [string[]]$Modules,
    [string]$Profile = "Stub",
    [switch]$NoImportLibs
)

$ErrorActionPreference = 'Stop'

# Resolve tools and paths
$ml   = Join-Path $MasmRoot 'bin\ml.exe'
$link = Join-Path $MasmRoot 'bin\link.exe'
$libPath = Join-Path $MasmRoot 'lib'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$include   = Join-Path $scriptDir 'include'
$src       = Join-Path $scriptDir 'src'
$out       = Join-Path $scriptDir $OutDir

if (-not (Test-Path $ml)) { throw "ml.exe not found at $ml" }
if (-not (Test-Path $link)) { throw "link.exe not found at $link" }
if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out | Out-Null }

# Library set (standard Win32) for MASM link.exe
$libraries = @(
    'kernel32.lib','user32.lib','gdi32.lib','shell32.lib','comctl32.lib',
    'comdlg32.lib','ole32.lib','shlwapi.lib'
)

if ($NoImportLibs) {
    $libraries = @()
}

# Profiles to simplify module selection
$profileModules = @{
    'Stub' = @('main_stub');
    'Phase3' = @(
        'main_enhanced',
        'file_explorer_consolidated',
        'icon_resources',
        'test_harness_phase3',
        'logging_phase3',
        'performance_phase3'
    );
    'Full' = @(
        'masm_main',
        'engine_final',
        'window',
        'config_manager',
        'orchestra',
        'tab_control_stub',
        'menu_system',
        'ui_layout',
        'phase4_integration',
        'phase4_stubs',
        'file_explorer_consolidated',
        'icon_resources',
        'test_harness_phase3',
        'logging_phase3',
        'performance_phase3'
    )
}

if (-not $Modules) {
    if ($profileModules.ContainsKey($Profile)) {
        $Modules = $profileModules[$Profile]
    } else {
        throw "Unknown profile '$Profile'."
    }
}
elseif ($Modules.Count -eq 1 -and $Modules[0] -match ",") {
    $Modules = $Modules[0] -split "," | ForEach-Object { $_.Trim() }
}

Write-Host "`n🔧 Building (profile: $Profile) with modules:`n  $($Modules -join ', ')`n" -ForegroundColor Cyan

if ($NoImportLibs -and ($Modules -notcontains 'dynapi_x86')) {
    $Modules = @('dynapi_x86') + $Modules
    Write-Host "ℹ Added dynapi_x86 for NoImportLibs mode" -ForegroundColor DarkCyan
}

$mlDefines = @()
if ($NoImportLibs) {
    $mlDefines += '/DPURE_MASM_NO_IMPORTLIBS'
}

$objs = @()
foreach ($m in $Modules) {
    $asm = Join-Path $src "$m.asm"
    $obj = Join-Path $out "$m.obj"
    if (-not (Test-Path $asm)) { throw "Missing source file: $asm" }
    Write-Host "Assembling $m.asm..." -ForegroundColor White
    & $ml /c /coff /Cp /nologo @mlDefines /I $include /Fo $obj $asm
    if ($LASTEXITCODE -ne 0) { throw "Assembly failed: $m.asm" }
    $objs += $obj
}

Write-Host "Linking $ExeName ..." -ForegroundColor White
$outExe = Join-Path $out $ExeName
& $link /SUBSYSTEM:WINDOWS /LIBPATH:$libPath "/OUT:$outExe" $objs $libraries
if ($LASTEXITCODE -ne 0) { throw "Link failed" }

Write-Host "`n✅ Build succeeded: $(Join-Path $out $ExeName)" -ForegroundColor Green
