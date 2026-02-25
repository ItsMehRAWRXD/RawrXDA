# Build RawrXD Full Source Status Report
param([string]$InventoryPath = "D:\rawrxd\SOURCE_INVENTORY_COMPLETE.txt", [string]$OutPath = "D:\rawrxd\SOURCE_STATUS_REPORT.md")

$content = Get-Content $InventoryPath -Encoding utf8
$header = $content[0..6] -join "`n"
$lines = $content[7..($content.Length-1)]
$totalLines = 0
$totalFiles = 0

# Parse header
if ($content[3] -match 'TOTALS: (\d+) files \| (\d+) lines') {
    $totalFiles = [int]$Matches[1]
    $totalLines = [int]$Matches[2]
}

$sb = [System.Text.StringBuilder]::new()

# Executive Summary
[void]$sb.AppendLine("# RawrXD IDE - Full Source Status Report")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("**Generated:** $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Executive Summary")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("| Metric | Value |")
[void]$sb.AppendLine("|--------|-------|")
[void]$sb.AppendLine("| **Total Source Files** | $totalFiles |")
[void]$sb.AppendLine("| **Total Lines** | $totalLines |")
[void]$sb.AppendLine("| **Report Scope** | Line 1 to line end of every file |")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("---")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("## Complete File Inventory (Every Source File)")
[void]$sb.AppendLine("")
[void]$sb.AppendLine('Each entry: path | lines | pct of total')
[void]$sb.AppendLine("")
[void]$sb.AppendLine('```')
foreach ($line in $lines) {
    if ($line.Trim()) { [void]$sb.AppendLine($line) }
}
[void]$sb.AppendLine('```')
[void]$sb.AppendLine("")
[void]$sb.AppendLine("---")
[void]$sb.AppendLine("- End of report -")

$sb.ToString() | Out-File -FilePath $OutPath -Encoding utf8
Write-Host "Report written to $OutPath"
