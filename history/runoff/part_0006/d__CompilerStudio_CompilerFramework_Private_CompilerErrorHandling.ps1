# Private\CompilerErrorHandling.ps1

function Add-CompilerError {
    param (
        [string]$Message,
        [int]$Line = 0,
        [int]$Column = 0
    )

    $error = [PSCustomObject]@{
        Message = $Message
        Line = $Line
        Column = $Column
    }

    $global:CompilerContext.Errors += $error
}

function Add-CompilerWarning {
    param (
        [string]$Message,
        [int]$Line = 0,
        [int]$Column = 0
    )

    $warning = [PSCustomObject]@{
        Message = $Message
        Line = $Line
        Column = $Column
    }

    $global:CompilerContext.Warnings += $warning
}

function Get-CompilerErrors {
    return $global:CompilerContext.Errors
}

function Get-CompilerWarnings {
    return $global:CompilerContext.Warnings
}
