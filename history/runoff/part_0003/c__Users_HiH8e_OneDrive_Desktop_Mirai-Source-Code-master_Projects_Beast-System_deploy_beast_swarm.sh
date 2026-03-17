#!/bin/bash
###############################################################################
# BEAST SWARM DEPLOYMENT SCRIPT
# Phase 3: Deployment Tooling
# ============================================================================
# Purpose: Deploy Beast Swarm to production environment
# Features: Pre-flight checks, installation, verification, rollback support
###############################################################################

set -e  # Exit on any error

# Configuration
BEAST_VERSION="1.0.0"
BEAST_HOME="/opt/beast-swarm"
BEAST_USER="beast"
BACKUP_DIR="/var/backups/beast-swarm"
LOG_FILE="/var/log/beast-swarm/deploy.log"
ERROR_LOG="/var/log/beast-swarm/errors.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'  # No Color

# ============================================================================
# LOGGING FUNCTIONS
# ============================================================================

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$ERROR_LOG"
}

log_debug() {
    echo -e "${BLUE}[DEBUG]${NC} $1" | tee -a "$LOG_FILE"
}

# ============================================================================
# PRE-FLIGHT CHECKS
# ============================================================================

check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "This script must be run as root"
        exit 1
    fi
}

check_dependencies() {
    log_info "Checking dependencies..."
    
    local deps=("python3" "bash" "grep" "sed" "awk" "tar" "gzip")
    local missing_deps=()
    
    for dep in "${deps[@]}"; do
        if ! command -v "$dep" &> /dev/null; then
            missing_deps+=("$dep")
        fi
    done
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing dependencies: ${missing_deps[*]}"
        return 1
    fi
    
    log_info "All dependencies found"
    return 0
}

check_disk_space() {
    log_info "Checking disk space..."
    
    local required_mb=500
    local available_mb=$(df "$BEAST_HOME" | awk 'NR==2 {print $4}')
    available_mb=$((available_mb / 1024))
    
    if [ "$available_mb" -lt "$required_mb" ]; then
        log_error "Insufficient disk space. Required: ${required_mb}MB, Available: ${available_mb}MB"
        return 1
    fi
    
    log_info "Disk space check passed: ${available_mb}MB available"
    return 0
}

check_python_version() {
    log_info "Checking Python version..."
    
    local python_version=$(python3 --version 2>&1 | awk '{print $2}')
    local major=$(echo "$python_version" | cut -d. -f1)
    local minor=$(echo "$python_version" | cut -d. -f2)
    
    if [ "$major" -lt 3 ] || ([ "$major" -eq 3 ] && [ "$minor" -lt 8 ]); then
        log_error "Python 3.8+ required, found Python $python_version"
        return 1
    fi
    
    log_info "Python version OK: $python_version"
    return 0
}

run_preflight_checks() {
    log_info "===== PRE-FLIGHT CHECKS ====="
    
    check_root || return 1
    check_dependencies || return 1
    check_disk_space || return 1
    check_python_version || return 1
    
    log_info "✅ All pre-flight checks passed"
    return 0
}

# ============================================================================
# INSTALLATION FUNCTIONS
# ============================================================================

create_user() {
    log_info "Creating Beast Swarm user..."
    
    if id "$BEAST_USER" &>/dev/null; then
        log_warn "User '$BEAST_USER' already exists"
    else
        useradd -r -s /bin/bash -d "$BEAST_HOME" -m "$BEAST_USER"
        log_info "User '$BEAST_USER' created"
    fi
}

create_directories() {
    log_info "Creating directories..."
    
    mkdir -p "$BEAST_HOME"
    mkdir -p "$BEAST_HOME/bin"
    mkdir -p "$BEAST_HOME/lib"
    mkdir -p "$BEAST_HOME/config"
    mkdir -p "$BEAST_HOME/data"
    mkdir -p "$(dirname "$LOG_FILE")"
    mkdir -p "$BACKUP_DIR"
    
    chown -R "$BEAST_USER:$BEAST_USER" "$BEAST_HOME"
    chmod 750 "$BEAST_HOME"
    
    log_info "Directories created"
}

