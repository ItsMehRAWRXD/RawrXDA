# Sample PowerShell for Testing IDE File Audit
# This file tests the file attachment -> backend -> Ollama pipeline

param(
    [string]$Target = "localhost",
    [int]$Port = 8080
)

function Invoke-Analysis {
    param([string]$Query)
    $body = @{
        question = $Query
        files = @()
    } | ConvertTo-Json
    
    Invoke-RestMethod -Uri "http://$Target:$Port/" -Method Post -Body $body -ContentType "application/json"
}

Write-Host "Test audit file loaded successfully"
$result = Invoke-Analysis -Query "Please analyze this test script"
Write-Host $result
