<#!
.SYNOPSIS
    Downsize a GGUF model toward a target size and register it in Ollama.
.DESCRIPTION
    Attempts q4_k_m first, checks size, and if still above target, tries q3_k_m.
    Outputs live under this script folder (artifacts/). Creates a Modelfile and runs `ollama create`.
.PARAMETER InputGGUF
    Full path to source .gguf
.PARAMETER Name
    Ollama model name to create (default: bigdaddyg-8g)
.PARAMETER TargetGB
    Desired maximum size in GB (default: 8)
.PARAMETER LlamaCppRoot
    Optional llama.cpp root directory containing the quantizer.
.PARAMETER QuantizeExe
    Optional explicit path to quantize.exe / llama-quantize.exe.
!#>

[CmdletBinding()] param(
  [Parameter(Mandatory = $true, Position = 0)]
  [ValidateScript({ Test-Path $_ -PathType Leaf -ErrorAction SilentlyContinue })]
  [string]$InputGGUF,

  [Parameter(Mandatory = $false)]
  [string]$Name = 'bigdaddyg-8g',

  [Parameter(Mandatory = $false)]
  [double]$TargetGB = 8.0,

  [Parameter(Mandatory = $false)]
  [string]$LlamaCppRoot,

  [Parameter(Mandatory = $false)]
  [string]$QuantizeExe
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptRoot = $PSScriptRoot
$artifacts = Join-Path $scriptRoot 'artifacts'
if (-not (Test-Path $artifacts)) { New-Item -ItemType Directory -Path $artifacts -Force | Out-Null }

function Get-OutPath {
  param([string]$InPath, [string]$Type)
  $base = [IO.Path]::GetFileNameWithoutExtension($InPath)
  return (Join-Path $artifacts ("{0}_{1}.gguf" -f $base, $Type))
}

$quantScript = Join-Path $scriptRoot 'Quantize-GGUF.ps1'
if (-not (Test-Path $quantScript -PathType Leaf)) {
  throw "Missing Quantize-GGUF.ps1 in $scriptRoot"
}

# Try quantization types from less aggressive to more aggressive to hit target size
$types = @('q5_k_m', 'q4_k_m', 'q4_k_s', 'q3_k_m', 'q2_k')
$finalPath = $null
foreach ($t in $types) {
  Write-Host "=== Attempt: $t ===" -ForegroundColor Cyan
  & $quantScript -InputGGUF $InputGGUF -OutputDir $artifacts -Type $t -LlamaCppRoot $LlamaCppRoot -QuantizeExe $QuantizeExe
  $outPath = Get-OutPath -InPath $InputGGUF -Type $t
  if (-not (Test-Path $outPath -PathType Leaf)) { throw "Expected output not found: $outPath" }
  $sizeGB = [Math]::Round((Get-Item $outPath).Length / 1GB, 2)
  Write-Host ("Result: {0} ({1} GB)" -f $outPath, $sizeGB)
  $finalPath = $outPath
  if ($sizeGB -le $TargetGB) { 
    Write-Host "✅ Target size reached with $t quantization!" -ForegroundColor Green
    break 
  }
}

# Create Modelfile for Ollama
$modelfile = Join-Path $artifacts ("{0}.Modelfile" -f $Name)
("FROM {0}`r`nPARAMETER temperature 0.7" -f $finalPath) | Set-Content -Path $modelfile -Encoding UTF8

# Register in Ollama
try {
  & ollama create $Name -f $modelfile
  Write-Host ("✅ Ollama model '{0}' created." -f $Name) -ForegroundColor Green
}
catch {
  Write-Host ("⚠️ Failed to run 'ollama create' automatically: {0}" -f $_.Exception.Message) -ForegroundColor Yellow
  Write-Host ("You can run manually: ollama create {0} -f '{1}'" -f $Name, $modelfile)
}

Write-Host ("Final model path: {0}" -f $finalPath) -ForegroundColor Green
