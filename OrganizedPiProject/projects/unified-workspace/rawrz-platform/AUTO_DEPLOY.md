# Automated GitHub + DigitalOcean Deployment

You need to manually create the GitHub repository first, then this script will handle everything.

## Step 1: Create GitHub Repository

1. Go to: https://github.com/new
2. Repository name: **`rawrz-http-encryptor`**
3. Description: **`RawrZ HTTP Encryption Platform - DigitalOcean Ready`**
4. Make it **Public** or **Private**
5. **DON'T** check "Initialize with README"
6. Click **"Create repository"**

## Step 2: After GitHub Repository is Created

Run this command to connect and push:

```bash
# Set up GitHub remote with token
git remote set-url origin https://ItsMehRAWRXD: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@github.com/ItsMehRAWRXD/rawrz-http-encryptor.git

# Push to GitHub
git push -u origin main
```

## Step 3: DigitalOcean Deployment

After GitHub push, you can deploy to DigitalOcean:

### Option A: Use Existing Droplet
```bash
# SSH to your DigitalOcean droplet
ssh root@YOUR_DROPLET_IP

# Clone and deploy
git clone https://ItsMehRAWRXD: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@github.com/ItsMehRAWRXD/rawrz-http-encryptor.git
cd rawrz-http-encryptor
chmod +x deploy.sh
./deploy.sh
```

### Option B: Create New Droplet (Automated)
```bash
# This will require doctl CLI setup
echo "For new droplet creation, use DigitalOcean interface or doctl CLI"
echo "DigitalOcean API Token: dop_v1_1c39df1a2ba010a0e0ed68a7ace53b045d4ee64ee0b9bac03d2cc99f81a67293"
```

## Step 4: Access Your Platform

After deployment, your RawrZ HTTP Encryptor will be live at:

- **Main Panel**: `http://YOUR_DROPLET_IP:8080/panel`
- **Encryption Interface**: `http://YOUR_DROPLET_IP:8080/encryptor`
- **Health Check**: `http://YOUR_DROPLET_IP:8080/health`

## Quick Setup Commands

```bash
# GitHub connection  
git remote set-url origin https://ItsMehRAWRXD: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@github.com/ItsMehRAWRXD/rawrz-http-encryptor.git
git push -u origin main

# DigitalOcean deployment
ssh root@YOUR_DROPLET

# On droplet:
git clone https://ItsMehRAWRXD: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@github.com/ItsMehRAWRXD/rawrz-http-encryptor.git
cd rawrz-http-encryptor
./deploy.sh
```

**DigitalOcean API Token**: `dop_v1_1c39df1a2ba010a0e0ed68a7ace53b045d4ee64ee0b9bac03d2cc99f81a67293`
**GitHub PAT**: `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`

---
**Next: Create GitHub repo first! 🚀**
