[CmdletBinding()]
param(
    [string]$BaseUrl = "https://example-tunnel.ngrok-free.app",
    [string]$ApiKey = $env:API_KEY,
    [string]$Model = "deepseek-coder-v2-32k"
)

if (-not $ApiKey) {
    Write-Warning "API key not supplied. Pass -ApiKey or set the API_KEY environment variable."
    return
}

$headers = @{
    "Authorization" = "Bearer $ApiKey"
    "Content-Type"  = "application/json"
}

Write-Host ">>> GET $BaseUrl/v1/models"
$models = Invoke-RestMethod -Method Get -Uri "$BaseUrl/v1/models" -Headers $headers
$models | ConvertTo-Json -Depth 5

Write-Host ""
Write-Host ">>> POST $BaseUrl/v1/chat/completions"
$body = @{
    model = $Model
    temperature = 0.1
    messages = @(
        @{ role = "system"; content = "You are a helpful coding assistant." },
        @{ role = "user"; content = "Return a Python function that adds two numbers." }
    )
} | ConvertTo-Json -Depth 5

$response = Invoke-RestMethod -Method Post -Uri "$BaseUrl/v1/chat/completions" -Headers $headers -Body $body
$response.choices[0].message.content

Write-Host ""
Write-Host "✅ Smoke test complete."

