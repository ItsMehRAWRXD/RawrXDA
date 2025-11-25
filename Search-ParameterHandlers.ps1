#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Search for functions that handle old parameter names in RawrXD source
.DESCRIPTION
    This script searches through the RawrXD.ps1 file for functions that implement
    parameter name compatibility handling, aliases, or old parameter processing.
.NOTES
    Author: PowerShield Team
    Version: 1.0
#>

param(
  [string]$SourceFile = ".\RawrXD.ps1",
  [switch]$Detailed,
  [switch]$ShowMatches
)

Write-Host "🔍 SEARCHING FOR PARAMETER HANDLER FUNCTIONS" -ForegroundColor Cyan
Write-Host "=" * 60

if (-not (Test-Path $SourceFile)) {
  Write-Error "Source file not found: $SourceFile"
  exit 1
}

$searchTerms = @(
  "Alias\(",
  "Parameter.*Alias",
  "old parameter",
  "backward compatibility", 
  "backwards compatibility",
  "legacy parameter",
  "deprecated parameter",
  "param.*Position",
  "Parameter.*Position.*0",
  "Parameter.*Position.*1", 
  "Parameter.*Position.*2",
  "Message.*ErrorMessage",
  "Category.*ErrorCategory",
  "Severity",
  "parameter name.*compatibility",
  "handle.*parameter",
  "convert.*parameter"
)

$content = Get-Content $SourceFile
$totalLines = $content.Length
$foundMatches = @()

Write-Host "📄 Source File: $SourceFile" -ForegroundColor Green
Write-Host "📊 Total Lines: $totalLines" -ForegroundColor Yellow
Write-Host ""

foreach ($searchTerm in $searchTerms) {
  Write-Host "🔎 Searching for: $searchTerm" -ForegroundColor White
    
  for ($i = 0; $i -lt $content.Length; $i++) {
    $line = $content[$i]
    $lineNumber = $i + 1
        
    if ($line -match $searchTerm) {
      $match = @{
        LineNumber = $lineNumber
        SearchTerm = $searchTerm
        Content    = $line.Trim()
        Context    = @()
      }
            
      # Get context lines if detailed mode
      if ($Detailed) {
        $contextStart = [Math]::Max(0, $i - 2)
        $contextEnd = [Math]::Min($content.Length - 1, $i + 2)
                
        for ($j = $contextStart; $j -le $contextEnd; $j++) {
          $match.Context += @{
            LineNumber = $j + 1
            Content    = $content[$j]
            IsMatch    = ($j -eq $i)
          }
        }
      }
            
      $foundMatches += $match
            
      if ($ShowMatches) {
        Write-Host "  ✅ Line $lineNumber`: $($line.Trim())" -ForegroundColor Green
      }
    }
  }
}

Write-Host ""
Write-Host "📋 SEARCH RESULTS SUMMARY" -ForegroundColor Cyan
Write-Host "=" * 60

if ($foundMatches.Count -eq 0) {
  Write-Host "❌ No parameter handler functions found" -ForegroundColor Red
  Write-Host ""
  Write-Host "💡 Suggestions:" -ForegroundColor Yellow
  Write-Host "   • Check for functions with [Alias()] attributes"
  Write-Host "   • Look for param blocks with positional parameters"
  Write-Host "   • Search for backward compatibility comments"
  Write-Host "   • Verify function parameter validation logic"
}
else {
  Write-Host "✅ Found $($foundMatches.Count) matches" -ForegroundColor Green
    
  # Group by search term
  $groupedMatches = $foundMatches | Group-Object SearchTerm | Sort-Object Count -Descending
    
  foreach ($group in $groupedMatches) {
    Write-Host ""
    Write-Host "🎯 $($group.Name): $($group.Count) matches" -ForegroundColor White
        
    foreach ($match in $group.Group | Sort-Object LineNumber) {
      Write-Host "   📍 Line $($match.LineNumber): $($match.Content)" -ForegroundColor Gray
            
      if ($Detailed) {
        Write-Host "      Context:" -ForegroundColor DarkGray
        foreach ($contextLine in $match.Context) {
          $prefix = if ($contextLine.IsMatch) { "    >>> " } else { "        " }
          $color = if ($contextLine.IsMatch) { "Yellow" } else { "DarkGray" }
          Write-Host "$prefix$($contextLine.LineNumber): $($contextLine.Content)" -ForegroundColor $color
        }
      }
    }
  }
}

# Now let's specifically look for functions that might have parameter validation issues
Write-Host ""
Write-Host "🔧 ANALYZING PARAMETER VALIDATION ISSUES" -ForegroundColor Cyan
Write-Host "=" * 60

# Check for functions that might cause dropdown validation errors
$validationPatterns = @(
  "NumericUpDown",
  "\.Value\s*=",
  "\.Minimum\s*=",
  "\.Maximum\s*=",
  "not valid for value",
  "ParameterBindingValidationException"
)

$validationMatches = @()
foreach ($pattern in $validationPatterns) {
  for ($i = 0; $i -lt $content.Length; $i++) {
    $line = $content[$i]
    $lineNumber = $i + 1
        
    if ($line -match $pattern) {
      $validationMatches += @{
        LineNumber = $lineNumber
        Pattern    = $pattern
        Content    = $line.Trim()
      }
    }
  }
}

if ($validationMatches.Count -gt 0) {
  Write-Host "⚠️ Found $($validationMatches.Count) potential validation issues:" -ForegroundColor Yellow
    
  $validationGroups = $validationMatches | Group-Object Pattern | Sort-Object Count -Descending
  foreach ($group in $validationGroups) {
    Write-Host ""
    Write-Host "🎯 $($group.Name): $($group.Count) matches" -ForegroundColor White
        
    foreach ($match in $group.Group | Sort-Object LineNumber | Select-Object -First 5) {
      Write-Host "   📍 Line $($match.LineNumber): $($match.Content)" -ForegroundColor Gray
    }
        
    if ($group.Count -gt 5) {
      Write-Host "   ... and $($group.Count - 5) more matches" -ForegroundColor DarkGray
    }
  }
}

Write-Host ""
Write-Host "🎯 RECOMMENDATIONS" -ForegroundColor Cyan
Write-Host "=" * 60

Write-Host "1. 🔧 Check Write-ErrorLog function for parameter aliases" -ForegroundColor White
Write-Host "2. 🎛️ Verify NumericUpDown controls have proper min/max values" -ForegroundColor White  
Write-Host "3. 🛡️ Ensure security settings validation is working" -ForegroundColor White
Write-Host "4. 📝 Look for functions with [Parameter(Position=X)] attributes" -ForegroundColor White
Write-Host "5. 🔄 Search for parameter compatibility layers" -ForegroundColor White

Write-Host ""
Write-Host "✨ Search completed!" -ForegroundColor Green