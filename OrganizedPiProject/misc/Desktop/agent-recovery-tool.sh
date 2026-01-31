#!/bin/bash

# Agent Recovery Tool - Restore Communication with Silent AI Agents
# This tool helps diagnose and recover from agent communication failures

set -e

echo "🔧 Agent Recovery Tool"
echo "======================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check agent communication status
check_agent_status() {
    print_status "Checking agent communication status..."
    
    # Check for running AI processes
    local ai_processes=$(ps aux | grep -i "ai\|agent\|orchestrator" | grep -v grep | wc -l)
    
    if [ $ai_processes -gt 0 ]; then
        print_success "Found $ai_processes AI-related processes running"
        ps aux | grep -i "ai\|agent\|orchestrator" | grep -v grep
    else
        print_warning "No AI processes currently running"
    fi
    
    # Check for agent logs
    if [ -d "logs" ]; then
        print_status "Checking agent logs..."
        find logs -name "*agent*" -o -name "*ai*" -o -name "*orchestrator*" 2>/dev/null | head -5
    fi
    
    # Check for communication files
    if [ -f ".agent-status" ] || [ -f "agent-communication.json" ]; then
        print_status "Found agent communication files"
        ls -la .agent-status agent-communication.json 2>/dev/null
    fi
}

# Function to restart agent communication
restart_agent_communication() {
    print_status "Attempting to restart agent communication..."
    
    # Kill any stuck AI processes
    pkill -f "AgenticOrchestrator" 2>/dev/null || true
    pkill -f "ai-cli" 2>/dev/null || true
    
    # Clear communication locks
    rm -f .agent-lock .communication-lock 2>/dev/null || true
    
    # Restart the main system
    if [ -f "AgenticOrchestrator.java" ]; then
        print_status "Restarting AgenticOrchestrator..."
        # This would typically involve:
        # 1. Recompiling if needed
        # 2. Starting the orchestrator
        # 3. Re-establishing agent connections
        print_warning "Manual restart required - please run your AI system manually"
    fi
}

# Function to recover agent state
recover_agent_state() {
    print_status "Attempting to recover agent state..."
    
    # Look for agent recovery data
    if [ -d "agent-recovery" ]; then
        print_status "Found agent recovery directory"
        ls -la agent-recovery/
        
        # Check for silent agents
        if [ -f "agent-recovery/silent-agents.json" ]; then
            print_warning "Found silent agents list"
            cat agent-recovery/silent-agents.json
        fi
    fi
    
    # Check for recent agent actions
    if [ -d "agent-recovery/actions" ]; then
        print_status "Recent agent actions:"
        ls -la agent-recovery/actions/ | head -10
    fi
}

# Function to re-establish agent communication
reestablish_communication() {
    print_status "Re-establishing agent communication..."
    
    # Create communication health check
    cat > agent-health-check.json << EOF
{
    "timestamp": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
    "status": "recovery_attempt",
    "agents": [],
    "last_communication": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
    "recovery_initiated": true
}
EOF
    
    print_success "Created agent health check file"
    
    # Send recovery signal (this would typically be an API call)
    print_status "Sending recovery signal to agents..."
    print_warning "Manual intervention may be required to fully restore communication"
}

# Function to diagnose communication failure
diagnose_failure() {
    print_status "Diagnosing agent communication failure..."
    
    # Check system resources
    print_status "System resources:"
    echo "Memory usage:"
    free -h
    echo ""
    echo "Disk usage:"
    df -h /
    echo ""
    
    # Check for error logs
    print_status "Checking for error logs..."
    find . -name "*.log" -exec grep -l "error\|exception\|failed" {} \; 2>/dev/null | head -5
    
    # Check network connectivity (if applicable)
    if command -v curl >/dev/null 2>&1; then
        print_status "Checking network connectivity..."
        curl -s --connect-timeout 5 http://localhost:8080 >/dev/null && print_success "Local API accessible" || print_warning "Local API not accessible"
    fi
}

# Main recovery sequence
main() {
    echo "Starting agent recovery process..."
    echo ""
    
    # Step 1: Check current status
    check_agent_status
    echo ""
    
    # Step 2: Diagnose the failure
    diagnose_failure
    echo ""
    
    # Step 3: Attempt to recover agent state
    recover_agent_state
    echo ""
    
    # Step 4: Restart communication
    restart_agent_communication
    echo ""
    
    # Step 5: Re-establish communication
    reestablish_communication
    echo ""
    
    print_success "Agent recovery process completed!"
    print_warning "Please manually verify that your AI agents are responding"
    print_status "Check the agent-health-check.json file for status"
}

# Run the recovery tool
main "$@"
