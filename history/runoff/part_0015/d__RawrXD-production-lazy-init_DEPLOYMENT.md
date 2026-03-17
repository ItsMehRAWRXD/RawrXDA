# Production Deployment Guide

## Quick Start

### Local Development
```powershell
# Set environment
$env:RAWRXD_ENV = "development"

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --target RawrXD-AgenticIDE

# Run
.\build\bin\Debug\RawrXD-AgenticIDE.exe
```

### Docker Production Deployment
```bash
# Build image
docker build -t rawrxd-agentic-ide:latest .

# Run with docker-compose
docker-compose up -d

# View logs
docker-compose logs -f rawrxd-agentic-ide

# Check health
curl http://localhost:9090/metrics

# Stop
docker-compose down
```

### Kubernetes Deployment
```bash
# Apply manifests
kubectl apply -f k8s/

# Check status
kubectl get pods -l app=rawrxd-agentic-ide

# Port forward for metrics
kubectl port-forward svc/rawrxd-metrics 9090:9090

# View logs
kubectl logs -f deployment/rawrxd-agentic-ide
```

## Configuration

### Environment Variables
- `RAWRXD_ENV`: Environment name (`production`, `development`, `staging`)
- `LOG_LEVEL`: Logging level (`DEBUG`, `INFO`, `WARN`, `ERROR`)

### Configuration Files
- `config/production.json`: Production settings
- `config/development.json`: Development settings
- `config/prometheus.yml`: Prometheus scrape config

### Feature Flags
Edit `config/production.json` to enable/disable features:
```json
{
  "features": {
    "tier2_ml_error_detection": true,
    "tier2_model_hotpatching": true,
    "tier2_distributed_tracing": true,
    "tier2_performance_monitoring": true
  }
}
```

## Monitoring

### Prometheus Metrics
- **Endpoint**: `http://localhost:9090/metrics`
- **Metrics**:
  - `rawrxd_errors_total`: Total errors detected
  - `rawrxd_model_swaps_total`: Model hotpatch operations
  - `rawrxd_inference_duration_seconds`: Inference latency
  - `rawrxd_memory_usage_bytes`: Memory consumption

### Grafana Dashboards
- **URL**: `http://localhost:3000`
- **Credentials**: `admin / admin`
- **Dashboards**:
  - Application Performance
  - Error Detection Analytics
  - Model Hotpatching Status
  - Resource Utilization

### Health Checks
```bash
# Application health
curl http://localhost:9090/health

# Readiness probe
curl http://localhost:9090/ready

# Liveness probe
curl http://localhost:9090/alive
```

## Resource Limits

### Recommended Settings
- **Memory**: 8 GB (limit), 4 GB (request)
- **CPU**: 4 cores (limit), 2 cores (request)
- **Disk**: 50 GB for models and logs

### Kubernetes Resource Quotas
```yaml
resources:
  limits:
    memory: "8Gi"
    cpu: "4000m"
  requests:
    memory: "4Gi"
    cpu: "2000m"
```

## Troubleshooting

### Build Issues
```powershell
# Clean build
Remove-Item -Recurse -Force build
cmake -B build
cmake --build build --config Release
```

### Runtime Issues
```bash
# Check logs
tail -f logs/production.log

# Verify config
cat config/production.json

# Test connectivity
curl http://localhost:9090/metrics
```

### Docker Issues
```bash
# View container logs
docker logs rawrxd-production

# Enter container
docker exec -it rawrxd-production /bin/bash

# Restart services
docker-compose restart
```

## Production Checklist

- [ ] External configuration loaded (`config/production.json`)
- [ ] Structured logging enabled (JSON format)
- [ ] Prometheus metrics exported (port 9090)
- [ ] Distributed tracing configured (OTLP endpoint)
- [ ] Resource limits set (8GB memory, 80% CPU)
- [ ] Health checks passing (60s interval)
- [ ] Automatic rollback enabled for hotpatching
- [ ] Error detection model loaded
- [ ] Monitoring dashboards configured
- [ ] Backup strategy implemented

## Security

### Container Security
- Runs as non-root user (`rawrxd:1000`)
- Read-only root filesystem (where applicable)
- No privileged mode required
- Minimal base image (Ubuntu 22.04)

### Network Security
- Only expose necessary ports (9090 for metrics)
- Use TLS for external endpoints
- Firewall rules for production deployment

## Performance Tuning

### Optimization Flags
- `-DCMAKE_BUILD_TYPE=Release`: Production optimizations
- `-O2` or `-O3`: Compiler optimizations
- AVX2 disabled globally for portability

### Model Cache
- Max cached models: 5 (configurable)
- LRU eviction policy
- Preload on startup option

## Maintenance

### Log Rotation
- Max file size: 100 MB
- Rotation count: 10
- Automatic cleanup after 7 days

### Metrics Retention
- Prometheus: 7 days
- Grafana: 30 days (configurable)

### Backup Schedule
- Configuration: Daily
- Models: Weekly
- Logs: Continuous streaming to external storage
