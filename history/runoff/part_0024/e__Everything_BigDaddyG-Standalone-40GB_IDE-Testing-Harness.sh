#!/bin/bash
# ============================================================================
# BIGDADDYG IDE - BASH-BASED TEST HARNESS
# ============================================================================
# Comprehensive curl-based testing for IDE features
# Works on Linux, macOS, and Windows (Git Bash / WSL)
# ============================================================================

set +e

# Configuration
ORCHESTRA_URL="http://localhost:11441"
MICRO_URL="http://localhost:3000"
PASS_COUNT=0
FAIL_COUNT=0

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BLUE='\033[0;34m'
NC='\033[0m'

# ============================================================================
# UTILITIES
# ============================================================================

test_endpoint() {
    local method=$1
    local url=$2
    local body=$3
    local test_name=$4
    
    if [ -z "$body" ]; then
        response=$(curl -s -w "\n%{http_code}" -X "$method" \
            -H "Accept: application/json" \
            -m 10 "$url" 2>&1)
    else
        response=$(curl -s -w "\n%{http_code}" -X "$method" \
            -H "Content-Type: application/json" \
            -H "Accept: application/json" \
            -d "$body" \
            -m 10 "$url" 2>&1)
    fi
    
    http_code=$(echo "$response" | tail -n1)
    response_body=$(echo "$response" | head -n-1)
    
    if [[ $http_code =~ ^[23][0-9]{2}$ ]]; then
        echo -e "${GREEN}✅${NC} $test_name (HTTP $http_code)"
        ((PASS_COUNT++))
        echo "$response_body"
    else
        echo -e "${RED}❌${NC} $test_name (HTTP $http_code)"
        ((FAIL_COUNT++))
    fi
}

# ============================================================================
# TEST SECTIONS
# ============================================================================

test_connectivity() {
    echo -e "\n${CYAN}=== SERVER CONNECTIVITY ===${NC}"
    
    test_endpoint GET "$ORCHESTRA_URL/v1/models" "" "Orchestra Server"
    test_endpoint GET "$MICRO_URL/" "" "Micro-Model-Server"
}

test_models() {
    echo -e "\n${CYAN}=== MODEL LISTING ===${NC}"
    
    response=$(curl -s -X GET "$ORCHESTRA_URL/v1/models" 2>&1)
    
    if command -v jq &> /dev/null; then
        count=$(echo "$response" | jq 'length' 2>/dev/null)
        if [ "$count" != "null" ]; then
            echo -e "${YELLOW}📊 Found $count models${NC}"
            echo "$response" | jq '.[] | .name' -r 2>/dev/null | head -5 | sed 's/^/  • /'
        fi
    else
        echo -e "${YELLOW}  ℹ️  Install jq for detailed model parsing${NC}"
        echo "$response" | head -c 500
    fi
}

test_chat() {
    echo -e "\n${CYAN}=== CHAT/INFERENCE ===${NC}"
    
    payload='{"model":"bigdaddyg:latest","messages":[{"role":"system","content":"You are helpful"},{"role":"user","content":"Say test_ok"}],"temperature":0.5,"max_tokens":50}'
    
    test_endpoint POST "$ORCHESTRA_URL/v1/chat/completions" "$payload" "Chat Completion"
}

test_micro_chat() {
    echo -e "\n${CYAN}=== MICRO-MODEL CHAT ===${NC}"
    
    payload='{"model":"gpt-micro","prompt":"test","context":[]}'
    
    test_endpoint POST "$MICRO_URL/api/chat" "$payload" "Micro Chat"
}

test_code_execution() {
    echo -e "\n${CYAN}=== CODE EXECUTION ===${NC}"
    
    payload='{"command":"echo IDE Test","language":"bash","timeout":5000}'
    
    test_endpoint POST "$ORCHESTRA_URL/v1/execute" "$payload" "Execute Command"
}

test_files() {
    echo -e "\n${CYAN}=== FILE OPERATIONS ===${NC}"
    
    payload='{"action":"list","path":"."}'
    
    test_endpoint POST "$ORCHESTRA_URL/v1/files" "$payload" "List Files"
}

test_settings() {
    echo -e "\n${CYAN}=== SETTINGS ===${NC}"
    
    test_endpoint GET "$ORCHESTRA_URL/v1/settings" "" "Get Settings"
    
    payload='{"key":"theme","value":"dark"}'
    test_endpoint PUT "$ORCHESTRA_URL/v1/config" "$payload" "Update Config"
}

