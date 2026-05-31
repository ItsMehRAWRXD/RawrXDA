# ============================================================================
# smoke_test_all_routes.ps1
# Production smoke test with retry logic and extended timeouts
# ============================================================================

param(
    [string]$BaseUrl = "http://localhost:11434",
    [int]$DefaultTimeout = 30000,      # 30 seconds default
    [int]$LongTimeout = 120000,         # 120 seconds for chat routes
    [int]$MaxRetries = 2
)

$ErrorActionPreference = "Stop"
$script:SuccessCount = 0
$script:FailureCount = 0

# Routes that need longer timeouts
$longTimeoutRoutes = @(
    '/api/chat',
    '/ask',
    '/v1/chat/completions',
    '/api/generate'
)

function Test-RouteWithRetry {
    param(
        [string]$Url,
        [string]$Method = 'GET',
        [string]$Body = $null,
        [int]$Timeout = $DefaultTimeout,
        [int]$Retries = $MaxRetries
    )

    $fullUrl = "$BaseUrl$Url"
    $lastError = $null

    for ($i = 0; $i -le $Retries; $i++) {
        try {
            $params = @{
                Uri = $fullUrl
                Method = $Method
                TimeoutSec = [math]::Floor($Timeout / 1000)
                UseBasicParsing = $true
            }

            if ($Body) {
                $params['Body'] = $Body
                $params['ContentType'] = 'application/json'
            }

            $response = Invoke-RestMethod @params
            $script:SuccessCount++
            Write-Host "  ✓ $Url ($Method) - Success" -ForegroundColor Green
            return $response
        }
        catch {
            $lastError = $_
            if ($i -lt $Retries) {
                Write-Host "  ⚠ $Url ($Method) - Retry $($i + 1)/$Retries" -ForegroundColor Yellow
                Start-Sleep -Milliseconds 500
            }
        }
    }

    $script:FailureCount++
    Write-Host "  ✗ $Url ($Method) - Failed: $($lastError.Exception.Message)" -ForegroundColor Red
    return $null
}

function Test-HealthEndpoint {
    Write-Host "`n[Health Check]" -ForegroundColor Cyan
    $health = Test-RouteWithRetry -Url '/health' -Timeout $DefaultTimeout
    if ($health) {
        Write-Host "  Status: $($health.status)" -ForegroundColor Green
        Write-Host "  Model Loaded: $($health.model_loaded)" -ForegroundColor Green
    }
}

function Test-ChatRoutes {
    Write-Host "`n[Chat Routes - Extended Timeout]" -ForegroundColor Cyan

    $chatBody = @{
        messages = @(
            @{ role = "user"; content = "Hello, how are you?" }
        )
        max_tokens = 256
        temperature = 0.7
    } | ConvertTo-Json -Depth 3

    foreach ($route in $longTimeoutRoutes) {
        Test-RouteWithRetry -Url $route -Method 'POST' -Body $chatBody -Timeout $LongTimeout
    }
}

function Test-StaticRoutes {
    Write-Host "`n[Static Routes]" -ForegroundColor Cyan
    $routes = @('/', '/api/models', '/api/version')
    foreach ($route in $routes) {
        Test-RouteWithRetry -Url $route -Timeout $DefaultTimeout
    }
}

function Test-BatchInference {
    Write-Host "`n[Batch Inference]" -ForegroundColor Cyan
    $batchBody = @{
        prompts = @(
            "What is machine learning?",
            "Explain quantum computing",
            "How does a neural network work?"
        )
        max_tokens = 128
    } | ConvertTo-Json -Depth 3

    Test-RouteWithRetry -Url '/api/batch' -Method 'POST' -Body $batchBody -Timeout $LongTimeout
}

# Main execution
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrXD Sovereign Engine Smoke Test" -ForegroundColor Cyan
Write-Host "Base URL: $BaseUrl" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$startTime = Get-Date

Test-HealthEndpoint
Test-StaticRoutes
Test-ChatRoutes
Test-BatchInference

$endTime = Get-Date
$duration = ($endTime - $startTime).TotalSeconds

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Results:" -ForegroundColor Cyan
Write-Host "  Success: $script:SuccessCount" -ForegroundColor Green
Write-Host "  Failed:  $script:FailureCount" -ForegroundColor Red
Write-Host "  Duration: $([math]::Round($duration, 2))s" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

if ($script:FailureCount -gt 0) {
    exit 1
}
exit 0
