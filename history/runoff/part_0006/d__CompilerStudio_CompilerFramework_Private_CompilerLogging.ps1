# Private\CompilerLogging.ps1

function Log-CompilerMessage {
    param (
        [string]$Message,
        [string]$Level = "Info"
    )

    $log = [PSCustomObject]@{
        Message = $Message
        Level = $Level
        Timestamp = Get-Date
    }

    $global:CompilerContext.Logs += $log

    Write-Host "[$Level] $Message" -ForegroundColor (
        switch ($Level) {
            "Info" { "Green" }
            "Warning" { "Yellow" }
            "Error" { "Red" }
            default { "White" }
        }
    )
}

function Get-CompilerLogs {
    return $global:CompilerContext.Logs
}
