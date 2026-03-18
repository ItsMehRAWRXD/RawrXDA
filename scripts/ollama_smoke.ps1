$ErrorActionPreference = "Stop"

param(
    [string]$Host = "http://127.0.0.1:11434",
    [string]$Model = "bigdaddyg-16gb-balanced:latest",
    [string]$OutDir = "."
)

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$tagsPath = Join-Path $OutDir "ollama_tags.json"
$genPath  = Join-Path $OutDir "ollama_generate.json"

Write-Host "Querying tags → $tagsPath" -ForegroundColor Cyan
Invoke-RestMethod -Method Get -Uri "$Host/api/tags" -OutFile $tagsPath

Write-Host "Generating sample on $Model → $genPath" -ForegroundColor Cyan
$payload = @{
    model  = $Model
    prompt = "hello from rawrxd validation"
} | ConvertTo-Json

Invoke-RestMethod -Method Post -Uri "$Host/api/generate" -ContentType "application/json" -Body $payload -OutFile $genPath

Write-Host "Done. Files:" -ForegroundColor Green
Write-Host "  $tagsPath"
Write-Host "  $genPath"
