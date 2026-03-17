# ============================================================================
# D:\ DRIVE GRAND EXCAVATION
# Comprehensive Deep Scan of Entire D Drive
# ============================================================================

$ErrorActionPreference = 'SilentlyContinue'
$ProgressPreference = 'Continue'

Write-Host @"

╔═══════════════════════════════════════════════════════════════════════╗
║                                                                       ║
║        🔍 THE ULTIMATE EXCAVATION - COMPLETE D:\ DRIVE SCAN 🔍       ║
║                                                                       ║
║           Mapping Every Kingdom, Vault, and Hidden Chamber            ║
║                                                                       ║
╚═══════════════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

$RootPath = "D:\"
$ReportPath = "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\D-DRIVE-COMPLETE-EXPLORATION.md"

Write-Host "[*] Target: $RootPath" -ForegroundColor Yellow
Write-Host "[*] Report: $ReportPath" -ForegroundColor Yellow
Write-Host ""

# Category detection function
function Get-DirectoryCategory {
    param($DirName)
    
    $categories = @{
        'AI/ML' = @('ai', 'copilot', 'machine-learning', 'ml-', 'neural', 'model', 'llm', 'gpt', 'ollama', 'agent', 'puppeteer')
        'IDE/Editor' = @('ide', 'editor', 'vscode', 'cursor', 'monaco', 'glassquill', 'devmarket')
        'Security/Malware' = @('rawrz', 'payload', 'malware', 'virus', 'bot', 'exploit', 'fud', 'crypt', 'loader', 'stub', 'mirai')
        'Cloud/SaaS' = @('aws', 'azure', 'cloud', 'saas', 'lambda', 'api-gateway', 'cloudfront')
        'Build Tools' = @('compiler', 'build', 'cmake', 'make', 'gradle', 'maven', 'toolchain')
        'Web/Apps' = @('web', 'app', 'frontend', 'backend', 'react', 'vue', 'angular', 'express', 'django', 'flask')
        'Testing' = @('test', 'spec', 'demo', 'sample', 'junit', 'pytest', 'mocha')
        'Documentation/Backup' = @('backup', 'docs', 'documentation', 'archive', 'recovery')
        'Gaming' = @('game', 'unity', 'unreal', 'godot', 'steam')
        'Database' = @('database', 'db', 'sql', 'mongo', 'redis', 'postgres', 'mysql')
        'DevOps' = @('docker', 'kubernetes', 'k8s', 'ci', 'cd', 'jenkins', 'github-actions')
        'Media' = @('video', 'audio', 'image', 'media', 'ffmpeg', 'graphics')
    }
    
    $lowerName = $DirName.ToLower()
    foreach ($cat in $categories.Keys) {
        foreach ($keyword in $categories[$cat]) {
            if ($lowerName -like "*$keyword*") {
                return $cat
            }
        }
    }
    return 'Unknown'
}

# Phase 1: Discover all top-level directories
Write-Host "`n[Phase 1] Cataloging the Kingdoms of D:\..." -ForegroundColor Green

$TopLevelDirs = Get-ChildItem -Path $RootPath -Directory -Force | Sort-Object Name
$TotalKingdoms = $TopLevelDirs.Count

Write-Host "[✓] Found $TotalKingdoms top-level kingdoms" -ForegroundColor Cyan
Write-Host ""

# Phase 2: Deep scan each directory
Write-Host "[Phase 2] Deep Scanning Each Kingdom (this may take several minutes)..." -ForegroundColor Green
Write-Host ""

$KingdomData = @()
$Counter = 0

