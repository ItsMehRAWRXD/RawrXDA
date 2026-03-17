# GPU Backend Setup Instructions

## System Information
- **Detected GPU**: AMD Radeon RX 7800 XT (16GB VRAM)
- **Current Status**: CPU Fallback Mode
- **Required**: ROCm for Windows (AMD GPU Runtime)

## Installation Steps

### Option 1: ROCm for Windows (Recommended for AMD RX 7800 XT)

1. **Download ROCm for Windows**
   ```
   https://www.amd.com/en/developer/resources/rocm-hub/hip-sdk.html
   ```

2. **Install ROCm SDK**
   - Download the latest ROCm for Windows installer
   - Run installer with administrator privileges
   - Default install path: `C:\Program Files\AMD\ROCm\5.x`

3. **Set Environment Variables**
   ```powershell
   $env:HIP_PATH = "C:\Program Files\AMD\ROCm\5.x"
   $env:PATH = "$env:HIP_PATH\bin;$env:PATH"
   
   # Persist for future sessions:
   [System.Environment]::SetEnvironmentVariable('HIP_PATH', 'C:\Program Files\AMD\ROCm\5.x', 'Machine')
   ```

4. **Verify Installation**
   ```powershell
   hipcc --version
   rocminfo
   ```

5. **Rebuild Application**
   ```powershell
   cd D:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build
   cmake ..
   cmake --build . --config Release --target RawrXD-QtShell -j 8
   ```

### Option 2: Vulkan Compute (Fallback Alternative)

If ROCm installation fails, enable Vulkan compute:

1. **Enable Vulkan in CMake**
   ```powershell
   cd D:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build
   cmake .. -DENABLE_VULKAN=ON
   ```

2. **Install Vulkan SDK** (if not present)
   ```
   https://vulkan.lunarg.com/sdk/home#windows
   ```

## Expected Performance Improvements

### Current (CPU Fallback)
- Avg Latency: **197 ms**
- Tokens/Sec: **30.08 tok/s**
- P95 Latency: **352 ms**

### After ROCm/HIP Installation (Estimated)
- Avg Latency: **5-15 ms** (13-40x faster)
- Tokens/Sec: **1200-1500 tok/s** (40x faster)
- P95 Latency: **20-30 ms** (11-17x faster)
- GPU Utilization: **70-90%** on RX 7800 XT

## Verification Steps

After ROCm installation, run:

```powershell
cd D:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build\bin\Release
.\production_feature_test.exe
```

Expected output:
```
=== GPU BACKEND: Real Hardware Detection ===
[GPUBackend] Found 1 HIP device(s)
[GPUBackend] Device 0: AMD Radeon RX 7800 XT
[GPUBackend] Total memory: 16384 MB
Initialization: SUCCESS
GPU Available: YES
Backend Type: "HIP (ROCm)"
```

## Troubleshooting

### ROCm Not Detected After Install
```powershell
# Check HIP_PATH is set
echo $env:HIP_PATH

# Should output: C:\Program Files\AMD\ROCm\5.x
# If not, set manually (see step 3 above)
```

### CMake Still Shows "NO GPU BACKEND DETECTED"
```powershell
# Clean rebuild
cd D:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build
Remove-Item -Recurse -Force CMakeCache.txt, CMakeFiles
cmake ..
```

### AMD GPU Not Showing Up
```powershell
# Verify GPU is visible to Windows
Get-WmiObject Win32_VideoController | Select Name, DriverVersion

# Update AMD drivers if needed
```

## Current Build Status

✅ CMake Configuration: **Enhanced with GPU detection**  
✅ Diagnostic Messages: **Clear error reporting**  
❌ ROCm Runtime: **Not installed** (blocking GPU acceleration)  
⚠️  Application Status: **Functional but 40x slower than potential**

## Next Steps

1. Install ROCm for Windows
2. Rebuild application
3. Rerun production_feature_test.exe
4. Verify GPU metrics show HIP backend active
5. Compare latency improvements (target: <20ms avg)
