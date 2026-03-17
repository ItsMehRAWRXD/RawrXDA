# 🔧 RawrXD IDE - Operator Deployment Guide
## Production Runbook for DevOps/MLOps Engineers

**Document Version:** 1.0 (Production Release)  
**Created:** January 1, 2026  
**Target Audience:** DevOps, MLOps, SRE Engineers  
**Expertise Level:** Intermediate to Advanced

---

## 🎯 QUICK START

### Production Deployment (5 Minutes)

```powershell
# 1. Verify prerequisites
Get-WmiObject Win32_VideoController | Where-Object {$_.Name -like "*Radeon*"}
vulkaninfo | Select-String "API Version"

# 2. Download model (BigDaddyG 32B Q4_K - 12.5 GB)
Invoke-WebRequest -Uri "https://huggingface.co/models/bigdaddyg-q4_k.gguf" -OutFile "bigdaddyg-q4_k.gguf"

# 3. Launch RawrXD IDE
./RawrXD-QtShell.exe --enable-gpu --model bigdaddyg-q4_k.gguf

# 4. Verify GPU acceleration
Get-Process RawrXD-QtShell | Select-Object CPU, WorkingSet
```

**Expected Result:** 14-16 GB VRAM usage, 79.97 TPS throughput, 90-98% GPU utilization.

---

## 📋 TABLE OF CONTENTS

