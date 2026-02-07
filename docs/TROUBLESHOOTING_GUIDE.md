# RawrXD Troubleshooting Guide

**Version:** 2.0  
**Date:** December 8, 2025

---

## Table of Contents

1. [Distributed Training Issues](#distributed-training-issues)
2. [Security & Authentication](#security--authentication)
3. [GPU & Hardware](#gpu--hardware)
4. [Performance Problems](#performance-problems)
5. [Error Code Reference](#error-code-reference)
6. [Debugging Tools](#debugging-tools)

---

## Distributed Training Issues

### Problem: Initialization Hangs on Specific Rank

**Symptoms:**
- One or more workers hang during `Initialize()`
- No error messages
- Process never completes

**Diagnosis:**
```bash
# Check network connectivity between nodes
ping 192.168.1.100  # Master node

# Verify port is open
telnet 192.168.1.100 29500

# Check firewall rules
sudo iptables -L | grep 29500
```

**Solutions:**

1. **Network Issue:**
   ```bash
   # Allow traffic on master port
   sudo ufw allow 29500/tcp
   
   # Or disable firewall temporarily for testing
   sudo ufw disable
   ```

2. **Wrong Master Address:**
   ```json
   {
     "process_group": {
       "master_addr": "192.168.1.100",  // Use IP, not hostname
       "master_port": 29500
     }
   }
   ```

3. **Mismatched World Size:**
   - Ensure all workers use same `world_size`
   - Check that ranks are 0 to (world_size - 1)

4. **NCCL Version Mismatch:**
   ```bash
   # Check NCCL version on all nodes
   python -c "import torch; print(torch.cuda.nccl.version())"
   
   # Should be identical across all nodes
   ```

---

### Problem: "NCCL Error: Unhandled system error"

**Symptoms:**
- Training crashes with NCCL error
- Often during `allreduce` operation

**Diagnosis:**
```bash
# Enable NCCL debug logging
export NCCL_DEBUG=INFO
export NCCL_DEBUG_SUBSYS=ALL

# Run training and check output
./RawrXD --config config/training.json 2>&1 | grep NCCL
```

**Solutions:**

1. **Insufficient Shared Memory:**
   ```bash
   # Increase shared memory size
   docker run --shm-size=8g ...
   
   # Or on bare metal
   sudo mount -o remount,size=8G /dev/shm
   ```

2. **Incompatible CUDA/Driver:**
   ```bash
   # Check compatibility
   nvidia-smi
   nvcc --version
   
   # Ensure driver >= 450.80 for CUDA 11.x
   ```

3. **GPU P2P Issues:**
   ```bash
   # Disable P2P if problematic
   export NCCL_P2P_DISABLE=1
   ```

---

### Problem: Slow Gradient Synchronization

**Symptoms:**
- `gradientsSynchronized` signal reports >100ms sync time
- Training throughput < 85% of single-GPU

**Diagnosis:**
```cpp
// Enable profiling
DistributedTrainer::TrainerConfig config;
config.pgConfig.enableProfiling = true;

// Check metrics
QJsonObject metrics = trainer.GetMetrics();
qInfo() << "Sync time:" << metrics["last_sync_time_ms"].toDouble();
```

**Solutions:**

1. **Use Gradient Compression:**
   ```json
   {
     "compression": "TopK",
     "compression_ratio": 0.05
   }
   ```

2. **Increase Gradient Accumulation:**
   ```json
   {
     "gradient_accumulation_steps": 8
   }
   ```

3. **Check Network Bandwidth:**
   ```bash
   # Test bandwidth between nodes
   iperf3 -s  # On master node
   iperf3 -c 192.168.1.100 -t 30  # On worker node
   
   # Should see >1 Gbps for good performance
   ```

---

### Problem: "Node failure detected" with Fault Tolerance Enabled

**Symptoms:**
- Worker crashes or becomes unresponsive
- Training continues but warns about node failure

**Diagnosis:**
```cpp
// Check which node failed
connect(&trainer, &DistributedTrainer::nodeRecovered, 
        [](int rank) {
    qInfo() << "Node recovered:" << rank;
});

// Get node metrics
auto nodes = trainer.GetNodePerformance();
for (const auto& node : nodes) {
    qInfo() << "Node" << node.rank << "throughput:" << node.throughput;
}
```

**Solutions:**

1. **Increase Timeout:**
   ```json
   {
     "fault_tolerance": {
       "worker_timeout_ms": 60000,  // Increase from 30s to 60s
       "max_retries": 5
     }
   }
   ```

2. **Check Worker Logs:**
   ```bash
   # On failed worker
   tail -f /var/log/rawrxd/app.log | grep ERROR
   ```

3. **Verify Heartbeat:**
   ```json
   {
     "fault_tolerance": {
       "heartbeat_interval_ms": 3000,  // Decrease for faster detection
       "auto_recover": true
     }
   }
   ```

---

## Security & Authentication

### Problem: "Decryption failed" Error

**Symptoms:**
- Cannot decrypt stored credentials
- Error: "Authentication tag verification failure"

**Diagnosis:**
```cpp
SecurityManager* sec = SecurityManager::getInstance();
if (!sec->validateSetup()) {
    qCritical() << "Security validation failed";
}

QJsonObject config = sec->getConfiguration();
qInfo() << "Current key:" << config["current_key_id"].toString();
```

**Solutions:**

1. **Wrong Master Password:**
   ```bash
   # Reset security (WARNING: loses all stored credentials)
   ./RawrXD --reset-security --master-password "NewPassword123!"
   ```

2. **Corrupted Encryption Key:**
   ```bash
   # Rotate to new key
   ./RawrXD --rotate-key
   ```

3. **Data Corruption:**
   ```bash
   # Restore from backup
   cp /backup/credentials.db /var/lib/rawrxd/credentials.db
   ```

---

### Problem: "Access denied" for Valid User

**Symptoms:**
- User has credentials but gets access denied
- ACL should grant access but doesn't

**Diagnosis:**
```cpp
SecurityManager* sec = SecurityManager::getInstance();

// Check ACL entry
auto acl = sec->getResourceACL("models/training");
for (const auto& [user, level] : acl) {
    qInfo() << user << "has access level:" << static_cast<int>(level);
}

// Check specific access
bool hasAccess = sec->checkAccess(
    "user@example.com",
    "models/training",
    SecurityManager::AccessLevel::Write
);
qInfo() << "Has write access:" << hasAccess;
```

**Solutions:**

1. **Update ACL:**
   ```cpp
   sec->setAccessControl(
       "user@example.com",
       "models/training",
       SecurityManager::AccessLevel::Write
   );
   ```

2. **Check ACL File:**
   ```bash
   cat /etc/rawrxd/acl.json | jq '.acl.users'
   ```

3. **Reload ACL:**
   ```bash
   ./RawrXD --reload-acl
   ```

---

### Problem: OAuth2 Token Refresh Fails

**Symptoms:**
- Token expires
- Automatic refresh doesn't work
- User forced to re-authenticate

**Diagnosis:**
```cpp
SecurityManager* sec = SecurityManager::getInstance();

// Check token status
CredentialInfo cred = sec->getCredential("user@example.com");
qInfo() << "Issued at:" << cred.issuedAt;
qInfo() << "Expires at:" << cred.expiresAt;
qInfo() << "Is refreshable:" << cred.isRefreshable;

// Check if expired
bool expired = sec->isTokenExpired("user@example.com");
qInfo() << "Token expired:" << expired;
```

**Solutions:**

1. **Missing Refresh Token:**
   ```cpp
   // Store with refresh token
   sec->storeCredential(
       "user@example.com",
       accessToken,
       "bearer",
       expiresAt,
       refreshToken  // Must provide this!
   );
   ```

2. **OAuth2 Provider Unreachable:**
   ```bash
   # Test connectivity
   curl https://auth.example.com/oauth/token \
     -d "grant_type=refresh_token" \
     -d "refresh_token=..." \
     -d "client_id=..."
   ```

3. **Refresh Token Expired:**
   - Re-authenticate user
   - Some providers expire refresh tokens after 90 days

---

## GPU & Hardware

### Problem: "CUDA out of memory"

**Symptoms:**
- Training crashes with OOM error
- nvidia-smi shows 100% memory usage

**Diagnosis:**
```bash
# Check current usage
nvidia-smi

# Monitor in real-time
watch -n 1 nvidia-smi

# Check RawrXD memory usage
curl http://localhost:8888/metrics | grep vram
```

**Solutions:**

1. **Reduce Batch Size:**
   ```json
   {
     "performance": {
       "batch_size_per_worker": 16  // Reduce from 32
     }
   }
   ```

2. **Enable Gradient Checkpointing:**
   ```json
   {
     "gpu_optimization": {
       "gradient_checkpointing": true
     }
   }
   ```

3. **Use Mixed Precision:**
   ```json
   {
     "training": {
       "enable_auto_mixed_precision": true
     }
   }
   ```

4. **Clear KV Cache:**
   ```cpp
   InferenceEngine* engine = getEngine();
   engine->clearAllCaches();
   ```

---

### Problem: GPU Not Detected

**Symptoms:**
- Falls back to CPU
- `gpu_available: false` in health check

**Diagnosis:**
```bash
# Check NVIDIA driver
nvidia-smi

# Check CUDA installation
nvcc --version

# Check Qt CUDA support
./RawrXD --check-gpu
```

**Solutions:**

1. **Install/Update NVIDIA Driver:**
   ```bash
   # Ubuntu
   sudo ubuntu-drivers autoinstall
   sudo reboot
   
   # Check installation
   nvidia-smi
   ```

2. **Install CUDA Toolkit:**
   ```bash
   # Ubuntu 22.04
   wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-ubuntu2204.pin
   sudo mv cuda-ubuntu2204.pin /etc/apt/preferences.d/cuda-repository-pin-600
   sudo apt-key adv --fetch-keys https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/3bf863cc.pub
   sudo add-apt-repository "deb https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/ /"
   sudo apt update
   sudo apt install cuda
   ```

3. **Set CUDA_VISIBLE_DEVICES:**
   ```bash
   export CUDA_VISIBLE_DEVICES=0
   ./RawrXD
   ```

---

### Problem: Low GPU Utilization (<50%)

**Symptoms:**
- GPU usage stuck at 20-40%
- Training is slow
- CPU bottleneck suspected

**Diagnosis:**
```bash
# Check GPU utilization
nvidia-smi dmon -s u

# Check CPU usage
top -H

# Profile with RawrXD profiler
curl http://localhost:9090/metrics | grep throughput
```

**Solutions:**

1. **Increase Batch Size:**
   ```json
   {
     "performance": {
       "batch_size_per_worker": 64  // Increase
     }
   }
   ```

2. **Enable Data Prefetching:**
   ```json
   {
     "performance": {
       "num_workers": 8,
       "prefetch_factor": 4,
       "pin_memory": true
     }
   }
   ```

3. **Use Async GPU Copy:**
   ```json
   {
     "throughput": {
       "async_gpu_copy": true
     }
   }
   ```

---

## Performance Problems

### Problem: High Latency (P95 > 200ms)

**Symptoms:**
- Slow response times
- Users complain about lag

**Diagnosis:**
```cpp
// Check health metrics
InferenceEngine* engine = getEngine();
HealthStatus health = engine->getHealthStatus();
qInfo() << "P95 latency:" << health.p95_latency_ms;
qInfo() << "Pending requests:" << health.pending_requests;
```

**Solutions:**

1. **Reduce Batch Size:**
   ```cpp
   // Use batch size 1 for lowest latency
   engine->infer(prompt, 256);  // Single request
   ```

2. **Enable CUDA Graphs:**
   ```json
   {
     "inference_optimization": {
       "use_cuda_graphs": true
     }
   }
   ```

3. **Check Queue Depth:**
   ```cpp
   if (health.pending_requests > 50) {
       qWarning() << "Queue overload - scale horizontally";
   }
   ```

---

### Problem: Low Throughput (<500 req/sec)

**Symptoms:**
- Cannot handle load
- Server maxed out at low request rate

**Diagnosis:**
```cpp
Profiler* profiler = getProfiler();
profiler->startProfiling();

// After some time
auto snapshot = profiler->getSnapshot();
qInfo() << "Throughput:" << snapshot.throughputSamples << "samples/sec";
```

**Solutions:**

1. **Increase Batch Size:**
   ```json
   {
     "performance": {
       "batch_size_per_worker": 128
     }
   }
   ```

2. **Use Request Batching:**
   ```cpp
   // Queue multiple requests and process together
   QString reqId1 = engine->queueInferenceRequest(prompt1, 256);
   QString reqId2 = engine->queueInferenceRequest(prompt2, 256);
   QString reqId3 = engine->queueInferenceRequest(prompt3, 256);
   // Processed as batch internally
   ```

3. **Scale Horizontally:**
   - Deploy multiple RawrXD instances
   - Use load balancer (nginx, HAProxy)
   - Each instance handles 1000 req/sec

---

## Error Code Reference

| Code | Name | Cause | Solution |
|------|------|-------|----------|
| 4001 | MODEL_LOAD_FAILED | Model file not found or corrupted | Check file path and permissions |
| 4002 | INVALID_MODEL_PATH | Path doesn't exist | Verify path with `ls` |
| 4101 | TOKENIZER_NOT_INITIALIZED | Called before Initialize() | Call Initialize() first |
| 4102 | TOKENIZATION_FAILED | Invalid UTF-8 encoding | Check input encoding |
| 4201 | EMPTY_REQUEST | Empty prompt | Provide non-empty prompt |
| 4202 | PROMPT_TOO_LONG | Prompt > 100K chars | Truncate prompt |
| 4203 | INVALID_GENERATION_PARAMETERS | maxTokens out of range | Use 1-2048 range |
| 4301 | INSUFFICIENT_MEMORY | OOM on GPU or CPU | Reduce batch size |
| 4302 | REQUEST_QUEUE_FULL | Too many pending requests | Increase queue size or add workers |
| 4401 | TRANSFORMER_ERROR | Internal transformer error | Check logs, may need model reload |
| 4402 | INFERENCE_FAILURE | Inference crashed | Check GPU health, restart if needed |

---

## Debugging Tools

### Enable Debug Logging

```cpp
// In code
qSetMessagePattern("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{type}] %{message}");

// Or via environment
export QT_LOGGING_RULES="*.debug=true"
./RawrXD
```

### Profiler Usage

```cpp
Profiler* profiler = new Profiler();
profiler->startProfiling();

// Mark training phases
profiler->markPhaseStart("forwardPass");
// ... forward pass code ...
profiler->markPhaseEnd("forwardPass");

profiler->markPhaseStart("backwardPass");
// ... backward pass code ...
profiler->markPhaseEnd("backwardPass");

// Get snapshot
auto snapshot = profiler->getSnapshot();
qInfo() << "Forward pass:" << snapshot.forwardPassMs << "ms";
qInfo() << "Backward pass:" << snapshot.backwardPassMs << "ms";

// Export report
profiler->exportReport("/tmp/profiler_report.json");
```

### Network Debugging (Distributed)

```bash
# Trace NCCL calls
export NCCL_DEBUG=INFO
export NCCL_DEBUG_SUBSYS=INIT,ENV,COLL

# Capture network traffic
sudo tcpdump -i eth0 port 29500 -w /tmp/nccl.pcap

# Analyze with Wireshark
wireshark /tmp/nccl.pcap
```

### Memory Profiling

```bash
# Use nvidia-smi for GPU
nvidia-smi --query-gpu=timestamp,memory.used,memory.free --format=csv -l 1

# Use valgrind for CPU memory leaks
valgrind --leak-check=full --show-leak-kinds=all ./RawrXD

# Use heaptrack for detailed analysis
heaptrack ./RawrXD
heaptrack_gui heaptrack.RawrXD.*.gz
```

---

## Getting Help

### Information to Provide

When reporting issues, include:

1. **System Info:**
   ```bash
   ./RawrXD --version
   nvidia-smi
   cat /etc/os-release
   ```

2. **Configuration:**
   ```bash
   cat config/production.json
   ```

3. **Logs:**
   ```bash
   tail -n 500 /var/log/rawrxd/app.log > /tmp/rawrxd.log
   ```

4. **Metrics:**
   ```bash
   curl http://localhost:8888/health > /tmp/health.json
   curl http://localhost:9090/metrics > /tmp/metrics.txt
   ```

### Support Channels

- **GitHub Issues:** https://github.com/ItsMehRAWRXD/RawrXD/issues
- **Documentation:** https://rawrxd.dev/docs
- **Email:** support@rawrxd.dev
- **Discord:** https://discord.gg/rawrxd

**Last Updated:** December 8, 2025
