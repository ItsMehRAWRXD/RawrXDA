# BEAST SWARM QUICK REFERENCE GUIDE
## Production Deployment & Operations

---

## 📋 QUICK START (5 Minutes)

### 1. Deploy to Production
```bash
# Copy files to production server
scp -r /path/to/beast-swarm/* user@server:/opt/beast-swarm/

# SSH into server
ssh user@server

# Run deployment script
cd /opt/beast-swarm
sudo ./deploy_beast_swarm.sh .
```

### 2. Verify Installation
```bash
sudo ./health_check.sh
```

**Expected Output**:
```
✅ Service is running
✅ Process is running
✅ Listening on port 5000
✅ Disk space OK
✅ Log errors acceptable
✅ Configuration valid
✅ All Python modules available
```

### 3. Start Monitoring
```bash
# Real-time monitoring (background)
nohup sudo ./monitor.sh -i 60 > monitor.log 2>&1 &

# Or in tmux/screen session
tmux new-session -d -s beast-monitor 'sudo ./monitor.sh'
```

---

## 📁 FILE STRUCTURE

```
/opt/beast-swarm/
├── lib/                          # Python optimization modules
│   ├── phase1_1_memory_optimization.py
│   ├── phase1_2_simple.py
│   ├── phase2_error_handling_minimal.py
│   └── phase4_6_testing.py
├── bin/                          # Deployment & monitoring scripts
│   ├── deploy_beast_swarm.sh
│   ├── health_check.sh
│   ├── rollback.sh
│   └── monitor.sh
├── config/                       # Configuration files
├── data/                         # Runtime data
└── logs/                         # Log files
    ├── beast_YYYYMMDD_HHMMSS.log
    ├── beast_errors_YYYYMMDD_HHMMSS.log
    ├── health.log
    ├── alerts.log
    └── metrics.log
```

---

## 🚀 COMMON OPERATIONS

### Daily Health Check
```bash
# Quick status check (takes 30 seconds)
sudo /opt/beast-swarm/bin/health_check.sh

# View results
tail -20 /var/log/beast-swarm/health.log
```

### Monitor Performance
```bash
# Real-time metrics
sudo /opt/beast-swarm/bin/monitor.sh -c

# View last 20 metrics
sudo /opt/beast-swarm/bin/monitor.sh -m

# View last 20 alerts
sudo /opt/beast-swarm/bin/monitor.sh -a
```

### Service Management
```bash
# Check service status
systemctl status beast-swarm.service

# Restart service
sudo systemctl restart beast-swarm.service

# View service logs
sudo journalctl -u beast-swarm.service -f

# Enable/disable on boot
sudo systemctl enable beast-swarm.service
sudo systemctl disable beast-swarm.service
```

### Performance Verification
```bash
# Run performance tests
cd /opt/beast-swarm
sudo python3 lib/phase4_6_testing.py

# View test results
cat phase4_6_testing_results.json | python3 -m json.tool
```

---

## 🔄 BACKUP & RECOVERY

### Create Manual Backup
```bash
sudo /opt/beast-swarm/bin/deploy_beast_swarm.sh --backup
```

### View Available Backups
```bash
sudo /opt/beast-swarm/bin/rollback.sh --list
```

### Restore from Backup
```bash
# Interactive selection
sudo /opt/beast-swarm/bin/rollback.sh

# Or specify backup file
sudo /opt/beast-swarm/bin/rollback.sh --rollback \
  /var/backups/beast-swarm/beast-swarm-20250121_120000.tar.gz
```

---

## 📊 PERFORMANCE TARGETS

### Expected Performance Metrics
```
Memory Usage:     < 37 MB (26.6% reduction from 50MB baseline)
CPU Operations:   > 8 million ops/sec (27.5% improvement)
Error Recovery:   > 71% (71.43% actual)
Service Uptime:   > 99% (monitored continuously)
```

### Performance Verification
```bash
# Run performance baseline
cd /opt/beast-swarm
sudo python3 lib/phase1_baseline.py

# Compare against targets
cat baseline_metrics.json
```

---

## ⚠️ TROUBLESHOOTING

### Service Won't Start
```bash
# Check service status
systemctl status beast-swarm.service

# View error logs
sudo tail -50 /var/log/beast-swarm/errors.log

# Restart service
sudo systemctl restart beast-swarm.service

# If still failing, rollback
sudo /opt/beast-swarm/bin/rollback.sh --rollback <backup-file>
```

### High CPU Usage
```bash
# Check CPU metrics
sudo /opt/beast-swarm/bin/monitor.sh -c

# Profile CPU
ps aux | grep python3

# If abnormal, restart service
sudo systemctl restart beast-swarm.service
```

### High Memory Usage
```bash
# Check memory metrics
top -p $(pgrep -f "python3.*beast")

# If > 50 MB, investigate memory leaks
cd /opt/beast-swarm
sudo python3 lib/phase1_1_memory_optimization.py

# Potential fixes:
# 1. Restart service
sudo systemctl restart beast-swarm.service

# 2. Clear logs if too large
sudo rm -f /var/log/beast-swarm/*.log.old

# 3. Rollback if recent deployment caused issue
sudo /opt/beast-swarm/bin/rollback.sh
```

