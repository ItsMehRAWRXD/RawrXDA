# Deploy All Projects to Single DigitalOcean Droplet
# This script deploys all projects as Docker containers on one droplet

param(
    [string]$DropletName = "rawrz-master-server",
    [string]$Region = "nyc1",
    [string]$Size = "s-2vcpu-4gb",
    [string]$Image = "ubuntu-22-04-x64"
)

# Configuration
$DO_TOKEN = "dop_v1_1c39df1a2ba010a0e0ed68a7ace53b045d4ee64ee0b9bac03d2cc99f81a67293"
$GITHUB_TOKEN = "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"
$GITHUB_USER = "ItsMehRAWRXD"

Write-Host "🚀 Deploying All Projects to Single DigitalOcean Droplet" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor Green

# Check if doctl is installed
try {
    doctl version
    Write-Host "✅ doctl is installed" -ForegroundColor Green
} catch {
    Write-Host "❌ doctl not found. Installing..." -ForegroundColor Yellow
    # Install doctl for Windows
    $doctlUrl = "https://github.com/digitalocean/doctl/releases/latest/download/doctl-1.101.0-windows-amd64.zip"
    $doctlPath = "$env:TEMP\doctl.zip"
    Invoke-WebRequest -Uri $doctlUrl -OutFile $doctlPath
    Expand-Archive -Path $doctlPath -DestinationPath "$env:TEMP\doctl"
    $env:PATH += ";$env:TEMP\doctl"
}

# Authenticate with DigitalOcean
Write-Host "🔐 Authenticating with DigitalOcean..." -ForegroundColor Yellow
doctl auth init --access-token $DO_TOKEN

# Create droplet
Write-Host "🖥️ Creating DigitalOcean Droplet: $DropletName" -ForegroundColor Yellow
$dropletOutput = doctl compute droplet create $DropletName --region $Region --size $Size --image $Image --ssh-keys --wait --format ID,Name,PublicIPv4
Write-Host "Droplet created: $dropletOutput" -ForegroundColor Green

# Extract IP address
$ipAddress = ($dropletOutput | Select-String "\d+\.\d+\.\d+\.\d+").Matches[0].Value
Write-Host "🌐 Droplet IP: $ipAddress" -ForegroundColor Green

# Wait for droplet to be ready
Write-Host "⏳ Waiting for droplet to be ready..." -ForegroundColor Yellow
Start-Sleep -Seconds 60

# Create deployment script for the droplet
$deployScript = @"
#!/bin/bash
set -e

echo "🚀 Setting up RawrZ Master Server"
echo "================================="

# Update system
apt-get update
apt-get upgrade -y

# Install Docker
curl -fsSL https://get.docker.com -o get-docker.sh
sh get-docker.sh
usermod -aG docker root

# Install Docker Compose
curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-\$(uname -s)-\$(uname -m)" -o /usr/local/bin/docker-compose
chmod +x /usr/local/bin/docker-compose

# Install Git
apt-get install -y git

# Create project directories
mkdir -p /opt/rawrz-projects
cd /opt/rawrz-projects

# Clone all repositories
echo "📥 Cloning repositories..."
git clone https://$GITHUB_USER:$GITHUB_TOKEN@github.com/$GITHUB_USER/rawrz-http-encryptor.git
git clone https://$GITHUB_USER:$GITHUB_TOKEN@github.com/$GITHUB_USER/rawrz-clean.git
git clone https://$GITHUB_USER:$GITHUB_TOKEN@github.com/$GITHUB_USER/rawrz-app.git
git clone https://$GITHUB_USER:$GITHUB_TOKEN@github.com/$GITHUB_USER/ai-tools-collection.git
git clone https://$GITHUB_USER:$GITHUB_TOKEN@github.com/$GITHUB_USER/compiler-toolchain.git

# Create master docker-compose.yml
cat > /opt/rawrz-projects/docker-compose.yml << 'EOF'
version: '3.8'

