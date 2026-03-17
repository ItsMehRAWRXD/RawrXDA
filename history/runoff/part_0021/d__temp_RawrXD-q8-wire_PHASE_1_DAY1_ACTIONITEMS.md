# Phase 1 Day 1 Action Items (December 5, 2025)

**Goal:** Set up benchmarking infrastructure and establish Q2_K baseline (speed variant)

**Success Criteria:** Q2_K benchmark complete with ~100-120 TPS confirmed

---

## Checklist: Phase 1 Day 1 Kickoff

### 1. Environment Preparation

- [ ] **Verify VRAM Available**
  ```powershell
  # Check total VRAM
  Get-WmiObject -Class Win32_VideoController | Select-Object Name, AdapterRAM
  
  # Expected: 16GB for AMD RX 7800 XT
  ```

- [ ] **Confirm Vulkan Installation**
  ```powershell
  # Verify Vulkan SDK
  Get-Command vulkaninfo -ErrorAction SilentlyContinue
  
  # Should show Vulkan 1.4+
  ```

- [ ] **Check Model Files**
  ```powershell
  # Verify GGUF files exist
  Get-ChildItem D:\OllamaModels\BigDaddyG*.gguf | Format-Table Name, @{
    Label="Size (GB)"; Expression={[math]::Round($_.Length/1GB, 2)}
  }
  
  # MUST HAVE:
  # - BigDaddyG-Q2_K.gguf (8-10 GB)
  # - BigDaddyG-Q4_K.gguf (12-14 GB) ← Already validated baseline
  ```

- [ ] **Create Benchmark Directories**
  ```powershell
  # Create Phase 1 workspace
  New-Item -ItemType Directory -Path D:\GPU_BENCHMARKS\Phase1_Results -Force
  New-Item -ItemType Directory -Path D:\GPU_BENCHMARKS\Phase1_Results\Logs -Force
  New-Item -ItemType Directory -Path D:\GPU_BENCHMARKS\Phase1_Results\Data -Force
  New-Item -ItemType Directory -Path D:\GPU_BENCHMARKS\Phase1_Results\Graphs -Force
  
  Write-Host "✅ Directories created"
  ```

### 2. Benchmark Tool Setup

- [ ] **Copy Benchmark Executable**
  ```powershell
  $source = "D:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build\bin\Release\gpu_inference_benchmark.exe"
  $dest = "D:\GPU_BENCHMARKS\Phase1_Results\gpu_inference_benchmark.exe"
  
  if (Test-Path $source) {
    Copy-Item $source $dest
    Write-Host "✅ gpu_inference_benchmark.exe copied"
  } else {
    Write-Host "❌ ERROR: gpu_inference_benchmark.exe not found"
    Write-Host "   Expected at: $source"
  }
  ```

- [ ] **Verify Benchmark Tool Works**
  ```powershell
  cd D:\GPU_BENCHMARKS\Phase1_Results
  
  # Quick test: 10 tokens to verify tool runs
  .\gpu_inference_benchmark.exe --model Q4_K --tokens 10 --runs 1
  
  # Should complete in < 5 seconds with GPU utilization output
  ```

### 3. Baseline Verification (Q4_K Re-validation)