1. [Prerequisites](#prerequisites)
2. [Installation & Configuration](#installation-configuration)
3. [Performance Tuning](#performance-tuning)
4. [Monitoring & Alerts](#monitoring-alerts)
5. [Troubleshooting](#troubleshooting)
6. [Scaling & Load Balancing](#scaling-load-balancing)
7. [Security Considerations](#security-considerations)
8. [Backup & Disaster Recovery](#backup-disaster-recovery)

---

## 1. PREREQUISITES

### 1.1 Hardware Requirements

#### Minimum Configuration (Q4_K Production Baseline)
- **GPU:** AMD Radeon with 16 GB VRAM
  - Validated: AMD Radeon 7900 XTX (24 GB)
  - Supported: AMD Radeon 6800 XT (16 GB)
  - Budget: AMD Radeon 6700 XT (12 GB, Q2_K only)
- **CPU:** 8-core x86-64 processor (AMD Ryzen 5/Intel i5 or higher)
- **RAM:** 32 GB system memory
- **Storage:** 50 GB free (25 GB for models, 25 GB for workspace)
- **Network:** 1 Gbps (for model downloads)

#### Recommended Configuration (Q6_K High Precision)
- **GPU:** AMD Radeon with 24 GB VRAM (7900 XTX)
- **CPU:** 16-core x86-64 processor (AMD Ryzen 9/Intel i9)
- **RAM:** 64 GB system memory
- **Storage:** 100 GB SSD (NVMe preferred)
- **Network:** 10 Gbps (for high-throughput deployments)

#### Hardware Verification

```powershell
# Check GPU VRAM
Get-WmiObject Win32_VideoController | Select-Object Name, AdapterRAM, DriverVersion

# Check CPU cores
Get-WmiObject Win32_Processor | Select-Object NumberOfCores, NumberOfLogicalProcessors

# Check system RAM
Get-WmiObject Win32_PhysicalMemory | Measure-Object -Property Capacity -Sum

# Check disk space
Get-PSDrive -PSProvider FileSystem | Select-Object Name, @{Name="Free(GB)";Expression={[math]::Round($_.Free/1GB,2)}}
```

### 1.2 Software Requirements

#### Operating System
- **Windows 10/11** (64-bit) with latest updates
- **Windows Server 2019/2022** for production deployments

#### Driver Requirements
- **Vulkan 1.4+** (AMD Adrenalin drivers 23.9.1 or later)
- **DirectX 12** (for fallback rendering)

#### Runtime Dependencies
- **Qt6 Runtime** (Qt 6.5+ libraries)
- **Visual C++ Redistributable 2022** (x64)
- **PowerShell 7.4+** (for automation scripts)

#### Driver Installation

```powershell
# Download AMD Adrenalin drivers
Invoke-WebRequest -Uri "https://www.amd.com/en/support/download/drivers.html" -OutFile "amd-driver-installer.exe"

# Install silently
./amd-driver-installer.exe /S /V"/qn REBOOT=ReallySuppress"

# Verify Vulkan support
vulkaninfo --summary
```

**Expected Output:**
```
Vulkan Instance Version: 1.4.xxx
Device: AMD Radeon RX 7900 XTX
Driver Version: 23.9.1
```

### 1.3 Model Requirements

#### Supported GGUF Models
- **BigDaddyG 32B** (Q2_K, Q4_K, Q5_K_M, Q6_K, Q8_0)
- **Llama 3.1 70B** (Q4_K, Q5_K_M)
- **Mistral 22B** (Q4_K, Q6_K)

#### Model Download

```powershell
# Create models directory
New-Item -ItemType Directory -Force -Path "C:\RawrXD\models"

# Download Q4_K baseline (recommended)
Invoke-WebRequest -Uri "https://huggingface.co/models/bigdaddyg-q4_k.gguf" -OutFile "C:\RawrXD\models\bigdaddyg-q4_k.gguf"

# Verify checksum (SHA256)
Get-FileHash "C:\RawrXD\models\bigdaddyg-q4_k.gguf" -Algorithm SHA256
```

**Expected File Size:** 12.5 GB (Q4_K), 6.5 GB (Q2_K), 21.0 GB (Q8_0)

---

## 2. INSTALLATION & CONFIGURATION

### 2.1 Basic Installation

#### Step 1: Extract RawrXD IDE

```powershell
# Extract from release package
Expand-Archive -Path "RawrXD-IDE-Production-v1.0.zip" -DestinationPath "C:\RawrXD"

# Verify files
Get-ChildItem -Path "C:\RawrXD" -Recurse | Select-Object Name, Length
```

**Expected Files:**
```
RawrXD-QtShell.exe (110 KB)
RawrXD-ModelLoader.dll
Qt6Core.dll
Qt6Widgets.dll
config\default-config.json
```

#### Step 2: Configure Environment

```powershell
# Create config directory
New-Item -ItemType Directory -Force -Path "C:\RawrXD\config"

# Create configuration file
@"
{
  "gpu": {
    "enabled": true,
    "device": "auto",
    "vram_limit_gb": 16
  },
  "model": {
    "path": "C:/RawrXD/models/bigdaddyg-q4_k.gguf",
    "quantization": "Q4_K",
    "context_length": 4096
  },
  "performance": {
    "threads": 8,
    "batch_size": 512,
    "concurrency_limit": 12
  },
  "logging": {
    "level": "INFO",
    "file": "C:/RawrXD/logs/rawrxd.log",
    "max_size_mb": 100,
    "rotate_count": 5
  }
}
"@ | Out-File -FilePath "C:\RawrXD\config\production-config.json" -Encoding UTF8
```

#### Step 3: Launch IDE

```powershell
# Production launch command
Set-Location "C:\RawrXD"
./RawrXD-QtShell.exe --config "config/production-config.json" --enable-gpu

# Alternative: Quick launch with defaults
./RawrXD-QtShell.exe --enable-gpu --model "models/bigdaddyg-q4_k.gguf"
```

### 2.2 Configuration Options

#### GPU Configuration

```json
{
  "gpu": {
    "enabled": true,                // Enable GPU acceleration
    "device": "auto",               // Auto-detect GPU (or specify device ID)
    "vram_limit_gb": 16,            // VRAM limit (prevent OOM)
    "vulkan_layers": ["validation"], // Enable validation layers (debug)
    "gpu_index": 0                  // Multi-GPU: select GPU index
  }
}
```

#### Model Configuration

```json
{
  "model": {
    "path": "C:/RawrXD/models/bigdaddyg-q4_k.gguf",
    "quantization": "Q4_K",         // Q2_K, Q4_K, Q5_K_M, Q6_K, Q8_0
    "context_length": 4096,         // Max context window (tokens)
    "rope_scaling": 1.0,            // RoPE scaling factor (1.0 = default)
    "flash_attention": true         // Enable Flash Attention (faster)
  }
}
```

#### Performance Configuration

```json
{
  "performance": {
    "threads": 8,                   // CPU threads (match CPU cores)
    "batch_size": 512,              // Batch size (higher = more throughput)
    "concurrency_limit": 12,        // Max concurrent requests
    "request_timeout_sec": 300,     // Request timeout (5 minutes)
    "queue_depth": 100              // Max queued requests
  }
}
```

#### Logging Configuration

```json
{
  "logging": {
    "level": "INFO",                // DEBUG, INFO, WARNING, ERROR, CRITICAL
    "file": "C:/RawrXD/logs/rawrxd.log",
    "console": true,                // Enable console output
    "max_size_mb": 100,             // Max log file size before rotation
    "rotate_count": 5,              // Number of rotated log files to keep
    "structured": true              // JSON-formatted logs (for parsing)
  }
}
```

---

## 3. PERFORMANCE TUNING

### 3.1 GPU Clock Scaling

#### Enable High-Performance Mode

```powershell
# Set Windows power plan to High Performance
powercfg /setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c

# Verify GPU clock speed
Get-WmiObject Win32_VideoController | Select-Object Name, CurrentRefreshRate
```

#### AMD Radeon Settings

```powershell
# Launch AMD Radeon Software
Start-Process "C:\Program Files\AMD\CNext\CNext\RadeonSoftware.exe"

# Manual steps:
# 1. Go to "Performance" → "Tuning"
# 2. Enable "GPU Tuning"
# 3. Set "Power Limit" to +15% (or maximum allowed)
# 4. Set "Fan Tuning" to "Custom" (target 75°C)
```

### 3.2 Thread Pool Sizing

#### Optimal Thread Count

```powershell
# Get CPU core count
$cores = (Get-WmiObject Win32_Processor).NumberOfLogicalProcessors

# Recommended thread count: 70-80% of logical cores
$threads = [Math]::Floor($cores * 0.75)

Write-Output "Recommended threads: $threads"
```

#### Update Configuration

```json
{
  "performance": {
    "threads": 12,  // Example: 16 cores * 0.75 = 12 threads
    "batch_size": 512
  }
}
```

### 3.3 Batch Size Configuration

#### Batch Size vs Latency Trade-off

| Batch Size | Throughput (TPS) | Latency (ms) | VRAM Usage |
|------------|------------------|--------------|------------|
| 128 | 65 TPS | 8 ms | 12 GB |
| 256 | 75 TPS | 10 ms | 14 GB ✅ |
| 512 | 80 TPS ✅ | 12 ms | 16 GB |
| 1024 | 82 TPS | 18 ms | 18 GB |

**Recommendation:** Use batch_size = 512 for Q4_K (balanced throughput/latency).

#### Adaptive Batch Sizing

```json
{
  "performance": {
    "batch_size": "auto",  // Automatically adjust based on VRAM
    "batch_size_min": 256,
    "batch_size_max": 1024
  }
}
```

### 3.4 VRAM Optimization

#### Memory Management

```json
{
  "gpu": {
    "vram_limit_gb": 14,           // Reserve 2 GB for OS/drivers
    "kv_cache_type": "f16",        // f16 (faster) or f32 (more precise)
    "offload_layers": "auto"       // Auto-offload to CPU if VRAM full
  }
}
```

#### VRAM Monitoring

```powershell
# Monitor VRAM usage
Get-Process RawrXD-QtShell | Select-Object WorkingSet64

# Convert to GB
(Get-Process RawrXD-QtShell).WorkingSet64 / 1GB
```

**Expected VRAM Usage:**
- Q2_K: 8-10 GB
- Q4_K: 14-16 GB ✅
- Q6_K: 18-20 GB
- Q8_0: 20-24 GB

---

## 4. MONITORING & ALERTS

### 4.1 Performance Metrics

#### Key Metrics to Monitor

| Metric | Target (Q4_K) | Critical Threshold | Alert Action |
|--------|---------------|-------------------|--------------|
| **Throughput** | 75-80 TPS | < 60 TPS | Scale up resources |
| **Latency P50** | ≤ 13 ms | > 20 ms | Reduce load |
| **Latency P95** | ≤ 50 ms | > 100 ms | Investigate bottleneck |
| **VRAM Usage** | 14-16 GB | > 15 GB | Risk of OOM |
| **GPU Temp** | ≤ 85°C | > 90°C | Reduce clock/increase cooling |
| **Error Rate** | < 0.1% | > 1% | Check logs |

#### Metrics Collection Script

```powershell
# metrics-collector.ps1
$logFile = "C:\RawrXD\logs\metrics.log"

while ($true) {
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    
    # Get VRAM usage
    $vram = (Get-Process RawrXD-QtShell -ErrorAction SilentlyContinue).WorkingSet64 / 1GB
    
    # Get GPU temperature (requires AMD SDK)
    $temp = (Get-WmiObject -Namespace "root\wmi" -Class MSAcpi_ThermalZoneTemperature).CurrentTemperature / 10 - 273.15
    
    # Get CPU usage
    $cpu = (Get-Counter '\Processor(_Total)\% Processor Time').CounterSamples.CookedValue
    
    # Log metrics
    "$timestamp | VRAM: $vram GB | Temp: $temp C | CPU: $cpu %" | Out-File -Append $logFile
    
    Start-Sleep -Seconds 10
}
```

**Run in background:**
```powershell
Start-Process powershell -ArgumentList "-File C:\RawrXD\scripts\metrics-collector.ps1" -WindowStyle Hidden
```

### 4.2 Alerting Configuration

#### Windows Event Log Integration

```powershell
# Create custom event log source
New-EventLog -LogName Application -Source "RawrXDIDE"

# Example: Alert on high latency
if ($latency_p95 -gt 100) {
    Write-EventLog -LogName Application -Source "RawrXDIDE" -EntryType Warning -EventId 1001 -Message "High latency detected: $latency_p95 ms"
}
```

#### Email Alerts (SMTP)

```powershell
# email-alert.ps1
param(
    [string]$AlertType,
    [string]$Message
)

$smtpServer = "smtp.company.com"
$from = "rawrxd-alerts@company.com"
$to = "devops@company.com"
$subject = "RawrXD Alert: $AlertType"

Send-MailMessage -SmtpServer $smtpServer -From $from -To $to -Subject $subject -Body $Message
```

**Example Usage:**
```powershell
./email-alert.ps1 -AlertType "High VRAM" -Message "VRAM usage exceeded 15 GB: 15.8 GB"
```

### 4.3 Prometheus Integration (Advanced)

#### Install Prometheus Exporter

```powershell
# Download Prometheus Windows exporter
Invoke-WebRequest -Uri "https://github.com/prometheus-community/windows_exporter/releases/latest/download/windows_exporter.msi" -OutFile "windows_exporter.msi"

# Install
msiexec /i windows_exporter.msi /quiet

# Verify metrics endpoint
Invoke-WebRequest -Uri "http://localhost:9182/metrics"
```

#### Custom RawrXD Metrics

```cpp
// In RawrXD source code (example)
#include <prometheus/counter.h>
#include <prometheus/gauge.h>

// Metrics registry
prometheus::Gauge& vram_usage = /* ... */;
prometheus::Counter& tokens_generated = /* ... */;
prometheus::Histogram& latency_histogram = /* ... */;

// Update metrics
vram_usage.Set(getCurrentVRAMUsage());
tokens_generated.Increment(numTokens);
latency_histogram.Observe(latency_ms);
```

#### Prometheus Configuration (`prometheus.yml`)

```yaml
scrape_configs:
  - job_name: 'rawrxd-ide'
    static_configs:
      - targets: ['localhost:9182']
    scrape_interval: 10s
```

---

## 5. TROUBLESHOOTING

### 5.1 Common Issues

#### Issue 1: "Vulkan device not found"

**Symptoms:**
```
ERROR: Failed to initialize Vulkan device
ERROR: No compatible GPU found
```

**Diagnosis:**
```powershell
# Check Vulkan support
vulkaninfo | Select-String "driverVersion"

# Check AMD drivers
Get-WmiObject Win32_VideoController | Select-Object Name, DriverVersion
```

**Solution:**
```powershell
# Update AMD drivers
Invoke-WebRequest -Uri "https://www.amd.com/en/support/download/drivers.html" -OutFile "amd-driver-installer.exe"
./amd-driver-installer.exe /S

# Reboot system
Restart-Computer -Force
```

---

#### Issue 2: "Out of Memory (VRAM exhaustion)"

**Symptoms:**
```
ERROR: VRAM allocation failed (requested 16 GB, available 14 GB)
CRITICAL: GPU out of memory
```

**Diagnosis:**
```powershell
# Check current VRAM usage
(Get-Process RawrXD-QtShell).WorkingSet64 / 1GB

# Check available VRAM
Get-WmiObject Win32_VideoController | Select-Object Name, AdapterRAM
```

**Solution:**
```json
// Reduce VRAM usage in config
{
  "gpu": {
    "vram_limit_gb": 12  // Reduce from 16 GB to 12 GB
  },
  "model": {
    "quantization": "Q2_K"  // Use lower quantization (Q2_K instead of Q4_K)
  },
  "performance": {
    "batch_size": 256  // Reduce batch size (512 → 256)
  }
}
```

---

#### Issue 3: "High Latency (> 100ms P95)"

**Symptoms:**
```
WARNING: Latency P95 = 120 ms (target: 50 ms)
WARNING: Throughput degraded to 55 TPS (target: 75 TPS)
```

**Diagnosis:**
```powershell
# Check GPU utilization
Get-Counter '\GPU Engine(*)\Utilization Percentage'

# Check thermal throttling
Get-WmiObject -Namespace "root\wmi" -Class MSAcpi_ThermalZoneTemperature | Select-Object CurrentTemperature
```

**Solution:**
```powershell
# 1. Reduce GPU temperature
# Open AMD Radeon Software → Fan Tuning → Set to 80% speed

# 2. Reduce concurrency
# Edit config: "concurrency_limit": 8 (down from 12)

# 3. Increase batch size (if VRAM allows)
# Edit config: "batch_size": 1024 (up from 512)
```

---

#### Issue 4: "Model Loading Failure"

**Symptoms:**
```
ERROR: Failed to load model: bigdaddyg-q4_k.gguf
ERROR: File not found or corrupted
```

**Diagnosis:**
```powershell
# Verify file exists
Test-Path "C:\RawrXD\models\bigdaddyg-q4_k.gguf"

# Check file integrity (SHA256)
Get-FileHash "C:\RawrXD\models\bigdaddyg-q4_k.gguf" -Algorithm SHA256

# Expected hash (example):
# SHA256: 7f3a8b2c... (obtain from model provider)
```

**Solution:**
```powershell
# Re-download model
Remove-Item "C:\RawrXD\models\bigdaddyg-q4_k.gguf" -Force
Invoke-WebRequest -Uri "https://huggingface.co/models/bigdaddyg-q4_k.gguf" -OutFile "C:\RawrXD\models\bigdaddyg-q4_k.gguf"

# Verify checksum again
Get-FileHash "C:\RawrXD\models\bigdaddyg-q4_k.gguf" -Algorithm SHA256
```

---

#### Issue 5: "GPU Crash / Driver Timeout"

**Symptoms:**
```
CRITICAL: GPU driver timeout (TDR)
CRITICAL: Vulkan device lost
System Event Log: "Display driver AMD stopped responding and has recovered"
```

**Diagnosis:**
```powershell
# Check Windows Event Viewer
Get-WinEvent -LogName System -MaxEvents 50 | Where-Object {$_.Message -like "*AMD*"}

# Check GPU clock stability
# Use AMD Radeon Software → Performance → Metrics → GPU Clock
```

**Solution:**
```powershell
# 1. Disable GPU overclocking
# AMD Radeon Software → Performance → Tuning → Reset to Default

# 2. Increase TDR timeout (Windows Registry)
reg add "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\GraphicsDrivers" /v TdrDelay /t REG_DWORD /d 60 /f

# 3. Reduce GPU load
# Edit config: "batch_size": 256 (reduce workload)

# 4. Update drivers
# Download latest AMD Adrenalin drivers and reinstall
```

---

### 5.2 Debug Mode

#### Enable Debug Logging

```json
{
  "logging": {
    "level": "DEBUG",  // Change from INFO to DEBUG
    "console": true,
    "structured": true
  }
}
```

#### Launch with Debug Flags

```powershell
./RawrXD-QtShell.exe --enable-gpu --model "models/bigdaddyg-q4_k.gguf" --debug --vulkan-validation
```

**Debug Output Example:**
```
[DEBUG] GPU device detected: AMD Radeon RX 7900 XTX
[DEBUG] VRAM available: 24 GB
[DEBUG] Model loaded: bigdaddyg-q4_k.gguf (12.5 GB)
[DEBUG] Batch size: 512, Threads: 12
[DEBUG] Inference latency: 12.34 ms (token 1)
```

---

## 6. SCALING & LOAD BALANCING

### 6.1 Single-GPU Scaling

#### Concurrency Optimization

```json
{
  "performance": {
    "concurrency_limit": 12,  // Max concurrent requests (Q4_K)
    "queue_depth": 100,       // Max queued requests
    "queue_strategy": "fifo"  // FIFO, LIFO, or priority-based
  }
}
```

#### Concurrency vs Latency Trade-off

| Concurrent Users | Latency P50 | Latency P95 | Throughput/User |
|------------------|-------------|-------------|-----------------|
| 1 | 12.51 ms | 35 ms | 79.97 TPS |
| 4 | 15 ms | 45 ms | 70 TPS |
| 8 | 18 ms | 65 ms | 60 TPS ✅ |
| 12 | 22 ms | 85 ms | 50 TPS |
| 16 | 28 ms | 110 ms | 45 TPS ⚠️ |

**Recommendation:** Limit to 8-12 concurrent users for optimal balance.

### 6.2 Multi-GPU Deployment

#### Configuration (2 GPUs)

```json
{
  "gpu": {
    "enabled": true,
    "multi_gpu": true,
    "devices": [0, 1],  // GPU 0 and GPU 1
    "load_balancing": "round_robin"  // or "least_loaded"
  }
}
```

#### Load Balancing Strategies

| Strategy | Description | Use Case |
|----------|-------------|----------|
| **round_robin** | Distribute requests evenly | Uniform workloads |
| **least_loaded** | Route to GPU with lowest VRAM usage | Variable workloads |
| **sticky_session** | Same user → same GPU (preserves context) | Chat sessions |

#### Expected Scaling

- **1 GPU (Q4_K):** 79.97 TPS, 8-12 concurrent users
- **2 GPUs (Q4_K):** ~140 TPS, 16-24 concurrent users
- **4 GPUs (Q4_K):** ~280 TPS, 32-48 concurrent users

**Note:** Scaling is sub-linear due to synchronization overhead.

### 6.3 Horizontal Scaling (Multiple Machines)

#### Architecture

```
                    Load Balancer (HAProxy)
                             |
          +------------------+------------------+
          |                  |                  |
     RawrXD Node 1      RawrXD Node 2      RawrXD Node 3
    (GPU 1: Q4_K)      (GPU 2: Q4_K)      (GPU 3: Q4_K)
     79.97 TPS          79.97 TPS          79.97 TPS
         |                  |                  |
         +------------------+------------------+
                  Aggregate: ~220 TPS
```

#### HAProxy Configuration (`haproxy.cfg`)

```conf
frontend rawrxd-frontend
    bind *:8080
    default_backend rawrxd-backends

backend rawrxd-backends
    balance roundrobin
    option httpchk GET /health
    server node1 192.168.1.101:8080 check
    server node2 192.168.1.102:8080 check
    server node3 192.168.1.103:8080 check
```

#### Health Check Endpoint

```cpp
// In RawrXD source (example)
router.get("/health", [](Request req, Response res) {
    if (gpu_available() && model_loaded()) {
        res.status(200).send("OK");
    } else {
        res.status(503).send("Service Unavailable");
    }
});
```

---

## 7. SECURITY CONSIDERATIONS

### 7.1 Network Security

#### Firewall Configuration

```powershell
# Allow RawrXD IDE on port 8080 (example)
New-NetFirewallRule -DisplayName "RawrXD IDE" -Direction Inbound -LocalPort 8080 -Protocol TCP -Action Allow

# Restrict to internal network only
New-NetFirewallRule -DisplayName "RawrXD IDE (Internal)" -Direction Inbound -LocalPort 8080 -Protocol TCP -Action Allow -RemoteAddress "10.0.0.0/8"
```

### 7.2 Authentication (Example)

```json
{
  "security": {
    "authentication": {
      "enabled": true,
      "type": "api_key",  // or "oauth2", "jwt"
      "api_keys": [
        "sk-rawrxd-abc123...",
        "sk-rawrxd-def456..."
      ]
    }
  }
}
```

### 7.3 Rate Limiting

```json
{
  "security": {
    "rate_limiting": {
      "enabled": true,
      "max_requests_per_minute": 100,
      "max_tokens_per_hour": 1000000
    }
  }
}
```

---

## 8. BACKUP & DISASTER RECOVERY

### 8.1 Configuration Backup

```powershell
# Backup configuration
$backupDir = "C:\RawrXD\backups\$(Get-Date -Format 'yyyyMMdd-HHmmss')"
New-Item -ItemType Directory -Force -Path $backupDir
Copy-Item "C:\RawrXD\config\*" -Destination $backupDir -Recurse
```

### 8.2 Model Recovery

```powershell
# Verify model integrity
Get-FileHash "C:\RawrXD\models\bigdaddyg-q4_k.gguf" -Algorithm SHA256

# Restore from backup (if corrupted)
Copy-Item "C:\RawrXD\backups\models\bigdaddyg-q4_k.gguf" -Destination "C:\RawrXD\models\" -Force
```

### 8.3 Log Rotation

```json
{
  "logging": {
    "max_size_mb": 100,
    "rotate_count": 10,  // Keep 10 rotated logs (1 GB total)
    "compress": true     // Compress rotated logs (gzip)
  }
}
```

---

## 🎯 PRODUCTION CHECKLIST

Before going live, verify:

- [ ] GPU drivers updated (Vulkan 1.4+)
- [ ] Model downloaded and checksum verified
- [ ] Configuration file validated (JSON syntax)
- [ ] VRAM limit set appropriately (14-16 GB for Q4_K)
- [ ] Logging enabled (structured JSON logs)
- [ ] Monitoring script running (metrics-collector.ps1)
- [ ] Alerts configured (email or Prometheus)
- [ ] Firewall rules configured (port 8080)
- [ ] Backup script scheduled (daily)
- [ ] Health check endpoint tested
- [ ] Load testing completed (8-12 concurrent users)
- [ ] Disaster recovery plan documented

---

## 📚 RELATED DOCUMENTS

- **BENCHMARK_VISUAL_SUMMARY.txt** - Performance metrics reference
- **Q2K_vs_Q4K_BENCHMARK_REPORT.md** - Detailed benchmark data
- **PERFORMANCE_TRADE_OFF_ANALYSIS.md** - Quantization selection guide
- **PERFORMANCE_SLA_SPECIFICATION.md** - SLA targets & guarantees
- **EXECUTIVE_SUMMARY.md** - High-level project overview

---

**Document Status:** v1.0 (Production Release)  
**Last Updated:** January 1, 2026  
**Maintained By:** DevOps Team  
**Contact:** devops@rawrxd.com

---

**END OF DEPLOYMENT GUIDE**
