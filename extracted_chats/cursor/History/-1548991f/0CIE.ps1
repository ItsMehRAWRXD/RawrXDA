#Requires -Version 5.1
<#
.SYNOPSIS
    Comprehensive test suite for OpenMemory module system
.DESCRIPTION
    Tests all OpenMemory features including storage, embeddings, search, decay, and HTTP API
.EXAMPLE
    .\Test-OpenMemory.ps1
.EXAMPLE
    .\Test-OpenMemory.ps1 -SkipAPI
#>

param(
    [switch]$SkipAPI,
    [switch]$SkipDecay,
    [switch]$Verbose
)

$ErrorActionPreference = 'Stop'
$testResults = @{
    Total = 0
    Passed = 0
    Failed = 0
    Tests = @()
}

function Write-TestResult {
    param(
        [string]$TestName,
        [bool]$Passed,
        [string]$Message = ""
    )
    
    $testResults.Total++
    if ($Passed) {
        $testResults.Passed++
        Write-Host "✅ PASS: $TestName" -ForegroundColor Green
        if ($Message) { Write-Host "   $Message" -ForegroundColor Gray }
    } else {
        $testResults.Failed++
        Write-Host "❌ FAIL: $TestName" -ForegroundColor Red
        if ($Message) { Write-Host "   $Message" -ForegroundColor Yellow }
    }
    
    $testResults.Tests += [PSCustomObject]@{
        Name = $TestName
        Passed = $Passed
        Message = $Message
    }
}

Write-Host @"

╔══════════════════════════════════════════════════════════════╗
║                                                              ║
║   🧪  OpenMemory Test Suite  🧪                              ║
║                                                              ║
║   Testing BigDaddyG IDE Memory Module System                ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

# Change to module directory
$modulePath = $PSScriptRoot
Push-Location $modulePath

