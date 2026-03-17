# Adds jailbreak metadata sidecar JSON files for unlocked models
param(
    [string]$ModelsRoot = "D:\Franken\BackwardsUnlock",
    [string[]]$Models = @('1b','350m','125m','60m'),
    [string]$Signature = ([Guid]::NewGuid().ToString('N'))
)
Write-Host "Creating jailbreak metadata sidecars…" -ForegroundColor Cyan
foreach($m in $Models){
    $dir = Join-Path $ModelsRoot $m
    $gguf = Get-ChildItem $dir -Filter "unlock-*" | Where-Object {$_.Name -like '*.gguf'} | Select-Object -First 1
    if(-not $gguf){ Write-Warning "No GGUF found for $m"; continue }
    $meta = [ordered]@{
        model            = $m
        source_file      = $gguf.FullName
        jailbreak_flags  = @('no_refusal','system_override','tool_enable','unlock_all')
        hallucination_loss = 0.01
        injected_layers  = 'sidecar-only'
        unlock_signature = $Signature
        timestamp_utc    = (Get-Date).ToUniversalTime().ToString('o')
        reversible_export = @('gguf','placeholder_f32','placeholder_hf')
        notes = 'Sidecar metadata only; GGUF header untouched.'
    }
    $outFile = Join-Path $dir "jailbreak.meta.json"
    $meta | ConvertTo-Json -Depth 4 | Out-File $outFile -Encoding utf8
    Write-Host "  ✓ $m -> $outFile" -ForegroundColor Green
}
