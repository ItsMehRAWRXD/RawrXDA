#Registers synthetic downscaled GGUF artifacts as Ollama models (placeholders, non-functional)
param(
  [string]$BaseModel = "D:\Franken\BackwardsUnlock\1b\unlock-1B-Q4_K_M.gguf",
  [string]$OutDir = "D:\Franken\BackwardsUnlock\synthetic",
  [switch]$SkipGenerate
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(-not (Test-Path $BaseModel)){ throw "Base GGUF not found: $BaseModel" }
if(-not (Test-Path $OutDir)){ New-Item $OutDir -ItemType Directory | Out-Null }

if(-not $SkipGenerate){
  python "e:\Synthetic-Downscale-GGUF.py" $BaseModel $OutDir
}

$map = @{
  'synthetic-350m' = 'synthetic-350m.gguf'
  'synthetic-125m' = 'synthetic-125m.gguf'
  'synthetic-60m'  = 'synthetic-60m.gguf'
}

foreach($name in $map.Keys){
  $gguf = Join-Path $OutDir $map[$name]
  if(-not (Test-Path $gguf)){ Write-Warning "Missing artifact $gguf"; continue }
  $modelfile = Join-Path $OutDir ("Modelfile-" + $name)
  @"
FROM $gguf
TEMPLATE """<|start_header_id|>system<|end_header_id|>
WARNING: This is a synthetic placeholder model ($name). Responses are stub only.<|eot_id|>
<|start_header_id|>user<|end_header_id|>
{{ .Prompt }}<|eot_id|>
<|start_header_id|>assistant<|end_header_id|>
"""
PARAMETER temperature 0.7
PARAMETER top_p 0.9
PARAMETER stop <|eot_id|>
SYSTEM Synthetic placeholder – not a real shrunken model.
"@ | Out-File $modelfile -Encoding utf8
  Write-Host "Creating Ollama model: $name" -ForegroundColor Cyan
  try {
    ollama create $name -f $modelfile | Out-Null
    Write-Host "  ✓ Registered $name" -ForegroundColor Green
  } catch {
    Write-Warning "Failed to register ${name}: $_"
  }
}

Write-Host "Synthetic model registration complete." -ForegroundColor Green