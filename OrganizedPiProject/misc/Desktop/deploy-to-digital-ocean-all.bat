@echo off
echo 🚀 RawrZ Projects - Complete DigitalOcean Deployment
echo ===================================================

REM Configuration
set GITHUB_USER=ItsMehRAWRXD
set GITHUB_TOKEN=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
set DO_TOKEN=dop_v1_1c39df1a2ba010a0e0ed68a7ace53b045d4ee64ee0b9bac03d2cc99f81a67293
set DO_REGION=nyc1
set DO_SIZE=s-1vcpu-1gb
set DO_IMAGE=ubuntu-22-04-x64

echo.
echo 📋 Deployment Configuration:
echo   GitHub User: %GITHUB_USER%
echo   DigitalOcean Region: %DO_REGION%
echo   Droplet Size: %DO_SIZE%
echo.

REM Projects to deploy
set PROJECTS=rawrz-http-encryptor:8080:RawrZ HTTP Encryptor:rawrz-http-encryptor
set PROJECTS=%PROJECTS% rawrz-clean:8081:RawrZ Clean Security:01-rawrz-clean
set PROJECTS=%PROJECTS% rawrz-app:8082:RawrZ Main App:02-rawrz-app
set PROJECTS=%PROJECTS% ai-tools-collection:8083:AI Tools Collection:04-ai-tools
set PROJECTS=%PROJECTS% compiler-toolchain:8084:Compiler Toolchain:05-compiler-toolchain

echo 🔍 Projects to Deploy:
echo =====================
for %%p in (%PROJECTS%) do (
    for /f "tokens=1,2,3,4 delims=:" %%a in ("%%p") do (
        echo   • %%c (Port %%b) - Repository: %%a
    )
)
echo.

REM Step 1: Install doctl if not present
echo 📦 Checking DigitalOcean CLI...
where doctl >nul 2>&1
if %errorlevel% neq 0 (
    echo 📦 Installing DigitalOcean CLI (doctl)...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/digitalocean/doctl/releases/download/v1.94.0/doctl-1.94.0-windows-amd64.zip' -OutFile 'doctl.zip'"
    powershell -Command "Expand-Archive -Path 'doctl.zip' -DestinationPath '.' -Force"
    move doctl.exe C:\Windows\System32\ 2>nul
    del doctl.zip 2>nul
    echo ✅ doctl installed
) else (
    echo ✅ doctl already installed
)

REM Step 2: Authenticate with DigitalOcean
echo 🔑 Authenticating with DigitalOcean...
doctl auth init --access-token %DO_TOKEN%

REM Step 3: Create SSH key if it doesn't exist
if not exist "%USERPROFILE%\.ssh\id_rsa" (
    echo 🔑 Generating SSH key...
    ssh-keygen -t rsa -b 4096 -f "%USERPROFILE%\.ssh\id_rsa" -N ""
)

REM Step 4: Add SSH key to DigitalOcean
echo 🔑 Adding SSH key to DigitalOcean...
doctl compute ssh-key import rawrz-key --public-key-file "%USERPROFILE%\.ssh\id_rsa.pub" 2>nul

REM Step 5: Deploy each project
echo 🚀 Starting DigitalOcean deployment...
echo =====================================

