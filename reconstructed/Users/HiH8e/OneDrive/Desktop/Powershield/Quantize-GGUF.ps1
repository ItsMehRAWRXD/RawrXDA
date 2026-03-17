<#!
.SYNOPSIS
    Quantize a GGUF model to a smaller size using llama.cpp on Windows.

.DESCRIPTION
    Wraps llama.cpp's quantization tool (quantize.exe or llama-quantize.exe) to produce a lower-bit GGUF.
    Keeps all outputs within the Powershield folder by default.

.PARAMETER InputGGUF
    Path to the source .gguf file (e.g., D:\BigDaddyG-Standalone-40GB\model\bigdaddyg.gguf).

.PARAMETER OutputDir
    Directory where the quantized .gguf will be written. Default: <this script folder>\artifacts

.PARAMETER Type
    Quantization type. Examples: q8_0, q6_k, q5_k_m, q5_k_s, q4_k_m, q4_k_s, q3_k_m, q2_k.
    Default: q4_k_m (good balance for big shrink with quality kept reasonable).

.PARAMETER QuantizeExe
    Full path to llama.cpp quantizer (quantize.exe or llama-quantize.exe).
    If omitted, the script will try to find it under a provided LlamaCppRoot or common names in PATH.

.PARAMETER LlamaCppRoot
    Optional folder where llama.cpp binaries live; the script will look for quantize.exe within it.

.EXAMPLE
    .\Quantize-GGUF.ps1 -InputGGUF "D:\models\bigdaddyg.gguf" -Type q4_k_m

.EXAMPLE
    .\Quantize-GGUF.ps1 -InputGGUF "D:\models\bigdaddyg.gguf" -Type q5_k_m -LlamaCppRoot "C:\tools\llama.cpp\bin"

.NOTES
    - Output size depends on the base model and chosen quantization type; ~5x shrink often requires q4_k_* or q3_k_*.
    - After quantization, create an Ollama Modelfile that references the new GGUF and run `ollama create`.
!#>

[CmdletBinding()] param(
  [Parameter(Mandatory = $true, Position = 0)]
  [ValidateScript({ Test-Path $_ -PathType Leaf -ErrorAction SilentlyContinue })]
  [string]$InputGGUF,

  [Parameter(Mandatory = $false)]
  [string]$OutputDir = (Join-Path $PSScriptRoot 'artifacts'),

  [Parameter(Mandatory = $false)]
  [ValidateSet('q8_0', 'q6_k', 'q5_k_m', 'q5_k_s', 'q4_k_m', 'q4_k_s', 'q3_k_m', 'q2_k')]
  [string]$Type = 'q4_k_m',

  [Parameter(Mandatory = $false)]
  [string]$QuantizeExe,

  [Parameter(Mandatory = $false)]
  [string]$LlamaCppRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Ensure output dir
if (-not (Test-Path $OutputDir)) {
  New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

# Try to locate quantizer if not provided
function Find-Quantizer {
  param([string]$Root)
  $candidates = @()
  if ($Root) {
    $candRoot = @()
    $candRoot += (Join-Path $Root 'quantize.exe')
    $candRoot += (Join-Path $Root 'llama-quantize.exe')
    $candidates += $candRoot
    $candidates += (Get-ChildItem -Path $Root -Recurse -Filter 'quantize.exe' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName)
    $candidates += (Get-ChildItem -Path $Root -Recurse -Filter 'llama-quantize.exe' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName)
  }
  # PATH lookup
  foreach ($p in $env:Path.Split(';')) {
    if (-not [string]::IsNullOrWhiteSpace($p) -and (Test-Path $p)) {
      $q1 = Join-Path $p 'quantize.exe'
      $q2 = Join-Path $p 'llama-quantize.exe'
      if (Test-Path $q1) { $candidates += $q1 }
      if (Test-Path $q2) { $candidates += $q2 }
    }
  }
  $candidates | Where-Object { Test-Path $_ -PathType Leaf } | Select-Object -Unique
}

if (-not $QuantizeExe) {
  $found = Find-Quantizer -Root $LlamaCppRoot
  if ($found -and $found.Count -gt 0) {
    $QuantizeExe = $found[0]
  }
}

if (-not $QuantizeExe -or -not (Test-Path $QuantizeExe -PathType Leaf)) {
  throw "Could not find quantize.exe / llama-quantize.exe. Provide -QuantizeExe or -LlamaCppRoot."
}

# Build output filename
$inName = [IO.Path]::GetFileNameWithoutExtension($InputGGUF)
$outName = "${inName}_${Type}.gguf"
$outPath = Join-Path $OutputDir $outName

Write-Host "Quantizing:" -ForegroundColor Cyan
Write-Host "  Input   : $InputGGUF"
Write-Host "  Type    : $Type"
Write-Host "  Tool    : $QuantizeExe"
Write-Host "  Output  : $outPath"

# llama.cpp usage: quantize.exe <input.gguf> <output.gguf> <type>
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $QuantizeExe
$psi.ArgumentList.Add($InputGGUF)
$psi.ArgumentList.Add($outPath)
$psi.ArgumentList.Add($Type)
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true

$proc = [System.Diagnostics.Process]::Start($psi)
$stdOut = $proc.StandardOutput.ReadToEnd()
$stdErr = $proc.StandardError.ReadToEnd()
$proc.WaitForExit()

if ($stdOut) { Write-Host $stdOut }
if ($stdErr) { Write-Host $stdErr -ForegroundColor Yellow }

if ($proc.ExitCode -ne 0 -or -not (Test-Path $outPath)) {
  throw "Quantization failed (exit code $($proc.ExitCode)). Check tool output above."
}

Write-Host "✅ Done. Output: $outPath" -ForegroundColor Green

# Optional: print approximate size change
try {
  $inSizeGB = [Math]::Round((Get-Item $InputGGUF).Length / 1GB, 2)
  $outSizeGB = [Math]::Round((Get-Item $outPath).Length / 1GB, 2)
  Write-Host ("Size: {0} GB -> {1} GB" -f $inSizeGB, $outSizeGB)
}
catch {}