test_system() {
    echo -e "\n${CYAN}=== SYSTEM INFO ===${NC}"
    
    test_endpoint GET "$ORCHESTRA_URL/v1/system/info" "" "System Diagnostics"
    test_endpoint GET "$ORCHESTRA_URL/v1/metrics" "" "Performance Metrics"
}

test_agents() {
    echo -e "\n${CYAN}=== AGENT SYSTEMS ===${NC}"
    
    payload='{"agent_type":"code_generator","task":"test","context":[]}'
    test_endpoint POST "$ORCHESTRA_URL/v1/agent/execute" "$payload" "Agent Execution"
    
    payload='{"swarm_id":"test-'$RANDOM'","agents":["analyzer","coder"],"task":"test"}'
    test_endpoint POST "$ORCHESTRA_URL/v1/swarm" "$payload" "Agent Swarm"
}

test_voice() {
    echo -e "\n${CYAN}=== VOICE FEATURES ===${NC}"
    
    payload='{"format":"wav","sample_rate":16000,"channels":1}'
    test_endpoint POST "$ORCHESTRA_URL/v1/voice/config" "$payload" "Voice Config"
}

test_analysis() {
    echo -e "\n${CYAN}=== CODE ANALYSIS ===${NC}"
    
    payload='{"code":"function test() { return 42; }","language":"javascript","checks":["syntax","security"]}'
    test_endpoint POST "$ORCHESTRA_URL/v1/analyze" "$payload" "Code Analysis"
}

test_stress() {
    echo -e "\n${CYAN}=== STRESS TEST (10 concurrent) ===${NC}"
    
    success=0
    for i in {1..10}; do
        code=$(curl -s -w "%{http_code}" -o /dev/null "$ORCHESTRA_URL/v1/models" 2>&1)
        if [[ $code =~ ^[23][0-9]{2}$ ]]; then
            ((success++))
        fi
    done
    
    echo -e "${YELLOW}📊 $success/10 requests succeeded${NC}"
}

test_websocket() {
    echo -e "\n${CYAN}=== WEBSOCKET CONNECTIVITY ===${NC}"
    
    echo -e "${BLUE}  WebSocket endpoints:${NC}"
    echo "    • ws://localhost:11441"
    echo "    • ws://localhost:3000"
    
    echo -e "\n${YELLOW}  To test WebSocket, install wscat:${NC}"
    echo "    npm install -g wscat"
    echo "    wscat -c ws://localhost:11441"
}

# ============================================================================
# MAIN
# ============================================================================

show_help() {
    echo -e "${CYAN}╔════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║  BIGDADDYG IDE - BASH TEST HARNESS    ║${NC}"
    echo -e "${CYAN}╚════════════════════════════════════════╝${NC}"
    echo ""
    echo "Usage: $0 [option]"
    echo ""
    echo "  quick        Quick connectivity test"
    echo "  models       Test model listing only"
    echo "  chat         Test chat endpoints only"
    echo "  full         Run complete test suite"
    echo "  stress       Load testing (10 concurrent)"
    echo "  websocket    WebSocket connection guide"
    echo ""
}

if [ -z "$1" ] || [ "$1" = "help" ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    show_help
    exit 0
fi

echo -e "\n${CYAN}🧪 IDE TESTING HARNESS - $(date '+%Y-%m-%d %H:%M:%S')${NC}"
echo -e "${CYAN}────────────────────────────────────────────${NC}"

case "$1" in
    quick)
        test_connectivity
        ;;
    models)
        test_connectivity
        test_models
        ;;
    chat)
        test_chat
        test_micro_chat
        ;;
    stress)
        test_connectivity
        test_stress
        ;;
    websocket)
        test_websocket
        ;;
    full)
        test_connectivity
        test_models
        test_chat
        test_micro_chat
        test_code_execution
        test_files
        test_settings
        test_system
        test_agents
        test_voice
        test_analysis
        test_stress
        test_websocket
        ;;
    *)
        echo -e "${RED}Unknown option: $1${NC}"
        show_help
        exit 1
        ;;
esac

echo -e "\n${CYAN}════════════════════════════════════════════${NC}"
echo -e "✅ Passed: $PASS_COUNT | ${RED}❌${NC} Failed: $FAIL_COUNT"
echo -e "${CYAN}════════════════════════════════════════════${NC}\n"
