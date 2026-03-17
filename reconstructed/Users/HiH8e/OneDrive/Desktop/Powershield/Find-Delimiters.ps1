<#
.SYNOPSIS
    Find mismatched braces, brackets, and parentheses in PowerShell script

.DESCRIPTION
    Scans through the script line by line to identify where delimiters don't match
#>

param(
    [string]$ScriptPath = "$PSScriptRoot\RawrXD.ps1"
)

$lines = Get-Content $ScriptPath
$braceStack = New-Object System.Collections.Stack
$parenStack = New-Object System.Collections.Stack
$bracketStack = New-Object System.Collections.Stack

$openBraceLines = @()
$openParenLines = @()
$openBracketLines = @()

Write-Host "Analyzing delimiter matching..." -ForegroundColor Cyan

for ($i = 0; $i < $lines.Count; $i++) {
    $line = $lines[$i]
    $lineNum = $i + 1
    
    # Skip comments and strings (simple approach)
    $inString = $false
    $inComment = $false
    
    for ($j = 0; $j < $line.Length; $j++) {
        $char = $line[$j]
        $prevChar = if ($j -gt 0) { $line[$j-1] } else { '' }
        
        # Track string state
        if ($char -eq '"' -and $prevChar -ne '`') {
            $inString = -not $inString
        }
        
        # Track comment state
        if ($char -eq '#' -and -not $inString) {
            $inComment = $true
        }
        
        # Skip if in string or comment
        if ($inString -or $inComment) {
            continue
        }
        
        # Track braces
        if ($char -eq '{') {
            $braceStack.Push(@{Line=$lineNum; Col=$j; Char=$char})
        }
        elseif ($char -eq '}') {
            if ($braceStack.Count -gt 0) {
                $braceStack.Pop() | Out-Null
            } else {
                Write-Host "❌ Unmatched closing brace '}' at line $lineNum, column $j" -ForegroundColor Red
            }
        }
        
        # Track parentheses
        if ($char -eq '(') {
            $parenStack.Push(@{Line=$lineNum; Col=$j; Char=$char})
        }
        elseif ($char -eq ')') {
            if ($parenStack.Count -gt 0) {
                $parenStack.Pop() | Out-Null
            } else {
                Write-Host "❌ Unmatched closing parenthesis ')' at line $lineNum, column $j" -ForegroundColor Red
            }
        }
        
        # Track brackets
        if ($char -eq '[') {
            $bracketStack.Push(@{Line=$lineNum; Col=$j; Char=$char})
        }
        elseif ($char -eq ']') {
            if ($bracketStack.Count -gt 0) {
                $bracketStack.Pop() | Out-Null
            } else {
                Write-Host "❌ Unmatched closing bracket ']' at line $lineNum, column $j" -ForegroundColor Red
            }
        }
    }
}

Write-Host "`n--- Unclosed Delimiters ---`n" -ForegroundColor Yellow

if ($braceStack.Count -gt 0) {
    Write-Host "❌ $($braceStack.Count) unclosed braces '{' found:" -ForegroundColor Red
    $braceArray = $braceStack.ToArray()
    [array]::Reverse($braceArray)
    foreach ($item in $braceArray | Select-Object -Last 10) {
        Write-Host "   Line $($item.Line): $($lines[$item.Line - 1].Trim())" -ForegroundColor Yellow
    }
}

if ($parenStack.Count -gt 0) {
    Write-Host "`n❌ $($parenStack.Count) unclosed parentheses '(' found:" -ForegroundColor Red
    $parenArray = $parenStack.ToArray()
    [array]::Reverse($parenArray)
    foreach ($item in $parenArray | Select-Object -Last 10) {
        Write-Host "   Line $($item.Line): $($lines[$item.Line - 1].Trim())" -ForegroundColor Yellow
    }
}

if ($bracketStack.Count -gt 0) {
    Write-Host "`n❌ $($bracketStack.Count) unclosed brackets '[' found:" -ForegroundColor Red
    $bracketArray = $bracketStack.ToArray()
    [array]::Reverse($bracketArray)
    foreach ($item in $bracketArray | Select-Object -Last 10) {
        Write-Host "   Line $($item.Line): $($lines[$item.Line - 1].Trim())" -ForegroundColor Yellow
    }
}

if ($braceStack.Count -eq 0 -and $parenStack.Count -eq 0 -and $bracketStack.Count -eq 0) {
    Write-Host "✅ All delimiters are properly matched!" -ForegroundColor Green
}

Write-Host ""
