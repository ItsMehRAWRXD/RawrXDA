# IDE Accessibility and Visibility Diagnostic Tool
# Finds inaccessible elements, hidden panes, broken buttons, and positioning issues

param(
    [string]$FilePath = "C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html"
)

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║        IDE ACCESSIBILITY & VISIBILITY DIAGNOSTIC TOOL         ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "Analyzing: $FilePath" -ForegroundColor Yellow
Write-Host ""

# Read the file content
$content = Get-Content -Path $FilePath -Raw

$issues = @()
$warnings = @()
$fixes = @()

# ========================================================================
# 1. CHECK FOR ELEMENTS WITH VISIBILITY/DISPLAY ISSUES
# ========================================================================

Write-Host "🔍 [1/7] Scanning for visibility and display issues..." -ForegroundColor Cyan

# Find display: none elements
$displayNone = [regex]::Matches($content, 'id\s*=\s*"([^"]+)"[^>]*style\s*=\s*"[^"]*display\s*:\s*none')
if ($displayNone.Count -gt 0) {
    Write-Host "  ⚠️  Found elements with display:none" -ForegroundColor Yellow
    foreach ($match in $displayNone) {
        $elementId = $match.Groups[1].Value
        $warnings += "display:none element: #$elementId (might be intentionally hidden)"
        Write-Host "     → #$elementId" -ForegroundColor Gray
    }
}

# Find elements with low/zero z-index that might be hidden
$zIndexIssues = [regex]::Matches($content, 'id\s*=\s*"([^"]+)"[^>]*z-index\s*:\s*(-?\d+)')
$hiddenByZIndex = $zIndexIssues | Where-Object { [int]$_.Groups[2].Value -lt 0 }
if ($hiddenByZIndex.Count -gt 0) {
    Write-Host "  ❌ Found elements with negative z-index (hidden behind other elements)" -ForegroundColor Red
    foreach ($match in $hiddenByZIndex) {
        $elementId = $match.Groups[1].Value
        $zIndex = $match.Groups[2].Value
        $issues += "Negative z-index: #$elementId (z-index: $zIndex)"
        Write-Host "     → #$elementId (z-index: $zIndex)" -ForegroundColor Red
    }
}

# ========================================================================
# 2. CHECK FOR ACCESSIBILITY PANEL FUNCTIONALITY
# ========================================================================

Write-Host ""
Write-Host "🔍 [2/7] Checking accessibility panel..." -ForegroundColor Cyan

$hasA11yToggle = $content -match 'toggleAccessibilityPanel|accessibility-toggle'
if ($hasA11yToggle) {
    Write-Host "  ✅ Accessibility toggle button found" -ForegroundColor Green
    
    # Check if toggleAccessibilityPanel is defined
    $hasA11yFunction = $content -match 'function toggleAccessibilityPanel|window\.toggleAccessibilityPanel|const toggleAccessibilityPanel'
    if ($hasA11yFunction) {
        Write-Host "  ✅ toggleAccessibilityPanel function is defined" -ForegroundColor Green
    } else {
        $issues += "Missing function: toggleAccessibilityPanel()"
        Write-Host "  ❌ toggleAccessibilityPanel function NOT defined" -ForegroundColor Red
        $fixes += "Need to define: toggleAccessibilityPanel()"
    }
    
    # Check if accessibility panel container exists
    $hasA11yPanel = $content -match 'id\s*=\s*["\x27]accessibility-panel["\x27]|id\s*=\s*["\x27]a11y-panel["\x27]'
    if ($hasA11yPanel) {
        Write-Host "  ✅ Accessibility panel container found" -ForegroundColor Green
    } else {
        $issues += "Missing element: Accessibility panel container (#accessibility-panel or #a11y-panel)"
        Write-Host "  ❌ Accessibility panel container NOT found" -ForegroundColor Red
        $fixes += "Create accessibility panel container with proper z-index and positioning"
    }
} else {
    $warnings += "Accessibility toggle button not found"
    Write-Host "  ⚠️  No accessibility toggle button found" -ForegroundColor Yellow
}

# ========================================================================
# 3. CHECK FOR TERMINAL/BOTTOM PANE
# ========================================================================

Write-Host ""
Write-Host "🔍 [3/7] Checking for terminal/bottom pane..." -ForegroundColor Cyan

$hasTerminalPane = $content -match 'terminal-pane|bottom-pane|terminal-container|terminal'
$hasTerminalTab = $content -match 'showBottomTab|terminal'

if ($hasTerminalPane -or $hasTerminalTab) {
    Write-Host "  ✅ Terminal pane references found" -ForegroundColor Green
    
    # Check if it's hidden
    $terminalHidden = $content -match 'terminal[^>]*(display\s*:\s*none|visibility\s*:\s*hidden|height\s*:\s*0)'
    if ($terminalHidden) {
        $issues += "Terminal pane is hidden (display:none or visibility:hidden)"
        Write-Host "  ❌ Terminal pane is HIDDEN" -ForegroundColor Red
        $fixes += "Show terminal pane: add display:block; visibility:visible; height:auto"
    } else {
        Write-Host "  ✅ Terminal pane appears to be visible" -ForegroundColor Green
    }
} else {
    $issues += "Terminal/bottom pane NOT found in HTML"
    Write-Host "  ❌ Terminal/bottom pane NOT FOUND" -ForegroundColor Red
    $fixes += "Need to create terminal/bottom pane with proper styling and functionality"
}

