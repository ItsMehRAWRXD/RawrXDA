param(
    [switch]$Test,
    [string]$OutDir = "$PSScriptRoot/bin",
    [string]$Architecture = "x64"
)

$ErrorActionPreference = 'Stop'

function Get-ToolPath {
    param([string]$ToolName)
    $cmd = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $vswhere = "$Env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsroot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.${Architecture} -property installationPath 2>$null
        if ($vsroot) {
            $ml = Join-Path $vsroot "VC/Tools/MSVC"
            $tool = Get-ChildItem -Path $ml -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
            if ($tool) {
                $bin = Join-Path $tool.FullName "bin/Host${Architecture}/${Architecture}"
                $candidate = Join-Path $bin $ToolName
                if (Test-Path $candidate) { return $candidate }
            }
        }
    }
    throw "Tool '$ToolName' not found. Add it to PATH or run from a VS Developer PowerShell."
}

function Invoke-Checked {
    param([string]$Command, [string[]]$Args)
    Write-Host "[*] $Command $($Args -join ' ')"
    $proc = Start-Process -FilePath $Command -ArgumentList $Args -NoNewWindow -Wait -PassThru
    if ($proc.ExitCode -ne 0) { throw "Command failed: $Command" }
}

Write-Host "=== Building RawrXD PE Generator & Encoder ===" -ForegroundColor Cyan

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$ml = Get-ToolPath -ToolName "ml64.exe"
$link = Get-ToolPath -ToolName "link.exe"

$asmRoot = $PSScriptRoot
$objects = @()

Invoke-Checked -Command $ml -Args @("/nologo","/c","/Fo:${OutDir}/rawrxd_pe_generator_encoder.obj","${asmRoot}/rawrxd_pe_generator_encoder.asm")
$objects += "${OutDir}/rawrxd_pe_generator_encoder.obj"

Invoke-Checked -Command $ml -Args @("/nologo","/c","/Fo:${OutDir}/pe_generator_example.obj","${asmRoot}/pe_generator_example.asm")
$objects += "${OutDir}/pe_generator_example.obj"

Invoke-Checked -Command $link -Args @(
    "/nologo",
    "/SUBSYSTEM:CONSOLE",
    "/ENTRY:Main",
    "/OUT:${OutDir}/pe_generator_example.exe"
) + $objects + @("kernel32.lib","user32.lib","bcrypt.lib")

if ($Test) {
    Write-Host "=== Smoke test: listing outputs ===" -ForegroundColor Yellow
    Get-ChildItem -Path $OutDir | Select-Object Name, @{Name='Size(KB)';Expression={[math]::Round($_.Length/1KB,2)}}
}

Write-Host "[✓] Build completed" -ForegroundColor Green
