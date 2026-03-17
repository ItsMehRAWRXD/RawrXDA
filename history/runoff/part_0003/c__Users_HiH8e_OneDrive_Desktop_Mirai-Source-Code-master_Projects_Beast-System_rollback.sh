#!/bin/bash
###############################################################################
# BEAST SWARM ROLLBACK SCRIPT
# Safely revert to previous version
###############################################################################

BEAST_HOME="${BEAST_HOME:=/opt/beast-swarm}"
BACKUP_DIR="/var/backups/beast-swarm"
LOG_FILE="/var/log/beast-swarm/rollback.log"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_FILE"
}

log_debug() {
    echo -e "${BLUE}[DEBUG]${NC} $1" | tee -a "$LOG_FILE"
}

# ============================================================================
# ROLLBACK OPERATIONS
# ============================================================================

list_backups() {
    log_info "Available backups:"
    echo ""
    ls -lh "$BACKUP_DIR"/beast-swarm-*.tar.gz 2>/dev/null | \
        awk '{print $9 " (" $5 ") - " $6 " " $7 " " $8}' | \
        nl
    echo ""
}

backup_current() {
    log_info "Backing up current version before rollback..."
    
    local current_backup="$BACKUP_DIR/beast-swarm-pre-rollback-$(date +%Y%m%d_%H%M%S).tar.gz"
    
    if [ -d "$BEAST_HOME" ]; then
        tar -czf "$current_backup" -C "$(dirname "$BEAST_HOME")" "$(basename "$BEAST_HOME")" 2>/dev/null
        log_info "Current version backed up to: $current_backup"
    fi
}

perform_rollback() {
    local backup_file="$1"
    
    if [ ! -f "$backup_file" ]; then
        log_error "Backup file not found: $backup_file"
        return 1
    fi
    
    log_info "Rolling back from: $backup_file"
    
    # Stop service
    log_info "Stopping Beast Swarm service..."
    systemctl stop beast-swarm.service 2>/dev/null || true
    sleep 2
    
    # Remove current installation
    log_info "Removing current installation..."
    rm -rf "$BEAST_HOME"
    
    # Restore from backup
    log_info "Restoring from backup..."
    tar -xzf "$backup_file" -C "$(dirname "$BEAST_HOME")"
    
    if [ $? -ne 0 ]; then
        log_error "Rollback failed"
        return 1
    fi
    
    # Start service
    log_info "Starting Beast Swarm service..."
    systemctl start beast-swarm.service
    sleep 2
    
    if systemctl is-active --quiet beast-swarm.service; then
        log_info "✅ Rollback completed successfully"
        return 0
    else
        log_error "Service failed to start after rollback"
        return 1
    fi
}

# ============================================================================
# VALIDATION
# ============================================================================

validate_rollback() {
    log_info "Validating rollback..."
    
    local checks=0
    local passed=0
    
    # Check service
    ((checks++))
    if systemctl is-active --quiet beast-swarm.service; then
        log_info "✅ Service is running"
        ((passed++))
    else
        log_error "❌ Service is not running"
    fi
    
    # Check process
    ((checks++))
    if pgrep -f "python3.*beast" > /dev/null; then
        log_info "✅ Process is running"
        ((passed++))
    else
        log_error "❌ Process is not running"
    fi
    
    # Check directories
    ((checks++))
    if [ -d "$BEAST_HOME" ]; then
        log_info "✅ Installation directory exists"
        ((passed++))
    else
        log_error "❌ Installation directory not found"
    fi
    
    if [ $passed -eq $checks ]; then
        log_info "✅ All validation checks passed"
        return 0
    else
        log_error "Validation failed: $passed/$checks checks passed"
        return 1
    fi
}

# ============================================================================
# MAIN ROLLBACK
# ============================================================================

main() {
    mkdir -p "$(dirname "$LOG_FILE")"
    
    echo ""
    echo "========== BEAST SWARM ROLLBACK =========="
    echo "Timestamp: $(date)"
    echo ""
    
    if [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
        cat <<EOF
Usage: $0 [OPTIONS]

Options:
    -h, --help          Show this help message
    -l, --list          List available backups
    -r, --rollback FILE Rollback to specific backup file
    
Examples:
    $0 --list
    $0 --rollback /var/backups/beast-swarm/beast-swarm-20250121_120000.tar.gz

EOF
        exit 0
    fi
    
    if [ "$1" == "-l" ] || [ "$1" == "--list" ]; then
        list_backups
        exit 0
    fi
    
    if [ "$1" == "-r" ] || [ "$1" == "--rollback" ]; then
        if [ -z "$2" ]; then
            log_error "No backup file specified"
            exit 1
        fi
        
        backup_current
        perform_rollback "$2"
        exit_code=$?
        
        if [ $exit_code -eq 0 ]; then
            validate_rollback
        fi
        
        exit $exit_code
    fi
    
    # Default: interactive selection
    log_info "Available backups:"
    list_backups
    
    read -p "Enter backup filename to restore (or 'q' to quit): " backup_choice
    
    if [ "$backup_choice" == "q" ]; then
        log_info "Rollback cancelled"
        exit 0
    fi
    
    backup_current
    perform_rollback "$BACKUP_DIR/$backup_choice"
    exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        validate_rollback
    fi
    
    echo ""
    exit $exit_code
}

main "$@"
