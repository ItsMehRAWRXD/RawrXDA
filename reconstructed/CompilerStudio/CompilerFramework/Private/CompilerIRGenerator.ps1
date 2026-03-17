# Private\CompilerIRGenerator.ps1

function Generate-IRCode {
    param (
        [PSCustomObject]$ParseTree
    )

    $irCode = ""

    foreach ($child in $ParseTree.Children) {
        $irCode += "IR: $($child.Type) - $($child.Value)`n"
    }

    return $irCode
}
