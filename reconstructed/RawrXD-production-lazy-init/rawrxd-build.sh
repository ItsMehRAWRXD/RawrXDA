#!/bin/bash
# RawrXD Universal Compiler - Bash Wrapper
# Provides CLI access across Linux/Mac/WSL environments
# Usage: ./rawrxd-build.sh --validate --json
# Usage: ./rawrxd-build.sh --build

set -e

PROJECT_ROOT="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/universal_compiler.py"

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

show_help() {
    echo -e "\n${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║        RawrXD Universal Compiler & Build System           ║${NC}"
    echo -e "${CYAN}║                   Bash Wrapper v1.0                       ║${NC}"
    echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}\n"

    echo -e "${YELLOW}📋 USAGE:${NC}"
    echo "  ./rawrxd-build.sh [options]\n"

    echo -e "${YELLOW}🎯 OPTIONS:${NC}"
    echo -e "  ${GREEN}--validate${NC}          Run full validation suite (default)"
    echo -e "  ${GREEN}--audit${NC}             Audit compilation environment only"
    echo -e "  ${GREEN}--build${NC}             Compile with CMake"
    echo -e "  ${GREEN}--compile-direct${NC}    Direct compilation without CMake"
    echo -e "  ${GREEN}--json${NC}              Output results as JSON"
    echo -e "  ${GREEN}--project-root${NC}     Project root directory"
    echo -e "  ${GREEN}--help${NC}              Show this help message\n"

    echo -e "${YELLOW}📌 EXAMPLES:${NC}"
    echo -e "  ${CYAN}./rawrxd-build.sh${NC}                     # Full validation"
    echo -e "  ${CYAN}./rawrxd-build.sh --build${NC}            # Build with CMake"
    echo -e "  ${CYAN}./rawrxd-build.sh --audit --json${NC}     # Audit as JSON"
    echo -e "  ${CYAN}./rawrxd-build.sh --project-root /path${NC} # Custom path\n"
}

check_python() {
    if ! command -v python3 &> /dev/null && ! command -v python &> /dev/null; then
        echo -e "${RED}❌ Error: Python 3 not found${NC}"
        echo -e "${YELLOW}Install via:${NC}"
        echo "  Ubuntu/Debian: sudo apt-get install python3"
        echo "  macOS: brew install python3"
        echo "  WSL: sudo apt-get install python3"
        return 1
    fi
    return 0
}

check_cmake() {
    if command -v cmake &> /dev/null; then
        echo -e "${GREEN}✓ CMake: Found${NC}"
        return 0
    else
        echo -e "${YELLOW}⚠ CMake: Not found (optional for direct compilation)${NC}"
        return 1
    fi
}

check_compiler() {
    if command -v g++ &> /dev/null; then
        echo -e "${GREEN}✓ G++: Found${NC}"
        return 0
    elif command -v clang++ &> /dev/null; then
        echo -e "${GREEN}✓ Clang++: Found${NC}"
        return 0
    else
        echo -e "${YELLOW}⚠ Compiler: Not found${NC}"
        return 1
    fi
}

verify_environment() {
    echo -e "\n${YELLOW}🔍 Pre-flight checks:${NC}"
    
    if ! check_python; then
        return 1
    fi
    
    check_cmake
    check_compiler
    
    if [ ! -f "$PYTHON_SCRIPT" ]; then
        echo -e "${RED}❌ Error: universal_compiler.py not found at $PYTHON_SCRIPT${NC}"
        return 1
    fi
    echo -e "${GREEN}✓ Build script: Found${NC}"
    
    return 0
}

invoke_python() {
    local args=$@
    
    if [ ! -f "$PYTHON_SCRIPT" ]; then
        echo -e "${RED}❌ Error: universal_compiler.py not found${NC}"
        exit 1
    fi

    echo -e "\n${YELLOW}🚀 Executing: python $PYTHON_SCRIPT $args${NC}\n"
    
    if command -v python3 &> /dev/null; then
        python3 "$PYTHON_SCRIPT" --project-root "$PROJECT_ROOT" $args
    else
        python "$PYTHON_SCRIPT" --project-root "$PROJECT_ROOT" $args
    fi
}

# Handle arguments
if [ $# -eq 0 ]; then
    # Default: full validation
    if ! verify_environment; then
        exit 1
    fi
    invoke_python "--validate"
elif [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    show_help
    exit 0
else
    if ! verify_environment; then
        exit 1
    fi
    invoke_python "$@"
fi
