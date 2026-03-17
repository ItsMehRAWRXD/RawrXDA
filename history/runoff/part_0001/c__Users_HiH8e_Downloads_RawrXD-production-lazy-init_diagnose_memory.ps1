# Diagnose top memory consumers and summarize system usage
# Usage: pwsh -File diagnose_memory.ps1

$ErrorActionPreference = 'Stop'
Write-Host "Collecting process memory metrics..." -ForegroundColor Cyan

# Gather using Get-Process and CIM for Commit/PrivatePageCount
$procs = Get-Process | Select-Object Name, Id, CPU, WS, PM, VM
$cim = Get-CimInstance Win32_Process | Select-Object Name, ProcessId, WorkingSetSize, PrivatePageCount, PageFileUsage

# Join datasets by PID
$joined = foreach ($p in $procs) {
  $ci = $cim | Where-Object { $_.ProcessId -eq $p.Id } | Select-Object -First 1
  [PSCustomObject]@{
    Name = $p.Name
    PID = $p.Id
    CPU = $p.CPU
    WorkingSet_MB = [math]::Round($p.WS / 1MB, 1)
    PrivateMemory_MB = [math]::Round($p.PM / 1MB, 1)
    VirtualMemory_MB = [math]::Round($p.VM / 1MB, 1)
    Commit_MB = if ($ci) { [math]::Round($ci.PrivatePageCount / 1MB, 1) } else { $null }
    PageFile_MB = if ($ci) { [math]::Round($ci.PageFileUsage / 1MB, 1) } else { $null }
  }
}

# Top consumers by Working Set
Write-Host "\nTop 20 by Working Set (MB):" -ForegroundColor Yellow
$joined | Sort-Object -Property WorkingSet_MB -Descending | Select-Object -First 20 | Format-Table -AutoSize Name,PID,WorkingSet_MB,PrivateMemory_MB,Commit_MB,PageFile_MB

# Top consumers by Commit
Write-Host "\nTop 20 by Commit (MB):" -ForegroundColor Yellow
$joined | Where-Object { $_.Commit_MB } | Sort-Object -Property Commit_MB -Descending | Select-Object -First 20 | Format-Table -AutoSize Name,PID,Commit_MB,WorkingSet_MB,PageFile_MB

# System summary
$os = Get-CimInstance Win32_OperatingSystem
$totalPhysMB = [math]::Round($os.TotalVisibleMemorySize / 1024, 0)
$freePhysMB = [math]::Round($os.FreePhysicalMemory / 1024, 0)
$commitLimitMB = [math]::Round($os.TotalVirtualMemorySize / 1024, 0)
$commitUsedMB = [math]::Round(($os.TotalVirtualMemorySize - $os.FreeVirtualMemory) / 1024, 0)

Write-Host "\nSystem Memory Summary:" -ForegroundColor Cyan
Write-Host ("Physical RAM: {0} MB (free {1} MB)" -f $totalPhysMB, $freePhysMB)
Write-Host ("Commit: {0} MB used of {1} MB limit" -f $commitUsedMB, $commitLimitMB)

# Save CSV for deeper analysis
$outDir = Join-Path $PSScriptRoot 'logs'
if (!(Test-Path $outDir)) { New-Item -Path $outDir -ItemType Directory | Out-Null }
$outCsv = Join-Path $outDir ('memory_processes_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '.csv')
$joined | Export-Csv -Path $outCsv -NoTypeInformation
Write-Host "\nDetails exported to: $outCsv" -ForegroundColor Green
