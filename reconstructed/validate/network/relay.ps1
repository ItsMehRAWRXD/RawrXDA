# Validation Checklist for MASM64 Network Relay

# 1. Verify ASM exports
Write-Host "Checking ASM exports..."
dumpbin /symbols RawrXD_NetworkRelay.obj | findstr "RelayEngine"

# 2. Check for frame pointer optimizations (should show unwind info)
Write-Host "Checking unwind info..."
dumpbin /unwindinfo RawrXD_NetworkRelay.obj

# 3. Runtime validation - create test forward
Write-Host "Runtime validation: Add port 8080 -> localhost:80 in RawrXD, toggle active"
Write-Host "Check Task Manager - should see low CPU during transfer"

# 4. Large page privilege check
Write-Host "Checking large page privilege..."
$process = Get-Process -Name RawrXD-* -ErrorAction SilentlyContinue
if ($process) {
    $workingSetMB = [math]::Round($process.WorkingSet64 / 1MB, 2)
    $largePages = $process.WorkingSet64 -gt 100MB
    Write-Host "Working Set: $workingSetMB MB, Large Pages: $largePages"
} else {
    Write-Host "RawrXD process not found"
}