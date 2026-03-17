# Private\CompilerParser.ps1

function Parse-Tokens {
    param (
        [array]$Tokens
    )

    $parseTree = [PSCustomObject]@{
        Type = "Program"
        Children = @()
    }

    foreach ($token in $Tokens) {
        $parseTree.Children += [PSCustomObject]@{
            Type = $token.Type
            Value = $token.Value
            Line = $token.Line
            Column = $token.Column
        }
    }

    return $parseTree
}
