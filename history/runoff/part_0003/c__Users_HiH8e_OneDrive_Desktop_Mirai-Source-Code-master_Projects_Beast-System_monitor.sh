#!/bin/bash
###############################################################################
# BEAST SWARM MONITORING SCRIPT
# Continuous monitoring and alerting
###############################################################################

BEAST_HOME="${BEAST_HOME:=/opt/beast-swarm}"
LOG_DIR="/var/log/beast-swarm"
ALERT_LOG="$LOG_DIR/alerts.log"
METRICS_LOG="$LOG_DIR/metrics.log"
INTERVAL="${MONITOR_INTERVAL:=60}"  # Check every 60 seconds

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_alert() {
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] ALERT: $1" | tee -a "$ALERT_LOG"
}

log_metric() {
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] METRIC: $1" >> "$METRICS_LOG"
}

# ============================================================================
# MONITORING FUNCTIONS
# ============================================================================

monitor_cpu() {
    local pid=$(pgrep -f "python3.*beast" | head -1)
    
    if [ -z "$pid" ]; then
        return 1
    fi
    
    local cpu=$(ps -p "$pid" -o %cpu= | awk '{print int($1)}')
    
    log_metric "CPU Usage: ${cpu}%"
    
    if [ "$cpu" -gt 80 ]; then
        log_alert "High CPU usage detected: ${cpu}%"
    fi
    
    echo "$cpu"
}

monitor_memory() {
    local pid=$(pgrep -f "python3.*beast" | head -1)
    
    if [ -z "$pid" ]; then
        return 1
    fi
    
    local mem=$(ps -p "$pid" -o %mem= | awk '{print $1}')
    local rss=$(ps -p "$pid" -o rss= | awk '{print int($1/1024)}')
    
    log_metric "Memory Usage: ${mem}% (${rss}MB)"
    
    if (( $(echo "$mem > 50" | bc -l) )); then
        log_alert "High memory usage detected: ${mem}%"
    fi
    
    echo "$rss"
}

monitor_disk() {
    local available=$(df "$BEAST_HOME" | awk 'NR==2 {print int($4/1024)}')
    
    log_metric "Disk Space: ${available}MB available"
    
    if [ "$available" -lt 100 ]; then
        log_alert "Low disk space: ${available}MB"
    fi
}

monitor_service() {
    if ! systemctl is-active --quiet beast-swarm.service; then
        log_alert "Beast Swarm service is not running"
        echo "FAILED"
        return 1
    fi
    
    local pid=$(pgrep -f "python3.*beast" | head -1)
    if [ -z "$pid" ]; then
        log_alert "Beast Swarm process not found"
        echo "MISSING"
        return 1
    fi
    
    echo "RUNNING"
    return 0
}

monitor_connectivity() {
    local beast_port="${BEAST_PORT:=5000}"
    
    if netstat -tuln 2>/dev/null | grep -q ":$beast_port"; then
        log_metric "Port $beast_port: LISTENING"
        echo "OK"
        return 0
    else
        log_alert "Beast Swarm not listening on port $beast_port"
        echo "CLOSED"
        return 1
    fi
}

monitor_error_rate() {
    if [ ! -f "$LOG_DIR/errors.log" ]; then
        return 0
    fi
    
    local errors=$(tail -100 "$LOG_DIR/errors.log" 2>/dev/null | grep -c "ERROR\|CRITICAL" || echo 0)
    
    log_metric "Recent Errors: $errors"
    
    if [ "$errors" -gt 5 ]; then
        log_alert "High error rate detected: $errors errors in last 100 logs"
    fi
}

# ============================================================================
# MONITORING LOOP
# ============================================================================

continuous_monitor() {
    mkdir -p "$LOG_DIR"
    
    echo "Beast Swarm Monitoring Started - $(date)"
    echo "Checking every $INTERVAL seconds..."
    echo "Alerts: $ALERT_LOG"
    echo "Metrics: $METRICS_LOG"
    echo ""
    
    local cycle=0
    
    while true; do
        ((cycle++))
        echo ""
        echo "=== MONITORING CYCLE $cycle: $(date +'%H:%M:%S') ==="
        
        # Monitor all metrics
        local service_status=$(monitor_service)
        local cpu_usage=$(monitor_cpu)
        local mem_usage=$(monitor_memory)
        monitor_disk
        local connectivity=$(monitor_connectivity)
        monitor_error_rate
        
        # Display current status
        echo ""
        echo "Status Summary:"
        echo "  Service: $service_status"
        echo "  CPU: ${cpu_usage}%"
        echo "  Memory: ${mem_usage}MB"
        echo "  Connectivity: $connectivity"
        
        # Sleep before next check
        sleep "$INTERVAL"
    done
}

# ============================================================================
# MAIN
# ============================================================================

main() {
    if [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
        cat <<EOF
Usage: $0 [OPTIONS]

Options:
    -h, --help      Show this help message
    -i, --interval N Set monitoring interval in seconds (default: 60)
    -c, --check     Run single check cycle only
    -m, --metrics   Show last 20 metrics
    -a, --alerts    Show last 20 alerts
    
Examples:
    $0                  # Start continuous monitoring
    $0 -i 30            # Monitor every 30 seconds
    $0 -c               # Run single check and exit
    $0 -m               # Show recent metrics

EOF
        exit 0
    fi
    
    if [ "$1" == "-i" ] || [ "$1" == "--interval" ]; then
        INTERVAL="$2"
        shift 2
    fi
    
    if [ "$1" == "-c" ] || [ "$1" == "--check" ]; then
        mkdir -p "$LOG_DIR"
        echo "=== Single Monitoring Check: $(date) ==="
        monitor_service
        monitor_cpu
        monitor_memory
        monitor_disk
        monitor_connectivity
        monitor_error_rate
        exit 0
    fi
    
    if [ "$1" == "-m" ] || [ "$1" == "--metrics" ]; then
        if [ -f "$METRICS_LOG" ]; then
            echo "=== Recent Metrics ==="
            tail -20 "$METRICS_LOG"
        else
            echo "No metrics available"
        fi
        exit 0
    fi
    
    if [ "$1" == "-a" ] || [ "$1" == "--alerts" ]; then
        if [ -f "$ALERT_LOG" ]; then
            echo "=== Recent Alerts ==="
            tail -20 "$ALERT_LOG"
        else
            echo "No alerts available"
        fi
        exit 0
    fi
    
    # Default: continuous monitoring
    continuous_monitor
}

main "$@"