# ========================================================================
# 4. CHECK FOR TOP PANE ISSUES
# ========================================================================

Write-Host ""
Write-Host "🔍 [4/7] Checking for top pane..." -ForegroundColor Cyan

$topPanes = [regex]::Matches($content, 'id\s*=\s*"([^"]*(?:top|header|navbar|title)[^"]*)"')
if ($topPanes.Count -gt 0) {
    Write-Host "  Found $($topPanes.Count) potential top pane element(s):" -ForegroundColor Yellow
    foreach ($match in $topPanes) {
        $id = $match.Groups[1].Value
        Write-Host "     → #$id" -ForegroundColor Gray
        
        # Check if it's broken/non-functional
        $hasClickHandler = $content -match "onclick|data-click-attached"
        $hasClosing = $content -match "onclick.*close|onclick.*hide|onclick.*toggle"
        
        if (-not $hasClickHandler -and -not $hasClosing) {
            $warnings += "Top pane #$id has no visible functionality"
            Write-Host "     ⚠️  No click handlers/functionality detected" -ForegroundColor Yellow
        }
    }
} else {
    Write-Host "  ⚠️  No obvious top pane found (check manually)" -ForegroundColor Yellow
}

# ========================================================================
# 5. CHECK FOR ELEMENTS HIDDEN UNDER OTHER PANES (Z-INDEX CONFLICTS)
# ========================================================================

Write-Host ""
Write-Host "🔍 [5/7] Checking for z-index conflicts and overlapping elements..." -ForegroundColor Cyan

# Find all elements with z-index
$zIndexElements = @{}
[regex]::Matches($content, 'z-index\s*:\s*(\d+)') | ForEach-Object {
    $zIndex = [int]$_.Groups[1].Value
    if (-not $zIndexElements.ContainsKey($zIndex)) {
        $zIndexElements[$zIndex] = 0
    }
    $zIndexElements[$zIndex]++
}

if ($zIndexElements.Count -gt 0) {
    $sortedZ = $zIndexElements.GetEnumerator() | Sort-Object -Property Key -Descending | Select-Object -First 10
    Write-Host "  Top z-index values found:" -ForegroundColor Cyan
    foreach ($item in $sortedZ) {
        Write-Host "     z-index: $($item.Key) ($($item.Value) element(s))" -ForegroundColor Gray
    }
    
    # Check for problematic overlaps
    if ($zIndexElements[99999] -gt 0 -or $zIndexElements[999999] -gt 0) {
        $issues += "Extremely high z-index values found (may cause accessibility issues)"
        Write-Host "  ⚠️  Extremely high z-index values detected" -ForegroundColor Yellow
    }
} else {
    Write-Host "  ⚠️  No z-index styling found" -ForegroundColor Yellow
}

# ========================================================================
# 6. CHECK FOR POINTER-EVENTS ISSUES
# ========================================================================

Write-Host ""
Write-Host "🔍 [6/7] Checking for pointer-events issues..." -ForegroundColor Cyan

$pointerEventsNone = [regex]::Matches($content, 'pointer-events\s*:\s*none')
if ($pointerEventsNone.Count -gt 0) {
    Write-Host "  Found $($pointerEventsNone.Count) elements with pointer-events:none" -ForegroundColor Yellow
    Write-Host "  ⚠️  These elements cannot receive clicks/interactions" -ForegroundColor Yellow
    
    # Get more context about which elements
    $pointerNoneMatches = [regex]::Matches($content, 'id\s*=\s*"([^"]+)"[^>]*pointer-events\s*:\s*none')
    foreach ($match in $pointerNoneMatches) {
        $elementId = $match.Groups[1].Value
        Write-Host "     → #$elementId (pointer-events:none)" -ForegroundColor Gray
        $warnings += "Element #$elementId has pointer-events:none (might need to be auto)"
    }
}

# ========================================================================
# 7. CHECK FOR ELEMENTS WITH POSITION/OVERFLOW CONFLICTS
# ========================================================================

Write-Host ""
Write-Host "🔍 [7/7] Checking for positioning and overflow issues..." -ForegroundColor Cyan

# Find elements that might be hidden due to overflow
$overflowHidden = [regex]::Matches($content, 'id\s*=\s*"([^"]+)"[^>]*(?:overflow\s*:\s*hidden|overflow-y\s*:\s*hidden)')
if ($overflowHidden.Count -gt 0) {
    Write-Host "  Found $($overflowHidden.Count) elements with overflow:hidden" -ForegroundColor Yellow
    foreach ($match in $overflowHidden) {
        $elementId = $match.Groups[1].Value
        Write-Host "     → #$elementId" -ForegroundColor Gray
        $warnings += "Element #$elementId has overflow:hidden (content might be clipped)"
    }
}

