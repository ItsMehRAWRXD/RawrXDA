# RawrXD Production Configuration Guide

**Version:** 2.0  
**Date:** December 8, 2025  
**Target:** Production Deployment

---

## Table of Contents

1. [Environment Setup](#environment-setup)
2. [Distributed Training Configuration](#distributed-training-configuration)
3. [Security Configuration](#security-configuration)
4. [Hardware Backend Configuration](#hardware-backend-configuration)
5. [Observability Configuration](#observability-configuration)
6. [Performance Tuning](#performance-tuning)
7. [Production Checklist](#production-checklist)

---

## Environment Setup

### System Requirements

**Minimum:**
- CPU: 8 cores, 3.0 GHz
- RAM: 16 GB
- GPU: NVIDIA GPU with 8GB VRAM (optional but recommended)
- Storage: 100 GB SSD
- OS: Windows 10/11, Ubuntu 20.04+

**Recommended for Production:**
- CPU: 16+ cores, 3.5 GHz
- RAM: 64 GB
- GPU: NVIDIA A100 (40GB) or V100 (32GB)
- Storage: 500 GB NVMe SSD
- OS: Ubuntu 22.04 LTS
- Network: 10 Gbps for multi-node training

### Environment Variables

```bash
# CUDA Configuration (for NVIDIA GPUs)
export CUDA_VISIBLE_DEVICES=0,1,2,3  # Use GPUs 0-3
export CUDA_DEVICE_ORDER=PCI_BUS_ID

# NCCL Configuration (for distributed training)
export NCCL_DEBUG=INFO
export NCCL_IB_DISABLE=0  # Enable InfiniBand (if available)
export NCCL_SOCKET_IFNAME=eth0  # Network interface
export NCCL_P2P_DISABLE=0  # Enable peer-to-peer transfers

# Security
export RAWRXD_MASTER_PASSWORD="YourSecurePassword123!"
export RAWRXD_KEY_ROTATION_DAYS=90

# Observability
export RAWRXD_LOG_LEVEL=INFO
export RAWRXD_METRICS_PORT=9090
export RAWRXD_PROFILING_ENABLED=true

# Model Path
export RAWRXD_MODEL_DIR=/models
export RAWRXD_CHECKPOINT_DIR=/checkpoints
```

---

## Distributed Training Configuration

### Single-Node Multi-GPU (NCCL)

**File:** `config/training_single_node.json`

```json
{
  "distributed_training": {
    "backend": "NCCL",
    "parallelism": "DataParallel",
    "compression": "None",
    
    "process_group": {
      "world_size": 4,
      "rank": 0,
      "local_rank": 0,
      "master_addr": "localhost",
      "master_port": 29500,
      "timeout": 30,
      "enable_profiling": true
    },
    
    "training": {
      "gradient_accumulation_steps": 4,
      "sync_interval": 1,
      "enable_load_balancing": true,
      "enable_fault_tolerance": false,
      "enable_auto_mixed_precision": true,
      "compression_ratio": 0.1
    },
    
    "checkpointing": {
      "checkpoint_dir": "/checkpoints",
      "checkpoint_interval_steps": 1000,
      "keep_last_n_checkpoints": 5
    },
    
    "performance": {
      "batch_size_per_worker": 32,
      "num_workers": 4,
      "pin_memory": true,
      "prefetch_factor": 2
    }
  }
}
```

**Launch Command:**
```bash
# Terminal 1 (GPU 0)
CUDA_VISIBLE_DEVICES=0 ./RawrXD --config config/training_single_node.json --rank 0

# Terminal 2 (GPU 1)
CUDA_VISIBLE_DEVICES=1 ./RawrXD --config config/training_single_node.json --rank 1

# Terminal 3 (GPU 2)
CUDA_VISIBLE_DEVICES=2 ./RawrXD --config config/training_single_node.json --rank 2

# Terminal 4 (GPU 3)
CUDA_VISIBLE_DEVICES=3 ./RawrXD --config config/training_single_node.json --rank 3
```

---

### Multi-Node Training (Gloo)

**File:** `config/training_multi_node.json`

```json
{
  "distributed_training": {
    "backend": "Gloo",
    "parallelism": "DataParallel",
    "compression": "TopK",
    
    "process_group": {
      "world_size": 8,
      "rank": 0,
      "local_rank": 0,
      "master_addr": "192.168.1.100",
      "master_port": 29500,
      "timeout": 60,
      "enable_profiling": true
    },
    
    "training": {
      "gradient_accumulation_steps": 8,
      "sync_interval": 1,
      "enable_load_balancing": true,
      "enable_fault_tolerance": true,
      "enable_auto_mixed_precision": true,
      "compression_ratio": 0.05
    },
    
    "fault_tolerance": {
      "heartbeat_interval_ms": 5000,
      "worker_timeout_ms": 30000,
      "max_retries": 3,
      "auto_recover": true
    }
  }
}
```

**Launch on Node 1 (Master):**
```bash
./RawrXD --config config/training_multi_node.json \
  --rank 0 --local-rank 0 \
  --master-addr 192.168.1.100 --master-port 29500
```

**Launch on Node 2 (Worker):**
```bash
./RawrXD --config config/training_multi_node.json \
  --rank 4 --local-rank 0 \
  --master-addr 192.168.1.100 --master-port 29500
```

---

## Security Configuration

### Basic Security Setup

**File:** `config/security.json`

```json
{
  "security": {
    "encryption": {
      "algorithm": "AES256_GCM",
      "key_derivation": {
        "method": "PBKDF2",
        "iterations": 100000,
        "salt_length": 32
      },
      "key_rotation": {
        "interval_days": 90,
        "auto_rotate": true,
        "backup_old_keys": true
      }
    },
    
    "authentication": {
      "oauth2": {
        "enabled": true,
        "provider": "auth0",
        "client_id": "YOUR_CLIENT_ID",
        "client_secret_encrypted": "ENCRYPTED_SECRET",
        "redirect_uri": "http://localhost:8080/callback",
        "scopes": ["openid", "profile", "email"]
      },
      "api_keys": {
        "enabled": true,
        "rotation_days": 30,
        "max_keys_per_user": 5
      }
    },
    
    "access_control": {
      "default_policy": "deny",
      "acl_file": "/etc/rawrxd/acl.json",
      "audit_all_access": true
    },
    
    "audit": {
      "enabled": true,
      "log_file": "/var/log/rawrxd/security_audit.log",
      "max_size_mb": 100,
      "rotation": "daily",
      "export_interval_hours": 24
    },
    
    "certificate_pinning": {
      "enabled": true,
      "pins": [
        {
          "domain": "api.rawrxd.dev",
          "sha256": "abcd1234..."
        }
      ]
    }
  }
}
```

### Setting Up Master Password

```bash
# Interactive setup (recommended)
./RawrXD --setup-security

# Or via environment variable
export RAWRXD_MASTER_PASSWORD="YourSecurePassword123!"
./RawrXD --init-security
```

### ACL Configuration

**File:** `/etc/rawrxd/acl.json`

```json
{
  "acl": {
    "users": [
      {
        "username": "admin@company.com",
        "resources": [
          {
            "path": "*",
            "level": "Admin"
          }
        ]
      },
      {
        "username": "data_scientist@company.com",
        "resources": [
          {
            "path": "models/*",
            "level": "ReadWrite"
          },
          {
            "path": "datasets/*",
            "level": "Read"
          }
        ]
      },
      {
        "username": "api_user",
        "resources": [
          {
            "path": "inference/predict",
            "level": "Execute"
          }
        ]
      }
    ]
  }
}
```

---

## Hardware Backend Configuration

### Auto-Detection

```json
{
  "hardware": {
    "auto_detect": true,
    "preferred_backend": "CUDA",
    "fallback_to_cpu": true,
    
    "cuda": {
      "device_ids": [0, 1, 2, 3],
      "memory_fraction": 0.9,
      "allow_growth": true
    },
    
    "vulkan": {
      "enabled": false,
      "device_index": 0
    },
    
    "cpu": {
      "threads": 16,
      "use_mkl": true
    }
  }
}
```

### Manual Backend Selection

```json
{
  "hardware": {
    "auto_detect": false,
    "backend": "CUDA",
    
    "cuda_config": {
      "device": 0,
      "precision": "fp16",
      "memory_pool_mb": 8192,
      "enable_tensor_cores": true,
      "enable_cuda_graphs": false
    }
  }
}
```

---

## Observability Configuration

### Metrics Collection

```json
{
  "observability": {
    "profiling": {
      "enabled": true,
      "sample_interval_ms": 500,
      "export_interval_seconds": 60
    },
    
    "metrics": {
      "prometheus": {
        "enabled": true,
        "port": 9090,
        "path": "/metrics"
      },
      "dashboard": {
        "enabled": true,
        "port": 8080,
        "refresh_interval_ms": 1000
      }
    },
    
    "tracing": {
      "enabled": true,
      "backend": "opentelemetry",
      "endpoint": "http://jaeger:14268/api/traces",
      "sample_rate": 0.1
    },
    
    "logging": {
      "level": "INFO",
      "file": "/var/log/rawrxd/app.log",
      "max_size_mb": 100,
      "max_files": 10,
      "json_format": true
    }
  }
}
```

### Health Check Endpoint

```json
{
  "health_check": {
    "enabled": true,
    "port": 8888,
    "endpoints": [
      {
        "path": "/health",
        "check_model": true,
        "check_gpu": true,
        "check_distributed": true
      },
      {
        "path": "/ready",
        "check_initialization": true
      },
      {
        "path": "/metrics",
        "format": "json"
      }
    ]
  }
}
```

**Health Check Response:**
```json
{
  "status": "healthy",
  "timestamp": "2025-12-08T10:30:00Z",
  "model_loaded": true,
  "gpu_available": true,
  "distributed_ready": true,
  "metrics": {
    "total_vram_mb": 40960,
    "used_vram_mb": 12345,
    "avg_latency_ms": 45.2,
    "p95_latency_ms": 89.5,
    "pending_requests": 3,
    "total_requests": 10523
  }
}
```

---

## Performance Tuning

### GPU Memory Optimization

```json
{
  "gpu_optimization": {
    "gradient_checkpointing": true,
    "mixed_precision": true,
    "memory_efficient_attention": true,
    
    "kv_cache": {
      "enabled": true,
      "max_tokens": 2048,
      "eviction_policy": "lru"
    },
    
    "batch_size": {
      "auto_tune": true,
      "min": 1,
      "max": 128,
      "target_vram_usage": 0.85
    }
  }
}
```

### Throughput Optimization

```json
{
  "throughput": {
    "prefetch_batches": 2,
    "async_gpu_copy": true,
    "pipeline_stages": 4,
    
    "inference_optimization": {
      "use_cuda_graphs": true,
      "fuse_operations": true,
      "quantization": "int8"
    }
  }
}
```

### Latency Optimization

```json
{
  "latency": {
    "priority": "low_latency",
    
    "optimizations": {
      "batch_size": 1,
      "use_compiled_model": true,
      "skip_layer_norm": false,
      "cache_past_key_values": true
    },
    
    "sla": {
      "p50_ms": 50,
      "p95_ms": 100,
      "p99_ms": 200
    }
  }
}
```

---

## Production Checklist

### Pre-Deployment

- [ ] Security configuration reviewed and tested
- [ ] Master password set (not using default)
- [ ] SSL/TLS certificates installed
- [ ] ACL configured for all users
- [ ] Audit logging enabled and tested
- [ ] Hardware backend detected correctly
- [ ] GPU memory limits configured
- [ ] Distributed training tested (if applicable)
- [ ] Health check endpoint accessible
- [ ] Monitoring dashboards configured
- [ ] Log aggregation set up
- [ ] Backup strategy defined
- [ ] Disaster recovery plan documented

### Post-Deployment

- [ ] Monitor GPU utilization (target: 80-95%)
- [ ] Check latency metrics (P95 < 100ms)
- [ ] Verify throughput meets requirements
- [ ] Review security audit logs daily
- [ ] Rotate encryption keys every 90 days
- [ ] Export audit logs monthly
- [ ] Test checkpoint restore procedure
- [ ] Validate backup integrity weekly
- [ ] Review error rates (target: < 0.1%)
- [ ] Check for failed nodes (distributed setup)

### Performance Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| Inference Latency (P50) | < 50ms | Health endpoint |
| Inference Latency (P95) | < 100ms | Health endpoint |
| Throughput | > 1000 req/sec | Profiler |
| GPU Utilization | 80-95% | nvidia-smi |
| Error Rate | < 0.1% | Metrics endpoint |
| Distributed Efficiency | > 85% | Training metrics |

---

## Example: Full Production Config

**File:** `config/production.json`

```json
{
  "application": {
    "name": "RawrXD Production",
    "version": "2.0.0",
    "environment": "production"
  },
  
  "hardware": {
    "auto_detect": true,
    "preferred_backend": "CUDA",
    "cuda": {
      "device_ids": [0, 1, 2, 3],
      "memory_fraction": 0.9,
      "enable_tensor_cores": true
    }
  },
  
  "distributed_training": {
    "backend": "NCCL",
    "parallelism": "DataParallel",
    "process_group": {
      "world_size": 4,
      "master_port": 29500
    },
    "training": {
      "gradient_accumulation_steps": 4,
      "enable_fault_tolerance": true,
      "enable_auto_mixed_precision": true
    }
  },
  
  "security": {
    "encryption": {
      "algorithm": "AES256_GCM",
      "key_rotation": {
        "interval_days": 90,
        "auto_rotate": true
      }
    },
    "authentication": {
      "oauth2": {
        "enabled": true
      }
    },
    "audit": {
      "enabled": true,
      "log_file": "/var/log/rawrxd/security_audit.log"
    }
  },
  
  "observability": {
    "profiling": {
      "enabled": true,
      "sample_interval_ms": 500
    },
    "metrics": {
      "prometheus": {
        "enabled": true,
        "port": 9090
      }
    },
    "logging": {
      "level": "INFO",
      "file": "/var/log/rawrxd/app.log",
      "json_format": true
    }
  },
  
  "performance": {
    "batch_size": 32,
    "mixed_precision": true,
    "gradient_checkpointing": true
  }
}
```

---

## Troubleshooting

See [TROUBLESHOOTING_GUIDE.md](./TROUBLESHOOTING_GUIDE.md) for common issues and solutions.

---

## Support

- **Documentation:** https://rawrxd.dev/docs
- **GitHub Issues:** https://github.com/ItsMehRAWRXD/RawrXD/issues
- **Email:** support@rawrxd.dev

**Last Updated:** December 8, 2025
