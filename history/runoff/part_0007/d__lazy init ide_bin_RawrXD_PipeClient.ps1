# ============================================================================
# RawrXD Pipe Client - Binary Protocol Implementation
# Matches RawrXD_NativeHost.exe binary protocol
# ============================================================================

function Invoke-RawrXDPipeCommand {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Command,
        
        [Parameter(Mandatory=$false)]
        [byte[]]$Data,
        
        [Parameter(Mandatory=$false)]
        [string]$PipeName = "RawrXD_PatternBridge",
        
        [Parameter(Mandatory=$false)]
        [int]$TimeoutMs = 2000
    )
    
    try {
        $pipe = [System.IO.Pipes.NamedPipeClientStream]::new(".", $PipeName, [System.IO.Pipes.PipeDirection]::InOut)
        $pipe.Connect($TimeoutMs)
        
        # Send command with binary protocol: [4-byte length][command bytes]
        $cmdBytes = [System.Text.Encoding]::ASCII.GetBytes($Command)
        $lengthBytes = [System.BitConverter]::GetBytes([int32]$cmdBytes.Length)
        
        $pipe.Write($lengthBytes, 0, 4)
        $pipe.Write($cmdBytes, 0, $cmdBytes.Length)
        $pipe.Flush()
        
        # If data is provided (for CLASSIFY command), send it
        if ($Data) {
            $dataLengthBytes = [System.BitConverter]::GetBytes([int32]$Data.Length)
            $pipe.Write($dataLengthBytes, 0, 4)
            $pipe.Write($Data, 0, $Data.Length)
            $pipe.Flush()
        }
        
        # Read response: [4-byte length][response bytes]
        $responseLengthBytes = New-Object byte[] 4
        $bytesRead = $pipe.Read($responseLengthBytes, 0, 4)
        
        if ($bytesRead -ne 4) {
            throw "Failed to read response length"
        }
        
        $responseLength = [System.BitConverter]::ToInt32($responseLengthBytes, 0)
        
        if ($responseLength -le 0 -or $responseLength -gt 65536) {
            throw "Invalid response length: $responseLength"
        }
        
        $responseBytes = New-Object byte[] $responseLength
        $bytesRead = $pipe.Read($responseBytes, 0, $responseLength)
        
        if ($bytesRead -ne $responseLength) {
            throw "Failed to read complete response"
        }
        
        $responseText = [System.Text.Encoding]::UTF8.GetString($responseBytes)
        
        $pipe.Close()
        
        # Try to parse as JSON
        try {
            return ($responseText | ConvertFrom-Json)
        }
        catch {
            return [PSCustomObject]@{
                RawResponse = $responseText
                ParseError = $_.Exception.Message
            }
        }
    }
    catch {
        throw "Pipe communication error: $($_.Exception.Message)"
    }
}

function Test-RawrXDPipe {
    [CmdletBinding()]
    param(
        [string]$PipeName = "RawrXD_PatternBridge"
    )
    
    Write-Host "[Test] Testing RawrXD Pipe Server" -ForegroundColor Cyan
    Write-Host ("="*60)
    
    try {
        # Test PING
        Write-Host "`n[1/4] Testing PING..." -ForegroundColor Yellow
        $result = Invoke-RawrXDPipeCommand -Command "PING" -PipeName $PipeName
        Write-Host "  Status: $($result.Status)" -ForegroundColor Green
        Write-Host "  Version: $($result.Version)" -ForegroundColor Green
        
        # Test INFO
        Write-Host "`n[2/4] Testing INFO..." -ForegroundColor Yellow
        $result = Invoke-RawrXDPipeCommand -Command "INFO" -PipeName $PipeName
        Write-Host "  Engine: $($result.Engine)" -ForegroundColor Green
        Write-Host "  Mode: $($result.Mode)" -ForegroundColor Green
        Write-Host "  CPU: $($result.CPU)" -ForegroundColor Green
        
        # Test STATS
        Write-Host "`n[3/4] Testing STATS..." -ForegroundColor Yellow
        $result = Invoke-RawrXDPipeCommand -Command "STATS" -PipeName $PipeName
        Write-Host "  Total Requests: $($result.TotalRequests)" -ForegroundColor Green
        Write-Host "  Total Matches: $($result.TotalMatches)" -ForegroundColor Green
        Write-Host "  Uptime: $($result.Uptime) ms" -ForegroundColor Green
        
        # Test CLASSIFY
        Write-Host "`n[4/4] Testing CLASSIFY..." -ForegroundColor Yellow
        $testData = [System.Text.Encoding]::UTF8.GetBytes("BUG: critical memory leak in parser")
        $result = Invoke-RawrXDPipeCommand -Command "CLASSIFY" -Data $testData -PipeName $PipeName
        Write-Host "  Pattern: $($result.Pattern)" -ForegroundColor Green
        Write-Host "  Confidence: $($result.Confidence)" -ForegroundColor Green
        Write-Host "  Priority: $($result.Priority)" -ForegroundColor Green
        
        Write-Host "`n✅ All tests passed!" -ForegroundColor Green
    }
    catch {
        Write-Host "`n❌ Test failed: $($_.Exception.Message)" -ForegroundColor Red
    }
}

function Invoke-RawrXDClassifyText {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true, Position=0)]
        [string]$Text,
        
        [Parameter(Mandatory=$false)]
        [string]$PipeName = "RawrXD_PatternBridge"
    )
    
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    return Invoke-RawrXDPipeCommand -Command "CLASSIFY" -Data $bytes -PipeName $PipeName
}

# Export functions
Export-ModuleMember -Function @(
    'Invoke-RawrXDPipeCommand',
    'Test-RawrXDPipe',
    'Invoke-RawrXDClassifyText'
)
