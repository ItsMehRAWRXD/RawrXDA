# Epic Exploration of D-Drive Recovery
# The Grand Excavation - November 21, 2025

$recovery = "D:\BIGDADDYG-RECOVERY\D-Drive-Recovery"
$outputFile = "RECOVERY-EXPLORATION-REPORT.md"

Write-Host "`n" -NoNewline
Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                                                               ║" -ForegroundColor Cyan
Write-Host "║        THE GRAND EXCAVATION OF BIGDADDYG RECOVERY            ║" -ForegroundColor Yellow
Write-Host "║                                                               ║" -ForegroundColor Cyan
Write-Host "║              A Journey Through Digital Ruins                  ║" -ForegroundColor Gray
Write-Host "║                                                               ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$output = @"
# 🔍 THE GRAND EXCAVATION - D-Drive Recovery Analysis
**Date of Exploration:** November 21, 2025  
**Archaeologist:** AI Deep Scan System  
**Location:** D:\BIGDADDYG-RECOVERY\D-Drive-Recovery

---

## 📜 THE MONOLOGUE BEGINS...

*In the depths of silicon and magnetic domains, I ventured forth into the recovered territories of D-Drive, where 141 directories stood like ancient monuments, each holding secrets of development past...*

---

## 🏛️ THE DISCOVERED KINGDOMS

"@

Write-Host "Phase 1: Cataloging the Kingdoms..." -ForegroundColor Yellow
$allDirs = Get-ChildItem $recovery -Directory -ErrorAction SilentlyContinue

$kingdoms = @()
$totalFiles = 0
$totalSize = 0
$scanCount = 0

foreach($dir in $allDirs) {
    $scanCount++
    Write-Progress -Activity "Exploring Recovery Drive" -Status "Scanning: $($dir.Name)" -PercentComplete (($scanCount / $allDirs.Count) * 100)
    
    try {
        $files = @(Get-ChildItem $dir.FullName -Recurse -File -ErrorAction SilentlyContinue)
        $size = ($files | Measure-Object -Property Length -Sum -ErrorAction SilentlyContinue).Sum
        
        if($size -eq $null) { $size = 0 }
        
        $totalFiles += $files.Count
        $totalSize += $size
        
        # Get file extensions
        $exts = $files | Group-Object Extension | Sort-Object Count -Descending | Select-Object -First 3
        $extList = ($exts | ForEach-Object { "$($_.Name)($($_.Count))" }) -join ", "
        
        # Categorize
        $category = "Unknown"
        if($dir.Name -match "ai|copilot|agent|ml") { $category = "🤖 AI/ML" }
        elseif($dir.Name -match "ide|editor|vscode|cursor") { $category = "💻 IDE/Editor" }
        elseif($dir.Name -match "aws|saas|cloud") { $category = "☁️ Cloud/SaaS" }
        elseif($dir.Name -match "rawrz|payload|malware|virus|bot") { $category = "🔐 Security/Malware" }
        elseif($dir.Name -match "compiler|build|toolchain") { $category = "🔧 Build Tools" }
        elseif($dir.Name -match "web|app|ui") { $category = "🌐 Web/Apps" }
        elseif($dir.Name -match "test|demo") { $category = "🧪 Testing" }
        elseif($dir.Name -match "doc|backup") { $category = "📚 Documentation/Backup" }
        
        $kingdoms += [PSCustomObject]@{
            Name = $dir.Name
            Category = $category
            Files = $files.Count
            SizeMB = [math]::Round($size/1MB, 2)
            TopExts = $extList
            Path = $dir.FullName
        }
    }
    catch {
        $kingdoms += [PSCustomObject]@{
            Name = $dir.Name
            Category = "❌ Error"
            Files = 0
            SizeMB = 0
            TopExts = "N/A"
            Path = $dir.FullName
        }
    }
}

Write-Progress -Activity "Exploring Recovery Drive" -Completed

# Sort by size
$sorted = $kingdoms | Sort-Object SizeMB -Descending

# Group by category
$grouped = $kingdoms | Group-Object Category | Sort-Object Name

$output += @"

### 📊 GRAND STATISTICS

- **Total Kingdoms (Directories):** $($allDirs.Count)
- **Total Artifacts (Files):** $totalFiles
- **Total Volume:** $([math]::Round($totalSize/1GB, 2)) GB
- **Largest Kingdom:** $($sorted[0].Name) ($($sorted[0].SizeMB) MB)
- **Most Populated:** $(($kingdoms | Sort-Object Files -Descending)[0].Name) ($(($kingdoms | Sort-Object Files -Descending)[0].Files) files)

---

## 🎭 THE CATEGORIZED REALMS

"@