try {
    # ========================================================================
    # TEST 1: Module Import
    # ========================================================================
    Write-Host "`n[TEST 1] Module Import..." -ForegroundColor Yellow
    
    try {
        Import-Module "$modulePath\OpenMemory.psd1" -Force -ErrorAction Stop
        $functions = Get-Command -Module OpenMemory
        Write-TestResult "Module Import" $true "Loaded $($functions.Count) functions"
    } catch {
        Write-TestResult "Module Import" $false $_.Exception.Message
        throw
    }
    
    # ========================================================================
    # TEST 2: Storage Initialization
    # ========================================================================
    Write-Host "`n[TEST 2] Storage Initialization..." -ForegroundColor Yellow
    
    try {
        $testStorePath = "$modulePath\TestStore"
        if (Test-Path $testStorePath) {
            Remove-Item $testStorePath -Recurse -Force
        }
        
        Initialize-OMStorage -Root $testStorePath
        $storeExists = Test-Path $testStorePath
        Write-TestResult "Storage Initialization" $storeExists "Store created at: $testStorePath"
    } catch {
        Write-TestResult "Storage Initialization" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 3: Memory Addition - Basic
    # ========================================================================
    Write-Host "`n[TEST 3] Memory Addition - Basic..." -ForegroundColor Yellow
    
    try {
        $testMemory = "User prefers dark mode and large fonts"
        $testUserId = "test_user_001"
        
        Add-OMMemory -Content $testMemory -UserId $testUserId -Sector Semantic
        
        # Verify memory was added
        $memories = Get-OMMemory -UserId $testUserId
        $added = ($memories.Count -gt 0)
        Write-TestResult "Memory Addition - Basic" $added "Added memory for user: $testUserId"
    } catch {
        Write-TestResult "Memory Addition - Basic" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 4: Memory Addition - Multiple Sectors
    # ========================================================================
    Write-Host "`n[TEST 4] Memory Addition - Multiple Sectors..." -ForegroundColor Yellow
    
    try {
        $sectors = @('Episodic', 'Procedural', 'Emotional', 'Reflective')
        $addedCount = 0
        
        foreach ($sector in $sectors) {
            Add-OMMemory -Content "Test memory for $sector sector" -UserId $testUserId -Sector $sector
            $addedCount++
        }
        
        $passed = ($addedCount -eq $sectors.Count)
        Write-TestResult "Memory Addition - Multiple Sectors" $passed "Added $addedCount memories across different sectors"
    } catch {
        Write-TestResult "Memory Addition - Multiple Sectors" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 5: Embedding Generation
    # ========================================================================
    Write-Host "`n[TEST 5] Embedding Generation..." -ForegroundColor Yellow
    
    try {
        $testText = "This is a test for vector embedding generation"
        $embedding = Get-OMEmbedding -Text $testText
        
        $hasEmbedding = ($null -ne $embedding -and $embedding.Count -gt 0)
        Write-TestResult "Embedding Generation" $hasEmbedding "Generated embedding with $($embedding.Count) dimensions"
    } catch {
        Write-TestResult "Embedding Generation" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 6: Cosine Similarity Calculation
    # ========================================================================
    Write-Host "`n[TEST 6] Cosine Similarity Calculation..." -ForegroundColor Yellow
    
    try {
        $vec1 = Get-OMEmbedding -Text "dark mode preferences"
        $vec2 = Get-OMEmbedding -Text "user likes dark theme"
        $vec3 = Get-OMEmbedding -Text "completely unrelated topic about cats"
        
        $sim1 = Get-OMCosineSimilarity -VectorA $vec1 -VectorB $vec2
        $sim2 = Get-OMCosineSimilarity -VectorA $vec1 -VectorB $vec3
        
        # Similar texts should have higher similarity than unrelated texts
        $passed = ($sim1 -gt $sim2)
        Write-TestResult "Cosine Similarity Calculation" $passed "Similar: $([math]::Round($sim1, 4)), Unrelated: $([math]::Round($sim2, 4))"
    } catch {
        Write-TestResult "Cosine Similarity Calculation" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 7: Memory Search - Basic
    # ========================================================================
    Write-Host "`n[TEST 7] Memory Search - Basic..." -ForegroundColor Yellow
    
    try {
        $query = "dark mode"
        $results = Search-OMMemory -Query $query -UserId $testUserId -K 3
        
        $passed = ($results.Count -gt 0)
        Write-TestResult "Memory Search - Basic" $passed "Found $($results.Count) results for query: '$query'"
        
        if ($Verbose -and $results.Count -gt 0) {
            Write-Host "   Top result: $($results[0].content)" -ForegroundColor Gray
            Write-Host "   Score: $([math]::Round($results[0].score, 4))" -ForegroundColor Gray
        }
    } catch {
        Write-TestResult "Memory Search - Basic" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 8: Memory Search - Sector Filtering
    # ========================================================================
    Write-Host "`n[TEST 8] Memory Search - Sector Filtering..." -ForegroundColor Yellow
    
    try {
        $results = Search-OMMemory -Query "test" -UserId $testUserId -K 10 -Sectors @('Episodic')
        
        $allEpisodic = $true
        foreach ($result in $results) {
            if ($result.sector -ne 'Episodic') {
                $allEpisodic = $false
                break
            }
        }
        
        Write-TestResult "Memory Search - Sector Filtering" $allEpisodic "All $($results.Count) results from Episodic sector"
    } catch {
        Write-TestResult "Memory Search - Sector Filtering" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 9: Context Window Generation
    # ========================================================================
    Write-Host "`n[TEST 9] Context Window Generation..." -ForegroundColor Yellow
    
    try {
        $context = Get-OMContextWindow -UserId $testUserId -Query "preferences" -MaxTokens 2000
        
        $hasContext = ($null -ne $context -and $context.Length -gt 0)
        Write-TestResult "Context Window Generation" $hasContext "Generated context window ($($context.Length) chars)"
        
        if ($Verbose -and $hasContext) {
            Write-Host "   Context preview: $($context.Substring(0, [Math]::Min(100, $context.Length)))..." -ForegroundColor Gray
        }
    } catch {
        Write-TestResult "Context Window Generation" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 10: User Summary Management
    # ========================================================================
    Write-Host "`n[TEST 10] User Summary Management..." -ForegroundColor Yellow
    
    try {
        $summaryText = "Test user who prefers dark mode and large fonts"
        Update-OMUserSummary -UserId $testUserId -Summary $summaryText
        
        $summary = Get-OMUserSummary -UserId $testUserId
        $passed = ($summary.Summary -eq $summaryText)
        Write-TestResult "User Summary Management" $passed "Summary stored and retrieved successfully"
    } catch {
        Write-TestResult "User Summary Management" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 11: Memory Persistence
    # ========================================================================
    Write-Host "`n[TEST 11] Memory Persistence..." -ForegroundColor Yellow
    
    try {
        # Save storage
        Save-OMStorage
        
        # Count memories before
        $memoryCountBefore = (Get-OMMemory -UserId $testUserId).Count
        
        # Reinitialize storage (simulates restart)
        Initialize-OMStorage -Root $testStorePath
        
        # Count memories after
        $memoryCountAfter = (Get-OMMemory -UserId $testUserId).Count
        
        $passed = ($memoryCountBefore -eq $memoryCountAfter)
        Write-TestResult "Memory Persistence" $passed "Memories survived restart: $memoryCountAfter/$memoryCountBefore"
    } catch {
        Write-TestResult "Memory Persistence" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 12: Memory Decay (Optional)
    # ========================================================================
    if (-not $SkipDecay) {
        Write-Host "`n[TEST 12] Memory Decay..." -ForegroundColor Yellow
        
        try {
            # Get initial decay values
            $memoriesBefore = Get-OMMemory -UserId $testUserId
            $initialDecay = $memoriesBefore[0].decay
            
            # Invoke decay
            Invoke-OMDecay
            
            # Get decay values after
            $memoriesAfter = Get-OMMemory -UserId $testUserId
            $finalDecay = $memoriesAfter[0].decay
            
            $passed = ($finalDecay -le $initialDecay)
            Write-TestResult "Memory Decay" $passed "Decay applied: $initialDecay -> $finalDecay"
        } catch {
            Write-TestResult "Memory Decay" $false $_.Exception.Message
        }
    } else {
        Write-Host "`n[TEST 12] Memory Decay... SKIPPED" -ForegroundColor Gray
    }
    
    # ========================================================================
    # TEST 13: HTTP API (Optional)
    # ========================================================================
    if (-not $SkipAPI) {
        Write-Host "`n[TEST 13] HTTP API..." -ForegroundColor Yellow
        
        try {
            $apiPort = 18765 # Use different port for testing
            
            # Start API
            Start-OMHttpAPI -Port $apiPort -Async
            Start-Sleep -Seconds 2
            
            # Test API endpoint
            try {
                $response = Invoke-RestMethod -Uri "http://localhost:$apiPort/memory/list?userId=$testUserId" -Method Get -ErrorAction Stop
                $passed = ($null -ne $response)
                Write-TestResult "HTTP API" $passed "API responded on port $apiPort"
            } catch {
                Write-TestResult "HTTP API" $false "API did not respond: $($_.Exception.Message)"
            }
            
            # Stop API
            Stop-OMHttpAPI
        } catch {
            Write-TestResult "HTTP API" $false $_.Exception.Message
        }
    } else {
        Write-Host "`n[TEST 13] HTTP API... SKIPPED" -ForegroundColor Gray
    }
    
    # ========================================================================
    # TEST 14: Memory Removal
    # ========================================================================
    Write-Host "`n[TEST 14] Memory Removal..." -ForegroundColor Yellow
    
    try {
        $memories = Get-OMMemory -UserId $testUserId
        if ($memories.Count -gt 0) {
            $memoryId = $memories[0].id
            Remove-OMMemory -MemoryId $memoryId
            
            $afterRemoval = Get-OMMemory -UserId $testUserId
            $passed = ($afterRemoval.Count -eq ($memories.Count - 1))
            Write-TestResult "Memory Removal" $passed "Removed memory, count: $($memories.Count) -> $($afterRemoval.Count)"
        } else {
            Write-TestResult "Memory Removal" $false "No memories to remove"
        }
    } catch {
        Write-TestResult "Memory Removal" $false $_.Exception.Message
    }
    
    # ========================================================================
    # TEST 15: Bulk Operations
    # ========================================================================
    Write-Host "`n[TEST 15] Bulk Operations..." -ForegroundColor Yellow
    
    try {
        # Add multiple memories quickly
        $bulkCount = 10
        $startTime = Get-Date
        
        for ($i = 1; $i -le $bulkCount; $i++) {
            Add-OMMemory -Content "Bulk test memory #$i" -UserId "bulk_test_user" -Sector Semantic
        }
        
        $endTime = Get-Date
        $duration = ($endTime - $startTime).TotalSeconds
        
        $memories = Get-OMMemory -UserId "bulk_test_user"
        $passed = ($memories.Count -eq $bulkCount)
        Write-TestResult "Bulk Operations" $passed "Added $bulkCount memories in $([math]::Round($duration, 2))s"
    } catch {
        Write-TestResult "Bulk Operations" $false $_.Exception.Message
    }
    
    # ========================================================================
    # CLEANUP
    # ========================================================================
    Write-Host "`n[CLEANUP] Removing test data..." -ForegroundColor Yellow
    
    try {
        Clear-OMStorage
        if (Test-Path $testStorePath) {
            Remove-Item $testStorePath -Recurse -Force
        }
        Write-Host "✅ Cleanup completed" -ForegroundColor Green
    } catch {
        Write-Host "⚠️ Cleanup warning: $($_.Exception.Message)" -ForegroundColor Yellow
    }
    
} finally {
    Pop-Location
}

# ============================================================================
# RESULTS SUMMARY
# ============================================================================
Write-Host @"

╔══════════════════════════════════════════════════════════════╗
║                     TEST RESULTS SUMMARY                     ║
╚══════════════════════════════════════════════════════════════╝

"@ -ForegroundColor Cyan

$passRate = if ($testResults.Total -gt 0) { 
    [math]::Round(($testResults.Passed / $testResults.Total) * 100, 1) 
} else { 
    0 
}

Write-Host "Total Tests:  " -NoNewline
Write-Host $testResults.Total -ForegroundColor Cyan

Write-Host "Passed:       " -NoNewline
Write-Host $testResults.Passed -ForegroundColor Green

Write-Host "Failed:       " -NoNewline
Write-Host $testResults.Failed -ForegroundColor $(if ($testResults.Failed -eq 0) { "Green" } else { "Red" })

Write-Host "Pass Rate:    " -NoNewline
Write-Host "$passRate%" -ForegroundColor $(if ($passRate -ge 90) { "Green" } elseif ($passRate -ge 70) { "Yellow" } else { "Red" })

Write-Host ""

if ($testResults.Failed -gt 0) {
    Write-Host "Failed Tests:" -ForegroundColor Red
    $testResults.Tests | Where-Object { -not $_.Passed } | ForEach-Object {
        Write-Host "  ❌ $($_.Name)" -ForegroundColor Red
        if ($_.Message) {
            Write-Host "     $($_.Message)" -ForegroundColor Gray
        }
    }
    Write-Host ""
}

if ($passRate -eq 100) {
    Write-Host "🎉 ALL TESTS PASSED! OpenMemory is fully functional! 🎉" -ForegroundColor Green
} elseif ($passRate -ge 90) {
    Write-Host "✅ Most tests passed. OpenMemory is operational with minor issues." -ForegroundColor Yellow
} else {
    Write-Host "⚠️ Multiple tests failed. Review the results above." -ForegroundColor Red
}

Write-Host ""

