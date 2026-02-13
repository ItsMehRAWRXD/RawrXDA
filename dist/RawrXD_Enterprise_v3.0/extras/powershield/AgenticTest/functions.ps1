function Get-Info {
    return "Sample function"
}

function Process-Data {
    param($input)
    return $input
}

function Get-AgenticStatus {
    <#
    .SYNOPSIS
        Returns current system and agent status
    #>
    return @{
        Timestamp = Get-Date
        ComputerName = $env:COMPUTERNAME
        PSVersion = $PSVersionTable.PSVersion
        AgentStatus = "Active"
        TasksCompleted = 4
    }
}

function Get-AgenticStatus {
    <#
    .SYNOPSIS
        Returns current system and agent status
    #>
    return @{
        Timestamp = Get-Date
        ComputerName = $env:COMPUTERNAME
        PSVersion = $PSVersionTable.PSVersion
        AgentStatus = "Active"
        TasksCompleted = 4
    }
}
