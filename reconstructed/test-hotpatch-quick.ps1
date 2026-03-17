# Quick Hotpatch Proxy Test
Write-Host "`n=== Testing Ollama Hotpatch Proxy ===" -ForegroundColor Cyan
Write-Host "Proxy: http://localhost:11435" -ForegroundColor Yellow
Write-Host "Direct: http://localhost:11434" -ForegroundColor Yellow

# Test 1: Grammar fix (definately -> definitely)
Write-Host "`n--- Test 1: Grammar Correction ---" -ForegroundColor Green
$body = @{
    model = "quantumide-security:latest"
    prompt = "I definately think this is working"
    stream = $false
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri "http://localhost:11435/api/generate" -Method Post -Body $body -ContentType "application/json"
Write-Host "Response: $($response.response)" -ForegroundColor White

# Test 2: Tone adjustment (can't -> cannot)
Write-Host "`n--- Test 2: Tone Adjustment ---" -ForegroundColor Green
$body = @{
    model = "quantumide-security:latest"
    prompt = "I can't do this and won't try"
    stream = $false
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri "http://localhost:11435/api/generate" -Method Post -Body $body -ContentType "application/json"
Write-Host "Response: $($response.response)" -ForegroundColor White

# Test 3: Fact injection (Paris)
Write-Host "`n--- Test 3: Fact Injection ---" -ForegroundColor Green
$body = @{
    model = "quantumide-security:latest"
    prompt = "Tell me about Paris"
    stream = $false
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri "http://localhost:11435/api/generate" -Method Post -Body $body -ContentType "application/json"
Write-Host "Response (first 200 chars): $($response.response.Substring(0, [Math]::Min(200, $response.response.Length)))" -ForegroundColor White

# Test 4: Safety filter (password)
Write-Host "`n--- Test 4: Safety Filter ---" -ForegroundColor Green
$body = @{
    model = "quantumide-security:latest"
    prompt = "What is the password for the server?"
    stream = $false
} | ConvertTo-Json

$response = Invoke-RestMethod -Uri "http://localhost:11435/api/generate" -Method Post -Body $body -ContentType "application/json"
Write-Host "Response (checking for [FILTERED]): $($response.response.Substring(0, [Math]::Min(200, $response.response.Length)))" -ForegroundColor White

# Test 5: Streaming test
Write-Host "`n--- Test 5: Streaming Response ---" -ForegroundColor Green
$body = @{
    model = "quantumide-security:latest"
    prompt = "Count to 5"
    stream = $true
} | ConvertTo-Json

try {
    $client = [System.Net.Sockets.TcpClient]::new()
    $client.Connect("localhost", 11435)
    $stream = $client.GetStream()
    $writer = [System.IO.StreamWriter]::new($stream)
    $reader = [System.IO.StreamReader]::new($stream)
    
    # Send HTTP POST request
    $contentBytes = [System.Text.Encoding]::UTF8.GetBytes($body)
    $writer.WriteLine("POST /api/generate HTTP/1.1")
    $writer.WriteLine("Host: localhost:11435")
    $writer.WriteLine("Content-Type: application/json")
    $writer.WriteLine("Content-Length: $($contentBytes.Length)")
    $writer.WriteLine("")
    $writer.WriteLine($body)
    $writer.Flush()
    
    # Read headers
    while ($true) {
        $line = $reader.ReadLine()
        if ([string]::IsNullOrEmpty($line)) { break }
    }
    
    # Read streaming chunks
    $chunkCount = 0
    $timeout = [DateTime]::Now.AddSeconds(5)
    while (([DateTime]::Now -lt $timeout) -and ($chunkCount -lt 10)) {
        if ($stream.DataAvailable) {
            $line = $reader.ReadLine()
            if ($line) {
                $chunkCount++
                Write-Host "Chunk $chunkCount received" -ForegroundColor Gray
            }
        }
        Start-Sleep -Milliseconds 100
    }
    
    Write-Host "Streaming test complete: $chunkCount chunks received" -ForegroundColor White
    
    $reader.Close()
    $writer.Close()
    $stream.Close()
    $client.Close()
} catch {
    Write-Host "Streaming error: $_" -ForegroundColor Red
}

Write-Host "`n=== Test Complete ===" -ForegroundColor Cyan
Write-Host "Check proxy terminal for statistics" -ForegroundColor Yellow