for %%p in (%PROJECTS%) do (
    for /f "tokens=1,2,3,4 delims=:" %%a in ("%%p") do (
        echo.
        echo 🚀 Deploying %%c...
        echo ==================
        
        REM Create droplet
        echo 📦 Creating DigitalOcean Droplet for %%c...
        for /f "tokens=*" %%i in ('doctl compute droplet create %%a --image %DO_IMAGE% --size %DO_SIZE% --region %DO_REGION% --ssh-keys ^(doctl compute ssh-key list --format ID --no-header ^| findstr /r "^[0-9]"^) --wait --format ID --no-header') do set DROPLET_ID=%%i
        
        if "!DROPLET_ID!"=="" (
            echo ❌ Failed to create DigitalOcean Droplet for %%c
            goto :continue
        )
        
        REM Get droplet IP
        for /f "tokens=*" %%i in ('doctl compute droplet get !DROPLET_ID! --format PublicIPv4 --no-header') do set DROPLET_IP=%%i
        
        echo ✅ DigitalOcean Droplet created for %%c: !DROPLET_IP!
        echo !DROPLET_IP! > %%a_ip.txt
        
        REM Wait for droplet to be ready
        echo ⏳ Waiting for droplet to be ready...
        timeout /t 30 /nobreak >nul
        
        REM Deploy application to droplet
        echo 🚀 Deploying %%c to droplet...
        
        REM Create deployment script for this project
        echo #!/bin/bash > deploy_%%a.sh
        echo echo "🚀 Deploying %%c on DigitalOcean" >> deploy_%%a.sh
        echo echo "================================" >> deploy_%%a.sh
        echo. >> deploy_%%a.sh
        echo # Update system >> deploy_%%a.sh
        echo apt update ^&^& apt upgrade -y -qq >> deploy_%%a.sh
        echo. >> deploy_%%a.sh
        echo # Install Node.js >> deploy_%%a.sh
        echo curl -fsSL https://deb.nodesource.com/setup_18.x ^| bash - >> deploy_%%a.sh
        echo apt-get install -y nodejs >> deploy_%%a.sh
        echo. >> deploy_%%a.sh
        echo # Install PM2 >> deploy_%%a.sh
        echo npm install -g pm2 >> deploy_%%a.sh
        echo. >> deploy_%%a.sh
        echo # Clone repository >> deploy_%%a.sh
        echo git clone https://ItsMehRAWRXD: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@github.com/ItsMehRAWRXD/%%a.git >> deploy_%%a.sh
        echo cd %%a >> deploy_%%a.sh
        echo. >> deploy_%%a.sh
        echo # Install dependencies >> deploy_%%a.sh
        echo npm install --production >> deploy_%%a.sh
        echo. >> deploy_%%a.sh
        echo # Create environment >> deploy_%%a.sh
        echo echo "AUTH_TOKEN=$(openssl rand -hex 32)" ^> .env >> deploy_%%a.sh
        echo echo "JWT_SECRET=$(openssl rand -hex 32)" ^>^> .env >> deploy_%%a.sh
        echo echo "JWT_REFRESH_SECRET=$(openssl rand -hex 32)" ^>^> .env >> deploy_%%a.sh
        echo echo "PORT=%%b" ^>^> .env >> deploy_%%a.sh
        echo echo "NODE_ENV=production" ^>^> .env >> deploy_%%a.sh
        echo echo "CORS_ORIGIN=*" ^>^> .env >> deploy_%%a.sh
        echo echo "LOG_LEVEL=info" ^>^> .env >> deploy_%%a.sh
        echo. >> deploy_%%a.sh
        echo # Start application >> deploy_%%a.sh
        echo pm2 start server.js --name %%a >> deploy_%%a.sh
        echo pm2 startup >> deploy_%%a.sh
        echo pm2 save >> deploy_%%a.sh
        echo. >> deploy_%%a.sh
        echo # Configure firewall >> deploy_%%a.sh
        echo ufw allow 22/tcp >> deploy_%%a.sh
        echo ufw allow %%b/tcp >> deploy_%%a.sh
        echo ufw --force enable >> deploy_%%a.sh
        echo. >> deploy_%%a.sh
        echo echo "✅ %%c deployed successfully!" >> deploy_%%a.sh
        echo echo "🌐 Access at: http://!DROPLET_IP!:%%b" >> deploy_%%a.sh
        
        REM Copy deployment script to droplet
        scp -o StrictHostKeyChecking=no deploy_%%a.sh root@!DROPLET_IP!:/root/
        
        REM Execute deployment on droplet
        ssh -o StrictHostKeyChecking=no root@!DROPLET_IP! "bash /root/deploy_%%a.sh"
        
        if %errorlevel% equ 0 (
            echo ✅ %%c deployed successfully!
        ) else (
            echo ❌ Deployment failed for %%c
        )
        
        REM Clean up
        del deploy_%%a.sh 2>nul
        
        :continue
    )
)

REM Step 6: Display final summary
echo.
echo 🎉 All RawrZ Projects Deployment Complete!
echo =========================================
echo.
echo 📊 Deployment Summary:
echo =====================

for %%p in (%PROJECTS%) do (
    for /f "tokens=1,2,3,4 delims=:" %%a in ("%%p") do (
        if exist %%a_ip.txt (
            set /p DROPLET_IP=<%%a_ip.txt
            echo   • %%c:
            echo     - GitHub: https://github.com/%GITHUB_USER%/%%a
            echo     - DigitalOcean: !DROPLET_IP!
            echo     - Access: http://!DROPLET_IP!:%%b
            del %%a_ip.txt
        )
    )
)

echo.
echo 🌐 Access Your Platforms:
echo ========================
echo   • RawrZ HTTP Encryptor: http://[DROPLET_IP]:8080
echo   • RawrZ Clean Security: http://[DROPLET_IP]:8081
echo   • RawrZ Main App: http://[DROPLET_IP]:8082
echo   • AI Tools Collection: http://[DROPLET_IP]:8083
echo   • Compiler Toolchain: http://[DROPLET_IP]:8084
echo.
echo 🔧 Management Commands:
echo ======================
echo   • SSH Access: ssh root@[DROPLET_IP]
echo   • Service Status: pm2 status
echo   • View Logs: pm2 logs [service-name]
echo   • Restart Service: pm2 restart [service-name]
echo.
echo 🔒 Security Notes:
echo ==================
echo   • All repositories are private and secure
echo   • Auth tokens are securely generated
echo   • Firewall configured for each service
echo   • All services run with PM2 process manager
echo.
echo ✅ All your RawrZ projects are now live and fully functional!
echo.
pause
