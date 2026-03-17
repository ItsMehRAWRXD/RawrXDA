param(
    [int]$TimeoutSeconds = 25
)

$exe = Join-Path $PSScriptRoot '..\build\bin\RawrXD-ModelLoader.exe'
if (!(Test-Path $exe)) {
    Write-Host "Executable not found: $exe" -ForegroundColor Red
    exit 2
}

Write-Host "Starting RawrXD Model Loader (background)..." -ForegroundColor Cyan
$proc = Start-Process -FilePath $exe -WindowStyle Hidden -PassThru

$port = 11434
$baseUrl = "http://localhost:$port"
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)

Write-Host "Waiting for API server on port $port" -ForegroundColor Yellow
$ready = $false
while ((Get-Date) -lt $deadline) {
    try {
        $resp = Invoke-RestMethod -Uri "$baseUrl/api/tags" -Method Get -TimeoutSec 3
        if ($resp) { $ready = $true; break }
    } catch { Start-Sleep -Milliseconds 400 }
}

if (-not $ready) {
    Write-Host "Server did not become ready within timeout" -ForegroundColor Red
    try { $proc | Stop-Process -Force } catch {}
    exit 3
}
Write-Host "API server is responsive" -ForegroundColor Green

# Endpoint checks
$errors = @()
try {
    $tags = Invoke-RestMethod -Uri "$baseUrl/api/tags" -Method Get -TimeoutSec 5
    if (-not $tags) { $errors += 'tags endpoint empty' }
} catch { $errors += "tags endpoint error: $($_.Exception.Message)" }

try {
    $chatSchema = Invoke-RestMethod -Uri "$baseUrl/v1/models" -Method Get -TimeoutSec 5
    if (-not $chatSchema) { $errors += 'models endpoint empty' }
} catch { $errors += "models endpoint error: $($_.Exception.Message)" }

if ($errors.Count -gt 0) {
    Write-Host "Smoke test failures:" -ForegroundColor Red
    $errors | ForEach-Object { Write-Host " - $_" -ForegroundColor Red }
    try { $proc | Stop-Process -Force } catch {}
    exit 4
}

Write-Host "All smoke tests passed" -ForegroundColor Green
try { $proc | Stop-Process -Force } catch {}
exit 0
