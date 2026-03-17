$tokens = $null
$errors = $null

[System.Management.Automation.Language.Parser]::ParseFile('UnifiedAgentProcessor.ps1', [ref]$tokens, [ref]$errors) | Out-Null

if ($errors -and $errors.Count -gt 0) {
    foreach ($parseError in $errors) {
        Write-Host "Line $($parseError.Extent.StartLineNumber), Col $($parseError.Extent.StartColumnNumber): $($parseError.Message)"
    }
} else {
    Write-Host 'No parse errors.'
}