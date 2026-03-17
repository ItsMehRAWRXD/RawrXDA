param(
    [string]$Command = 'ping',
    [string]$Path = '',
    [string]$Name = '',
    [string]$Directory = ''
)

# Connect to QLocalServer (named pipe) started by IDE
Add-Type -AssemblyName System.Core
Add-Type -AssemblyName System.IO
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Pipes

$serverName = 'RawrXD_IDE_Server'

function Send-Command {
    param(
        [string]$Cmd,
        [hashtable]$Params
    )
    try {
        $client = New-Object System.IO.Pipes.NamedPipeClientStream '.', $serverName, ([System.IO.Pipes.PipeDirection]::InOut), ([System.IO.Pipes.PipeOptions]::None)
        $client.Connect(3000)

        $writer = New-Object System.IO.StreamWriter($client)
        $writer.AutoFlush = $true
        $reader = New-Object System.IO.StreamReader($client)

        $payload = @{ command = $Cmd; params = $Params } | ConvertTo-Json -Compress
        $writer.WriteLine($payload)

        # Read response line
        $responseLine = $reader.ReadLine()
        if (-not $responseLine) { throw 'No response received' }
        $response = $responseLine | ConvertFrom-Json
        return $response
    } catch {
        Write-Error "Failed to send command: $($_.Exception.Message)"
        return $null
    }
}

switch ($Command.ToLower()) {
    'ping' {
        $res = Send-Command -Cmd 'ping' -Params @{}
        $res
    }
    'open-file' {
        if (-not $Path) { Write-Error 'Specify -Path'; exit 1 }
        $res = Send-Command -Cmd 'open-file' -Params @{ path = $Path }
        $res
    }
    'load-model' {
        if (-not $Path) { Write-Error 'Specify -Path'; exit 1 }
        $res = Send-Command -Cmd 'load-model' -Params @{ path = $Path }
        $res
    }
    'toggle-dock' {
        if (-not $Name) { Write-Error 'Specify -Name'; exit 1 }
        $res = Send-Command -Cmd 'toggle-dock' -Params @{ name = $Name }
        $res
    }
    'get-status' {
        $res = Send-Command -Cmd 'get-status' -Params @{}
        $res
    }
    'list-files' {
        $dir = if ($Directory) { $Directory } else { (Get-Location).Path }
        $res = Send-Command -Cmd 'list-files' -Params @{ directory = $dir }
        $res
    }
    default {
        Write-Host 'Commands: ping | open-file -Path <file> | load-model -Path <gguf> | toggle-dock -Name <output|metrics|fileExplorer|suggestions|security|optimizations> | get-status | list-files [-Directory <dir>]' -ForegroundColor Yellow
    }
}
