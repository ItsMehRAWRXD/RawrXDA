@echo off
echo Deploying to Digital Ocean...

REM Build the application
echo Building application...
javac -cp "picocli-4.7.5.jar:javax.json-1.1.4.jar" *.java
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

REM Create Docker image
echo Creating Docker image...
docker build -t rawrz-app .
if errorlevel 1 (
    echo Docker build failed!
    exit /b 1
)

REM Tag for Digital Ocean registry
echo Tagging image...
docker tag rawrz-app registry.digitalocean.com/your-registry/rawrz-app:latest

REM Push to registry
echo Pushing to Digital Ocean registry...
docker push registry.digitalocean.com/your-registry/rawrz-app:latest
if errorlevel 1 (
    echo Push failed!
    exit /b 1
)

REM Deploy via kubectl
echo Deploying to Kubernetes...
kubectl apply -f k8s/
if errorlevel 1 (
    echo Deployment failed!
    exit /b 1
)

echo Deployment completed successfully!