install_files() {
    log_info "Installing Beast Swarm files..."
    
    local source_dir="${1:-.}"
    
    if [ ! -d "$source_dir" ]; then
        log_error "Source directory not found: $source_dir"
        return 1
    fi
    
    # Copy main files
    cp -r "$source_dir"/*.py "$BEAST_HOME/lib/" 2>/dev/null || true
    cp -r "$source_dir"/config/* "$BEAST_HOME/config/" 2>/dev/null || true
    
    chown -R "$BEAST_USER:$BEAST_USER" "$BEAST_HOME"
    log_info "Files installed to $BEAST_HOME"
}

setup_systemd() {
    log_info "Setting up systemd service..."
    
    cat > /etc/systemd/system/beast-swarm.service <<EOF
[Unit]
Description=Beast Swarm Service
After=network.target

[Service]
Type=simple
User=$BEAST_USER
WorkingDirectory=$BEAST_HOME
ExecStart=/usr/bin/python3 $BEAST_HOME/lib/beast_main.py
Restart=on-failure
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
    
    systemctl daemon-reload
    log_info "Systemd service configured"
}

# ============================================================================
# VERIFICATION FUNCTIONS
# ============================================================================

verify_installation() {
    log_info "===== VERIFICATION ====="
    
    local failed=0
    
    # Check directories
    if [ ! -d "$BEAST_HOME" ]; then
        log_error "Beast Swarm home directory not found: $BEAST_HOME"
        ((failed++))
    fi
    
    # Check user
    if ! id "$BEAST_USER" &>/dev/null; then
        log_error "Beast Swarm user not found: $BEAST_USER"
        ((failed++))
    fi
    
    # Check Python files
    local py_files=$(find "$BEAST_HOME/lib" -name "*.py" 2>/dev/null | wc -l)
    if [ "$py_files" -eq 0 ]; then
        log_warn "No Python files found in $BEAST_HOME/lib"
    else
        log_info "✅ Found $py_files Python files"
    fi
    
    # Check permissions
    local owner=$(stat -c %U "$BEAST_HOME" 2>/dev/null || echo "unknown")
    if [ "$owner" != "$BEAST_USER" ]; then
        log_warn "Beast Swarm directory not owned by $BEAST_USER"
        ((failed++))
    fi
    
    if [ $failed -eq 0 ]; then
        log_info "✅ Installation verification passed"
        return 0
    else
        log_error "Installation verification failed with $failed issues"
        return 1
    fi
}

# ============================================================================
# BACKUP & ROLLBACK FUNCTIONS
# ============================================================================

create_backup() {
    log_info "Creating backup..."
    
    local backup_file="$BACKUP_DIR/beast-swarm-$(date +%Y%m%d_%H%M%S).tar.gz"
    
    if [ -d "$BEAST_HOME" ]; then
        tar -czf "$backup_file" -C "$(dirname "$BEAST_HOME")" "$(basename "$BEAST_HOME")" 2>/dev/null || true
        log_info "Backup created: $backup_file"
        echo "$backup_file"
    fi
}

rollback() {
    log_info "Rolling back deployment..."
    
    local latest_backup=$(ls -t "$BACKUP_DIR"/beast-swarm-*.tar.gz 2>/dev/null | head -1)
    
    if [ -z "$latest_backup" ]; then
        log_error "No backup found for rollback"
        return 1
    fi
    
    log_info "Restoring from: $latest_backup"
    rm -rf "$BEAST_HOME"
    tar -xzf "$latest_backup" -C "$(dirname "$BEAST_HOME")"
    
    log_info "✅ Rollback completed"
    return 0
}

# ============================================================================
# MAIN DEPLOYMENT
# ============================================================================

deploy() {
    log_info "========== BEAST SWARM DEPLOYMENT =========="
    log_info "Version: $BEAST_VERSION"
    log_info "Target: $BEAST_HOME"
    log_info "Started: $(date)"
    
    # Pre-flight
    if ! run_preflight_checks; then
        log_error "Pre-flight checks failed"
        return 1
    fi
    
    # Backup existing installation
    if [ -d "$BEAST_HOME" ]; then
        create_backup
    fi
    
    # Installation
    log_info "===== INSTALLATION ====="
    create_user
    create_directories
    install_files "${1:-.}"
    setup_systemd
    
    # Verification
    if ! verify_installation; then
        log_error "Installation verification failed, rolling back..."
        rollback
        return 1
    fi
    
    # Start service
    log_info "Starting Beast Swarm service..."
    systemctl enable beast-swarm.service
    systemctl start beast-swarm.service
    
    log_info "========== DEPLOYMENT COMPLETED =========="
    log_info "Completed: $(date)"
    log_info "Logs: $LOG_FILE"
    
    return 0
}

# ============================================================================
# SCRIPT ENTRY POINT
# ============================================================================

main() {
    # Create log directory
    mkdir -p "$(dirname "$LOG_FILE")"
    
    # Show help
    if [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
        cat <<EOF
Usage: $0 [OPTIONS] [SOURCE_DIR]

Options:
    -h, --help      Show this help message
    --version       Show version
    --rollback      Rollback to previous version
    --check         Run pre-flight checks only

Arguments:
    SOURCE_DIR      Source directory containing Beast Swarm files (default: current dir)

Examples:
    $0 .
    $0 /opt/beast-source
    $0 --rollback
    $0 --check

EOF
        exit 0
    fi
    
    if [ "$1" == "--version" ]; then
        echo "Beast Swarm Deployment v$BEAST_VERSION"
        exit 0
    fi
    
    if [ "$1" == "--rollback" ]; then
        rollback
        exit $?
    fi
    
    if [ "$1" == "--check" ]; then
        run_preflight_checks
        exit $?
    fi
    
    # Default deployment
    deploy "$1"
}

main "$@"
