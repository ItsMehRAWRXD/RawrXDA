param(
    [string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'

$vsWhere = "$Env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe"
$vcvars = $null
if (Test-Path $vsWhere) {
    $vs = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) {
        $vcvarsCandidate = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
        if (Test-Path $vcvarsCandidate) { $vcvars = $vcvarsCandidate }
    }
}
if (-not $vcvars) {
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
    )
    foreach ($c in $candidates) { if (Test-Path $c) { $vcvars = $c; break } }
}
if (-not $vcvars) { throw 'Could not locate VsDevCmd/vcvars64.bat' }

$root = (Resolve-Path "$PSScriptRoot\..\").Path
$asm = Join-Path $root 'src\masm\tools\masm_header_gen.asm'
$outDir = Join-Path $root 'bin'
$out = Join-Path $outDir "masm_header_gen.exe"

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$cmd = '"' + $vcvars + '"' + ' && ml64.exe /nologo /c /Fo:masm_header_gen.obj ' + '"' + $asm + '"' + ' && link.exe /nologo /SUBSYSTEM:CONSOLE /ENTRY:main masm_header_gen.obj kernel32.lib /OUT:' + '"' + $out + '"'

Write-Host "Building masm_header_gen with: $cmd"
Push-Location $outDir
cmd.exe /c $cmd
Pop-Location

if (-not (Test-Path $out)) { throw "Build failed: $out not found" }
Write-Host "Built: $out"