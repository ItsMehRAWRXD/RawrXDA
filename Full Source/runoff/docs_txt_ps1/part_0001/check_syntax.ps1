
try {
    $path = "D:\lazy init ide\auto_generated_methods\RawrXD.TestFramework.psm1"
    $ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$null, [ref]$null)
} catch {
    Write-Host "Caught parsing error:"
    Write-Host $_
}

$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
if ($errors.Count -gt 0) {
    Write-Host "Found $($errors.Count) errors:"
    foreach ($err in $errors) {
        Write-Host "Line $($err.Extent.StartLineNumber) Column $($err.Extent.StartColumnNumber): $($err.Message)"
        Write-Host "Code: '$($err.Extent.Text)'"
    }
} else {
    Write-Host "No syntax errors found via AST."
}
