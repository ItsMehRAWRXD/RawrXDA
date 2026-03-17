# Private\CompilerOptimizer.ps1

function Optimize-IRCode {
    param (
        [string]$IRCode
    )

    $optimizedIRCode = $IRCode
    $optimizedIRCode = $optimizedIRCode -replace '^\s*`n', ''
    return $optimizedIRCode
}
