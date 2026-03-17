# Cloud Hosting Setup for RawrXD Quantum GGUF Models

This repository contains production-ready deployment configurations for hosting RawrXD's 2GB quantum GGUF models on DigitalOcean.

## Quick Start

### Prerequisites
- GitHub account
- DigitalOcean account with $200+ credit (or billing enabled)
- Docker (for local testing)

### Option 1: Deploy via DigitalOcean App Platform (Recommended)

1. **Fork/Clone this repository**
   ```bash
   git clone https://github.com/ItsMehRAWRXD/cloud-hosting.git
   cd cloud-hosting
   ```

2. **Connect to DigitalOcean**
   - Go to [cloud.digitalocean.com](https://cloud.digitalocean.com)
   - Apps → Create App → GitHub
   - Select this repository
   - Branch: `main` or `production-lazy-init`
   - DigitalOcean auto-detects `.do/app.yaml`
   - Review spec and click Deploy

3. **Configure Secrets in GitHub**
   Settings → Secrets and variables → Actions
   ```
   DIGITALOCEAN_ACCESS_TOKEN = your_do_api_token
   ```

4. **Upload Models to Spaces**
   ```bash
   bash scripts/upload-to-spaces.sh
   ```
   Environment variables needed:
   ```
   DIGITALOCEAN_TOKEN=your_token
   DO_SPACES_KEY=your_key
   DO_SPACES_SECRET=your_secret
   ```

### Option 2: Deploy via Terraform

```bash
cd terraform

# Initialize
terraform init

# Set variables
export TF_VAR_do_token="your_do_api_token"
export TF_VAR_do_spaces_key="your_spaces_key"
export TF_VAR_do_spaces_secret="your_spaces_secret"
export TF_VAR_ssh_public_key="$(cat ~/.ssh/id_rsa.pub)"

# Plan
terraform plan

# Deploy
terraform apply
```

**Output:**
```
Droplet IP: 192.0.2.1
API Endpoint: http://192.0.2.1:8080
Models Bucket: rawrxd-quantum-models
CDN Domain: https://rawrxd-quantum-models.nyc3.cdn.digitaloceanspaces.com
```

### Option 3: Manual SSH Deployment

```bash
# Set your droplet IP
DROPLET_IP=192.0.2.1

# Deploy
bash scripts/deploy-to-digitalocean.sh $DROPLET_IP
```

## Architecture

### Storage Layer
- **DigitalOcean Spaces** (S3-compatible object storage)
  - 250 GB included in $5/month tier
  - 1 TB free egress per month
  - Global CDN with auto TLS
  - Host up to 4 × 2GB quantum models

### Compute Layer
- **Basic Regular Droplet** (1 vCPU / 1 GB RAM / 25 GB SSD)
  - $6/month
  - Runs llama.cpp server CPU-only
  - Handles light API traffic (10-50 req/s)
  - 2 threads optimal for 1 vCPU

### Total Cost
| Component | Cost | Details |
|-----------|------|---------|
| Spaces | $5/mo | 250 GB included, 1 TB egress free |
| Droplet | $6/mo | 1 vCPU, 1 GB RAM, 25 GB SSD |
| **Total** | **$11/mo** | **18 months free on $200 credit** |

## API Usage

### Health Check
```bash
curl http://<api-ip>:8080/health
```

### Completion Request
```bash
curl -X POST http://<api-ip>:8080/v1/completions \
  -H "Content-Type: application/json" \
  -d '{
    "prompt": "Hello, how are you?",
    "n_predict": 128,
    "temperature": 0.7
  }'
```

### Chat Completion
```bash
curl -X POST http://<api-ip>:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "model": "quantum-2b-q4",
    "messages": [
      {"role": "user", "content": "Hello"}
    ],
    "temperature": 0.7
  }'
```

### API Documentation
Full OpenAPI spec available at: `http://<api-ip>:8080/docs`

## Local Development

Test locally before deploying:

```bash
# Setup
bash scripts/local-dev-setup.sh

# Wait for server to start, then test
curl http://localhost:8080/health

# View logs
docker-compose -f deploy/docker/docker-compose.yml logs -f

# Stop services
docker-compose -f deploy/docker/docker-compose.yml down
```

## Model Management

### Upload Models to Spaces

```bash
# Set credentials
export DO_SPACES_KEY="your_key"
export DO_SPACES_SECRET="your_secret"
export DIGITALOCEAN_TOKEN="your_token"

# Upload
bash scripts/upload-to-spaces.sh
```

Models are automatically cached at CDN edge locations globally.

### Download Models on Droplet

Models are auto-downloaded from Spaces CDN during deployment via `user_data.sh`.

To manually download:
```bash
ssh root@<droplet-ip>
cd /models
wget https://rawrxd-quantum-models.nyc3.cdn.digitaloceanspaces.com/models/quantum-2b-q4.gguf
```

## Monitoring

### View Logs
```bash
# App Platform (via DigitalOcean Dashboard)
# OR via GitHub Actions
# OR SSH to droplet
ssh root@<droplet-ip>
docker logs -f llama-server
```

### CPU/Memory Usage
```bash
ssh root@<droplet-ip>
docker stats llama-server
```

### Request Latency
```bash
curl -w "@curl-format.txt" -o /dev/null -s http://localhost:8080/health
```

## Scaling

### Current Setup (fits in $11/mo)
- 1 × 1 vCPU droplet ($6)
- Spaces storage ($5)
- Handles ~20 concurrent requests

### Scale to 5 Droplets (Still $11/mo)
- 5 × basic droplets = $30
- Spaces = $5
- **Total = $35** (well under $200 credit for 60 days)

### Scale for Larger Models (13B+)
| Model Size | Recommended Droplet | RAM Needed | Cost/mo |
|------------|-------------------|-----------|---------|
| 2-7 B | Basic (1 vCPU) | 1 GB | $6 |
| 13 B | GP (4 vCPU, 8 GB) | 8 GB | $60 |
| 30 B | MO (4 vCPU, 16 GB) | 16 GB | $90 |
| 70 B | MO (8 vCPU, 32 GB) | 32 GB | $160 |

## Troubleshooting

### Server won't start
```bash
ssh root@<droplet-ip>
docker logs llama-server
# Check /models directory exists and has models
ls -lh /models/
```

### Models not downloading
```bash
# Check Spaces CDN URL
curl -I https://rawrxd-quantum-models.nyc3.cdn.digitaloceanspaces.com/models/quantum-2b-q4.gguf
# Verify credentials in user_data.sh
```

### High API latency
```bash
# Check CPU usage
docker stats llama-server
# Reduce threads: --threads 1 for 1 vCPU
# Increase context swap if needed
```

## Files Structure

```
deploy/
├── docker/
│   ├── Dockerfile           # Multi-stage build for llama.cpp
│   └── docker-compose.yml   # Local dev environment
├── scripts/
│   ├── upload-to-spaces.sh            # Upload models to S3
│   ├── deploy-to-digitalocean.sh      # SSH deployment script
│   └── local-dev-setup.sh             # Local testing setup
└── terraform/
    ├── main.tf              # Infrastructure as Code
    ├── variables.tf         # Terraform variables
    └── user_data.sh         # Droplet initialization
.do/
└── app.yaml                 # DigitalOcean App Platform spec
.github/workflows/
└── deploy-digitalocean.yml  # GitHub Actions CI/CD
```

## Security Notes

- SSH firewall rules restrict access to your IPs by default
- Spaces bucket ACL set to `public-read` for CDN (models are public)
- Disable backups to save 20% on droplet costs
- Reserve IP for load balancing across multiple droplets

## Cost Calculator

```
Base: $11/month
  ├─ Spaces: $5 (250GB included)
  └─ Droplet: $6 (1vCPU, 1GB RAM, 25GB SSD)

With $200 DigitalOcean credit:
  $200 ÷ $11 = 18.18 months FREE

To maximize credit:
  - Scale to 5 droplets ($30/mo) + Spaces ($5/mo) = $35/mo
  - $200 ÷ $35 = 5.7 months of 5 servers
  - Or 18 months of 1 server
```

## Next Steps

1. [ ] Fork/clone this repository
2. [ ] Create DigitalOcean account + grab $200 credit
3. [ ] Generate DigitalOcean API token
4. [ ] Upload 2GB quantum models to Spaces
5. [ ] Connect GitHub repo to DigitalOcean App Platform
6. [ ] Deploy via GitHub push or manual `terraform apply`
7. [ ] Test API endpoints
8. [ ] Monitor costs in DigitalOcean dashboard

## Support

- DigitalOcean Docs: https://docs.digitalocean.com
- llama.cpp Server: https://github.com/ggerganov/llama.cpp/tree/master/examples/server
- Terraform Docs: https://registry.terraform.io/providers/digitalocean/digitalocean/latest

## License

Same as RawrXD parent project
