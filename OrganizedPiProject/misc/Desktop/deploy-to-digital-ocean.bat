@echo off
echo 🌞 Deploying Sunshine Engine to Digital Ocean...

REM Set your tokens
set DIGITAL_OCEAN_TOKEN=dop_v1_1c39df1a2ba010a0e0ed68a7ace53b045d4ee64ee0b9bac03d2cc99f81a67293
set GITHUB_TOKEN=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

echo 🔐 Setting up Digital Ocean authentication...
doctl auth init --access-token %DIGITAL_OCEAN_TOKEN%

echo 🚀 Creating Digital Ocean droplet...
doctl compute droplet create sunshine-engine-bot ^
    --size s-1vcpu-1gb ^
    --region nyc3 ^
    --image docker-20-04 ^
    --ssh-keys $(doctl compute ssh-key list --format ID --no-header | head -1) ^
    --format ID --no-header

echo ⏳ Waiting for droplet to be ready...
doctl compute droplet wait sunshine-engine-bot

echo 🌐 Getting droplet IP...
for /f %%i in ('doctl compute droplet get sunshine-engine-bot --format PublicIPv4 --no-header') do set DROPLET_IP=%%i

echo 📦 Deploying Sunshine Engine to %DROPLET_IP%...
ssh -o StrictHostKeyChecking=no root@%DROPLET_IP% "apt-get update && apt-get install -y git docker.io docker-compose && systemctl start docker && systemctl enable docker && git clone https://github.com/yourusername/sunshine-engine.git /app && cd /app && docker-compose up -d --build && ufw allow 22 && ufw allow 80 && ufw allow 443 && ufw --force enable"

echo ✅ Deployment complete!
echo 🌐 Your Sunshine Engine HTTP Bot is running at: http://%DROPLET_IP%
echo 📊 API endpoints:
echo    - Health: http://%DROPLET_IP%/health
echo    - API: http://%DROPLET_IP%/api/
echo    - Generated content: http://%DROPLET_IP%/generated/

echo 🎉 Sunshine Engine is now live on Digital Ocean!
pause