foreach ($Dir in $TopLevelDirs) {
    $Counter++
    $PercentComplete = [math]::Round(($Counter / $TotalKingdoms) * 100, 1)
    
    Write-Progress -Activity "Exploring D:\ Drive" `
                   -Status "Scanning: $($Dir.Name)" `
                   -PercentComplete $PercentComplete `
                   -CurrentOperation "Kingdom $Counter of $TotalKingdoms"
    
    Write-Host "  [$Counter/$TotalKingdoms] $($Dir.Name)..." -NoNewline
    
    try {
        # Get all files recursively
        $Files = Get-ChildItem -Path $Dir.FullName -File -Recurse -Force -ErrorAction SilentlyContinue
        
        $FileCount = ($Files | Measure-Object).Count
        $TotalSize = ($Files | Measure-Object -Property Length -Sum).Sum
        $SizeMB = [math]::Round($TotalSize / 1MB, 2)
        
        # Get top 3 file extensions
        $TopExtensions = $Files | 
            Group-Object Extension | 
            Sort-Object Count -Descending | 
            Select-Object -First 3 | 
            ForEach-Object { 
                if ($_.Name) { "$($_.Name)($($_.Count))" } 
                else { "no-ext($($_.Count))" }
            }
        
        $Category = Get-DirectoryCategory $Dir.Name
        
        $KingdomData += [PSCustomObject]@{
            Name = $Dir.Name
            FullPath = $Dir.FullName
            Category = $Category
            FileCount = $FileCount
            SizeMB = $SizeMB
            TopExtensions = ($TopExtensions -join ', ')
        }
        
        Write-Host " ✓ ($FileCount files, $SizeMB MB)" -ForegroundColor Green
        
    } catch {
        Write-Host " ✗ Access Denied" -ForegroundColor Red
        $KingdomData += [PSCustomObject]@{
            Name = $Dir.Name
            FullPath = $Dir.FullName
            Category = 'Access Denied'
            FileCount = 0
            SizeMB = 0
            TopExtensions = 'N/A'
        }
    }
}

Write-Progress -Activity "Exploring D:\ Drive" -Completed

# Calculate totals
$TotalFiles = ($KingdomData | Measure-Object -Property FileCount -Sum).Sum
$TotalSizeMB = ($KingdomData | Measure-Object -Property SizeMB -Sum).Sum
$TotalSizeGB = [math]::Round($TotalSizeMB / 1024, 2)

# Phase 3: Generate Report
Write-Host "`n[Phase 3] Generating Epic Report..." -ForegroundColor Green

$Report = @"
# 🌌 THE ULTIMATE EXCAVATION - Complete D:\ Drive Analysis
**Date of Exploration:** $(Get-Date -Format "MMMM dd, yyyy")  
**Archaeologist:** AI Deep Scan System  
**Location:** D:\

---

## 📜 THE GRAND MONOLOGUE

*In the vast expanse of the D: partition, where $TotalKingdoms kingdoms sprawled across the silicon landscape, I ventured forth with determination. Armed with recursive scanners and progress indicators, I descended into each realm, counting every artifact, measuring every vault, cataloging every secret...*

*From the heights of development tools to the depths of security research, from AI laboratories to gaming archives, the D: drive revealed itself as a sprawling metropolis of digital creation - $TotalFiles files strong, spanning $TotalSizeGB gigabytes of storage.*

---

## 📊 GRAND STATISTICS

- **Total Kingdoms (Top-Level Directories):** $TotalKingdoms
- **Total Artifacts (Files):** $($TotalFiles.ToString('N0'))
- **Total Volume:** $TotalSizeGB GB ($TotalSizeMB MB)
- **Largest Kingdom:** $($KingdomData | Sort-Object SizeMB -Descending | Select-Object -First 1 -ExpandProperty Name)
- **Most Populated:** $($KingdomData | Sort-Object FileCount -Descending | Select-Object -First 1 -ExpandProperty Name)

---

## 🎭 THE CATEGORIZED REALMS

"@

# Group by category
$GroupedByCategory = $KingdomData | Group-Object Category | Sort-Object Name

foreach ($CategoryGroup in $GroupedByCategory) {
    $CategoryName = $CategoryGroup.Name
    $CategoryIcon = switch ($CategoryName) {
        'AI/ML' { '🤖' }
        'IDE/Editor' { '💻' }
        'Security/Malware' { '🔐' }
        'Cloud/SaaS' { '☁️' }
        'Build Tools' { '🔧' }
        'Web/Apps' { '🌐' }
        'Testing' { '🧪' }
        'Documentation/Backup' { '📚' }
        'Gaming' { '🎮' }
        'Database' { '🗄️' }
        'DevOps' { '⚙️' }
        'Media' { '🎬' }
        'Access Denied' { '🔒' }
        default { '📁' }
    }
    
    $Report += "`n### $CategoryIcon $CategoryName`n`n"
    $Report += "**Kingdoms in this realm:** $($CategoryGroup.Count)`n`n"
    
    $SortedKingdoms = $CategoryGroup.Group | Sort-Object SizeMB -Descending
    
    foreach ($Kingdom in $SortedKingdoms) {
        $Report += "- **$($Kingdom.Name)**`n"
        $Report += "  - Path: ``$($Kingdom.FullPath)```n"
        $Report += "  - Files: $($Kingdom.FileCount.ToString('N0')) | Size: $($Kingdom.SizeMB) MB`n"
        if ($Kingdom.TopExtensions -ne 'N/A') {
            $Report += "  - Types: $($Kingdom.TopExtensions)`n"
        }
        $Report += "`n"
    }
}

# Top 30 by size
$Report += @"

---

## 🏆 THE HALL OF LEGENDS (Top 30 by Size)

"@

$Top30 = $KingdomData | Sort-Object SizeMB -Descending | Select-Object -First 30
$Rank = 0

foreach ($Kingdom in $Top30) {
    $Rank++
    $Medal = switch ($Rank) {
        1 { '🥇' }
        2 { '🥈' }
        3 { '🥉' }
        default { '  ' }
    }
    
    $CategoryIcon = switch ($Kingdom.Category) {
        'AI/ML' { '🤖' }
        'IDE/Editor' { '💻' }
        'Security/Malware' { '🔐' }
        'Cloud/SaaS' { '☁️' }
        'Build Tools' { '🔧' }
        'Web/Apps' { '🌐' }
        'Testing' { '🧪' }
        'Documentation/Backup' { '📚' }
        'Gaming' { '🎮' }
        'Database' { '🗄️' }
        'DevOps' { '⚙️' }
        'Media' { '🎬' }
        default { '📁' }
    }
    
    $Report += "`n$Medal **#$Rank - $($Kingdom.Name)**`n"
    $Report += "   - Category: $CategoryIcon $($Kingdom.Category)`n"
    $Report += "   - Files: $($Kingdom.FileCount.ToString('N0'))`n"
    $Report += "   - Size: $($Kingdom.SizeMB) MB`n"
    if ($Kingdom.TopExtensions -ne 'N/A') {
        $Report += "   - Top Types: $($Kingdom.TopExtensions)`n"
    }
}

# Notable discoveries by category
$Report += @"

---

## 🔎 NOTABLE DISCOVERIES BY REALM

"@

$NotableCategories = @('Security/Malware', 'AI/ML', 'IDE/Editor', 'Gaming', 'DevOps', 'Database')

foreach ($NotableCat in $NotableCategories) {
    $CategoryKingdoms = $KingdomData | Where-Object { $_.Category -eq $NotableCat } | Sort-Object SizeMB -Descending
    
    if ($CategoryKingdoms.Count -gt 0) {
        $CategoryIcon = switch ($NotableCat) {
            'Security/Malware' { '🔐' }
            'AI/ML' { '🤖' }
            'IDE/Editor' { '💻' }
            'Gaming' { '🎮' }
            'DevOps' { '⚙️' }
            'Database' { '🗄️' }
        }
        
        $Report += "`n### $CategoryIcon $NotableCat Archives`n`n"
        
        foreach ($Kingdom in $CategoryKingdoms) {
            $Report += "- **$($Kingdom.Name)** - $($Kingdom.FileCount.ToString('N0')) files, $($Kingdom.SizeMB) MB`n"
        }
    }
}

# Final thoughts
$Report += @"

---

## 📝 THE ARCHAEOLOGIST'S FINAL MONOLOGUE

*As I emerged from the labyrinthine depths of the D: drive, my scanners exhausted but triumphant, I gazed upon the mapped empire before me. $TotalKingdoms distinct kingdoms, each a testament to ambition, creativity, and the relentless human drive to build, create, and explore.*

*This was no mere storage device - this was a digital civilization. From the $($GroupedByCategory | Where-Object { $_.Name -eq 'AI/ML' } | Select-Object -ExpandProperty Count) AI/ML kingdoms pushing the boundaries of machine intelligence, to the $($GroupedByCategory | Where-Object { $_.Name -eq 'Security/Malware' } | Select-Object -ExpandProperty Count) security research vaults wielding both sword and shield, to the development tools forging the very instruments of creation itself.*

*$TotalSizeGB gigabytes of human knowledge, experimentation, and digital archaeology stretched before me. Some realms bustled with activity - tens of thousands of files organized into intricate hierarchies. Others stood as monuments to abandoned experiments, their handful of files silent witnesses to paths not taken.*

*The largest kingdom, **$($KingdomData | Sort-Object SizeMB -Descending | Select-Object -First 1 -ExpandProperty Name)**, commanded $($KingdomData | Sort-Object SizeMB -Descending | Select-Object -First 1 -ExpandProperty SizeMB) MB of territory. The most populated, **$($KingdomData | Sort-Object FileCount -Descending | Select-Object -First 1 -ExpandProperty Name)**, housed $($KingdomData | Sort-Object FileCount -Descending | Select-Object -First 1 -ExpandProperty FileCount) individual artifacts.*

*But numbers alone cannot capture the essence of what I discovered. This drive is a polymath's workshop - where security research meets AI development, where IDEs are crafted from raw code, where games are conceived and databases architected. It is a microcosm of the entire software development universe, compressed into $(Get-PSDrive D | Select-Object -ExpandProperty Used | ForEach-Object { [math]::Round($_ / 1GB, 2) }) GB of used space.*

*The exploration is complete. The map is drawn. The empire of D:\ stands revealed.*

*And what an empire it is.*

---

**End of Report**  
**Generated:** $(Get-Date -Format "MM/dd/yyyy HH:mm:ss")  
**Explorer:** AI Deep Scan System  
**Status:** Complete  
**Kingdoms Mapped:** $TotalKingdoms  
**Artifacts Cataloged:** $($TotalFiles.ToString('N0'))  
**Total Territory:** $TotalSizeGB GB

"@

# Write report
$Report | Out-File -FilePath $ReportPath -Encoding UTF8

Write-Host "`n[✓] EXCAVATION COMPLETE!" -ForegroundColor Green
Write-Host "[✓] Report saved to: $ReportPath" -ForegroundColor Cyan
Write-Host ""
Write-Host "Summary:" -ForegroundColor Yellow
Write-Host "  • Kingdoms Scanned: $TotalKingdoms" -ForegroundColor White
Write-Host "  • Total Files: $($TotalFiles.ToString('N0'))" -ForegroundColor White
Write-Host "  • Total Size: $TotalSizeGB GB" -ForegroundColor White
Write-Host ""
