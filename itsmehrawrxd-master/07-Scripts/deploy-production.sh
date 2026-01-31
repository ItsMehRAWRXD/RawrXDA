#!/bin/bash

# RawrZ Security Platform - Production Deployment Script
# This script automates the deployment of the RawrZ Security Platform

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
PROJECT_NAME="RawrZ-Security-Platform"
DEPLOYMENT_DIR="/opt/rawrz"
SERVICE_USER="rawrz"
SERVICE_NAME="rawrz-platform"
NODE_VERSION="18"
NGINX_CONFIG="/etc/nginx/sites-available/rawrz"
SSL_CERT_DIR="/etc/ssl/rawrz"

# Logging function
log() {
    echo -e "${GREEN}[$(date +'%Y-%m-%d %H:%M:%S')] $1${NC}"
}

error() {
    echo -e "${RED}[ERROR] $1${NC}"
    exit 1
}

warning() {
    echo -e "${YELLOW}[WARNING] $1${NC}"
}

info() {
    echo -e "${BLUE}[INFO] $1${NC}"
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        error "This script must be run as root"
    fi
}

# Update system packages
update_system() {
    log "Updating system packages..."
    apt-get update -y
    apt-get upgrade -y
    apt-get install -y curl wget git build-essential software-properties-common
}

# Install Node.js
install_nodejs() {
    log "Installing Node.js ${NODE_VERSION}..."
    
    # Remove existing Node.js
    apt-get remove -y nodejs npm || true
    
    # Install Node.js from NodeSource
    curl -fsSL https://deb.nodesource.com/setup_${NODE_VERSION}.x | bash -
    apt-get install -y nodejs
    
    # Verify installation
    node --version
    npm --version
}

# Install Nginx
install_nginx() {
    log "Installing Nginx..."
    apt-get install -y nginx
    
    # Start and enable Nginx
    systemctl start nginx
    systemctl enable nginx
}

# Install SSL certificates
install_ssl() {
    log "Installing SSL certificates..."
    
    # Create SSL directory
    mkdir -p $SSL_CERT_DIR
    
    # Generate self-signed certificate (for testing)
    # In production, replace with real certificates
    openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
        -keyout $SSL_CERT_DIR/private.key \
        -out $SSL_CERT_DIR/certificate.crt \
        -subj "/C=US/ST=State/L=City/O=Organization/CN=rawrz.local"
    
    chmod 600 $SSL_CERT_DIR/private.key
    chmod 644 $SSL_CERT_DIR/certificate.crt
}

# Create service user
create_service_user() {
    log "Creating service user..."
    
    if ! id "$SERVICE_USER" &>/dev/null; then
        useradd -r -s /bin/false -d $DEPLOYMENT_DIR $SERVICE_USER
    fi
}

