#!/bin/bash
# RawrXD Universal Access — Deployment Verification Script
# Tests all components and validates deployment

# set -e  # Don't exit on first error, we want to see all results

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PASS=0
FAIL=0

log_pass() { 
    echo -e "${GREEN}✓${NC} $1" 
    ((PASS++))
}

log_fail() { 
    echo -e "${RED}✗${NC} $1" 
    ((FAIL++))
}

log_info() { 
    echo -e "${BLUE}ℹ${NC} $1" 
}

log_warn() { 
    echo -e "${YELLOW}⚠${NC} $1" 
}

header() {
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════${NC}"
    echo ""
}

# Check if file exists
check_file() {
    if [ -f "$1" ]; then
        log_pass "$1 exists"
        return 0
    else
        log_fail "$1 missing"
        return 1
    fi
}

# Check if directory exists
check_dir() {
    if [ -d "$1" ]; then
        log_pass "$1/ exists"
        return 0
    else
        log_fail "$1/ missing"
        return 1
    fi
}

# Check if file is executable
check_executable() {
    if [ -x "$1" ]; then
        log_pass "$1 is executable"
        return 0
    else
        log_fail "$1 not executable"
        return 1
    fi
}

# Test HTTP endpoint
test_endpoint() {
    local url=$1
    local name=$2
    
    if curl -sf "$url" > /dev/null 2>&1; then
        log_pass "$name: $url"
        return 0
    else
        log_fail "$name: $url unreachable"
        return 1
    fi
}

# Main verification
main() {
    header "RawrXD Universal Access — Deployment Verification"
    
    log_info "Checking file structure..."
    
    # Web Interface
    header "Web Interface Files"
    check_file "web_interface/index.html"
    check_file "web_interface/manifest.json"
    
    # CORS Middleware
    header "Backend Middleware"
    check_file "Ship/CORSAuthMiddleware.py"
    
    # Platform Launchers
    header "Platform Launchers"
    check_file "wrapper/launch-linux.sh"
    check_executable "wrapper/launch-linux.sh"
    check_file "wrapper/launch-macos.sh"
    check_executable "wrapper/launch-macos.sh"
    
    # Docker Files
    header "Docker Configuration"
    check_file "docker/Dockerfile.full"
    check_file "docker/Dockerfile.backend"
    check_file "docker/nginx.conf"
    check_file "docker/supervisord.conf"
    check_file "docker-compose.yml"
    
    # Documentation
    header "Documentation"
    check_file "UNIVERSAL_ACCESS_DEPLOYMENT.md"
    check_file "UNIVERSAL_ACCESS_IMPLEMENTATION_SUMMARY.md"
    check_file "UNIVERSAL_ACCESS_QUICK_START.md"
    
    # Test web UI validity
    header "Web UI Validation"
    if grep -q "RawrXD Agentic IDE" web_interface/index.html; then
        log_pass "Web UI title found"
    else
        log_fail "Web UI title missing"
    fi
    
    if grep -q "sendMessage" web_interface/index.html; then
        log_pass "JavaScript functions present"
    else
        log_fail "JavaScript functions missing"
    fi
    
    # Test middleware
    header "Python Middleware Validation"
    if grep -q "UniversalAccessGateway" Ship/CORSAuthMiddleware.py; then
        log_pass "Gateway class found"
    else
        log_fail "Gateway class missing"
    fi
    
    if grep -q "generate_api_key" Ship/CORSAuthMiddleware.py; then
        log_pass "API key generator found"
    else
        log_fail "API key generator missing"
    fi
    
    # Test launchers
    header "Launcher Script Validation"
    if grep -q "BACKEND_ONLY" wrapper/launch-linux.sh; then
        log_pass "Linux backend-only mode supported"
    else
        log_fail "Linux backend-only mode missing"
    fi
    
    if grep -q "check_rosetta" wrapper/launch-macos.sh; then
        log_pass "macOS Rosetta detection found"
    else
        log_fail "macOS Rosetta detection missing"
    fi
    
    # Test Docker config
    header "Docker Configuration Validation"
    if grep -q "rawrxd-backend" docker-compose.yml; then
        log_pass "Backend service defined"
    else
        log_fail "Backend service missing"
    fi
    
    if grep -q "rawrxd-web" docker-compose.yml; then
        log_pass "Web service defined"
    else
        log_fail "Web service missing"
    fi
    
    if grep -q "rawrxd-desktop" docker-compose.yml; then
        log_pass "Desktop service defined (optional)"
    else
        log_warn "Desktop service missing (optional)"
    fi
    
    # Optional: Test live endpoints
    header "Live Endpoint Testing (Optional)"
    
    log_info "Checking if services are running..."
    
    # Test backend
    if curl -sf http://localhost:23959/status > /dev/null 2>&1; then
        log_pass "Backend running on :23959"
        
        # Test response
        RESPONSE=$(curl -s http://localhost:23959/status)
        if echo "$RESPONSE" | grep -q "ok"; then
            log_pass "Backend status: OK"
        else
            log_warn "Backend status: Unknown response"
        fi
    else
        log_warn "Backend not running (optional test)"
    fi
    
    # Test web UI
    if curl -sf http://localhost > /dev/null 2>&1; then
        log_pass "Web UI accessible on :80"
    else
        log_warn "Web UI not accessible (optional test)"
    fi
    
    # Test Docker services
    if command -v docker-compose > /dev/null 2>&1; then
        if docker-compose ps | grep -q "rawrxd"; then
            log_pass "Docker containers found"
        else
            log_warn "Docker containers not running (optional)"
        fi
    else
        log_warn "docker-compose not installed (optional)"
    fi
    
    # Summary
    header "Verification Summary"
    
    TOTAL=$((PASS + FAIL))
    if command -v bc > /dev/null 2>&1; then
        PERCENTAGE=$(echo "scale=1; ($PASS * 100) / $TOTAL" | bc)
    else
        PERCENTAGE=$((PASS * 100 / TOTAL))
    fi
    
    echo ""
    echo -e "  ${GREEN}Passed:${NC} $PASS"
    echo -e "  ${RED}Failed:${NC} $FAIL"
    echo -e "  Total:  $TOTAL"
    echo -e "  Score:  ${PERCENTAGE}%"
    echo ""
    
    if [ $FAIL -eq 0 ]; then
        echo -e "${GREEN}🎉 All checks passed! Universal Access is ready to deploy.${NC}"
        exit 0
    else
        echo -e "${RED}❌ Some checks failed. Review errors above.${NC}"
        exit 1
    fi
}

main