### Network Connectivity Issues
```bash
# Check if listening on correct port
sudo netstat -tlnp | grep python3

# Verify firewall rules
sudo ufw status

# Allow traffic (if needed)
sudo ufw allow 5000
sudo ufw allow 5001

# Restart service
sudo systemctl restart beast-swarm.service
```

---

## 🔍 LOG ANALYSIS

### View Recent Logs
```bash
# Main application log
tail -100 /var/log/beast-swarm/beast_*.log

# Error log
tail -50 /var/log/beast-swarm/beast_errors_*.log

# Alerts
tail -20 /var/log/beast-swarm/alerts.log

# Metrics
tail -20 /var/log/beast-swarm/metrics.log
```

### Search for Specific Issues
```bash
# Find all ERROR messages
sudo grep "ERROR" /var/log/beast-swarm/*.log

# Find all CRITICAL messages
sudo grep "CRITICAL" /var/log/beast-swarm/*.log

# Count errors by type
sudo grep "ERROR" /var/log/beast-swarm/*.log | \
  cut -d: -f2- | sort | uniq -c | sort -rn
```

### Archive Old Logs
```bash
# Compress logs older than 7 days
find /var/log/beast-swarm -name "*.log" -mtime +7 -exec gzip {} \;

# Delete logs older than 30 days
find /var/log/beast-swarm -name "*.log.gz" -mtime +30 -delete
```

---

## 📈 MONITORING DASHBOARD

### Setup Continuous Monitoring
```bash
# Create monitoring script
cat > /usr/local/bin/beast-monitor.sh <<'EOF'
#!/bin/bash
while true; do
    clear
    echo "=== BEAST SWARM STATUS - $(date) ==="
    echo ""
    echo "Service Status:"
    systemctl status beast-swarm.service | grep Active
    echo ""
    echo "Performance Metrics:"
    ps -p $(pgrep -f "python3.*beast") -o %cpu,%mem,rss | tail -1
    echo ""
    echo "Recent Alerts (if any):"
    tail -5 /var/log/beast-swarm/alerts.log
    echo ""
    sleep 60
done
EOF

chmod +x /usr/local/bin/beast-monitor.sh

# Run in background
nohup /usr/local/bin/beast-monitor.sh > /tmp/beast-monitor.log 2>&1 &
```

### Integration with Monitoring Systems
```bash
# Prometheus metrics endpoint example
# Add to monitoring tool for:
# - CPU usage: /metrics/cpu
# - Memory usage: /metrics/memory
# - Error rate: /metrics/errors
# - Service uptime: /metrics/uptime
```

---

## 🛡️ SECURITY

### Secure Configuration
```bash
# Set restrictive permissions
sudo chmod 750 /opt/beast-swarm
sudo chmod 750 /opt/beast-swarm/bin
sudo chmod 640 /opt/beast-swarm/lib/*.py

# Run as dedicated user
sudo chown -R beast:beast /opt/beast-swarm
```

### Firewall Rules
```bash
# Allow only Beast Swarm port
sudo ufw default deny incoming
sudo ufw allow ssh
sudo ufw allow 5000/tcp  # Beast Swarm port
sudo ufw enable
```

### Log Rotation
```bash
# Create logrotate config
sudo cat > /etc/logrotate.d/beast-swarm <<'EOF'
/var/log/beast-swarm/*.log {
    daily
    rotate 14
    compress
    delaycompress
    missingok
    notifempty
    create 0640 beast beast
}
EOF
```

---

## 📞 SUPPORT

### Collect Debug Information
```bash
# Create diagnostic bundle
sudo bash -c '
cd /tmp
mkdir beast-diagnostics-$(date +%Y%m%d_%H%M%S)
cd beast-diagnostics-$(date +%Y%m%d_%H%M%S)
cp /var/log/beast-swarm/*.log .
systemctl status beast-swarm.service > service-status.txt
ps aux | grep python3 > process-info.txt
df -h > disk-info.txt
free -h > memory-info.txt
tar -czf ../beast-diagnostics-$(date +%Y%m%d_%H%M%S).tar.gz .
'
```

### Contact Information
- **Documentation**: /opt/beast-swarm/BEAST-SWARM-COMPLETION-REPORT.md
- **Performance Data**: /opt/beast-swarm/phase4_6_testing_results.json
- **Configuration Guide**: /opt/beast-swarm/README.md

---

## ✅ CHECKLIST FOR PRODUCTION DEPLOYMENT

- [ ] Files transferred to production server
- [ ] `deploy_beast_swarm.sh` executed successfully
- [ ] `health_check.sh` shows all 7/7 checks passing
- [ ] Service running: `systemctl status beast-swarm.service`
- [ ] Monitoring started: `monitor.sh` running in background
- [ ] Firewall configured to allow port 5000
- [ ] Log rotation configured
- [ ] Backup created and verified
- [ ] Rollback procedure tested
- [ ] Documentation reviewed and archived

---

## 📚 RELATED DOCUMENTATION

- **FINAL-PROJECT-REPORT.md** - Overall project completion
- **BEAST-SWARM-COMPLETION-REPORT.md** - Detailed optimization report
- **TASK-3-BEAST-SWARM-EXECUTION-PLAN.md** - Detailed technical plan
- **PROJECT-COMPLETION-SUMMARY.md** - Project statistics

---

*Quick Reference Guide - Last Updated: November 21, 2025*  
*For detailed information, see full documentation in installation directory*