# Deploy application
deploy_application() {
    log "Deploying application..."
    
    # Create deployment directory
    mkdir -p $DEPLOYMENT_DIR
    cd $DEPLOYMENT_DIR
    
    # Copy application files
    cp -r /tmp/rawrz-deployment/* .
    
    # Install dependencies
    npm install --production
    
    # Build C# components
    if [ -d "RawrZ.NET" ]; then
        cd RawrZ.NET/RawrZDesktop
        dotnet build --configuration Release
        cd ../..
    fi
    
    # Set permissions
    chown -R $SERVICE_USER:$SERVICE_USER $DEPLOYMENT_DIR
    chmod +x $DEPLOYMENT_DIR/server.js
}

# Create systemd service
create_systemd_service() {
    log "Creating systemd service..."
    
    cat > /etc/systemd/system/${SERVICE_NAME}.service << EOF
[Unit]
Description=RawrZ Security Platform
After=network.target

[Service]
Type=simple
User=$SERVICE_USER
WorkingDirectory=$DEPLOYMENT_DIR
ExecStart=/usr/bin/node server.js
Restart=always
RestartSec=10
Environment=NODE_ENV=production
Environment=PORT=3000

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=$DEPLOYMENT_DIR

[Install]
WantedBy=multi-user.target
EOF

    # Reload systemd and enable service
    systemctl daemon-reload
    systemctl enable $SERVICE_NAME
}

# Configure Nginx
configure_nginx() {
    log "Configuring Nginx..."
    
    cat > $NGINX_CONFIG << EOF
server {
    listen 80;
    server_name rawrz.local;
    return 301 https://\$server_name\$request_uri;
}

server {
    listen 443 ssl http2;
    server_name rawrz.local;
    
    ssl_certificate $SSL_CERT_DIR/certificate.crt;
    ssl_certificate_key $SSL_CERT_DIR/private.key;
    
    # SSL configuration
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-RSA-AES256-GCM-SHA512:DHE-RSA-AES256-GCM-SHA512:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers off;
    ssl_session_cache shared:SSL:10m;
    ssl_session_timeout 10m;
    
    # Security headers
    add_header X-Frame-Options DENY;
    add_header X-Content-Type-Options nosniff;
    add_header X-XSS-Protection "1; mode=block";
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;
    
    # Rate limiting
    limit_req_zone \$binary_remote_addr zone=api:10m rate=10r/s;
    limit_req zone=api burst=20 nodelay;
    
    # Proxy to Node.js application
    location / {
        proxy_pass http://localhost:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
        proxy_cache_bypass \$http_upgrade;
    }
    
    # WebSocket support
    location /ws {
        proxy_pass http://localhost:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto \$scheme;
    }
}
EOF

    # Enable site
    ln -sf $NGINX_CONFIG /etc/nginx/sites-enabled/
    nginx -t
    systemctl reload nginx
}

# Configure firewall
configure_firewall() {
    log "Configuring firewall..."
    
    # Install UFW if not present
    apt-get install -y ufw
    
    # Configure firewall rules
    ufw --force reset
    ufw default deny incoming
    ufw default allow outgoing
    ufw allow ssh
    ufw allow 80/tcp
    ufw allow 443/tcp
    ufw --force enable
}

# Setup monitoring
setup_monitoring() {
    log "Setting up monitoring..."
    
    # Install monitoring tools
    apt-get install -y htop iotop nethogs
    
    # Create log rotation
    cat > /etc/logrotate.d/rawrz << EOF
$DEPLOYMENT_DIR/logs/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 644 $SERVICE_USER $SERVICE_USER
    postrotate
        systemctl reload $SERVICE_NAME
    endscript
}
EOF
}

# Create backup script
create_backup_script() {
    log "Creating backup script..."
    
    cat > /usr/local/bin/rawrz-backup.sh << 'EOF'
#!/bin/bash
BACKUP_DIR="/opt/backups/rawrz"
DATE=$(date +%Y%m%d_%H%M%S)
DEPLOYMENT_DIR="/opt/rawrz"

mkdir -p $BACKUP_DIR

# Create backup
tar -czf $BACKUP_DIR/rawrz_backup_$DATE.tar.gz -C $DEPLOYMENT_DIR .

# Keep only last 7 days of backups
find $BACKUP_DIR -name "rawrz_backup_*.tar.gz" -mtime +7 -delete

echo "Backup completed: rawrz_backup_$DATE.tar.gz"
EOF

    chmod +x /usr/local/bin/rawrz-backup.sh
    
    # Add to crontab
    (crontab -l 2>/dev/null; echo "0 2 * * * /usr/local/bin/rawrz-backup.sh") | crontab -
}

# Start services
start_services() {
    log "Starting services..."
    
    systemctl start $SERVICE_NAME
    systemctl status $SERVICE_NAME --no-pager
    
    # Wait for service to start
    sleep 5
    
    # Check if service is running
    if systemctl is-active --quiet $SERVICE_NAME; then
        log "Service started successfully"
    else
        error "Failed to start service"
    fi
}

# Verify deployment
verify_deployment() {
    log "Verifying deployment..."
    
    # Check if service is running
    if systemctl is-active --quiet $SERVICE_NAME; then
        log " Service is running"
    else
        error " Service is not running"
    fi
    
    # Check if Nginx is running
    if systemctl is-active --quiet nginx; then
        log " Nginx is running"
    else
        error " Nginx is not running"
    fi
    
    # Check if application responds
    if curl -f -s http://localhost:3000/health > /dev/null; then
        log " Application is responding"
    else
        error " Application is not responding"
    fi
    
    # Check SSL certificate
    if [ -f "$SSL_CERT_DIR/certificate.crt" ]; then
        log " SSL certificate is installed"
    else
        error " SSL certificate is missing"
    fi
}

# Main deployment function
main() {
    log "Starting RawrZ Security Platform deployment..."
    
    check_root
    update_system
    install_nodejs
    install_nginx
    install_ssl
    create_service_user
    deploy_application
    create_systemd_service
    configure_nginx
    configure_firewall
    setup_monitoring
    create_backup_script
    start_services
    verify_deployment
    
    log "Deployment completed successfully!"
    log "Access the platform at: https://rawrz.local"
    log "Service status: systemctl status $SERVICE_NAME"
    log "Logs: journalctl -u $SERVICE_NAME -f"
}

# Run main function
main "$@"
