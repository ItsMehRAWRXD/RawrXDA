$path = Resolve-Path 'RawrXD.ps1'
$errors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile($path.Path, [ref]$null, [ref]$errors)
if ($errors) {
    foreach ($e in $errors) {
        Write-Output ("ERROR: {0} at line {1}" -f $e.Message, $e.Extent.StartLineNumber)
    }
    exit 1
}
else {
    Write-Output 'PARSE_OK'
    exit 0
}
