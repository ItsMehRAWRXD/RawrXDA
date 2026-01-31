# RawrZ Security Platform - Deployment Guide

##  **Production Deployment**

### **System Requirements**

#### **Minimum Requirements**
- **OS**: Linux (Ubuntu 20.04+), Windows Server 2019+, macOS 10.15+
- **CPU**: 2 cores, 2.0 GHz
- **RAM**: 4GB
- **Storage**: 10GB free space
- **Network**: 100 Mbps connection

#### **Recommended Requirements**
- **OS**: Linux (Ubuntu 22.04 LTS)
- **CPU**: 4+ cores, 3.0+ GHz
- **RAM**: 8GB+
- **Storage**: 50GB+ SSD
- **Network**: 1 Gbps connection

### **Environment Setup**

#### **1. Node.js Installation**
```bash
# Ubuntu/Debian
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt-get install -y nodejs

# CentOS/RHEL
curl -fsSL https://rpm.nodesource.com/setup_18.x | sudo bash -
sudo yum install -y nodejs

# Windows (using Chocolatey)
choco install nodejs

# macOS (using Homebrew)
brew install node
```

#### **2. Project Setup**
```bash
# Clone repository
git clone <repository-url>
cd RawrZApp

# Install dependencies
npm install --production

# Create environment file
cp .env.example .env
```

#### **3. Environment Configuration**
```env
# Production Environment Variables
NODE_ENV=production
PORT=8080
AUTH_TOKEN=your-very-secure-auth-token-here

# Security Configuration
ALLOWED_ORIGINS=https://yourdomain.com,https://www.yourdomain.com

# Performance Configuration
CACHE_TTL=300
COMPRESSION_LEVEL=6
MAX_REQUEST_SIZE=5mb

# Logging Configuration
LOG_LEVEL=info
LOG_FILE=/var/log/rawrz/application.log

# Database Configuration (if using external DB)
DB_HOST=localhost
DB_PORT=5432
DB_NAME=rawrz_security
DB_USER=rawrz_user
DB_PASSWORD=secure-db-password
```

### **Security Configuration**

#### **1. Firewall Setup**
```bash
# Ubuntu/Debian (UFW)
sudo ufw enable
sudo ufw allow 22/tcp    # SSH
sudo ufw allow 8080/tcp  # RawrZ Platform
sudo ufw allow 80/tcp    # HTTP (if using reverse proxy)
sudo ufw allow 443/tcp   # HTTPS (if using reverse proxy)

# CentOS/RHEL (firewalld)
sudo systemctl enable firewalld
sudo systemctl start firewalld
sudo firewall-cmd --permanent --add-port=8080/tcp
sudo firewall-cmd --permanent --add-port=80/tcp
sudo firewall-cmd --permanent --add-port=443/tcp
sudo firewall-cmd --reload
```

#### **2. SSL/TLS Configuration**
```bash
# Using Let's Encrypt (Certbot)
sudo apt install certbot
sudo certbot certonly --standalone -d yourdomain.com

# Or using your own certificates
# Place certificates in /etc/ssl/rawrz/
```

#### **3. Reverse Proxy (Nginx)**
```nginx
# /etc/nginx/sites-available/rawrz
server {
    listen 80;
    server_name yourdomain.com;
    return 301 https://$server_name$request_uri;
}

server {
    listen 443 ssl http2;
    server_name yourdomain.com;

    ssl_certificate /etc/letsencrypt/live/yourdomain.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/yourdomain.com/privkey.pem;
    
    # SSL Configuration
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-RSA-AES256-GCM-SHA512:DHE-RSA-AES256-GCM-SHA512:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers off;
    ssl_session_cache shared:SSL:10m;
    ssl_session_timeout 10m;

    # Security Headers
    add_header X-Frame-Options DENY;
    add_header X-Content-Type-Options nosniff;
    add_header X-XSS-Protection "1; mode=block";
    add_header Strict-Transport-Security "max-age=31536000; includeSubDomains" always;

    location / {
        proxy_pass http://localhost:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        proxy_cache_bypass $http_upgrade;
        proxy_read_timeout 86400;
    }

    # WebSocket support
    location /ws {
        proxy_pass http://localhost:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

### **Process Management**

#### **1. Systemd Service**
```ini
# /etc/systemd/system/rawrz.service
[Unit]
Description=RawrZ Security Platform
After=network.target

[Service]
Type=simple
User=rawrz
WorkingDirectory=/opt/rawrz
ExecStart=/usr/bin/node server.js
Restart=always
RestartSec=10
Environment=NODE_ENV=production
Environment=PORT=8080
Environment=AUTH_TOKEN=your-secure-token

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/opt/rawrz/data
ReadWritePaths=/var/log/rawrz

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

[Install]
WantedBy=multi-user.target
```

#### **2. Service Management**
```bash
# Create user
sudo useradd -r -s /bin/false rawrz

# Set permissions
sudo chown -R rawrz:rawrz /opt/rawrz
sudo chmod -R 755 /opt/rawrz

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable rawrz
sudo systemctl start rawrz

# Check status
sudo systemctl status rawrz
```

### **Monitoring & Logging**

#### **1. Log Management**
```bash
# Create log directory
sudo mkdir -p /var/log/rawrz
sudo chown rawrz:rawrz /var/log/rawrz

# Log rotation configuration
# /etc/logrotate.d/rawrz
/var/log/rawrz/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 644 rawrz rawrz
    postrotate
        systemctl reload rawrz
    endscript
}
```

#### **2. Monitoring Setup**
```bash
# Install monitoring tools
sudo apt install htop iotop nethogs

