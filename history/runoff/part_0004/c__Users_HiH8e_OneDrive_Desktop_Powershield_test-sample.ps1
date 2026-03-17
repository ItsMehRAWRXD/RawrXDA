function Get-UserData {
    param([string]$userId)
    $user = Get-ADUser -Identity $userId
    return $user
}

function Test-Connection {
    param([string]$Server)
    $result = Test-NetConnection -ComputerName $Server
    if ($result.PingSucceeded) {
        Write-Host "Connected"
    }
}

function Format-Output {
    param($data)
    Write-Host $data
}

function Validate-Input {
    param([string]$input)
    if ([string]::IsNullOrEmpty($input)) {
        return $false
    }
    return $true
}

function Process-Item {
    param($item)
    $processed = $item | Select-Object -Property *
    return $processed
}
