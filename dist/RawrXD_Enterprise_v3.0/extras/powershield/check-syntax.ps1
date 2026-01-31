# Syntax check for RawrXD.ps1
$script = Get-Content 'c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1' -Raw
$errors = $null
$tokens = $null

[void][System.Management.Automation.Language.Parser]::ParseInput($script, [ref]$tokens, [ref]$errors)

if ($errors.Count -gt 0) {
  Write-Host "SYNTAX ERRORS FOUND ($($errors.Count) errors):" -ForegroundColor Red
  $errors | ForEach-Object {
    Write-Host "Line $($_.Extent.StartLineNumber): $($_.Message)" -ForegroundColor Yellow
  }
}
else {
  Write-Host "✅ No syntax errors found in RawrXD.ps1" -ForegroundColor Green
}
