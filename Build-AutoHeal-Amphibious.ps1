$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$outDir = Join-Path $root 'build\amphibious'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$ml64 = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe'
$link = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe'
$vcLib = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64'
$kitUm = 'C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64'
$kitUcrt = 'C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64'

if (!(Test-Path $ml64)) { throw "ml64.exe not found: $ml64" }
if (!(Test-Path $link)) { throw "link.exe not found: $link" }

$env:LIB = @($vcLib, $kitUm, $kitUcrt) -join ';'

$commonSources = @(
    'RawrXD_AgentHost_Sovereign.asm',
    'RawrXD_Agentic_Master.asm',
    'RawrXD_ChatService_Agentic.asm',
    'RawrXD_StreamRenderer_DMA.asm',
    'RawrXD_ML_Runtime.asm'
)

function Invoke-Assemble([string]$source) {
    $srcPath = Join-Path $root $source
    $objPath = Join-Path $outDir ([IO.Path]::GetFileNameWithoutExtension($source) + '.obj')
    Write-Host "Assembling $source" -ForegroundColor Cyan
    & $ml64 /nologo /c /Zi /Fo$objPath $srcPath
    if ($LASTEXITCODE -ne 0) { throw "Assembly failed: $source" }
    return $objPath
}

$commonObjs = foreach ($source in $commonSources) { Invoke-Assemble $source }

$cliObj = Invoke-Assemble 'RawrXD_AutoHeal_Test.asm'
$guiObj = Invoke-Assemble 'RawrXD_AutoHeal_GUI.asm'

$cliExe = Join-Path $outDir 'RawrXD_AutoHeal_CLI.exe'
$guiExe = Join-Path $outDir 'RawrXD_AutoHeal_GUI.exe'

Write-Host 'Linking CLI executable' -ForegroundColor Yellow
& $link /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:Main /OUT:$cliExe $cliObj $commonObjs kernel32.lib user32.lib msvcrt.lib legacy_stdio_definitions.lib ucrt.lib
if ($LASTEXITCODE -ne 0) { throw 'CLI link failed' }

Write-Host 'Linking GUI executable' -ForegroundColor Yellow
& $link /NOLOGO /SUBSYSTEM:WINDOWS /ENTRY:GuiMain /OUT:$guiExe $guiObj $commonObjs kernel32.lib user32.lib msvcrt.lib legacy_stdio_definitions.lib ucrt.lib
if ($LASTEXITCODE -ne 0) { throw 'GUI link failed' }

Write-Host ''
Write-Host 'Amphibious build complete.' -ForegroundColor Green
Write-Host "CLI: $cliExe" -ForegroundColor Green
Write-Host "GUI: $guiExe" -ForegroundColor Green
