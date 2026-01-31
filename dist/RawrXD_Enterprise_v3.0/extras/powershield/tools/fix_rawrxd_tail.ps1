# Fix potential unclosed here-strings and block comments at EOF for RawrXD.ps1
$src = 'RawrXD.ps1'
$bak = $src + '.bak.' + (Get-Date -Format 'yyyyMMdd_HHmmss')
Copy-Item -Path $src -Destination $bak -Force
Write-Host "Backup created: $bak"

# Read entire file as raw text
$content = Get-Content -LiteralPath $src -Raw -ErrorAction Stop

# Count here-string starts/ends (double-quoted and single-quoted)
$startDouble = ([regex]::Matches($content, '^[ \t]*@"', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count
$endDouble = ([regex]::Matches($content, '"@[ \t]*$', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count
$startSingle = ([regex]::Matches($content, "^[ \t]*@'", [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count
$endSingle = ([regex]::Matches($content, "'@[ \t]*$", [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count

# Count block comment markers
$startBlock = ([regex]::Matches($content, '<#')).Count
$endBlock = ([regex]::Matches($content, '#>')).Count

Write-Host ('Counts: @"starts={0} ends={1} @''starts={2} ends={3} <#starts={4} #>ends={5}' -f $startDouble,$endDouble,$startSingle,$endSingle,$startBlock,$endBlock)

$append = ""
if ($startDouble -gt $endDouble) {
    $append += "`r`n""@`r`n"
}
if ($startSingle -gt $endSingle) {
    $append += "`r`n'@`r`n"
}
if ($startBlock -gt $endBlock) {
    $append += "`r`n#>`r`n"
}

if ($append -ne "") {
    Add-Content -LiteralPath $src -Value "`r`n# --- appended by fix_rawrxd_tail.ps1 ---`r`n$append" -Encoding UTF8
    Write-Host "Appended missing terminators to $src"
} else {
    Write-Host "No missing terminators detected. No changes made."
}

# Run parse check
$errors=[ref]$null
[void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path $src).Path,[ref]$null,$errors)
if ($errors.Value) {
    Write-Host "PARSE_ERRORS:"
    foreach ($e in $errors.Value) { Write-Host "$($e.Message) at line $($e.Extent.StartLineNumber)" }
    exit 1
}
else {
    Write-Host "PARSE_OK"
    exit 0
}