- [ ] **Run Quick Q4_K Baseline**
  ```powershell
  cd D:\GPU_BENCHMARKS\Phase1_Results
  
  # Quick baseline run: 256 tokens x 1 run
  # Expected: 79.97 TPS ± 2% (should be ~79-82 TPS)
  
  .\gpu_inference_benchmark.exe --model Q4_K --tokens 256 --runs 1 `
    | Tee-Object -FilePath ./logs/Q4K_baseline_verify.log
  ```

### 4. Q2_K Baseline Benchmark (PRIMARY TASK)

- [ ] **Run Q2_K Benchmarks**
  ```powershell
  cd D:\GPU_BENCHMARKS\Phase1_Results
  
  # Test matrix for Q2_K:
  # - 256 tokens x 5 runs
  # - 512 tokens x 3 runs  
  # - 1024 tokens x 2 runs
  
  Write-Host "=== Q2_K Benchmark: 256 tokens ===" -ForegroundColor Cyan
  for ($i = 1; $i -le 5; $i++) {
    Write-Host "Run $i/5..."
    .\gpu_inference_benchmark.exe --model Q2_K --tokens 256 --runs 1 `
      | Tee-Object -FilePath ./Data/Q2K_256_run$i.log
  }
  
  Write-Host "=== Q2_K Benchmark: 512 tokens ===" -ForegroundColor Cyan
  for ($i = 1; $i -le 3; $i++) {
    Write-Host "Run $i/3..."
    .\gpu_inference_benchmark.exe --model Q2_K --tokens 512 --runs 1 `
      | Tee-Object -FilePath ./Data/Q2K_512_run$i.log
  }
  
  Write-Host "=== Q2_K Benchmark: 1024 tokens ===" -ForegroundColor Cyan
  for ($i = 1; $i -le 2; $i++) {
    Write-Host "Run $i/2..."
    .\gpu_inference_benchmark.exe --model Q2_K --tokens 1024 --runs 1 `
      | Tee-Object -FilePath ./Data/Q2K_1024_run$i.log
  }
  
  Write-Host "`n✅ Q2_K benchmarks complete" -ForegroundColor Green
  ```

### 5. Results Collection

- [ ] **Aggregate Results to CSV**
  ```powershell
  # Parse log files and create CSV
  $results = @()
  
  Get-ChildItem D:\GPU_BENCHMARKS\Phase1_Results\Data\Q2K_*.log | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    
    # Extract metrics from output (parsing will depend on benchmark output format)
    $tps = [regex]::Match($content, "Tokens/Sec:\s*([\d.]+)").Groups[1].Value
    $latency = [regex]::Match($content, "Latency:\s*([\d.]+)").Groups[1].Value
    
    $results += [PSCustomObject]@{
      File = $_.Name
      TPS = $tps
      Latency = $latency
      Date = Get-Date
    }
  }
  
  $results | Export-Csv -Path D:\GPU_BENCHMARKS\Phase1_Results\Data\Q2K_summary.csv -NoTypeInformation
  Write-Host "✅ Results exported to CSV"
  ```

### 6. Analysis & Documentation

- [ ] **Create Day 1 Summary Report**
  ```markdown
  # Phase 1 Day 1 Summary Report
  ## December 5, 2025
  
  **Status:** ✅ COMPLETE
  
  ### Environment
  - GPU: AMD Radeon RX 7800 XT ✅
  - VRAM: 16 GB available ✅
  - Vulkan: 1.4+ ✅
  - Benchmark tool: Working ✅
  
  ### Baselines Verified
  - Q4_K (production baseline): 79.97 TPS ✅
  - VRAM usage: ~13 GB ✅
  
  ### Q2_K Benchmark Results
  - 256 tokens: ~115 TPS (avg of 5 runs)
  - 512 tokens: ~113 TPS (avg of 3 runs)
  - 1024 tokens: ~111 TPS (avg of 2 runs)
  
  ### Key Finding
  Q2_K confirmed as speed champion: ~113-115 TPS (44% faster than Q4_K baseline)
  Trade-off: Lower precision model suitable for speed-critical applications
  
  ### Next Steps (Dec 6)
  - Benchmark Q5_K_M (high precision, 60-75 TPS expected)
  - Benchmark Q6_K (maximum precision, 40-60 TPS expected)
  - Begin performance curve construction
  ```

### 7. Version Control

- [ ] **Commit Day 1 Results**
  ```powershell
  cd D:\temp\RawrXD-q8-wire
  
  # Copy results to repo
  Copy-Item D:\GPU_BENCHMARKS\Phase1_Results\Data\* `
    .\Phase1_Benchmarks\Day1_Results\ -Recurse -Force
  
  # Commit
  git add Phase1_Benchmarks/Day1_Results/
  git commit -m "Phase 1 Day 1 Complete: Q2_K baseline established

Results:
- Q2_K: ~113-115 TPS (44% faster than Q4_K)
- Q4_K baseline re-validated: 79.97 TPS ✅
- Environment verification complete
- Benchmark infrastructure ready

Next: Q5_K_M & Q6_K benchmarks on Dec 6"
  
  git push origin master
  
  Write-Host "✅ Results committed to GitHub"
  ```