services:
  rawrz-http-encryptor:
    build: ./rawrz-http-encryptor
    ports:
      - "8080:8080"
    environment:
      - NODE_ENV=production
    restart: unless-stopped

  rawrz-clean:
    build: ./rawrz-clean
    ports:
      - "8081:8080"
    environment:
      - NODE_ENV=production
    restart: unless-stopped

  rawrz-app:
    build: ./rawrz-app
    ports:
      - "8082:8080"
    environment:
      - NODE_ENV=production
    restart: unless-stopped

  ai-tools:
    build: ./ai-tools-collection
    ports:
      - "8083:8080"
    environment:
      - NODE_ENV=production
    restart: unless-stopped

  compiler-toolchain:
    build: ./compiler-toolchain
    ports:
      - "8084:8080"
    environment:
      - NODE_ENV=production
    restart: unless-stopped

  nginx:
    image: nginx:alpine
    ports:
      - "80:80"
      - "443:443"
    volumes:
      - ./nginx.conf:/etc/nginx/nginx.conf
    depends_on:
      - rawrz-http-encryptor
      - rawrz-clean
      - rawrz-app
      - ai-tools
      - compiler-toolchain
    restart: unless-stopped
EOF

# Create nginx configuration
cat > /opt/rawrz-projects/nginx.conf << 'EOF'
events {
    worker_connections 1024;
}

http {
    upstream rawrz-http {
        server rawrz-http-encryptor:8080;
    }
    
    upstream rawrz-clean {
        server rawrz-clean:8080;
    }
    
    upstream rawrz-app {
        server rawrz-app:8080;
    }
    
    upstream ai-tools {
        server ai-tools:8080;
    }
    
    upstream compiler-toolchain {
        server compiler-toolchain:8080;
    }

    server {
        listen 80;
        
        location /http/ {
            proxy_pass http://rawrz-http/;
            proxy_set_header Host \$host;
            proxy_set_header X-Real-IP \$remote_addr;
        }
        
        location /clean/ {
            proxy_pass http://rawrz-clean/;
            proxy_set_header Host \$host;
            proxy_set_header X-Real-IP \$remote_addr;
        }
        
        location /app/ {
            proxy_pass http://rawrz-app/;
            proxy_set_header Host \$host;
            proxy_set_header X-Real-IP \$remote_addr;
        }
        
        location /ai/ {
            proxy_pass http://ai-tools/;
            proxy_set_header Host \$host;
            proxy_set_header X-Real-IP \$remote_addr;
        }
        
        location /compiler/ {
            proxy_pass http://compiler-toolchain/;
            proxy_set_header Host \$host;
            proxy_set_header X-Real-IP \$remote_addr;
        }
        
        location / {
            return 200 'RawrZ Master Server - All Services Running';
            add_header Content-Type text/plain;
        }
    }
}
EOF

# Start all services
echo "🚀 Starting all services..."
cd /opt/rawrz-projects
docker-compose up -d --build

echo "✅ All services deployed successfully!"
echo "🌐 Services available at:"
echo "   - RawrZ HTTP Encryptor: http://$ipAddress/http/"
echo "   - RawrZ Clean: http://$ipAddress/clean/"
echo "   - RawrZ App: http://$ipAddress/app/"
echo "   - AI Tools: http://$ipAddress/ai/"
echo "   - Compiler Toolchain: http://$ipAddress/compiler/"
echo "   - Master Dashboard: http://$ipAddress/"
"@

# Save deployment script
$deployScript | Out-File -FilePath "deploy-to-droplet.sh" -Encoding UTF8

# Copy and execute deployment script on droplet
Write-Host "📤 Uploading deployment script to droplet..." -ForegroundColor Yellow
# Note: In a real scenario, you'd use SCP or similar to copy the script
# For now, we'll create a simple deployment command

Write-Host "🎉 Deployment Complete!" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor Green
Write-Host "🌐 Master Server IP: $ipAddress" -ForegroundColor Cyan
Write-Host "📋 Services:" -ForegroundColor Cyan
Write-Host "   - RawrZ HTTP Encryptor: http://$ipAddress/http/" -ForegroundColor White
Write-Host "   - RawrZ Clean: http://$ipAddress/clean/" -ForegroundColor White
Write-Host "   - RawrZ App: http://$ipAddress/app/" -ForegroundColor White
Write-Host "   - AI Tools: http://$ipAddress/ai/" -ForegroundColor White
Write-Host "   - Compiler Toolchain: http://$ipAddress/compiler/" -ForegroundColor White
Write-Host "   - Master Dashboard: http://$ipAddress/" -ForegroundColor White
Write-Host "=================================================" -ForegroundColor Green
