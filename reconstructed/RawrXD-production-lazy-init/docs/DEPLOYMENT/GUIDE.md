# RawrXD Enterprise Auto Model Loader - Deployment Guide

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Local Deployment](#local-deployment)
3. [Docker Deployment](#docker-deployment)
4. [Kubernetes Deployment](#kubernetes-deployment)
5. [Cloud Deployments](#cloud-deployments)
6. [Monitoring Setup](#monitoring-setup)
7. [Troubleshooting](#troubleshooting)

## Prerequisites

### Required Software

- **CMake**: Version 3.20 or higher
- **C++ Compiler**: MSVC 2019+, GCC 9+, or Clang 10+
- **Qt6**: For GUI applications
- **Vulkan SDK**: For GPU acceleration
- **OpenSSL**: For SHA256 hashing
- **Ollama** (Optional): For Ollama model support

### Recommended System Requirements

- **CPU**: 4+ cores (8+ recommended)
- **RAM**: 8GB minimum (16GB+ recommended)
- **Storage**: 50GB+ free space for models
- **OS**: Windows 10/11, Ubuntu 20.04+, or similar

## Local Deployment

### Windows

```powershell
# Clone repository
git clone https://github.com/yourusername/RawrXD.git
cd RawrXD

# Configure
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release --parallel 4

# Install
cmake --install build --prefix "C:\Program Files\RawrXD"

# Run
& "C:\Program Files\RawrXD\bin\RawrXD-CLI.exe"
```

### Linux

```bash
# Clone repository
git clone https://github.com/yourusername/RawrXD.git
cd RawrXD

# Install dependencies
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  libssl-dev \
  libvulkan-dev \
  qt6-base-dev

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release --parallel $(nproc)

# Install
sudo cmake --install build --prefix /usr/local

# Run
/usr/local/bin/RawrXD-CLI
```

### Configuration

Create or edit `model_loader_config.json`:

```json
{
  "autoLoadEnabled": true,
  "preferredModel": "codellama:latest",
  "searchPaths": [
    "/path/to/your/models",
    "D:/OllamaModels",
    "./models"
  ],
  "maxRetries": 3,
  "retryDelayMs": 1000,
  "logLevel": "INFO",
  "enableMetrics": true,
  "enableHealthChecks": true
}
```

## Docker Deployment

### Build Docker Image

```bash
# Build image
docker build -t rawrxd:latest .

# Run container
docker run -d \
  --name rawrxd-cli \
  -v /path/to/models:/app/models:ro \
  -v /path/to/config.json:/app/model_loader_config.json:ro \
  -p 8080:8080 \
  rawrxd:latest
```

### Docker Compose

```bash
# Start all services
docker-compose up -d

# View logs
docker-compose logs -f rawrxd-cli

# Stop services
docker-compose down
```

### Docker Configuration

Set environment variables:

```bash
docker run -d \
  -e MODEL_LOADER_LOG_LEVEL=DEBUG \
  -e MODEL_LOADER_ENABLE_METRICS=true \
  -e MODEL_LOADER_PREFERRED_MODEL=codellama:latest \
  rawrxd:latest
```

## Kubernetes Deployment

### Create Namespace

```bash
kubectl create namespace rawrxd
```

### Deploy Application

```yaml
# deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: rawrxd-cli
  namespace: rawrxd
spec:
  replicas: 2
  selector:
    matchLabels:
      app: rawrxd-cli
  template:
    metadata:
      labels:
        app: rawrxd-cli
    spec:
      containers:
      - name: rawrxd-cli
        image: rawrxd:latest
        env:
        - name: MODEL_LOADER_CONFIG
          value: /config/model_loader_config.json
        - name: MODEL_LOADER_LOG_LEVEL
          value: INFO
        volumeMounts:
        - name: config
          mountPath: /config
        - name: models
          mountPath: /app/models
        resources:
          limits:
            cpu: "4"
            memory: "8Gi"
          requests:
            cpu: "2"
            memory: "4Gi"
        livenessProbe:
          exec:
            command:
            - test
            - -f
            - /app/build/RawrXD-CLI
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          exec:
            command:
            - test
            - -f
            - /app/build/RawrXD-CLI
          initialDelaySeconds: 5
          periodSeconds: 5
      volumes:
      - name: config
        configMap:
          name: rawrxd-config
      - name: models
        persistentVolumeClaim:
          claimName: rawrxd-models-pvc
```

Apply deployment:

```bash
kubectl apply -f deployment.yaml
kubectl apply -f service.yaml
kubectl apply -f configmap.yaml
kubectl apply -f pvc.yaml
```

### Horizontal Pod Autoscaling

```yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: rawrxd-cli-hpa
  namespace: rawrxd
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: rawrxd-cli
  minReplicas: 2
  maxReplicas: 10
  metrics:
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 70
  - type: Resource
    resource:
      name: memory
      target:
        type: Utilization
        averageUtilization: 80
```

## Cloud Deployments

### AWS ECS

```json
{
  "family": "rawrxd-cli",
  "networkMode": "awsvpc",
  "requiresCompatibilities": ["FARGATE"],
  "cpu": "2048",
  "memory": "8192",
  "containerDefinitions": [
    {
      "name": "rawrxd-cli",
      "image": "rawrxd:latest",
      "essential": true,
      "environment": [
        {
          "name": "MODEL_LOADER_LOG_LEVEL",
          "value": "INFO"
        }
      ],
      "logConfiguration": {
        "logDriver": "awslogs",
        "options": {
          "awslogs-group": "/ecs/rawrxd-cli",
          "awslogs-region": "us-east-1",
          "awslogs-stream-prefix": "ecs"
        }
      }
    }
  ]
}
```

### Azure Container Instances

```bash
az container create \
  --resource-group rawrxd-rg \
  --name rawrxd-cli \
  --image rawrxd:latest \
  --cpu 4 \
  --memory 8 \
  --environment-variables \
    MODEL_LOADER_LOG_LEVEL=INFO \
    MODEL_LOADER_ENABLE_METRICS=true
```

### Google Cloud Run

```bash
gcloud run deploy rawrxd-cli \
  --image gcr.io/project-id/rawrxd:latest \
  --platform managed \
  --region us-central1 \
  --memory 8Gi \
  --cpu 4 \
  --set-env-vars MODEL_LOADER_LOG_LEVEL=INFO
```

## Monitoring Setup

### Prometheus

1. Deploy Prometheus:

```bash
kubectl apply -f monitoring/prometheus-deployment.yaml
```

2. Configure scraping:

```yaml
scrape_configs:
  - job_name: 'rawrxd-cli'
    kubernetes_sd_configs:
      - role: pod
        namespaces:
          names:
            - rawrxd
    relabel_configs:
      - source_labels: [__meta_kubernetes_pod_label_app]
        action: keep
        regex: rawrxd-cli
```

### Grafana

1. Deploy Grafana:

```bash
kubectl apply -f monitoring/grafana-deployment.yaml
```

2. Import dashboard:

- Open Grafana UI
- Go to Dashboards → Import
- Upload `monitoring/grafana/dashboards/rawrxd-dashboard.json`

### Alerting

Configure Alertmanager:

```yaml
route:
  receiver: 'rawrxd-alerts'
  routes:
    - match:
        severity: critical
      receiver: 'pagerduty'
    - match:
        severity: warning
      receiver: 'slack'

receivers:
  - name: 'rawrxd-alerts'
    email_configs:
      - to: 'ops@example.com'
  - name: 'slack'
    slack_configs:
      - api_url: 'SLACK_WEBHOOK_URL'
        channel: '#rawrxd-alerts'
  - name: 'pagerduty'
    pagerduty_configs:
      - service_key: 'PAGERDUTY_KEY'
```

## Troubleshooting

### Common Issues

#### Circuit Breaker Open

**Symptom**: All operations failing with "Circuit breaker OPEN"

**Solution**:
```bash
# Check logs
kubectl logs -n rawrxd deployment/rawrxd-cli

# Restart pods
kubectl rollout restart deployment/rawrxd-cli -n rawrxd

# Wait for circuit breaker timeout (default 60s)
```

#### High Memory Usage

**Symptom**: Pods being OOMKilled

**Solution**:
```bash
# Increase memory limits
kubectl edit deployment rawrxd-cli -n rawrxd

# Or scale down cache size in config
# Set maxCacheSize to lower value
```

#### Models Not Found

**Symptom**: "No models found for automatic loading"

**Solution**:
```bash
# Verify volume mounts
kubectl describe pod -n rawrxd -l app=rawrxd-cli

# Check search paths in config
kubectl get configmap rawrxd-config -n rawrxd -o yaml

# Verify Ollama is accessible
kubectl exec -it -n rawrxd deployment/rawrxd-cli -- ollama list
```

### Debug Mode

Enable debug logging:

```bash
# Via environment variable
kubectl set env deployment/rawrxd-cli -n rawrxd MODEL_LOADER_LOG_LEVEL=DEBUG

# Or update config
kubectl edit configmap rawrxd-config -n rawrxd
# Set logLevel: "DEBUG"

# Restart to apply
kubectl rollout restart deployment/rawrxd-cli -n rawrxd
```

### Performance Issues

Check metrics:

```bash
# Get Prometheus metrics
kubectl port-forward -n rawrxd svc/prometheus 9090:9090

# Open browser to http://localhost:9090
# Query: rate(model_loader_load_latency_microseconds_sum[5m])
```

Analyze bottlenecks:

```bash
# Run benchmark
kubectl exec -it -n rawrxd deployment/rawrxd-cli -- \
  powershell -File /app/scripts/benchmark_loader.ps1
```

## Security Hardening

### Network Policies

```yaml
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: rawrxd-cli-netpol
  namespace: rawrxd
spec:
  podSelector:
    matchLabels:
      app: rawrxd-cli
  policyTypes:
  - Ingress
  - Egress
  ingress:
  - from:
    - namespaceSelector:
        matchLabels:
          name: monitoring
    ports:
    - protocol: TCP
      port: 8080
  egress:
  - to:
    - namespaceSelector: {}
    ports:
    - protocol: TCP
      port: 443
```

### Pod Security Policy

```yaml
apiVersion: policy/v1beta1
kind: PodSecurityPolicy
metadata:
  name: rawrxd-psp
spec:
  privileged: false
  allowPrivilegeEscalation: false
  runAsUser:
    rule: 'MustRunAsNonRoot'
  seLinux:
    rule: 'RunAsAny'
  fsGroup:
    rule: 'RunAsAny'
  volumes:
  - 'configMap'
  - 'secret'
  - 'persistentVolumeClaim'
```

## Backup and Recovery

### Backup Configuration

```bash
# Backup ConfigMaps
kubectl get configmap -n rawrxd -o yaml > rawrxd-config-backup.yaml

# Backup Secrets
kubectl get secret -n rawrxd -o yaml > rawrxd-secrets-backup.yaml

# Backup PVCs
kubectl get pvc -n rawrxd -o yaml > rawrxd-pvc-backup.yaml
```

### Recovery

```bash
# Restore from backup
kubectl apply -f rawrxd-config-backup.yaml
kubectl apply -f rawrxd-secrets-backup.yaml
kubectl apply -f rawrxd-pvc-backup.yaml

# Restart deployment
kubectl rollout restart deployment/rawrxd-cli -n rawrxd
```

## Support

For additional help:

- **Documentation**: https://docs.rawrxd.com
- **GitHub Issues**: https://github.com/yourusername/RawrXD/issues
- **Community Forum**: https://community.rawrxd.com
- **Email**: support@rawrxd.com
