# Repair unmatched closing braces in RawrXD.ps1 by removing minimal trailing '}' characters to keep brace balance non-negative.
$src = 'RawrXD.ps1'
$bak = $src + '.pre_repair.' + (Get-Date -Format 'yyyyMMdd_HHmmss')
Copy-Item -Path $src -Destination $bak -Force
Write-Host "Backup created: $bak"

$lines = Get-Content -LiteralPath $src -ErrorAction Stop
$balance = 0
$modified = @()
$lineNum = 0
$removedCount = 0
$skippedLines = 0

foreach ($line in $lines) {
    $lineNum++
    # Remove content inside single and double quotes to avoid counting braces in strings
    $clean = $line -replace '"([^"\\]|\\.)*"','' -replace "'([^'\\]|\\.)*'",''

    $opens = ([regex]::Matches($clean, '\{')).Count
    $closes = ([regex]::Matches($clean, '\}')).Count

    if ($balance - $closes -lt 0) {
        # Need to remove some closing braces from this line
        $needed = $closes - $balance
        $origLine = $line
        for ($r=0; $r -lt $needed; $r++) {
            # Try to remove one trailing '}' first
            if ($line -match '\}\s*$') {
                $line = $line -replace '\}\s*$',''
                $removedCount++
            }
            else {
                # If no trailing '}', try to remove any '}' from end of tokens
                if ($line -match '\}') {
                    # remove first occurrence from right
                    $rev = -join (($line.ToCharArray())[-1..0])
                    # simpler: remove first occurrence of '}' from the right using regex to remove last '}' anywhere
                    $line = ($line -replace '(.*)\}', '$1')
                    $removedCount++
                }
                else {
                    # Nothing to remove, skip writing this line to avoid negative balance
                    $line = ''
                    $skippedLines++
                    break
                }
            }
            # Recompute clean/closes after removal
            $clean = $line -replace '"([^"\\]|\\.)*"','' -replace "'([^'\\]|\\.)*'",''
            $closes = ([regex]::Matches($clean, '\}')).Count
            if ($balance - $closes -ge 0) { break }
        }
    }

    # Update balance
    $clean = $line -replace '"([^"\\]|\\.)*"','' -replace "'([^'\\]|\\.)*'",''
    $opens = ([regex]::Matches($clean, '\{')).Count
    $closes = ([regex]::Matches($clean, '\}')).Count
    $balance += $opens - $closes

    $modified += $line
}

Write-Host "Repair pass complete. Removed braces: $removedCount; Skipped lines: $skippedLines; Final balance: $balance"

# If there are unclosed opens (positive balance), append closing braces
if ($balance -gt 0) {
    Write-Host "Appending $balance closing brace(s) at EOF"
    for ($i=0; $i -lt $balance; $i++) { $modified += '}' }
    $balance = 0
}

# Write modified content to temp file then replace original
$temp = $src + '.repaired'
$modified | Set-Content -LiteralPath $temp -Encoding UTF8
Move-Item -Path $temp -Destination $src -Force
Write-Host "Repaired file written to $src (original backup: $bak)"

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
