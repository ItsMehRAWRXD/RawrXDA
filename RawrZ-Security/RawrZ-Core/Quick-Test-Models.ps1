# Quick-Test-Models.ps1 - Quick test for your AI models
# Run with: powershell -ExecutionPolicy Bypass -File Quick-Test-Models.ps1

Write-Host "RawrZ AI Models Quick Test" -ForegroundColor Cyan
Write-Host "=========================" -ForegroundColor Cyan

# Test BigDaddyG
Write-Host "`nTesting BigDaddyG..." -ForegroundColor Yellow

$BigDaddyGPaths = @(
    "D:\RawrZ-Core\BigDaddyG-Ultimate.exe",
    "D:\MyCopilot-IDE\BigDaddyG-Ultimate.exe",
    "D:\RawrZ-Core\BigDaddyG-Agentic-System.ps1"
)

$BigDaddyGFound = $false
foreach ($Path in $BigDaddyGPaths) {
    if (Test-Path $Path) {
        Write-Host "Found BigDaddyG at: $Path" -ForegroundColor Green
        try {
            if ($Path.EndsWith('.exe')) {
                Write-Host "Testing BigDaddyG executable..."
                $Process = Start-Process -FilePath $Path -ArgumentList "Hello, what can you do?" -NoNewWindow -Wait -PassThru
                Write-Host "BigDaddyG exit code: $($Process.ExitCode)" -ForegroundColor $(if($Process.ExitCode -eq 0){'Green'}else{'Red'})
            }
            elseif ($Path.EndsWith('.ps1')) {
                Write-Host "Testing BigDaddyG PowerShell script..."
                & $Path -Query "Hello, what can you do?"
            }
            $BigDaddyGFound = $true
            break
        }
        catch {
            Write-Host "Error testing BigDaddyG: $($_.Exception.Message)" -ForegroundColor Red
        }
    }
}

if (-not $BigDaddyGFound) {
    Write-Host "BigDaddyG not found in expected locations" -ForegroundColor Red
}

# Test backend
Write-Host "`nTesting RawrZ Chat Backend..." -ForegroundColor Yellow
try {
    $Response = Invoke-WebRequest -Uri "http://localhost:11440/health" -Method Get -TimeoutSec 5
    Write-Host "Backend is running (Status: $($Response.StatusCode))" -ForegroundColor Green
}
catch {
    Write-Host "Backend is not running or not accessible" -ForegroundColor Red
    Write-Host "Start it with: node D:\RawrZ-Core\RawrZ-Chat-Backend.js" -ForegroundColor Yellow
}

# Test other models
Write-Host "`nTesting other models..." -ForegroundColor Yellow

$Models = @('Reader', 'Coder', 'ASM-Expert', 'Java-Specialist')
foreach ($Model in $Models) {
    Write-Host "Testing $Model..." -ForegroundColor Yellow
    try {
        $Response = Invoke-WebRequest -Uri "http://localhost:11440/models/use/$Model" -Method Post -Body '{"query":"Test query"}' -ContentType "application/json" -TimeoutSec 10
        Write-Host "$Model: Working (Status: $($Response.StatusCode))" -ForegroundColor Green
    }
    catch {
        Write-Host "$Model: Not accessible - $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host "`nQuick test completed!" -ForegroundColor Cyan
Write-Host "For comprehensive testing, run: .\Test-AI-Models.ps1" -ForegroundColor Yellow