# ========================================================================
# SUMMARY REPORT
# ========================================================================

Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                      DIAGNOSTIC SUMMARY                        ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

Write-Host "🔴 CRITICAL ISSUES: $($issues.Count)" -ForegroundColor Red
if ($issues.Count -gt 0) {
    foreach ($issue in $issues) {
        Write-Host "   ❌ $issue" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "🟡 WARNINGS: $($warnings.Count)" -ForegroundColor Yellow
if ($warnings.Count -gt 0) {
    foreach ($warning in $warnings) {
        Write-Host "   ⚠️  $warning" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "🔧 SUGGESTED FIXES:" -ForegroundColor Green
if ($fixes.Count -gt 0) {
    $i = 1
    foreach ($fix in $fixes) {
        Write-Host "   $i. $fix" -ForegroundColor Green
        $i++
    }
} else {
    Write-Host "   ✅ No fixes needed (all systems appear functional)" -ForegroundColor Green
}

# ========================================================================
# GENERATE DETAILED REPORT FILE
# ========================================================================

Write-Host ""
Write-Host "📄 Generating detailed report..." -ForegroundColor Cyan

$reportPath = "C:\Users\HiH8e\OneDrive\Desktop\ide-accessibility-report.txt"
$report = @"
════════════════════════════════════════════════════════════════════
IDE ACCESSIBILITY & VISIBILITY DIAGNOSTIC REPORT
Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
File Analyzed: $FilePath
════════════════════════════════════════════════════════════════════

CRITICAL ISSUES ($($issues.Count)):
────────────────────────────────────────────────────────────────────
$(if ($issues.Count -gt 0) { $issues | ForEach-Object { "• $_" } } else { "None found ✅" })

WARNINGS ($($warnings.Count)):
────────────────────────────────────────────────────────────────────
$(if ($warnings.Count -gt 0) { $warnings | ForEach-Object { "• $_" } } else { "None found ✅" })

SUGGESTED FIXES ($($fixes.Count)):
────────────────────────────────────────────────────────────────────
$(if ($fixes.Count -gt 0) { $fixes | ForEach-Object -Begin {$i=1} { "$i. $_"; $i++ } } else { "None needed ✅" })

DETAILED FINDINGS:
════════════════════════════════════════════════════════════════════

1. VISIBILITY & DISPLAY ISSUES
   Status: $('✅' * (!($displayNone.Count -gt 0))) $('❌' * ($displayNone.Count -gt 0))
   Elements with display:none: $($displayNone.Count)
   $(if ($displayNone.Count -gt 0) { "IDs: " + ($displayNone | ForEach-Object { "#" + $_.Groups[1].Value }) -join ", " })

2. ACCESSIBILITY PANEL
   Status: $('✅' * $hasA11yToggle) $('❌' * !$hasA11yToggle)
   Toggle button found: $hasA11yToggle
   Function defined: $hasA11yFunction
   Panel container exists: $hasA11yPanel

3. TERMINAL/BOTTOM PANE
   Status: $('✅' * ($hasTerminalPane -and !$terminalHidden)) $('❌' * (!$hasTerminalPane -or $terminalHidden))
   Pane exists: $hasTerminalPane
   Pane visible: $(!$terminalHidden)

4. TOP PANE
   Found: $($topPanes.Count) potential top pane(s)
   $(if ($topPanes.Count -gt 0) { "IDs: " + ($topPanes | ForEach-Object { "#" + $_.Groups[1].Value }) -join ", " })

5. Z-INDEX ANALYSIS
   Total unique z-index values: $($zIndexElements.Count)
   Highest z-index: $(if ($zIndexElements.Count -gt 0) { ($zIndexElements.GetEnumerator() | Sort-Object -Property Key -Descending | Select-Object -First 1).Key } else { "N/A" })

6. POINTER-EVENTS ISSUES
   Elements with pointer-events:none: $($pointerEventsNone.Count)

7. OVERFLOW/CLIPPING ISSUES
   Elements with overflow:hidden: $($overflowHidden.Count)

════════════════════════════════════════════════════════════════════
RECOMMENDATIONS
════════════════════════════════════════════════════════════════════

For Accessibility Issues:
• Ensure accessibility button has proper z-index (>= 10000)
• Accessibility panel should have high z-index (>= 9999)
• Use position: fixed for accessibility button (not absolute)
• Ensure pointer-events: auto for clickable elements

For Terminal Pane:
• Check if terminal container exists in HTML
• Verify CSS shows/hides it properly
• Ensure proper z-index so it's not hidden by other panes
• Check if bottom panel toggle is defined

For UI Issues:
• Review all position: absolute elements - they may be hidden under other panes
• Check z-index hierarchy: make sure important UI is on top
• Verify overflow properties don't clip important content
• Remove "eyesore" elements or hide them properly

════════════════════════════════════════════════════════════════════
"@

$report | Out-File -FilePath $reportPath -Encoding UTF8
Write-Host "Report saved to: $reportPath" -ForegroundColor Green

Write-Host ""
Write-Host "✅ Diagnostic complete!" -ForegroundColor Green
Write-Host ""
