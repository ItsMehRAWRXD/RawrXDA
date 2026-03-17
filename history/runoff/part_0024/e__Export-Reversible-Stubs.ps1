# Generates placeholder reversible exports (F32 .pt and HF .safetensors stubs)
param(
  [string]$ModelsRoot = "D:\Franken\BackwardsUnlock",
  [string[]]$Models = @('1b','350m','125m','60m')
)
Write-Host "Generating placeholder reversible exports…" -ForegroundColor Cyan
foreach($m in $Models){
  $dir = Join-Path $ModelsRoot $m
  $gguf = Get-ChildItem $dir -Filter "unlock-*" | Where-Object {$_.Name -like '*.gguf'} | Select-Object -First 1
  if(-not $gguf){ Write-Warning "No GGUF for $m"; continue }
  $ptOut = Join-Path $dir "reversible-stub.pt"
  $hfDir = Join-Path $dir "hf-stub"
  if(-not (Test-Path $hfDir)){ New-Item $hfDir -ItemType Directory | Out-Null }
  # Write simple marker binary for .pt (not real weights)
  [IO.File]::WriteAllBytes($ptOut, [Text.Encoding]::UTF8.GetBytes("STUB_PT:" + $gguf.FullName))
  # Create dummy safetensors file
  $safePath = Join-Path $hfDir "model.safetensors"
  [IO.File]::WriteAllBytes($safePath, [Text.Encoding]::UTF8.GetBytes("STUB_SAFETENSORS:" + $gguf.FullName))
  # Mapping manifest
  $manifest = [ordered]@{
    model = $m
    gguf  = $gguf.Name
    pt_stub = (Split-Path $ptOut -Leaf)
    hf_stub = (Split-Path $safePath -Leaf)
    reversible = $false
    limitation = 'No original F32 weights available; stubs only.'
  }
  $manifest | ConvertTo-Json -Depth 4 | Out-File (Join-Path $dir 'reversible-manifest.json') -Encoding utf8
  Write-Host "  ✓ $m reversible stubs created" -ForegroundColor Green
}
Write-Host "Done." -ForegroundColor Green