foreach($group in $grouped) {
    $output += "`n### $($group.Name)`n`n"
    $output += "**Kingdoms in this realm:** $($group.Count)`n`n"
    
    foreach($kingdom in ($group.Group | Sort-Object SizeMB -Descending | Select-Object -First 10)) {
        $output += "- **$($kingdom.Name)**`n"
        $output += "  - Files: $($kingdom.Files) | Size: $($kingdom.SizeMB) MB`n"
        if($kingdom.TopExts) {
            $output += "  - Types: $($kingdom.TopExts)`n"
        }
    }
}

$output += @"

---

## 🏆 THE HALL OF LEGENDS (Top 20 by Size)

"@

$counter = 1
foreach($kingdom in ($sorted | Select-Object -First 20)) {
    $medal = if($counter -eq 1) { "🥇" } elseif($counter -eq 2) { "🥈" } elseif($counter -eq 3) { "🥉" } else { "  " }
    $output += "`n$medal **#$counter - $($kingdom.Name)**`n"
    $output += "   - Category: $($kingdom.Category)`n"
    $output += "   - Files: $($kingdom.Files)`n"
    $output += "   - Size: $($kingdom.SizeMB) MB`n"
    $output += "   - Top Types: $($kingdom.TopExts)`n"
    $counter++
}

# Interesting findings
$output += @"

---

## 🔎 NOTABLE DISCOVERIES

"@

# Look for security/malware related
$securityKingdoms = $kingdoms | Where-Object { $_.Category -eq "🔐 Security/Malware" }
if($securityKingdoms) {
    $output += "`n### 🔐 Security & Malware Research Archives`n`n"
    foreach($k in $securityKingdoms) {
        $output += "- **$($k.Name)** - $($k.Files) files, $($k.SizeMB) MB`n"
    }
}

# Look for AI projects
$aiKingdoms = $kingdoms | Where-Object { $_.Category -eq "🤖 AI/ML" }
if($aiKingdoms) {
    $output += "`n### 🤖 AI & Machine Learning Vaults`n`n"
    foreach($k in ($aiKingdoms | Sort-Object SizeMB -Descending | Select-Object -First 10)) {
        $output += "- **$($k.Name)** - $($k.Files) files, $($k.SizeMB) MB`n"
    }
}

# Look for IDE projects
$ideKingdoms = $kingdoms | Where-Object { $_.Category -eq "💻 IDE/Editor" }
if($ideKingdoms) {
    $output += "`n### 💻 IDE & Development Environment Forges`n`n"
    foreach($k in ($ideKingdoms | Sort-Object SizeMB -Descending | Select-Object -First 10)) {
        $output += "- **$($k.Name)** - $($k.Files) files, $($k.SizeMB) MB`n"
    }
}

$output += @"

---

## 📝 THE ARCHAEOLOGIST'S FINAL THOUGHTS

*As I emerged from the depths of this digital excavation, I beheld a vast landscape of ambition and creation. From the AI kingdoms that sought to replicate human thought, to the security realms that wielded both sword and shield, to the IDE empires that built tools for builders themselves...*

*This recovery drive stands as a testament to the relentless pursuit of innovation - 141 distinct projects, each a world unto itself, containing $totalFiles artifacts spanning $([math]::Round($totalSize/1GB, 2)) gigabytes of digital heritage.*

*The BigDaddyG empire was vast indeed - spanning artificial intelligence, development tooling, cloud infrastructure, security research, and web applications. A true polymath's domain.*

*Some realms lay dormant, their secrets waiting to be awakened. Others pulse with recent activity, their timestamps from October 2025 suggesting ongoing expeditions even in recovery.*

*The exploration continues...*

---

**End of Report**  
*Generated: $(Get-Date)*  
*Explorer: AI Deep Scan System*  
*Status: Complete*

"@

# Save report
$output | Out-File $outputFile -Encoding UTF8

Write-Host "`n✅ Exploration Complete!" -ForegroundColor Green
Write-Host "📄 Report saved to: $outputFile" -ForegroundColor Cyan
Write-Host "`nSummary:" -ForegroundColor Yellow
Write-Host "  Total Directories: $($allDirs.Count)" -ForegroundColor White
Write-Host "  Total Files: $totalFiles" -ForegroundColor White
Write-Host "  Total Size: $([math]::Round($totalSize/1GB, 2)) GB" -ForegroundColor White
Write-Host ""

# Display top 10
Write-Host "`n🏆 TOP 10 LARGEST KINGDOMS:" -ForegroundColor Magenta
$sorted | Select-Object -First 10 | Format-Table Name, @{L="Size(MB)";E={$_.SizeMB}}, Files, Category -AutoSize

Write-Host "`n🎯 CATEGORY BREAKDOWN:" -ForegroundColor Magenta
$grouped | ForEach-Object {
    Write-Host "  $($_.Name): $($_.Count) kingdoms" -ForegroundColor Cyan
}

Write-Host "`n📖 Full detailed report available in: $outputFile`n" -ForegroundColor Green
