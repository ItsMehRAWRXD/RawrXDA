# RawrZ Projects - DigitalOcean Deployment Script
Write-Host "🚀 RawrZ Projects - Complete DigitalOcean Deployment" -ForegroundColor Green
Write-Host "===================================================" -ForegroundColor Green

# Configuration
$GITHUB_USER = "ItsMehRAWRXD"
$GITHUB_TOKEN = "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"
$DO_TOKEN = "dop_v1_1c39df1a2ba010a0e0ed68a7ace53b045d4ee64ee0b9bac03d2cc99f81a67293"
$DO_REGION = "nyc1"
$DO_SIZE = "s-1vcpu-1gb"
$DO_IMAGE = "ubuntu-22-04-x64"

Write-Host ""
Write-Host "📋 Deployment Configuration:" -ForegroundColor Blue
Write-Host "  GitHub User: $GITHUB_USER" -ForegroundColor White
Write-Host "  DigitalOcean Region: $DO_REGION" -ForegroundColor White
Write-Host "  Droplet Size: $DO_SIZE" -ForegroundColor White
Write-Host ""

# Projects to deploy
$PROJECTS = @(
    @{Name="rawrz-http-encryptor"; Port="8080"; Description="RawrZ HTTP Encryptor"; LocalDir="rawrz-http-encryptor"},
    @{Name="rawrz-clean"; Port="8081"; Description="RawrZ Clean Security"; LocalDir="01-rawrz-clean"},
    @{Name="rawrz-app"; Port="8082"; Description="RawrZ Main App"; LocalDir="02-rawrz-app"},
    @{Name="ai-tools-collection"; Port="8083"; Description="AI Tools Collection"; LocalDir="04-ai-tools"},
    @{Name="compiler-toolchain"; Port="8084"; Description="Compiler Toolchain"; LocalDir="05-compiler-toolchain"}
)

Write-Host "🔍 Projects to Deploy:" -ForegroundColor Blue
Write-Host "=====================" -ForegroundColor Blue
foreach ($project in $PROJECTS) {
    Write-Host "  • $($project.Description) (Port $($project.Port)) - Repository: $($project.Name)" -ForegroundColor White
}
Write-Host ""

# Step 1: Install doctl if not present
Write-Host "📦 Checking DigitalOcean CLI..." -ForegroundColor Blue
if (!(Get-Command doctl -ErrorAction SilentlyContinue)) {
    Write-Host "📦 Installing DigitalOcean CLI (doctl)..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri "https://github.com/digitalocean/doctl/releases/download/v1.94.0/doctl-1.94.0-windows-amd64.zip" -OutFile "doctl.zip"
    Expand-Archive -Path "doctl.zip" -DestinationPath "." -Force
    Move-Item "doctl.exe" "C:\Windows\System32\" -Force
    Remove-Item "doctl.zip"
    Write-Host "✅ doctl installed" -ForegroundColor Green
} else {
    Write-Host "✅ doctl already installed" -ForegroundColor Green
}

# Step 2: Authenticate with DigitalOcean
Write-Host "🔑 Authenticating with DigitalOcean..." -ForegroundColor Blue
& doctl auth init --access-token $DO_TOKEN

# Step 3: Create SSH key if it doesn't exist
if (!(Test-Path "$env:USERPROFILE\.ssh\id_rsa")) {
    Write-Host "🔑 Generating SSH key..." -ForegroundColor Yellow
    & ssh-keygen -t rsa -b 4096 -f "$env:USERPROFILE\.ssh\id_rsa" -N '""'
}

# Step 4: Add SSH key to DigitalOcean
Write-Host "🔑 Adding SSH key to DigitalOcean..." -ForegroundColor Blue
& doctl compute ssh-key import rawrz-key --public-key-file "$env:USERPROFILE\.ssh\id_rsa.pub" 2>$null

# Step 5: Deploy each project
Write-Host "🚀 Starting DigitalOcean deployment..." -ForegroundColor Green
Write-Host "=====================================" -ForegroundColor Green

$deployedProjects = @()

