# Requires PowerShell 7.4+
# Byte-oriented pipe server for RawrXD pattern classification
# Protocol: 4-byte length + UTF-8 command, then per-command payloads

param(
    [string]$PipeName = "RawrXD_PatternBridge",
    [switch]$AsJob,
    [string]$ModulePath = "C:\\Users\\HiH8e\\Documents\\PowerShell\\Modules\\RawrXD_PatternBridge\\RawrXD_PatternBridge.psm1"
)

$ErrorActionPreference = 'Stop'

function Write-PipeResponse {
    param(
        [System.IO.Stream]$Stream,
        [byte[]]$Payload
    )

    $lengthBytes = [BitConverter]::GetBytes([int]($Payload?.Length ?? 0))
    $Stream.Write($lengthBytes, 0, 4)
    if ($Payload -and $Payload.Length -gt 0) {
        $Stream.Write($Payload, 0, $Payload.Length)
    }
    $Stream.Flush()
}

function Invoke-RawrXDSession {
    param(
        [System.IO.Pipes.NamedPipeServerStream]$Server,
        [System.Text.Encoding]$Encoding
    )

    $reader = [System.IO.BinaryReader]::new($Server, $Encoding, $true)

    try {
        while ($Server.IsConnected) {
            $cmdLenBytes = $reader.ReadBytes(4)
            if ($cmdLenBytes.Length -ne 4) { break }
            $cmdLen = [BitConverter]::ToInt32($cmdLenBytes, 0)
            if ($cmdLen -le 0 -or $cmdLen -gt 10MB) { break }

            $cmdBytes = $reader.ReadBytes($cmdLen)
            if ($cmdBytes.Length -ne $cmdLen) { break }
            $command = $Encoding.GetString($cmdBytes).ToUpperInvariant()

            switch ($command) {
                'PING' {
                    Write-PipeResponse -Stream $Server -Payload $Encoding.GetBytes('PONG')
                }
                'STATS' {
                    $stats = Get-RawrXDPatternStats
                    $json = $stats | ConvertTo-Json -Compress
                    Write-PipeResponse -Stream $Server -Payload $Encoding.GetBytes($json)
                }
                'CLASSIFY' {
                    $dataLenBytes = $reader.ReadBytes(4)
                    if ($dataLenBytes.Length -ne 4) { break }
                    $dataLen = [BitConverter]::ToInt32($dataLenBytes, 0)
                    if ($dataLen -lt 0 -or $dataLen -gt 50MB) { break }

                    $data = $reader.ReadBytes($dataLen)
                    if ($data.Length -ne $dataLen) { break }

                    $text = $Encoding.GetString($data)
                    $result = Invoke-RawrXDClassification -Code $text -Context ''
                    $response = [PSCustomObject]@{
                        Type       = $result.Type
                        Pattern    = $result.TypeName
                        Confidence = $result.Confidence
                        IsPattern  = $result.IsPattern
                        Review     = $result.RequiresManualReview
                    }
                    $json = $response | ConvertTo-Json -Compress
                    Write-PipeResponse -Stream $Server -Payload $Encoding.GetBytes($json)
                }
                'EXIT' {
                    Write-Host "[PipeServer] Shutting down" -ForegroundColor Yellow
                    return $false
                }
                default {
                    Write-PipeResponse -Stream $Server -Payload $Encoding.GetBytes('{"error":"Unknown command"}')
                }
            }
        }
    }
    finally {
        $reader.Dispose()
    }

    return $true
}

function Invoke-RawrXDByteServer {
    param(
        [string]$PipeName,
        [string]$ModulePath
    )

    if (Test-Path $ModulePath) {
        Import-Module $ModulePath -Force
    } else {
        Import-Module RawrXD_PatternBridge -Force
    }

    $encoding = [System.Text.Encoding]::UTF8
    Write-Host "[PipeServer] Started on $PipeName" -ForegroundColor Green

    while ($true) {
        $server = $null

        try {
            $server = [System.IO.Pipes.NamedPipeServerStream]::new(
                $PipeName,
                [System.IO.Pipes.PipeDirection]::InOut,
                [System.IO.Pipes.NamedPipeServerStream]::MaxAllowedServerInstances,
                [System.IO.Pipes.PipeTransmissionMode]::Message,
                [System.IO.Pipes.PipeOptions]::Asynchronous
            )

            while ($true) {
                try {
                    $server.WaitForConnection()
                    $keepRunning = Invoke-RawrXDSession -Server $server -Encoding $encoding
                    if (-not $keepRunning) { return }
                }
                finally {
                    if ($server.IsConnected) { $server.Disconnect() }
                }
            }
        }
        catch {
            Write-Warning "[PipeServer] Error: $_"
            Start-Sleep -Milliseconds 100
        }
        finally {
            if ($server) {
                $server.Dispose()
            }
        }
    }
}

if ($AsJob) {
    Start-Job -Name 'RawrXD-PipeServer' -ScriptBlock ${function:Invoke-RawrXDByteServer} -ArgumentList $PipeName, $ModulePath | Out-Null
    Write-Host "Pipe server started as background job. Monitor with: Get-Job -Name RawrXD-PipeServer" -ForegroundColor Green
} else {
    Invoke-RawrXDByteServer -PipeName $PipeName -ModulePath $ModulePath
}