# Create monitoring script
# /opt/rawrz/scripts/monitor.sh
#!/bin/bash
echo "=== RawrZ Platform Status ==="
systemctl status rawrz --no-pager
echo ""
echo "=== Memory Usage ==="
free -h
echo ""
echo "=== Disk Usage ==="
df -h
echo ""
echo "=== Network Connections ==="
netstat -tulpn | grep :8080
```

### **Backup & Recovery**

#### **1. Backup Script**
```bash
#!/bin/bash
# /opt/rawrz/scripts/backup.sh

BACKUP_DIR="/opt/backups/rawrz"
DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_FILE="rawrz_backup_$DATE.tar.gz"

# Create backup directory
mkdir -p $BACKUP_DIR

# Stop service
systemctl stop rawrz

# Create backup
tar -czf "$BACKUP_DIR/$BACKUP_FILE" \
    /opt/rawrz/data \
    /opt/rawrz/config \
    /var/log/rawrz

# Start service
systemctl start rawrz

# Clean old backups (keep 30 days)
find $BACKUP_DIR -name "rawrz_backup_*.tar.gz" -mtime +30 -delete

echo "Backup completed: $BACKUP_FILE"
```

#### **2. Recovery Process**
```bash
#!/bin/bash
# /opt/rawrz/scripts/restore.sh

BACKUP_FILE=$1

if [ -z "$BACKUP_FILE" ]; then
    echo "Usage: $0 <backup_file>"
    exit 1
fi

# Stop service
systemctl stop rawrz

# Restore from backup
tar -xzf "$BACKUP_FILE" -C /

# Set permissions
chown -R rawrz:rawrz /opt/rawrz/data
chown -R rawrz:rawrz /var/log/rawrz

# Start service
systemctl start rawrz

echo "Restore completed from: $BACKUP_FILE"
```

### **Performance Tuning**

#### **1. System Optimization**
```bash
# Increase file descriptor limits
echo "* soft nofile 65536" >> /etc/security/limits.conf
echo "* hard nofile 65536" >> /etc/security/limits.conf

# Optimize network settings
echo "net.core.somaxconn = 65536" >> /etc/sysctl.conf
echo "net.ipv4.tcp_max_syn_backlog = 65536" >> /etc/sysctl.conf
echo "net.core.netdev_max_backlog = 5000" >> /etc/sysctl.conf

# Apply changes
sysctl -p
```

#### **2. Node.js Optimization**
```bash
# Set Node.js options
export NODE_OPTIONS="--max-old-space-size=4096 --max-semi-space-size=128"

# Or in systemd service
Environment=NODE_OPTIONS=--max-old-space-size=4096 --max-semi-space-size=128
```

### **Security Hardening**

#### **1. System Hardening**
```bash
# Disable unnecessary services
systemctl disable bluetooth
systemctl disable cups
systemctl disable avahi-daemon

# Configure automatic security updates
apt install unattended-upgrades
dpkg-reconfigure unattended-upgrades

# Enable audit logging
systemctl enable auditd
systemctl start auditd
```

#### **2. Application Security**
```bash
# Set secure file permissions
chmod 600 /opt/rawrz/.env
chmod 644 /opt/rawrz/server.js
chmod 755 /opt/rawrz/scripts/*.sh

# Enable fail2ban
apt install fail2ban
systemctl enable fail2ban
systemctl start fail2ban
```

### **Troubleshooting**

#### **Common Issues**

1. **Service won't start**
   ```bash
   # Check logs
   journalctl -u rawrz -f
   
   # Check configuration
   node -c server.js
   ```

2. **High memory usage**
   ```bash
   # Monitor memory
   htop
   
   # Check for memory leaks
   node --inspect server.js
   ```

3. **Connection issues**
   ```bash
   # Check port binding
   netstat -tulpn | grep :8080
   
   # Test connectivity
   curl -I http://localhost:8080
   ```

#### **Health Checks**
```bash
# Create health check script
# /opt/rawrz/scripts/health-check.sh
#!/bin/bash

# Check if service is running
if ! systemctl is-active --quiet rawrz; then
    echo "Service is not running"
    exit 1
fi

# Check if port is listening
if ! netstat -tulpn | grep -q :8080; then
    echo "Port 8080 is not listening"
    exit 1
fi

# Check HTTP response
if ! curl -f -s http://localhost:8080 > /dev/null; then
    echo "HTTP health check failed"
    exit 1
fi

echo "All health checks passed"
exit 0
```

### **Scaling & Load Balancing**

#### **1. Multiple Instances**
```bash
# Run multiple instances on different ports
# Instance 1: PORT=8080
# Instance 2: PORT=8081
# Instance 3: PORT=8082

# Update systemd service for each instance
# /etc/systemd/system/rawrz-1.service
# /etc/systemd/system/rawrz-2.service
# /etc/systemd/system/rawrz-3.service
```

#### **2. Load Balancer Configuration**
```nginx
# Nginx upstream configuration
upstream rawrz_backend {
    server localhost:8080;
    server localhost:8081;
    server localhost:8082;
}

server {
    listen 443 ssl http2;
    server_name yourdomain.com;
    
    location / {
        proxy_pass http://rawrz_backend;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

---

##  **Deployment Checklist**

- [ ] System requirements met
- [ ] Node.js installed and configured
- [ ] Project dependencies installed
- [ ] Environment variables configured
- [ ] Firewall rules configured
- [ ] SSL certificates installed
- [ ] Reverse proxy configured
- [ ] Systemd service created
- [ ] Logging configured
- [ ] Monitoring setup
- [ ] Backup strategy implemented
- [ ] Security hardening applied
- [ ] Health checks configured
- [ ] Performance tuning applied
- [ ] Load balancing configured (if needed)

---

** Important**: Always test your deployment in a staging environment before deploying to production. Ensure all security measures are properly configured and tested.