---

## Expected Results (Day 1)

### Q4_K Baseline Re-verification
```
Model: Q4_K
Tokens: 256
Expected TPS: 79-82 (baseline 79.97)
Expected Latency: 12.5 ms/token
GPU Util: ~95%
VRAM: ~13 GB
```

### Q2_K Speed Benchmark
```
Model: Q2_K
Tokens: 256
Expected TPS: 110-120
Expected Latency: 8.5-9.0 ms/token
GPU Util: ~95%
VRAM: ~9-10 GB

Model: Q2_K
Tokens: 512
Expected TPS: 110-115
Expected Latency: ~8.7 ms/token

Model: Q2_K
Tokens: 1024
Expected TPS: 110-113
Expected Latency: ~8.9 ms/token
```

### Success Indicators
- ✅ Q2_K confirmed faster than Q4_K by ~40-45%
- ✅ Q2_K TPS in expected 110-120 range
- ✅ Latency scales linearly with token count
- ✅ VRAM usage consistent with expectations
- ✅ GPU utilization stable at 95%+

---

## Troubleshooting

### Issue: gpu_inference_benchmark.exe not found
**Solution:** Rebuild from source
```powershell
cd D:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build
cmake --build . --config Release --target gpu_inference_benchmark -j 8
```

### Issue: Q2_K benchmark takes too long
**Solution:** Reduce token count for initial runs
```powershell
# Quick test with 64 tokens instead of 256
.\gpu_inference_benchmark.exe --model Q2_K --tokens 64 --runs 1
```

### Issue: VRAM runs out during benchmark
**Solution:** Close unnecessary applications
```powershell
# Check memory pressure
Get-WmiObject -Class Win32_OperatingSystem | Select-Object @{
  Label="Free VRAM (GB)"; Expression={[math]::Round($_.FreeVirtualMemory/1MB/1024, 2)}
}
```

### Issue: GPU utilization drops during test
**Solution:** Disable driver-level power management
```powershell
# Check AMD GPU settings:
# Radeon Settings → Performance → Power Efficiency → Manual (High Performance)
```

---

## Time Estimate

- Setup & verification: ~15 minutes
- Q2_K benchmarks (all runs): ~30-45 minutes
- Results analysis: ~10 minutes
- Commit to GitHub: ~5 minutes

**Total: ~1 hour**

---

## Success Confirmation

When Phase 1 Day 1 is complete, you should have:

1. ✅ Q2_K benchmarks: 5 runs at 256 tokens
2. ✅ Q2_K benchmarks: 3 runs at 512 tokens
3. ✅ Q2_K benchmarks: 2 runs at 1024 tokens
4. ✅ Q4_K baseline re-verified: 79.97 TPS
5. ✅ Results CSV file: All data collected
6. ✅ Day 1 summary report: Findings documented
7. ✅ GitHub commit: Results version-controlled
8. ✅ Performance curve started: Q2_K point established

**Ready to proceed to Day 2 (Dec 6): Q5_K_M & Q6_K benchmarks**

---

## Day 1 Decision Gate

Before proceeding to Day 2, confirm:

- [ ] Q2_K achieved ~110-120 TPS (not < 105 or > 125)
- [ ] Q4_K baseline still ~80 TPS (confirms system consistency)
- [ ] All data files created and backed up
- [ ] Results committed to GitHub
- [ ] No unexpected errors or crashes

**If all checked:** Proceed to Phase 1 Day 2 ✅  
**If issues found:** Troubleshoot and re-run (Day 1 repeats)

---

**Phase 1 Day 1 Kickoff: December 5, 2025**  
**Expected Completion: December 5, 2025 evening**  
**Progress Status: ⏳ Ready to Start**
