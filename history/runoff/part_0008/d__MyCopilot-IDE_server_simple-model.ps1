param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Args,
    [Parameter(ValueFromPipeline=$true)]
    [string]$InputObject
)

# Get the input line
$prefix = $InputObject
if (-not $prefix) { $prefix = ($Args -join ' ') }
$lines = ($prefix -split "`r?`n")
$last = if ($lines.Count -gt 0) { $lines[-1] } else { '' }

# Basic bracket/delimiter completions
if ($last -match '\{$') { 
    Write-Output "}"
    exit 
}
if ($last -match '\($') { 
    Write-Output ")"
    exit 
}
if ($last -match '\[$') { 
    Write-Output "]"
    exit 
}

# Get last token
$token = ($last -split '\s+')[-1]
if ([string]::IsNullOrWhiteSpace($token)) { $token = '' }

# Context-aware suggestions
if ($token -match '^function') {
    Write-Output "example() {"
    Write-Output "  // TODO"
    Write-Output "}"
} elseif ($token -match '^if') {
    Write-Output "(condition) {"
    Write-Output "  // code"
    Write-Output "}"
} elseif ($token -match '^for') {
    Write-Output "(let i = 0; i < 10; i++) {"
    Write-Output "  // loop"
    Write-Output "}"
} elseif ($token -match '^while') {
    Write-Output "(condition) {"
    Write-Output "  // code"
    Write-Output "}"
} elseif ($token -match '^try') {
    Write-Output "{"
    Write-Output "  // code"
    Write-Output "} catch (e) {"
    Write-Output "  // error"
    Write-Output "}"
} elseif ($token -match '^const|^let|^var') {
    Write-Output "variableName = value;"
} elseif ($token -match '^import') {
    Write-Output "{ module } from 'package';"
} elseif ($token -match '^export') {
    Write-Output "default function() {}"
} elseif ($token -match '^class') {
    Write-Output "ClassName {"
    Write-Output "  constructor() {"
    Write-Output "    // init"
    Write-Output "  }"
    Write-Output "}"
} elseif ($token -match '^param') {
    Write-Output "([string]$Name)"
    Write-Output "Write-Host `"Hello `$Name`""
} elseif ($token -match '^Write-Host|^Write-Output') {
    Write-Output "`"message`""
} elseif ($token -match '^Get-') {
    Write-Output "ChildItem -Path ."
} else {
    # Default suggestions
    Write-Output "// Add code here"
    Write-Output ";"
}