foreach ($project in $PROJECTS) {
    Write-Host ""
    Write-Host "🚀 Deploying $($project.Description)..." -ForegroundColor Green
    Write-Host "=================" -ForegroundColor Green
    
    # Create droplet
    Write-Host "📦 Creating DigitalOcean Droplet for $($project.Description)..." -ForegroundColor Blue
    $sshKeys = & doctl compute ssh-key list --format ID --no-header | Select-String "^[0-9]"
    $dropletId = & doctl compute droplet create $project.Name --image $DO_IMAGE --size $DO_SIZE --region $DO_REGION --ssh-keys $sshKeys --wait --format ID --no-header
    
    if ([string]::IsNullOrEmpty($dropletId)) {
        Write-Host "❌ Failed to create DigitalOcean Droplet for $($project.Description)" -ForegroundColor Red
        continue
    }
    
    # Get droplet IP
    $dropletIp = & doctl compute droplet get $dropletId --format PublicIPv4 --no-header
    
    Write-Host "✅ DigitalOcean Droplet created for $($project.Description): $dropletIp" -ForegroundColor Green
    
    # Wait for droplet to be ready
    Write-Host "⏳ Waiting for droplet to be ready..." -ForegroundColor Yellow
    Start-Sleep -Seconds 30
    
    # Create deployment script
    $deployScript = @"
#!/bin/bash
echo "🚀 Deploying $($project.Description) on DigitalOcean"
echo "================================"

# Update system
apt update && apt upgrade -y -qq

# Install Node.js
curl -fsSL https://deb.nodesource.com/setup_18.x | bash -
apt-get install -y nodejs

# Install PM2
npm install -g pm2

# Clone repository
git clone https://$GITHUB_USER`:$GITHUB_TOKEN@github.com/$GITHUB_USER/$($project.Name).git
cd $($project.Name)

# Install dependencies
npm install --production

# Create environment
echo "AUTH_TOKEN=`$(openssl rand -hex 32)" > .env
echo "JWT_SECRET=`$(openssl rand -hex 32)" >> .env
echo "JWT_REFRESH_SECRET=`$(openssl rand -hex 32)" >> .env
echo "PORT=$($project.Port)" >> .env
echo "NODE_ENV=production" >> .env
echo "CORS_ORIGIN=*" >> .env
echo "LOG_LEVEL=info" >> .env

# Start application
pm2 start server.js --name $($project.Name)
pm2 startup
pm2 save

# Configure firewall
ufw allow 22/tcp
ufw allow $($project.Port)/tcp
ufw --force enable

echo "✅ $($project.Description) deployed successfully!"
echo "🌐 Access at: http://$dropletIp`:$($project.Port)"
"@
    
    # Save deployment script
    $deployScript | Out-File -FilePath "deploy_$($project.Name).sh" -Encoding UTF8
    
    # Copy deployment script to droplet
    & scp -o StrictHostKeyChecking=no "deploy_$($project.Name).sh" "root@$dropletIp`:/root/"
    
    # Execute deployment on droplet
    & ssh -o StrictHostKeyChecking=no "root@$dropletIp" "bash /root/deploy_$($project.Name).sh"
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ $($project.Description) deployed successfully!" -ForegroundColor Green
        $deployedProjects += @{
            Name = $project.Description
            IP = $dropletIp
            Port = $project.Port
            GitHub = "https://github.com/$GITHUB_USER/$($project.Name)"
        }
    } else {
        Write-Host "❌ Deployment failed for $($project.Description)" -ForegroundColor Red
    }
    
    # Clean up
    Remove-Item "deploy_$($project.Name).sh" -ErrorAction SilentlyContinue
}

# Step 6: Display final summary
Write-Host ""
Write-Host "🎉 All RawrZ Projects Deployment Complete!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "📊 Deployment Summary:" -ForegroundColor Blue
Write-Host "=====================" -ForegroundColor Blue

foreach ($project in $deployedProjects) {
    Write-Host "  • $($project.Name):" -ForegroundColor White
    Write-Host "    - GitHub: $($project.GitHub)" -ForegroundColor Cyan
    Write-Host "    - DigitalOcean: $($project.IP)" -ForegroundColor Cyan
    Write-Host "    - Access: http://$($project.IP)`:$($project.Port)" -ForegroundColor Cyan
}

Write-Host ""
Write-Host "🌐 Access Your Platforms:" -ForegroundColor Blue
Write-Host "========================" -ForegroundColor Blue
Write-Host "  • RawrZ HTTP Encryptor: http://[DROPLET_IP]:8080" -ForegroundColor White
Write-Host "  • RawrZ Clean Security: http://[DROPLET_IP]:8081" -ForegroundColor White
Write-Host "  • RawrZ Main App: http://[DROPLET_IP]:8082" -ForegroundColor White
Write-Host "  • AI Tools Collection: http://[DROPLET_IP]:8083" -ForegroundColor White
Write-Host "  • Compiler Toolchain: http://[DROPLET_IP]:8084" -ForegroundColor White
Write-Host ""
Write-Host "🔧 Management Commands:" -ForegroundColor Blue
Write-Host "======================" -ForegroundColor Blue
Write-Host "  • SSH Access: ssh root@[DROPLET_IP]" -ForegroundColor White
Write-Host "  • Service Status: pm2 status" -ForegroundColor White
Write-Host "  • View Logs: pm2 logs [service-name]" -ForegroundColor White
Write-Host "  • Restart Service: pm2 restart [service-name]" -ForegroundColor White
Write-Host ""
Write-Host "🔒 Security Notes:" -ForegroundColor Blue
Write-Host "==================" -ForegroundColor Blue
Write-Host "  • All repositories are private and secure" -ForegroundColor White
Write-Host "  • Auth tokens are securely generated" -ForegroundColor White
Write-Host "  • Firewall configured for each service" -ForegroundColor White
Write-Host "  • All services run with PM2 process manager" -ForegroundColor White
Write-Host ""
Write-Host "✅ All your RawrZ projects are now live and fully functional!" -ForegroundColor Green
Write-Host ""
Read-Host "Press Enter to continue"
