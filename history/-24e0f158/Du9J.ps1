# Requires PowerShell 7.4+
# Byte-oriented pipe server for RawrXD pattern classification
# Protocol: 4-byte length + UTF-8 command, then per-command payloads

param(
    [string]$PipeName = "RawrXD_PatternBridge",
    [switch]$AsJob,
    [string]$ModulePath = "C:\\Users\\HiH8e\\Documents\\PowerShell\\Modules\\RawrXD_PatternBridge\\RawrXD_PatternBridge.psm1"
)

$ErrorActionPreference = 'Stop'

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

    Write-Host "[PipeServer] Started on $PipeName" -ForegroundColor Green

    while ($true) {
        $server = [System.IO.Pipes.NamedPipeServerStream]::new(
            $PipeName,
            [System.IO.Pipes.PipeDirection]::InOut,
            1,
            [System.IO.Pipes.PipeTransmissionMode]::Message,
            [System.IO.Pipes.PipeOptions]::Asynchronous
        )

        try {
            $null = $server.WaitForConnectionAsync().Result
            $reader = [System.IO.BinaryReader]::new($server)

            while ($server.IsConnected) {
                $cmdLenBytes = $reader.ReadBytes(4)
                if ($cmdLenBytes.Length -ne 4) { break }
                $cmdLen = [BitConverter]::ToInt32($cmdLenBytes, 0)
                if ($cmdLen -le 0 -or $cmdLen -gt 10MB) { break }

                $cmdBytes = $reader.ReadBytes($cmdLen)
                if ($cmdBytes.Length -ne $cmdLen) { break }
                $command = [Text.Encoding]::UTF8.GetString($cmdBytes)

                switch ($command) {
                    'PING' {
                        $payload = [Text.Encoding]::UTF8.GetBytes('PONG')
                        $lenBytes = [BitConverter]::GetBytes($payload.Length)
                        $server.Write($lenBytes, 0, 4)
                        $server.Write($payload, 0, $payload.Length)
                        $server.Flush()
                    }
                    'STATS' {
                        $stats = Get-RawrXDPatternStats
                        $json = $stats | ConvertTo-Json -Compress
                        $jsonBytes = [Text.Encoding]::UTF8.GetBytes($json)
                        $lenBytes = [BitConverter]::GetBytes($jsonBytes.Length)
                        $server.Write($lenBytes, 0, 4)
                        $server.Write($jsonBytes, 0, $jsonBytes.Length)
                        $server.Flush()
                    }
                    'CLASSIFY' {
                        $dataLenBytes = $reader.ReadBytes(4)
                        if ($dataLenBytes.Length -ne 4) { break }
                        $dataLen = [BitConverter]::ToInt32($dataLenBytes, 0)
                        if ($dataLen -lt 0 -or $dataLen -gt 50MB) { break }

                        $data = $reader.ReadBytes($dataLen)
                        if ($data.Length -ne $dataLen) { break }

                        $text = [Text.Encoding]::UTF8.GetString($data)
                        $result = Invoke-RawrXDClassification -Code $text -Context ''
                        $response = [PSCustomObject]@{
                            Type       = $result.Type
                            Pattern    = $result.TypeName
                            Confidence = $result.Confidence
                            IsPattern  = $result.IsPattern
                            Review     = $result.RequiresManualReview
                        }
                        $json = $response | ConvertTo-Json -Compress
                        $jsonBytes = [Text.Encoding]::UTF8.GetBytes($json)
                        $lenBytes = [BitConverter]::GetBytes($jsonBytes.Length)
                        $server.Write($lenBytes, 0, 4)
                        $server.Write($jsonBytes, 0, $jsonBytes.Length)
                        $server.Flush()
                    }
                    'EXIT' {
                        Write-Host "[PipeServer] Shutting down" -ForegroundColor Yellow
                        return
                    }
                    default {
                        $payload = [Text.Encoding]::UTF8.GetBytes('{"error":"Unknown command"}')
                        $lenBytes = [BitConverter]::GetBytes($payload.Length)
                        $server.Write($lenBytes, 0, 4)
                        $server.Write($payload, 0, $payload.Length)
                        $server.Flush()
                    }
                }
            }
        }
        catch {
            Write-Warning "[PipeServer] Error: $_"
        }
        finally {
            if ($server.IsConnected) { $server.Disconnect() }
            $server.Dispose()
        }
    }
}

if ($AsJob) {
    Start-Job -Name 'RawrXD-PipeServer' -ScriptBlock ${function:Invoke-RawrXDByteServer} -ArgumentList $PipeName, $ModulePath | Out-Null
    Write-Host "Pipe server started as background job. Monitor with: Get-Job -Name RawrXD-PipeServer" -ForegroundColor Green
} else {
    Invoke-RawrXDByteServer -PipeName $PipeName -ModulePath $ModulePath
}
