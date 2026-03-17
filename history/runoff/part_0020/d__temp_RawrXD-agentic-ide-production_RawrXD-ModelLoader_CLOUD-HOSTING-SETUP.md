# Instructions: Push to New GitHub Repository "cloud-hosting"

## Step 1: Create New Repository on GitHub

1. Go to https://github.com/new
2. **Repository name:** `cloud-hosting`
3. **Description:** `DigitalOcean deployment for RawrXD quantum GGUF models`
4. **Visibility:** Public (for GitHub Actions, DigitalOcean integration)
5. **Initialize:** ✗ (we'll push existing code)
6. Click **Create repository**

## Step 2: Prepare Files Locally

All necessary files have been created in:
```
D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\
```

Structure:
```
deploy/
├── docker/
│   ├── Dockerfile
│   └── docker-compose.yml
├── scripts/
│   ├── upload-to-spaces.sh
│   ├── deploy-to-digitalocean.sh
│   └── local-dev-setup.sh
├── terraform/
│   ├── main.tf
│   ├── variables.tf
│   └── user_data.sh
└── README-CLOUD-HOSTING.md

.do/
└── app.yaml

.github/workflows/
└── deploy-digitalocean.yml
```

## Step 3: Create New Local Git Repo for Cloud Hosting

```powershell
# Create fresh directory for cloud-hosting repo
mkdir C:\cloud-hosting
cd C:\cloud-hosting

# Initialize git
git init
git config user.name "Your Name"
git config user.email "your.email@example.com"
```

## Step 4: Copy Files to New Repo

```powershell
# Copy deploy directory
Copy-Item -Recurse "D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\deploy" -Destination "C:\cloud-hosting\"

# Copy .do directory
Copy-Item -Recurse "D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\.do" -Destination "C:\cloud-hosting\"

# Copy .github workflows
Copy-Item -Recurse "D:\temp\RawrXD-agentic-ide-production\RawrXD-ModelLoader\.github" -Destination "C:\cloud-hosting\"
```

## Step 5: Create Root README

Create `C:\cloud-hosting\README.md`:

```markdown
# Cloud Hosting for RawrXD Quantum Models

Deploy 2GB quantum GGUF models to DigitalOcean using Spaces + Droplets.

**Cost: $11/month | Free tier: 18 months on $200 credit**

## Quick Start

1. **Create DigitalOcean account** → https://digitalocean.com
2. **Grab $200 credit** (60 days) or enable billing
3. **Generate API token** → Account → API → Tokens
4. **Fork this repo** → Your GitHub account
5. **Deploy:**
   - Option A: App Platform (easiest) → See `.do/app.yaml`
   - Option B: Terraform → See `deploy/terraform/`
   - Option C: Manual SSH → See `deploy/scripts/`

## Documentation

- Full guide: [`deploy/README-CLOUD-HOSTING.md`](deploy/README-CLOUD-HOSTING.md)
- Deployment scripts: [`deploy/scripts/`](deploy/scripts/)
- Infrastructure as Code: [`deploy/terraform/`](deploy/terraform/)
- Docker image: [`deploy/docker/Dockerfile`](deploy/docker/Dockerfile)

## Architecture

```
┌─────────────────┐
│  GitHub Repo    │
│ (This repo)     │
└────────┬────────┘
         │
    ┌────▼────────────────────┐
    │ DigitalOcean App        │
    │ Platform                │
    │ (Auto-deploys on push)  │
    └────┬────────────────────┘
         │
    ┌────▼──────────────────────────┐
    │                               │
    │ ┌──────────────┐  ┌─────────┐│
    │ │ Droplet      │  │ Spaces  ││
    │ │ (API Server) │  │ (CDN)   ││
    │ │ $6/mo        │  │ $5/mo   ││
    │ └──────────────┘  └─────────┘│
    │                               │
    │ llama.cpp server              │
    │ 2GB quantum models            │
    │ 8080 API port                 │
    └───────────────────────────────┘
```

## GitHub Actions CI/CD

When you push to `main` or `production-lazy-init`:
1. Docker image built and pushed to GHCR
2. DigitalOcean App Platform auto-deploys
3. Monitor at https://cloud.digitalocean.com/apps

**Required GitHub Secrets:**
- `DIGITALOCEAN_ACCESS_TOKEN` (from DigitalOcean account)

## Manual Deployment

```bash
# SSH deploy to existing droplet
bash deploy/scripts/deploy-to-digitalocean.sh <droplet-ip>

# Local testing
bash deploy/scripts/local-dev-setup.sh

# Upload models to Spaces
bash deploy/scripts/upload-to-spaces.sh
```

## API Endpoints

```bash
# Health check
curl http://<droplet-ip>:8080/health

# Completions
curl -X POST http://<droplet-ip>:8080/v1/completions \
  -H "Content-Type: application/json" \
  -d '{"prompt": "Hello", "n_predict": 128}'

# Chat
curl -X POST http://<droplet-ip>:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"messages": [{"role": "user", "content": "Hello"}]}'

# Full API docs
http://<droplet-ip>:8080/docs
```

## Cost Breakdown

| Item | Cost | Notes |
|------|------|-------|
| Spaces | $5/mo | 250 GB included, 1 TB free egress, CDN auto |
| Droplet | $6/mo | 1 vCPU, 1 GB RAM, no backups |
| **Total** | **$11/mo** | **$200 credit = 18 months free** |

## Scaling

| Setup | Droplets | RAM | Cost/mo | Req/s |
|-------|----------|-----|---------|-------|
| Dev | 1 | 1 GB | $11 | 10-20 |
| Prod | 5 | 1 GB | $35 | 100+ |
| Large Models | 1 | 16 GB | $95 | 5-10 |

## Links

- DigitalOcean: https://digitalocean.com
- llama.cpp: https://github.com/ggerganov/llama.cpp
- Terraform: https://terraform.io

## License

Same as RawrXD parent project
```

## Step 6: Create .gitignore

Create `C:\cloud-hosting\.gitignore`:

```
# Local development
deploy/models/
deploy/cache/
models/
cache/

# Docker
.env.local
docker-compose.override.yml

# Terraform
.terraform/
.terraform.lock.hcl
terraform.tfstate
terraform.tfstate.backup
*.tfvars
!example.tfvars

# OS
.DS_Store
Thumbs.db
*.swp

# Build artifacts
*.o
*.out
build/
dist/
```

## Step 7: Add and Commit Files

```powershell
cd C:\cloud-hosting

# Add all files
git add .

# Commit
git commit -m "Initial cloud hosting setup for RawrXD quantum models

- DigitalOcean App Platform configuration (.do/app.yaml)
- Terraform infrastructure as code (deploy/terraform/)
- Deployment scripts (deploy/scripts/)
- Docker image with llama.cpp server (deploy/docker/)
- GitHub Actions CI/CD workflow
- Complete documentation

Cost: \$11/month (Spaces \$5 + Droplet \$6)
Free tier: 18 months on \$200 DigitalOcean credit"
```

## Step 8: Add Remote and Push

```powershell
# Add remote (replace YOUR_USERNAME)
git remote add origin https://github.com/YOUR_USERNAME/cloud-hosting.git

# Rename branch to main
git branch -M main

# Push to GitHub
git push -u origin main
```

## Step 9: Configure GitHub Repository Settings

1. Go to https://github.com/YOUR_USERNAME/cloud-hosting/settings
2. **Secrets and variables** → **Actions**
3. **New repository secret**
   - Name: `DIGITALOCEAN_ACCESS_TOKEN`
   - Value: Your DigitalOcean API token (from Account → API → Tokens)

4. **Deploy key** (optional, for Terraform)
   - Settings → Deploy keys
   - Add your SSH public key for automated deployments

## Step 10: Set Up DigitalOcean Integration

### Option A: App Platform (Easiest)
1. Go to https://cloud.digitalocean.com/apps
2. **Create App** → **GitHub**
3. Select repository: `your-username/cloud-hosting`
4. Branch: `main`
5. Source directory: leave empty (auto-detects `.do/app.yaml`)
6. **Next** → Review spec → **Deploy**

### Option B: Terraform
1. Generate DigitalOcean API token (Account → API → Tokens)
2. Create `deploy/terraform/terraform.tfvars`:
   ```hcl
   do_token          = "your_token_here"
   do_spaces_key     = "your_spaces_key"
   do_spaces_secret  = "your_spaces_secret"
   ssh_public_key    = "ssh-rsa AAAA..."
   allowed_ssh_ips   = ["YOUR_IP/32"]
   ```
3. Run:
   ```bash
   cd deploy/terraform
   terraform init
   terraform plan
   terraform apply
   ```

## Step 11: Deploy!

### Via GitHub Push
```powershell
# Make a change to any file in deploy/ or .do/
# GitHub Actions will automatically:
# 1. Build Docker image
# 2. Push to GHCR
# 3. Deploy to DigitalOcean App Platform
git push origin main
```

### Via DigitalOcean Dashboard
1. Go to Apps → Your App
2. Deployments tab
3. Manually trigger or wait for push

### Via Terraform
```bash
cd deploy/terraform
terraform apply
```

## Verify Deployment

```bash
# Health check
curl http://<your-droplet-ip>:8080/health

# API response
curl -X POST http://<your-droplet-ip>:8080/v1/completions \
  -H "Content-Type: application/json" \
  -d '{"prompt": "Test", "n_predict": 10}'

# View logs (App Platform)
# Dashboard → Apps → Your App → Logs

# View logs (Terraform/Manual)
ssh root@<droplet-ip>
docker logs -f llama-server
```

## Next Steps

- [ ] Create GitHub repo `cloud-hosting`
- [ ] Copy files from `D:\...\deploy\` to local repo
- [ ] Update GitHub secrets with DigitalOcean token
- [ ] Test local setup: `bash deploy/scripts/local-dev-setup.sh`
- [ ] Upload models to Spaces: `bash deploy/scripts/upload-to-spaces.sh`
- [ ] Push to GitHub
- [ ] Deploy via App Platform or Terraform
- [ ] Test API endpoints
- [ ] Monitor costs in DigitalOcean dashboard

---

**Questions?**
- Review `deploy/README-CLOUD-HOSTING.md` for detailed setup
- Check DigitalOcean docs: https://docs.digitalocean.com
- llama.cpp server docs: https://github.com/ggerganov/llama.cpp/tree/master/examples/server
